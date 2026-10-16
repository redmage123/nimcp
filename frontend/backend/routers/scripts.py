"""Training script runner endpoints."""
from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

import script_runner

router = APIRouter(prefix="/api/scripts", tags=["scripts"])


class ScriptRunRequest(BaseModel):
    script_id: str
    brain_id: int


@router.get("/")
async def list_scripts():
    return script_runner.list_scripts()


@router.post("/run")
async def run_script(req: ScriptRunRequest):
    try:
        run = await script_runner.start_script(req.script_id, req.brain_id)
        return run.to_dict()
    except RuntimeError as e:
        raise HTTPException(409, str(e))
    except ValueError as e:
        raise HTTPException(400, str(e))
    except FileNotFoundError as e:
        raise HTTPException(404, str(e))


@router.get("/status")
async def script_status():
    status = script_runner.get_status()
    if status is None:
        return {"status": "idle"}
    return status


@router.post("/stop")
async def stop_script():
    ok = await script_runner.stop_script()
    if not ok:
        raise HTTPException(404, "No running script to stop")
    return {"stopped": True}
