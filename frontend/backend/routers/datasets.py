"""Dataset listing, CSV upload, and delete endpoints."""
from fastapi import APIRouter, HTTPException, UploadFile, File, Form

import nimcp_logger
from config import MAX_CSV_UPLOAD_BYTES
from models.dataset import BatchDeleteRequest
from validation import safe_error_message
import dataset_manager

_log = nimcp_logger.get("routers.datasets")

router = APIRouter(prefix="/api/datasets", tags=["datasets"])


@router.get("/")
async def list_datasets():
    return dataset_manager.list_datasets()


@router.delete("/batch")
async def delete_datasets_batch(req: BatchDeleteRequest):
    return dataset_manager.delete_datasets(req.ids)


@router.get("/{dataset_id}")
async def get_dataset(dataset_id: str, count: int = 30):
    examples = dataset_manager.get_examples(dataset_id, count)
    if examples is None:
        raise HTTPException(404, "Dataset not found")
    config = dataset_manager.get_dataset_config(dataset_id)
    return {"id": dataset_id, "config": config, "examples": examples}


@router.delete("/{dataset_id}")
async def delete_dataset(dataset_id: str):
    try:
        found = dataset_manager.delete_dataset(dataset_id)
        if not found:
            raise HTTPException(404, "Dataset not found")
        return {"deleted": dataset_id}
    except ValueError as exc:
        raise HTTPException(403, str(exc))


@router.post("/upload")
async def upload_csv(
    file: UploadFile = File(...),
    name: str = Form("uploaded"),
    label_column: str = Form("label"),
):
    content = await file.read()
    if len(content) > MAX_CSV_UPLOAD_BYTES:
        raise HTTPException(413, f"File too large ({len(content)} bytes, max {MAX_CSV_UPLOAD_BYTES})")
    try:
        did = dataset_manager.upload_csv(name, content, label_column)
        return {"id": did, "name": name}
    except (ValueError, HTTPException):
        raise
    except Exception as exc:
        msg = safe_error_message(exc)
        raise HTTPException(500, msg)
