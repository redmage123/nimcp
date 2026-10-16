"""Shared validation and sanitization helpers."""
import asyncio
import math
import re
import traceback

from fastapi import HTTPException

import nimcp_logger

_log = nimcp_logger.get("validation")

_SAFE_NAME_RE = re.compile(r"^[a-zA-Z0-9][a-zA-Z0-9_\-]{0,63}$")
_SAFE_ID_RE = re.compile(r"[^a-zA-Z0-9_\-]")


def sanitize_snapshot_name(name: str) -> str:
    """Validate snapshot name — no path traversal, safe characters only."""
    if not _SAFE_NAME_RE.match(name):
        raise HTTPException(
            422,
            "Snapshot name must be 1-64 chars, start with alphanumeric, "
            "and contain only letters, digits, hyphens, underscores.",
        )
    return name


def validate_features(features: list[float], max_len: int = 10000) -> None:
    """Reject empty, oversized, or NaN/Inf feature vectors."""
    if not features:
        raise HTTPException(422, "Feature vector must not be empty")
    if len(features) > max_len:
        raise HTTPException(422, f"Feature vector too long ({len(features)} > {max_len})")
    for i, v in enumerate(features):
        if not math.isfinite(v):
            raise HTTPException(422, f"Feature[{i}] is not finite ({v})")


def validate_csv_bytes(content: bytes, max_bytes: int) -> str:
    """Validate CSV upload size and encoding, return decoded text."""
    if len(content) > max_bytes:
        raise HTTPException(
            413, f"CSV file too large ({len(content)} bytes, max {max_bytes})"
        )
    try:
        return content.decode("utf-8")
    except UnicodeDecodeError:
        raise HTTPException(400, "CSV file is not valid UTF-8")


def sanitize_dataset_id(raw: str) -> str:
    """Create a safe dataset ID from user-provided name."""
    safe = _SAFE_ID_RE.sub("_", raw.lower().strip())
    if not safe or not safe[0].isalnum():
        safe = "ds_" + safe
    return f"csv_{safe}"[:128]


def safe_error_message(exc: Exception) -> str:
    """Log the real exception and return a generic message for the client."""
    _log.error("Internal error: %s", exc, exc_info=True)
    return "Internal server error"


async def c_api_call(func, *args, timeout: float = 30.0):
    """Run a blocking C API call in a thread with timeout."""
    try:
        return await asyncio.wait_for(asyncio.to_thread(func, *args), timeout=timeout)
    except asyncio.TimeoutError:
        _log.error("C API call timed out after %.1fs: %s(%s)", timeout, func.__name__, args)
        raise HTTPException(504, "C library call timed out")
