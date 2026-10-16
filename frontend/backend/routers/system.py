"""System status and version endpoints."""
import time
from fastapi import APIRouter

import nimcp

import nimcp_logger
from brain_manager import manager

_log = nimcp_logger.get("routers.system")

router = APIRouter(prefix="/api/system", tags=["system"])

_start_time = time.time()


@router.get("/status")
async def status():
    brains = manager.list_brains()
    return {
        "version": nimcp.version(),
        "brain_count": len(brains),
        "uptime_seconds": round(time.time() - _start_time, 1),
    }


@router.get("/version")
async def version():
    return {"version": nimcp.version()}
