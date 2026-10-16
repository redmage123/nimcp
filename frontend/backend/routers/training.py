"""Training configuration, control, and status endpoints."""
from fastapi import APIRouter, HTTPException

import nimcp_logger
from brain_manager import manager
from config import C_API_TIMEOUT_SECONDS
from models.training import TrainingStart, LearnBatch
from validation import c_api_call
import training_runner
import dataset_manager

_log = nimcp_logger.get("routers.training")

router = APIRouter(prefix="/api/training", tags=["training"])


@router.post("/{brain_id}/start")
async def start(brain_id: int, req: TrainingStart):
    if not manager.has_brain(brain_id):
        raise HTTPException(404, "Brain not found")
    try:
        run = await training_runner.start_training(
            brain_id, req.dataset_id, req.epochs, req.batch_size,
            strategy=req.strategy,
            bio_blend=req.biological_blend,
            stdp_confidence=req.stdp_confidence,
        )
        return run.progress_dict()
    except RuntimeError as e:
        msg = str(e)
        if "already in progress" in msg:
            raise HTTPException(409, msg)
        raise HTTPException(400, msg)


@router.post("/{brain_id}/stop")
async def stop(brain_id: int):
    ok = await training_runner.stop_training(brain_id)
    if not ok:
        raise HTTPException(404, "No active training")
    return {"stopped": True}


@router.get("/{brain_id}/status")
async def status(brain_id: int):
    run = training_runner.get_run(brain_id)
    if run is None:
        return {"brain_id": brain_id, "running": False}
    return run.progress_dict()


@router.post("/{brain_id}/learn-batch")
async def learn_batch(brain_id: int, req: LearnBatch):
    if not manager.has_brain(brain_id):
        raise HTTPException(404, "Brain not found")
    examples = dataset_manager.get_examples(req.dataset_id, req.count)
    if not examples:
        raise HTTPException(404, "Dataset not found or empty")

    # Auto-materialize PENDING brain from full batch
    if not manager.is_materialized(brain_id):
        await c_api_call(
            manager.materialize_from_dataset, brain_id, examples,
            timeout=C_API_TIMEOUT_SECONDS,
        )

    trained = 0
    last_loss = 0.0
    for ex in examples:
        features = ex.get("features", ex.get("input", []))
        label = str(ex.get("label", ex.get("class", "0")))
        try:
            loss = await c_api_call(
                manager.learn, brain_id, features, label, 1.0,
                timeout=C_API_TIMEOUT_SECONDS,
            )
            if loss is not None:
                last_loss = float(loss)
                trained += 1
        except Exception as exc:
            _log.warning("learn_batch step failed for brain %d: %s", brain_id, exc)
            continue

    probe = await c_api_call(manager.probe_brain, brain_id, timeout=C_API_TIMEOUT_SECONDS)
    return {"trained": trained, "last_loss": last_loss, "probe": probe}
