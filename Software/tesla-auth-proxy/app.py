import base64
import hashlib
import hmac
import json
import logging
import os
import ssl
import time
from pathlib import Path
from typing import Any

import httpx
from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel, Field

APP_VERSION = "0.1.0"
PORT = int(os.getenv("PORT", "8787"))
LOG_LEVEL = os.getenv("LOG_LEVEL", "info").upper()
TESLA_AUTH_HOST = os.getenv("TESLA_AUTH_HOST", "https://auth.tesla.com")
TESLA_AUTH_PATH = os.getenv("TESLA_AUTH_PATH", "/oauth2/v3")
TESLA_AUTH_CLIENT_ID = os.getenv("TESLA_AUTH_CLIENT_ID", "ownerapi")
TESLA_AUTH_CLIENT_SECRET = os.getenv("TESLA_AUTH_CLIENT_SECRET", "")
PROXY_SHARED_SECRET = os.getenv("PROXY_SHARED_SECRET", "")
REFRESH_TOKEN_STORE_PATH = Path(os.getenv("REFRESH_TOKEN_STORE_PATH", "/data/refresh-tokens.json"))

logging.basicConfig(level=LOG_LEVEL, format="%(asctime)s %(levelname)s %(message)s")
logger = logging.getLogger("tesla-auth-proxy")

app = FastAPI(title="Tesla Auth Proxy", version=APP_VERSION)


class RefreshRequest(BaseModel):
    device_id: str = Field(min_length=1)
    refresh_token: str | None = None


class RefreshResponse(BaseModel):
    ok: bool
    token_type: str
    access_token: str
    refresh_token: str
    expires_in: int
    created_at: int
    issuer_url: str | None = None


class ErrorResponse(BaseModel):
    ok: bool = False
    error_code: str
    message: str
    retryable: bool = True


class StoredToken(BaseModel):
    refresh_token: str
    updated_at: int


class TokenStore(BaseModel):
    devices: dict[str, StoredToken] = Field(default_factory=dict)


def _load_store() -> TokenStore:
    if not REFRESH_TOKEN_STORE_PATH.exists():
        return TokenStore()
    try:
        data = json.loads(REFRESH_TOKEN_STORE_PATH.read_text(encoding="utf-8"))
        return TokenStore.model_validate(data)
    except Exception as exc:  # pragma: no cover - defensive
        logger.warning("token store load failed: %s", exc)
        return TokenStore()


def _save_store(store: TokenStore) -> None:
    REFRESH_TOKEN_STORE_PATH.parent.mkdir(parents=True, exist_ok=True)
    REFRESH_TOKEN_STORE_PATH.write_text(store.model_dump_json(indent=2), encoding="utf-8")


def _canonical_payload(device_id: str, timestamp: str, nonce: str, body: dict[str, Any]) -> bytes:
    body_json = json.dumps(body, separators=(",", ":"), sort_keys=True)
    canonical = "\n".join(["POST", "/api/v1/tesla/refresh", device_id, timestamp, nonce, body_json])
    return canonical.encode("utf-8")


def _verify_signature(device_id: str, timestamp: str, nonce: str, signature: str, body: dict[str, Any]) -> None:
    if not PROXY_SHARED_SECRET:
        return

    try:
        request_time = int(timestamp)
    except ValueError as exc:
        raise HTTPException(status_code=401, detail={"ok": False, "error_code": "invalid_timestamp", "message": "X-Timestamp must be unix epoch seconds", "retryable": False}) from exc

    now = int(time.time())
    if abs(now - request_time) > 300:
        raise HTTPException(status_code=401, detail={"ok": False, "error_code": "stale_request", "message": "Request timestamp is outside the allowed window", "retryable": False})

    expected = hmac.new(
        PROXY_SHARED_SECRET.encode("utf-8"),
        _canonical_payload(device_id, timestamp, nonce, body),
        hashlib.sha256,
    ).digest()
    expected_b64 = base64.urlsafe_b64encode(expected).rstrip(b"=").decode("ascii")

    if not hmac.compare_digest(expected_b64, signature):
        raise HTTPException(status_code=401, detail={"ok": False, "error_code": "bad_signature", "message": "Invalid request signature", "retryable": False})


@app.get("/healthz")
def healthz() -> dict[str, Any]:
    return {"ok": True, "service": "tesla-auth-proxy", "version": APP_VERSION}


@app.post("/api/v1/tesla/refresh")
async def refresh_tesla_tokens(
    request: RefreshRequest,
    x_device_id: str | None = Header(default=None, alias="X-Device-Id"),
    x_timestamp: str | None = Header(default=None, alias="X-Timestamp"),
    x_nonce: str | None = Header(default=None, alias="X-Nonce"),
    x_signature: str | None = Header(default=None, alias="X-Signature"),
) -> dict[str, Any]:
    device_id = x_device_id or request.device_id
    if not device_id:
        raise HTTPException(status_code=400, detail={"ok": False, "error_code": "missing_device_id", "message": "device_id is required", "retryable": False})

    if PROXY_SHARED_SECRET:
        if not all([x_timestamp, x_nonce, x_signature]):
            raise HTTPException(status_code=401, detail={"ok": False, "error_code": "missing_auth", "message": "Signed requests require X-Timestamp, X-Nonce, and X-Signature", "retryable": False})
        _verify_signature(device_id, x_timestamp or "", x_nonce or "", x_signature or "", request.model_dump(mode="json"))

    store = _load_store()
    refresh_token = request.refresh_token or store.devices.get(device_id, StoredToken(refresh_token="", updated_at=0)).refresh_token
    if not refresh_token:
        raise HTTPException(status_code=400, detail={"ok": False, "error_code": "missing_refresh_token", "message": "refresh_token is required when the proxy does not store it", "retryable": False})

    token_url = f"{TESLA_AUTH_HOST.rstrip('/')}{TESLA_AUTH_PATH.rstrip('/')}/token"
    form = {
        "grant_type": "refresh_token",
        "scope": "openid email offline_access",
        "client_id": TESLA_AUTH_CLIENT_ID,
        "refresh_token": refresh_token,
    }
    if TESLA_AUTH_CLIENT_SECRET:
        form["client_secret"] = TESLA_AUTH_CLIENT_SECRET

    ssl_context = ssl.create_default_context()
    ssl_context.minimum_version = ssl.TLSVersion.TLSv1_3

    try:
        async with httpx.AsyncClient(http2=True, timeout=30.0, verify=ssl_context) as client:
            response = await client.post(token_url, data=form, headers={"Content-Type": "application/x-www-form-urlencoded"})
    except httpx.HTTPError as exc:
        logger.warning("Tesla refresh transport error for device_id=%s: %s", device_id, exc)
        raise HTTPException(status_code=502, detail={"ok": False, "error_code": "transport_error", "message": str(exc), "retryable": True}) from exc

    if response.status_code != 200:
        logger.warning("Tesla refresh failed for device_id=%s status=%s body=%s", device_id, response.status_code, response.text)
        raise HTTPException(
            status_code=502,
            detail={
                "ok": False,
                "error_code": "token_refresh_failed",
                "message": "Tesla refresh failed",
                "retryable": True,
            },
        )

    body = response.json()
    created_at = int(time.time())
    refreshed_token = body.get("refresh_token", refresh_token)
    store.devices[device_id] = StoredToken(refresh_token=refreshed_token, updated_at=created_at)
    _save_store(store)

    issuer_url = f"{TESLA_AUTH_HOST.rstrip('/')}{TESLA_AUTH_PATH.rstrip('/')}"
    return {
        "ok": True,
        "token_type": body.get("token_type", "Bearer"),
        "access_token": body["access_token"],
        "refresh_token": refreshed_token,
        "expires_in": int(body.get("expires_in", 0)),
        "created_at": int(body.get("created_at", created_at)),
        "issuer_url": issuer_url,
    }
