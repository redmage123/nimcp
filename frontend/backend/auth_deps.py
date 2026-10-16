"""FastAPI dependency functions for role-based access control."""
from fastapi import HTTPException, Request

import nimcp_logger

_log = nimcp_logger.get("auth_deps")


def get_current_user(request: Request) -> dict:
    """Extract authenticated user info from request state (set by middleware).

    Returns: {"username": str, "role": str}
    """
    username = getattr(request.state, "username", None)
    role = getattr(request.state, "role", None)
    if not username:
        raise HTTPException(401, "Not authenticated")
    return {"username": username, "role": role or "user"}


def require_admin(request: Request) -> dict:
    """Require admin role. Raises 403 if not admin."""
    user = get_current_user(request)
    if user["role"] != "admin":
        raise HTTPException(403, "Admin access required")
    return user
