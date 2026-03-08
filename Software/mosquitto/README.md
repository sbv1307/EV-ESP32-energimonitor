# Mosquitto Docker Setup (Persistent Logs + Persistent Subscriber Trace)

This folder contains a ready-to-use Mosquitto setup for Docker Desktop on Windows, including:

- Persistent broker data and broker logs
- A subscriber container that continuously traces MQTT traffic
- Trace output persisted to a host-mounted file

## What Was Added

`Perfekt, jeg opdaterer subscriber-containeren til at skrive trace-output permanent til en host-mappet volume, samtidig med at du stadig kan se live`.

In practice, this means:

- `mqtt-subscriber` writes to `Software/mosquitto/subscriber-data/mqtt-trace.log`
- You can still follow live output with `docker logs -f mosquitto-subscriber`
- Trace timestamps are written in local timezone (same TZ rule as firmware `configureTime()`)

The folder is named `subscriber-data` (not `subscriber`) to avoid name conflicts with an existing file/folder in this project.

## Folder Structure

Expected structure:

```text
Software/mosquitto/
  docker-compose.yml
  README.md
  mosquitto/
    config/
      mosquitto.conf
    data/
    log/
  subscriber-data/
    mqtt-trace.log  (created automatically)
```

## docker-compose.yml

Use this content in `Software/mosquitto/docker-compose.yml`:

```yaml
version: "3.8"

services:
  mosquitto:
    image: eclipse-mosquitto:2
    container_name: mosquitto-ev-charging
    restart: unless-stopped
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - ./mosquitto/config:/mosquitto/config
      - ./mosquitto/data:/mosquitto/data
      - ./mosquitto/log:/mosquitto/log

  mqtt-subscriber:
    image: eclipse-mosquitto:2
    container_name: mosquitto-subscriber
    restart: unless-stopped
    depends_on:
      - mosquitto
    environment:
      - TZ=CET-1CEST,M3.5.0/2,M10.5.0/3
    volumes:
      - ./subscriber-data:/subscriber
    command:
      - sh
      - -c
      - >-
        mosquitto_sub -h mosquitto-ev-charging -p 1883
        -t "ev-e-charging/#"
        -t "homeassistant/#"
        -v |
        while IFS= read -r line; do
          printf "%s %s\n" "$$(date +"%Y-%m-%d %H:%M:%S%z")" "$$line";
        done |
        tee -a /subscriber/mqtt-trace.log
```

## mosquitto.conf

Use this content in `Software/mosquitto/mosquitto/config/mosquitto.conf`:

```conf
persistence true
persistence_location /mosquitto/data/

# Broker listeners
listener 1883 0.0.0.0
protocol mqtt

listener 9001 0.0.0.0
protocol websockets

# Auth
allow_anonymous true
# password_file /mosquitto/config/passwordfile

# Logging
log_dest stdout
log_dest file /mosquitto/log/mosquitto.log
log_timestamp true
log_timestamp_format %Y-%m-%d %H:%M:%S
log_type error
log_type warning
log_type notice
log_type information
log_type subscribe
log_type unsubscribe
log_type websockets
```

## How To Use

From PowerShell:

```powershell
cd Software/mosquitto
docker compose up -d
```

Check running containers:

```powershell
docker ps
```

Follow live subscriber output:

```powershell
docker logs -f mosquitto-subscriber
```

Follow broker logs:

```powershell
docker logs -f mosquitto-ev-charging
```

## Where Logs Are Stored

Broker log file:

- `Software/mosquitto/mosquitto/log/mosquitto.log`

Subscriber trace file:

- `Software/mosquitto/subscriber-data/mqtt-trace.log`

## Restart / Recreate Subscriber

If you change the subscriber command or want a fresh container:

```powershell
cd Software/mosquitto
docker compose up -d --force-recreate mqtt-subscriber
```

## Stop Everything

```powershell
cd Software/mosquitto
docker compose down
```

## Quick Verify

Use these commands when you quickly want to confirm everything works end-to-end:

1. Start services:

```powershell
cd Software/mosquitto
docker compose up -d
```

2. Watch live subscriber output from container logs:

```powershell
docker logs -f mosquitto-subscriber
```

3. Watch persisted trace file on host in real time:

```powershell
Get-Content .\subscriber-data\mqtt-trace.log -Wait
```

## Notes

- Inside Docker, `127.0.0.1` points to the container itself, not the broker container.
- The subscriber therefore uses `-h mosquitto-ev-charging` (the broker container name).
- `mosquitto_sub` `%I` is UTC-oriented in your output. To get local timestamps matching firmware timezone rules, the compose setup prefixes each line with container-local `date` and sets `TZ=CET-1CEST,M3.5.0/2,M10.5.0/3`.
- In `docker-compose.yml`, shell variables must be escaped as `$$...` (for example `$$line`) so Compose does not consume them before the container starts.
- Your traced topics are:
  - `ev-e-charging/#`
  - `homeassistant/#`
