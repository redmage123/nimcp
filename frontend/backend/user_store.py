"""JSON file-based user store with bcrypt passwords and role management."""
import json
import os
import threading

from passlib.hash import bcrypt as _bcrypt

import nimcp_logger
from config import AUTH_USER, AUTH_PASS_HASH

_log = nimcp_logger.get("user_store")
_STORE_PATH = os.path.join(os.path.dirname(__file__), "users.json")
_lock = threading.Lock()


def _load() -> dict[str, dict]:
    if not os.path.isfile(_STORE_PATH):
        return {}
    with open(_STORE_PATH) as f:
        return json.load(f)


def _save(users: dict[str, dict]) -> None:
    with open(_STORE_PATH, "w") as f:
        json.dump(users, f, indent=2)


def _ensure_seed_user(users: dict[str, dict]) -> bool:
    """Seed the admin user from .env if not already in the store."""
    if AUTH_USER in users or not AUTH_PASS_HASH:
        return False
    users[AUTH_USER] = {"password_hash": AUTH_PASS_HASH, "role": "admin"}
    return True


def _migrate_users(users: dict[str, dict]) -> bool:
    """Auto-add role field to existing records missing it."""
    changed = False
    for username, entry in users.items():
        if "role" not in entry:
            # Seed user gets admin, everyone else gets user
            entry["role"] = "admin" if username == AUTH_USER else "user"
            changed = True
    return changed


def verify(username: str, password: str) -> bool:
    """Verify credentials against the user store (and .env seed user)."""
    with _lock:
        users = _load()
        changed = _ensure_seed_user(users)
        changed = _migrate_users(users) or changed
        if changed:
            _save(users)

    entry = users.get(username)
    if not entry:
        return False
    try:
        return _bcrypt.verify(password, entry["password_hash"])
    except Exception as exc:
        _log.warning("Password verify error for %s: %s", username, exc)
        return False


def register(username: str, password: str) -> None:
    """Register a new user. Raises ValueError if username taken or invalid."""
    username = username.strip()
    if not username or len(username) < 2:
        raise ValueError("Username must be at least 2 characters")
    if len(username) > 64:
        raise ValueError("Username must be 64 characters or fewer")
    if not username.isalnum() and not all(c.isalnum() or c in "_-" for c in username):
        raise ValueError("Username may only contain letters, numbers, hyphens, and underscores")
    if len(password) < 6:
        raise ValueError("Password must be at least 6 characters")

    with _lock:
        users = _load()
        _ensure_seed_user(users)
        if username in users:
            raise ValueError("Username already taken")
        users[username] = {"password_hash": _bcrypt.hash(password), "role": "user"}
        _save(users)
    _log.info("Registered new user: %s", username)


def user_exists(username: str) -> bool:
    with _lock:
        users = _load()
        _ensure_seed_user(users)
        return username in users


def get_role(username: str) -> str:
    """Get user's role. Returns 'user' if not found or no role set."""
    with _lock:
        users = _load()
        _ensure_seed_user(users)
        _migrate_users(users)
        entry = users.get(username)
        if not entry:
            return "user"
        return entry.get("role", "user")


def get_user_info(username: str) -> dict | None:
    """Get user info dict (without password_hash). Returns None if not found."""
    with _lock:
        users = _load()
        _ensure_seed_user(users)
        _migrate_users(users)
        entry = users.get(username)
        if not entry:
            return None
        return {"username": username, "role": entry.get("role", "user")}


def set_role(username: str, role: str) -> bool:
    """Set user's role. Returns False if user not found."""
    if role not in ("admin", "user"):
        raise ValueError("Role must be 'admin' or 'user'")
    with _lock:
        users = _load()
        _ensure_seed_user(users)
        if username not in users:
            return False
        users[username]["role"] = role
        _save(users)
    _log.info("Set role for %s to %s", username, role)
    return True


def list_users() -> list[dict]:
    """List all users (without password hashes)."""
    with _lock:
        users = _load()
        _ensure_seed_user(users)
        _migrate_users(users)
        return [
            {"username": u, "role": entry.get("role", "user")}
            for u, entry in users.items()
        ]
