"""NIMCP Monitoring Frontend — FastAPI application."""
import asyncio
import base64
import os
import secrets
import time
from contextlib import asynccontextmanager

from fastapi import FastAPI, Request, WebSocket
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, RedirectResponse, Response
from fastapi.staticfiles import StaticFiles
from starlette.middleware.base import BaseHTTPMiddleware

import nimcp

import nimcp_logger
from config import (
    AUTH_PASS_HASH, AUTH_USER, CLIENT_DIST, CORS_ALLOWED_ORIGINS,
    DEV_MODE, LOG_LEVEL, SSL_CERTFILE, SSL_KEYFILE,
)
from brain_manager import manager
import user_store
from ws.handler import websocket_handler
from ws.probe_streamer import probe_loop
from routers import auth, brains, training, datasets, chat, system, scripts, conversations, admin, media

_log = nimcp_logger.get("main")


def _extract_basic_auth(request: Request) -> tuple[bool, str]:
    """Validate HTTP Basic Auth and return (authed, username)."""
    auth_header = request.headers.get("authorization", "")
    if not auth_header.startswith("Basic "):
        return False, ""
    try:
        decoded = base64.b64decode(auth_header[6:]).decode("utf-8")
        username, _, password = decoded.partition(":")
    except Exception as exc:
        _log.warning("Auth decode error: %s", exc)
        return False, ""
    if user_store.verify(username, password):
        return True, username
    return False, ""


class RequestContextMiddleware(BaseHTTPMiddleware):
    """Assign request ID, log request/response, catch unhandled exceptions."""
    async def dispatch(self, request: Request, call_next):
        rid = nimcp_logger.new_request_id()
        _log.info("%s %s", request.method, request.url.path)
        t0 = time.monotonic()
        try:
            response = await call_next(request)
        except Exception as exc:
            elapsed = (time.monotonic() - t0) * 1000
            _log.error("Unhandled exception after %.1fms: %s", elapsed, exc, exc_info=True)
            return JSONResponse(
                status_code=500,
                content={"detail": "Internal server error"},
                headers={"X-Request-ID": rid},
            )
        elapsed = (time.monotonic() - t0) * 1000
        _log.info("%s %s → %d (%.1fms)", request.method, request.url.path, response.status_code, elapsed)
        response.headers["X-Request-ID"] = rid
        return response


class BasicAuthMiddleware(BaseHTTPMiddleware):
    """Require HTTP Basic Auth on API/WS requests when credentials are configured.

    Static files (the React app) are served without auth so the frontend
    can render its own login/register pages.  Only /api/* and /ws/* are
    protected — with /api/auth/* exempted for login and registration.
    """

    async def dispatch(self, request: Request, call_next):
        if not AUTH_PASS_HASH:
            return await call_next(request)
        path = request.url.path
        # Let the React app and its assets through — auth is handled in-app
        if not (path.startswith("/api/") or path.startswith("/ws/")):
            return await call_next(request)
        # Auth endpoints are public (login, register)
        if path.startswith("/api/auth/"):
            return await call_next(request)
        authed, username = _extract_basic_auth(request)
        if authed:
            # Set identity on request state for downstream handlers
            request.state.username = username
            request.state.role = user_store.get_role(username)
            return await call_next(request)
        _log.warning("Auth failed: %s %s", request.method, request.url.path)
        # Return 401 WITHOUT WWW-Authenticate header to prevent the
        # browser's native Basic Auth popup — the React app handles login.
        return JSONResponse(
            status_code=401,
            content={"detail": "Unauthorized"},
        )


class HTTPSRedirectMiddleware(BaseHTTPMiddleware):
    """Redirect HTTP requests to HTTPS."""
    async def dispatch(self, request: Request, call_next):
        if request.url.scheme == "http" and not DEV_MODE:
            url = request.url.replace(scheme="https")
            return RedirectResponse(url=str(url), status_code=301)
        return await call_next(request)


@asynccontextmanager
async def lifespan(app: FastAPI):
    nimcp_logger.setup_logging(LOG_LEVEL)
    _log.info("NIMCP Dashboard starting (dev_mode=%s, log_level=%s)", DEV_MODE, LOG_LEVEL)
    nimcp.init()
    manager.load_all_from_disk()
    manager.ensure_athena()
    probe_task = asyncio.create_task(probe_loop())
    yield
    _log.info("NIMCP Dashboard shutting down")
    probe_task.cancel()
    manager.destroy_all()
    nimcp.shutdown()


app = FastAPI(title="NIMCP Dashboard", version=nimcp.version() if hasattr(nimcp, 'version') else "2.6.3", lifespan=lifespan)

# Global exception handler
@app.exception_handler(Exception)
async def _global_exception_handler(request: Request, exc: Exception):
    _log.error("Unhandled exception on %s %s: %s", request.method, request.url.path, exc, exc_info=True)
    return JSONResponse(
        status_code=500,
        content={"detail": "Internal server error"},
    )

# Middleware stack (outermost first)
app.add_middleware(RequestContextMiddleware)
app.add_middleware(BasicAuthMiddleware)
app.add_middleware(HTTPSRedirectMiddleware)

# CORS
if CORS_ALLOWED_ORIGINS:
    # Production: explicit origin list, credentials allowed
    app.add_middleware(
        CORSMiddleware,
        allow_origins=CORS_ALLOWED_ORIGINS,
        allow_credentials=True,
        allow_methods=["GET", "POST", "DELETE", "PATCH"],
        allow_headers=["Authorization", "Content-Type"],
    )
elif DEV_MODE:
    # Dev mode: allow all origins (no credentials — spec forbids * + credentials)
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=False,
        allow_methods=["GET", "POST", "DELETE", "PATCH", "OPTIONS"],
        allow_headers=["Authorization", "Content-Type"],
    )

# REST routers
app.include_router(auth.router)
app.include_router(brains.router)
app.include_router(training.router)
app.include_router(datasets.router)
app.include_router(chat.router)
app.include_router(system.router)
app.include_router(scripts.router)
app.include_router(conversations.router)
app.include_router(admin.router)
app.include_router(media.router)


# WebSocket — check Authorization header OR ?auth= query param
@app.websocket("/ws/{brain_id}")
async def ws_endpoint(ws: WebSocket, brain_id: int):
    username = ""
    if AUTH_PASS_HASH:
        # Try the Authorization header first (e.g. URL-embedded credentials)
        from starlette.requests import Request as StarletteRequest
        scope = ws.scope.copy()
        scope["type"] = "http"
        fake_req = StarletteRequest(scope)
        authed, username = _extract_basic_auth(fake_req)
        # Fall back to ?auth= query param (base64-encoded user:pass)
        if not authed:
            from urllib.parse import parse_qs
            qs = parse_qs(ws.scope.get("query_string", b"").decode())
            token = qs.get("auth", [None])[0]
            if token:
                try:
                    decoded = base64.b64decode(token).decode("utf-8")
                    username, _, password = decoded.partition(":")
                    if user_store.verify(username, password):
                        authed = True
                except Exception:
                    pass
        if not authed:
            await ws.close(code=4401, reason="Unauthorized")
            return
    await websocket_handler(ws, brain_id, username)


# Static files for production
if os.path.isdir(CLIENT_DIST):
    app.mount("/", StaticFiles(directory=CLIENT_DIST, html=True), name="static")


if __name__ == "__main__":
    import uvicorn
    ssl_kwargs = {}
    if os.path.isfile(SSL_CERTFILE) and os.path.isfile(SSL_KEYFILE):
        ssl_kwargs = {"ssl_certfile": SSL_CERTFILE, "ssl_keyfile": SSL_KEYFILE}
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=DEV_MODE, **ssl_kwargs)
