"""JSON file-based conversation storage — one file per conversation, per user directory."""
import json
import os
import time
import uuid

import nimcp_logger

_log = nimcp_logger.get("conversation_store")
_CONV_DIR = os.path.join(os.path.dirname(__file__), "conversations")


def _user_dir(username: str) -> str:
    return os.path.join(_CONV_DIR, username)


def _conv_path(username: str, conv_id: str) -> str:
    return os.path.join(_user_dir(username), f"{conv_id}.json")


def _load_conv(username: str, conv_id: str) -> dict | None:
    path = _conv_path(username, conv_id)
    if not os.path.isfile(path):
        return None
    with open(path) as f:
        return json.load(f)


def _save_conv(username: str, conv_id: str, data: dict) -> None:
    udir = _user_dir(username)
    os.makedirs(udir, exist_ok=True)
    with open(_conv_path(username, conv_id), "w") as f:
        json.dump(data, f, indent=2)


def list_conversations(username: str) -> list[dict]:
    """List all conversations for a user (without messages)."""
    udir = _user_dir(username)
    if not os.path.isdir(udir):
        return []
    result = []
    for fname in os.listdir(udir):
        if not fname.endswith(".json"):
            continue
        try:
            with open(os.path.join(udir, fname)) as f:
                data = json.load(f)
            result.append({
                "id": data["id"],
                "title": data.get("title", "New conversation"),
                "brain_id": data.get("brain_id", 0),
                "created_at": data.get("created_at", ""),
                "updated_at": data.get("updated_at", ""),
                "message_count": len(data.get("messages", [])),
            })
        except Exception:
            continue
    result.sort(key=lambda c: c["updated_at"], reverse=True)
    return result


def create_conversation(username: str, brain_id: int = 0, title: str = "New conversation") -> str:
    """Create a new conversation. Returns conv_id."""
    conv_id = str(uuid.uuid4())
    now = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    data = {
        "id": conv_id,
        "title": title,
        "brain_id": brain_id,
        "created_at": now,
        "updated_at": now,
        "messages": [],
    }
    _save_conv(username, conv_id, data)
    _log.debug("Created conversation %s for %s", conv_id, username)
    return conv_id


def get_conversation(username: str, conv_id: str) -> dict | None:
    """Get full conversation with messages."""
    return _load_conv(username, conv_id)


def append_message(username: str, conv_id: str, role: str, text: str,
                   metadata: dict | None = None) -> bool:
    """Append a message to a conversation. Auto-titles from first user message."""
    data = _load_conv(username, conv_id)
    if data is None:
        return False
    now = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    msg = {
        "role": role,
        "text": text,
        "timestamp": now,
    }
    if metadata:
        msg["metadata"] = metadata
    data["messages"].append(msg)
    data["updated_at"] = now
    # Auto-title from first user message
    if role == "user" and data.get("title") == "New conversation" and text.strip():
        data["title"] = text.strip()[:80]
    _save_conv(username, conv_id, data)
    return True


def delete_conversation(username: str, conv_id: str) -> bool:
    """Delete a conversation."""
    path = _conv_path(username, conv_id)
    if not os.path.isfile(path):
        return False
    os.remove(path)
    _log.debug("Deleted conversation %s for %s", conv_id, username)
    return True


def rename_conversation(username: str, conv_id: str, title: str) -> bool:
    """Rename a conversation."""
    data = _load_conv(username, conv_id)
    if data is None:
        return False
    data["title"] = title
    data["updated_at"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    _save_conv(username, conv_id, data)
    return True
