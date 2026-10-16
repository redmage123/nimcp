"""JSON storage for custom probe configurations — one file per user."""
import json
import os
import uuid

import nimcp_logger

_log = nimcp_logger.get("probe_config_store")
_STORE_DIR = os.path.join(os.path.dirname(__file__), "probe_configs")


def _user_path(username: str) -> str:
    return os.path.join(_STORE_DIR, f"{username}.json")


def _load_user_probes(username: str) -> list[dict]:
    path = _user_path(username)
    if not os.path.isfile(path):
        return []
    with open(path) as f:
        return json.load(f)


def _save_user_probes(username: str, probes: list[dict]) -> None:
    os.makedirs(_STORE_DIR, exist_ok=True)
    with open(_user_path(username), "w") as f:
        json.dump(probes, f, indent=2)


def list_probes(username: str) -> list[dict]:
    """List all probe configs for a user."""
    return _load_user_probes(username)


def get_probe(username: str, probe_id: str) -> dict | None:
    """Get a specific probe config."""
    for p in _load_user_probes(username):
        if p.get("id") == probe_id:
            return p
    return None


def save_probe(username: str, config: dict) -> dict:
    """Create or update a probe config. Returns the saved config."""
    if "id" not in config:
        config["id"] = str(uuid.uuid4())
    probes = _load_user_probes(username)
    # Update existing or append
    found = False
    for i, p in enumerate(probes):
        if p.get("id") == config["id"]:
            probes[i] = config
            found = True
            break
    if not found:
        probes.append(config)
    _save_user_probes(username, probes)
    _log.debug("Saved probe %s for %s", config["id"], username)
    return config


def delete_probe(username: str, probe_id: str) -> bool:
    """Delete a probe config. Returns True if found and deleted."""
    probes = _load_user_probes(username)
    new_probes = [p for p in probes if p.get("id") != probe_id]
    if len(new_probes) == len(probes):
        return False
    _save_user_probes(username, new_probes)
    _log.debug("Deleted probe %s for %s", probe_id, username)
    return True
