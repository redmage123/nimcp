"""Admin-only endpoints — Athena management, user management, probes."""
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel

import nimcp_logger
import user_store
import probe_config_store
from auth_deps import require_admin
from brain_manager import manager

_log = nimcp_logger.get("routers.admin")

router = APIRouter(prefix="/api/admin", tags=["admin"])


class RoleUpdate(BaseModel):
    role: str


# --- Athena management ---

@router.get("/athena/status", dependencies=[Depends(require_admin)])
async def athena_status():
    probe = manager.probe_brain(0)
    meta = manager.get_metadata(0)
    if not probe or not meta:
        return {"loaded": False}
    return {
        "loaded": True,
        "name": meta.name,
        "num_neurons": probe.get("num_neurons", 0),
        "total_inferences": probe.get("total_inferences", 0),
        "total_learning_steps": probe.get("total_learning_steps", 0),
        "accuracy": probe.get("accuracy", 0.0),
        "memory_bytes": probe.get("memory_bytes", 0),
    }


@router.post("/athena/save", dependencies=[Depends(require_admin)])
async def athena_save():
    manager.save_brain_async(0)
    return {"saved": True}


# --- User management ---

@router.get("/users", dependencies=[Depends(require_admin)])
async def list_users():
    return user_store.list_users()


@router.patch("/users/{target_username}", dependencies=[Depends(require_admin)])
async def update_user_role(target_username: str, req: RoleUpdate):
    try:
        ok = user_store.set_role(target_username, req.role)
    except ValueError as exc:
        raise HTTPException(400, str(exc))
    if not ok:
        raise HTTPException(404, "User not found")
    return {"username": target_username, "role": req.role}


# --- Probe config CRUD (persisted to JSON files) ---

@router.get("/probes")
async def list_probes(user: dict = Depends(require_admin)):
    return probe_config_store.list_probes(user["username"])


@router.post("/probes")
async def save_probe(config: dict, user: dict = Depends(require_admin)):
    return probe_config_store.save_probe(user["username"], config)


@router.put("/probes/{probe_id}")
async def update_probe(probe_id: str, config: dict, user: dict = Depends(require_admin)):
    config["id"] = probe_id
    return probe_config_store.save_probe(user["username"], config)


@router.delete("/probes/{probe_id}")
async def delete_probe(probe_id: str, user: dict = Depends(require_admin)):
    if not probe_config_store.delete_probe(user["username"], probe_id):
        raise HTTPException(404, "Probe config not found")
    return {"deleted": probe_id}


# --- Live probe data (real-time monitoring) ---

@router.get("/probes/live", dependencies=[Depends(require_admin)])
async def live_probe_data(brain_id: int = 0):
    """Return full probe dict for a brain, intended for real-time monitoring.

    Calls brain.probe() via BrainManager and returns all available metrics.
    Defaults to the primary Athena brain (brain_id=0).
    """
    probe = manager.probe_brain(brain_id)
    if probe is None:
        raise HTTPException(404, f"Brain {brain_id} not found or probe unavailable")

    # Augment probe with avatar/emotion state
    avatar = manager.get_avatar_state(brain_id)
    if avatar:
        probe["emotion_valence"] = avatar.get("valence", 0.0)
        probe["emotion_arousal"] = avatar.get("arousal", 0.0)
        probe["emotion_id"] = avatar.get("emotion_id", 18)
        probe["emotion_intensity"] = avatar.get("emotion_intensity", 0.0)
        probe["is_speaking"] = avatar.get("is_speaking", False)
        probe["speech_pitch_hz"] = avatar.get("pitch_hz", 0.0)

    return probe
