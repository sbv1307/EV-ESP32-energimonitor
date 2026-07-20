# Tesla Auth Proxy

A small local service that refreshes Tesla OAuth tokens over HTTP/2 + TLS 1.3 and returns normalized token data to the ESP32 over HTTP/1.1.

## Docker Compose

Use [docker-compose.yml](docker-compose.yml) to run the proxy on the Raspberry Pi.

## Porting To Raspberry Pi

This proxy is intentionally small so it can run on the same Raspberry Pi that already hosts TeslaMate.

### Recommended deployment model

1. Keep TeslaMate in its existing container or compose project.
2. Run this proxy as a separate compose service on the same Pi.
3. Bind the proxy to `127.0.0.1` or your LAN-only interface and keep it off the public internet.
4. Let the ESP32 talk to the proxy over normal HTTP/1.1, while the proxy talks to Tesla over HTTP/2 + TLS 1.3.

### Why the Pi is a good host

1. It is already always-on if TeslaMate is running there.
2. Docker keeps the service isolated and easy to restart.
3. The Pi has enough CPU and memory for this proxy with plenty of headroom.
4. You can reuse the same backup, monitoring, and maintenance routine you already have for TeslaMate.

### Porting steps

Run these steps as the `tmmgr` user (the same user that owns the TeslaMate installation). That user already has Docker group membership, so no extra permission setup is needed.

```bash
# SSH into the Pi as tmmgr (or: sudo -u tmmgr -i)
mkdir -p ~/tesla-auth-proxy
cd ~/tesla-auth-proxy
```

1. Copy the contents of this directory to `~/tesla-auth-proxy` on the Pi, or clone the repository and copy just this folder.
2. Create a `.env` file from [.env.example](.env.example) and fill in the secrets and Tesla settings:

   ```bash
   cp .env.example .env
   nano .env   # set PROXY_SHARED_SECRET and any Tesla overrides
   ```

3. Create the data directory so refresh tokens can be persisted:

   ```bash
   mkdir -p data
   ```

4. Start the service:

   ```bash
   docker compose up -d
   ```

5. Verify the service is healthy:

   ```bash
   curl http://127.0.0.1:8787/healthz
   # Expected: {"ok":true, ...}
   ```

6. Point the ESP32 firmware at the proxy URL instead of calling Tesla directly.

### Raspberry Pi prerequisites

1. Docker Engine must be installed.
2. Docker Compose must be available as `docker compose`.
3. The Pi must have outbound internet access to `auth.tesla.com`.
4. The Pi clock should be correct, ideally using NTP, because token refresh uses timestamps and TLS validation.
5. If you enable HMAC request signing, keep the shared secret out of the compose file and place it in `.env` or a mounted secret file.

### Suggested Pi directory layout

```text
/home/tmmgr/tesla-auth-proxy/
  app.py
  Dockerfile
  docker-compose.yml
  requirements.txt
  .env                      ← not committed to git; contains secrets
  data/
    refresh-tokens.json     ← auto-created by the proxy on first refresh
```

Using `/home/tmmgr` rather than `/opt` keeps all Tesla-related services under one user, simplifies backups, and avoids any ownership/permission issues since `tmmgr` already controls that directory.

### Deploying alongside TeslaMate

If TeslaMate is already running in Docker on the Pi, this proxy can be deployed independently.

1. Do not merge it into the TeslaMate container unless you explicitly want a single combined service.
2. Keep the proxy separate so you can upgrade or roll it back without touching TeslaMate.
3. If you already use a reverse proxy such as Traefik, Caddy, Nginx, or a similar front end, add this service as a local-only upstream.

### Updating the service later

1. Pull the latest code.
2. Rebuild the image with `docker compose build`.
3. Restart with `docker compose up -d`.
4. Check `GET /healthz` and then verify a token refresh from the ESP32.

### Backup considerations

1. If you already back up `/home/tmmgr`, both the `data/` directory and `.env` are covered automatically.
2. If you back up selectively, the two critical items are:
   - `~/tesla-auth-proxy/data/refresh-tokens.json` — persisted refresh tokens.
   - `~/tesla-auth-proxy/.env` — shared secret and Tesla config.
3. If you ever move the Pi to new hardware, restore those two items first and then bring the container back up with `docker compose up -d`.

### If you want to run it without Docker

You can, but Docker is the simpler path on a Pi.

1. Install Python 3.12 or newer.
2. Install the dependencies from `requirements.txt` in a virtual environment.
3. Run the app with `uvicorn app:app --host 0.0.0.0 --port 8787`.
4. Use a systemd service if you want it to start on boot.

For this project, Docker is the preferred option because it matches the way TeslaMate is already deployed.

## Minimal API Spec

### `GET /healthz`

Returns a simple liveness response.

Example response:

```json
{
  "ok": true,
  "service": "tesla-auth-proxy",
  "version": "0.1.0"
}
```

### `POST /api/v1/tesla/refresh`

Refreshes Tesla tokens.

Request headers:

- `Content-Type: application/json`
- `X-Device-Id`: stable ESP32 device identifier
- `X-Timestamp`: unix epoch seconds
- `X-Nonce`: random per-request nonce
- `X-Signature`: HMAC signature over the canonical request

Request body:

```json
{
  "device_id": "esp32-doit_B0A732325D68",
  "refresh_token": "optional-if-server-stores-it"
}
```

Success response:

```json
{
  "ok": true,
  "token_type": "Bearer",
  "access_token": "...",
  "refresh_token": "...",
  "expires_in": 28800,
  "created_at": 1752999600,
  "issuer_url": "https://auth.tesla.com/oauth2/v3"
}
```

Error response:

```json
{
  "ok": false,
  "error_code": "token_refresh_failed",
  "message": "Tesla refresh failed",
  "retryable": true
}
```

## ESP32 Integration Contract

The ESP32 should call this proxy with normal HTTP/1.1, then store the returned `access_token`, `refresh_token`, `expires_in`, and `created_at` using the same NVS flow it already uses today.

## Environment Variable Reference

All variables are set in `.env` (copy from [.env.example](.env.example)).

| Variable | Default | Change? | Explanation |
|---|---|---|---|
| `TZ` | `Europe/Copenhagen` | Only if Pi is in a different timezone | Timezone for container log timestamps. |
| `LOG_LEVEL` | `info` | Use `debug` when troubleshooting | Controls proxy log verbosity. `info` logs each request and result. `debug` is verbose. `warning` logs only problems. |
| `TESLA_AUTH_HOST` | `https://auth.tesla.com` | **No** | The Tesla authentication server hostname. Together with `TESLA_AUTH_PATH` it forms the full token endpoint: `TESLA_AUTH_HOST + TESLA_AUTH_PATH + /token`. Do not change unless Tesla moves to a different hostname. |
| `TESLA_AUTH_PATH` | `/oauth2/v3` | **No** | The path on the Tesla auth host. Combined result: `https://auth.tesla.com/oauth2/v3/token` — the same URL the ESP32 firmware calls directly today. Split from the host so either part can be overridden independently (e.g. for local mock testing), following the same pattern as TeslaMate. |
| `TESLA_AUTH_CLIENT_ID` | `ownerapi` | **No** | The OAuth client identifier expected by Tesla. `ownerapi` is the standard value used by all third-party Tesla tools including TeslaMate. |
| `TESLA_AUTH_CLIENT_SECRET` | *(empty)* | **No** | Optional OAuth client secret. Tesla does not require one for the `ownerapi` client, so leave blank. |
| `PROXY_SHARED_SECRET` | *(must be set)* | **Yes — required** | Shared secret between the proxy and the ESP32, used to sign requests with HMAC-SHA256. Generate with `openssl rand -hex 32` on the Pi. The same value must go into `privateConfig.h` on the ESP32 side. If left blank the proxy accepts any request without authentication — only safe if the port is unreachable from outside your LAN. |
| `REFRESH_TOKEN_STORE_PATH` | `/data/refresh-tokens.json` | **No** | Where the proxy persists refresh tokens between container restarts. The `data/` path maps to `~/tesla-auth-proxy/data/` on the Pi via the Docker volume mount. |

## Next step

Wire the ESP32 firmware to call the proxy instead of Tesla directly by updating `privateConfig.h` with the proxy URL and `PROXY_SHARED_SECRET`.
