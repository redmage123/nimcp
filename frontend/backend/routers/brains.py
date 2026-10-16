"""Brain CRUD, probe, predict, learn, and snapshot endpoints."""
from fastapi import APIRouter, BackgroundTasks, HTTPException
import os
import tempfile

import nimcp_logger
from brain_manager import manager
from config import C_API_TIMEOUT_SECONDS
from models.brain import BrainCreate, BrainUpdate, BrainResize, BrainPredict, BrainLearn, SnapshotCreate
from validation import c_api_call

_log = nimcp_logger.get("routers.brains")

router = APIRouter(prefix="/api/brains", tags=["brains"])


@router.post("/")
async def create_brain(req: BrainCreate):
    # Apply conversational defaults when fields are omitted
    task = req.task if req.task is not None else 4          # TASK_ASSOCIATION
    num_inputs = req.num_inputs if req.num_inputs is not None else 128
    num_outputs = req.num_outputs if req.num_outputs is not None else 32
    try:
        bid = await c_api_call(
            manager.create_brain,
            req.name, req.size, task, num_inputs, num_outputs,
            num_neurons=req.num_neurons,
            timeout=C_API_TIMEOUT_SECONDS,
        )
    except ValueError as e:
        raise HTTPException(409, str(e))
    probe = await c_api_call(manager.probe_brain, bid, timeout=C_API_TIMEOUT_SECONDS)
    return {"id": bid, "probe": probe}


@router.get("/")
async def list_brains():
    return await c_api_call(manager.list_brains, timeout=C_API_TIMEOUT_SECONDS)


@router.get("/{bid}")
async def get_brain(bid: int):
    detail = await c_api_call(manager.get_brain_detail, bid, timeout=C_API_TIMEOUT_SECONDS)
    if detail is None:
        raise HTTPException(404, "Brain not found")
    return detail


@router.patch("/{bid}")
async def update_brain(bid: int, req: BrainUpdate):
    ok = await c_api_call(manager.rename_brain, bid, req.name, timeout=C_API_TIMEOUT_SECONDS)
    if not ok:
        raise HTTPException(404, "Brain not found")
    return {"id": bid, "name": req.name}


@router.post("/{bid}/resize")
async def resize_brain(bid: int, req: BrainResize):
    ok = await c_api_call(manager.resize_brain, bid, req.num_neurons, timeout=C_API_TIMEOUT_SECONDS)
    if not ok:
        raise HTTPException(404, "Brain not found or resize failed")
    probe = await c_api_call(manager.probe_brain, bid, timeout=C_API_TIMEOUT_SECONDS)
    return {"id": bid, "num_neurons": req.num_neurons, "probe": probe}


@router.delete("/{bid}")
async def delete_brain(bid: int):
    ok = await c_api_call(manager.destroy_brain, bid, timeout=C_API_TIMEOUT_SECONDS)
    if not ok:
        raise HTTPException(404, "Brain not found")
    return {"deleted": bid}


@router.get("/{bid}/probe")
async def probe_brain(bid: int):
    probe = await c_api_call(manager.probe_brain, bid, timeout=C_API_TIMEOUT_SECONDS)
    if probe is None:
        raise HTTPException(404, "Brain not found")
    return probe


@router.get("/{bid}/probe/history")
async def probe_history(bid: int):
    return await c_api_call(manager.get_probe_history, bid, timeout=C_API_TIMEOUT_SECONDS)


@router.post("/{bid}/predict")
async def predict(bid: int, req: BrainPredict):
    result = await c_api_call(manager.predict, bid, req.features, timeout=C_API_TIMEOUT_SECONDS)
    if result is None:
        raise HTTPException(404, "Brain not found")
    label, confidence = result
    return {"label": label, "confidence": float(confidence)}


@router.post("/{bid}/learn")
async def learn(bid: int, req: BrainLearn, bg: BackgroundTasks):
    label_str = str(req.label)
    result = await c_api_call(
        manager.learn, bid, req.features, label_str, req.confidence,
        timeout=C_API_TIMEOUT_SECONDS,
    )
    if result is None:
        raise HTTPException(404, "Brain not found")
    probe = await c_api_call(manager.probe_brain, bid, timeout=C_API_TIMEOUT_SECONDS)
    bg.add_task(manager.save_brain_async, bid)
    return {"loss": result, "probe": probe}


@router.post("/{bid}/snapshots")
async def create_snapshot(bid: int, req: SnapshotCreate):
    if not manager.has_brain(bid):
        raise HTTPException(404, "Brain not found")
    path = os.path.join(tempfile.gettempdir(), f"nimcp_snap_{bid}_{req.name}.bin")
    ok = await c_api_call(manager.save_brain, bid, path, timeout=C_API_TIMEOUT_SECONDS)
    if not ok:
        raise HTTPException(500, "Failed to save snapshot")
    manager.register_snapshot(bid, req.name, path)
    _log.info("Created snapshot '%s' for brain %d", req.name, bid)
    return {"name": req.name, "path": path}


@router.get("/{bid}/snapshots")
async def list_snapshots(bid: int):
    if not manager.has_brain(bid):
        raise HTTPException(404, "Brain not found")
    return manager.list_snapshot_names(bid)


@router.post("/{bid}/snapshots/{name}/restore")
async def restore_snapshot(bid: int, name: str):
    path = manager.get_snapshot_path(bid, name)
    if path is None:
        raise HTTPException(404, "Snapshot not found")
    ok = await c_api_call(manager.load_brain, bid, path, timeout=C_API_TIMEOUT_SECONDS)
    if not ok:
        raise HTTPException(500, "Failed to restore snapshot")
    _log.info("Restored snapshot '%s' for brain %d", name, bid)
    probe = await c_api_call(manager.probe_brain, bid, timeout=C_API_TIMEOUT_SECONDS)
    return {"restored": name, "probe": probe}


@router.delete("/{bid}/snapshots/{name}")
async def delete_snapshot(bid: int, name: str):
    path = manager.get_snapshot_path(bid, name)
    if path is None:
        raise HTTPException(404, "Snapshot not found")
    try:
        os.remove(path)
    except OSError:
        pass
    manager.remove_snapshot(bid, name)
    _log.info("Deleted snapshot '%s' for brain %d", name, bid)
    return {"deleted": name}


@router.post("/{bid}/cow/snapshot")
async def cow_snapshot(bid: int):
    ok = await c_api_call(manager.snapshot_cow, bid, timeout=C_API_TIMEOUT_SECONDS)
    if not ok:
        raise HTTPException(404, "Brain not found or COW snapshot failed")
    return {"snapshot": True}


@router.post("/{bid}/cow/restore")
async def cow_restore(bid: int):
    ok = await c_api_call(manager.restore_cow, bid, timeout=C_API_TIMEOUT_SECONDS)
    if not ok:
        raise HTTPException(404, "Brain not found or COW restore failed")
    probe = await c_api_call(manager.probe_brain, bid, timeout=C_API_TIMEOUT_SECONDS)
    return {"restored": True, "probe": probe}


# --- Speech & Avatar ---

@router.post("/{bid}/speak")
async def speak(bid: int, body: dict = {}):
    """Generate spoken text from brain's language production pipeline."""
    if not manager.has_brain(bid):
        raise HTTPException(404, "Brain not found")
    semantic_vector = body.get("semantic_vector")
    result = await c_api_call(manager.speak, bid, semantic_vector,
                               timeout=C_API_TIMEOUT_SECONDS)
    if result is None:
        raise HTTPException(500, "Speech generation failed")
    return result


@router.get("/{bid}/avatar")
async def get_avatar_state(bid: int):
    """Get avatar visual state (FACS AUs, visemes, gaze, emotion, voice)."""
    if not manager.has_brain(bid):
        raise HTTPException(404, "Brain not found")
    result = await c_api_call(manager.get_avatar_state, bid,
                               timeout=C_API_TIMEOUT_SECONDS)
    if result is None:
        raise HTTPException(500, "Avatar state unavailable")
    return result


# --- Identity / Self-Image ---

@router.get("/{bid}/identity")
async def get_identity(bid: int):
    """Get avatar identity (self-chosen appearance parameters)."""
    result = manager.get_identity(bid)
    if result is None:
        raise HTTPException(404, "Brain not found")
    return result


@router.put("/{bid}/identity")
async def set_identity(bid: int, body: dict):
    """Set avatar identity parameters. Merges with existing values."""
    result = manager.set_identity(bid, body)
    if result is None:
        raise HTTPException(404, "Brain not found")
    return result
