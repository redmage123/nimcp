"""Training script runner — launches scripts/ as subprocesses with stdout streaming."""
import asyncio
import os
import signal
from typing import Optional

import nimcp_logger
from brain_manager import manager
from config import BRAIN_STORAGE_DIR

_log = nimcp_logger.get("script_runner")

PROJECT_ROOT = os.environ.get("NIMCP_ROOT", "/home/bbrelin/nimcp")

AVAILABLE_SCRIPTS = {
    "train_local": {
        "name": "Local Dataset Training",
        "description": "MathQA + Gutenberg datasets",
        "path": os.path.join(PROJECT_ROOT, "scripts", "train_local.py"),
    },
    "train_foundation": {
        "name": "Foundation Model Training",
        "description": "Progressive multi-domain curriculum learning",
        "path": os.path.join(PROJECT_ROOT, "scripts", "train_foundation_model.py"),
    },
    "streaming_train": {
        "name": "Streaming Training",
        "description": "Continuous streaming data training",
        "path": os.path.join(PROJECT_ROOT, "scripts", "streaming_train.py"),
    },
    "progressive_training": {
        "name": "Progressive Training",
        "description": "Curriculum-based progressive training",
        "path": os.path.join(PROJECT_ROOT, "scripts", "progressive_training.py"),
    },
    "hybrid_train": {
        "name": "Hybrid Training",
        "description": "Combined online + batch training",
        "path": os.path.join(PROJECT_ROOT, "scripts", "hybrid_train.py"),
    },
    "parallel_train": {
        "name": "Parallel Training",
        "description": "Multi-worker parallel training",
        "path": os.path.join(PROJECT_ROOT, "scripts", "parallel_train.py"),
    },
}

MAX_STDOUT_LINES = 200


class ScriptRun:
    def __init__(self, script_id: str, brain_id: int):
        self.script_id = script_id
        self.brain_id = brain_id
        self.status = "starting"  # starting, running, completed, failed, stopped
        self.exit_code: Optional[int] = None
        self.stdout_lines: list[str] = []
        self._process: Optional[asyncio.subprocess.Process] = None
        self._task: Optional[asyncio.Task] = None

    def to_dict(self) -> dict:
        return {
            "script_id": self.script_id,
            "brain_id": self.brain_id,
            "status": self.status,
            "exit_code": self.exit_code,
            "stdout_lines": self.stdout_lines[-20:],
            "total_lines": len(self.stdout_lines),
        }


_current_run: Optional[ScriptRun] = None


def list_scripts() -> list[dict]:
    result = []
    for sid, info in AVAILABLE_SCRIPTS.items():
        result.append({
            "id": sid,
            "name": info["name"],
            "description": info["description"],
            "exists": os.path.isfile(info["path"]),
        })
    return result


async def start_script(script_id: str, brain_id: int) -> ScriptRun:
    global _current_run
    if _current_run is not None and _current_run.status in ("starting", "running"):
        raise RuntimeError("A script is already running")
    if script_id not in AVAILABLE_SCRIPTS:
        raise ValueError(f"Unknown script: {script_id}")
    script_info = AVAILABLE_SCRIPTS[script_id]
    if not os.path.isfile(script_info["path"]):
        raise FileNotFoundError(f"Script not found: {script_info['path']}")

    # Save brain to disk so script can load it
    manager.save_brain_async(brain_id)

    run = ScriptRun(script_id, brain_id)
    _current_run = run
    run._task = asyncio.create_task(_run_script(run, script_info["path"]))
    return run


async def _run_script(run: ScriptRun, script_path: str):
    brain_dir = os.path.join(BRAIN_STORAGE_DIR, str(run.brain_id))
    brain_bin = os.path.join(brain_dir, "brain.bin")
    output_bin = os.path.join(brain_dir, "brain_trained.bin")

    cmd = [
        "python3", script_path,
        "--brain-path", brain_bin,
        "--output-path", output_bin,
    ]

    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"

    # Pass HuggingFace token if available (for streaming training)
    hf_token = os.environ.get("HF_TOKEN")
    if hf_token:
        env["HF_TOKEN"] = hf_token

    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            env=env,
            cwd=PROJECT_ROOT,
        )
        run._process = proc
        run.status = "running"
        _log.info("Script %s started (pid=%d) for brain %d", run.script_id, proc.pid, run.brain_id)

        while True:
            line = await proc.stdout.readline()
            if not line:
                break
            text = line.decode("utf-8", errors="replace").rstrip("\n")
            run.stdout_lines.append(text)
            if len(run.stdout_lines) > MAX_STDOUT_LINES:
                run.stdout_lines = run.stdout_lines[-MAX_STDOUT_LINES:]

        await proc.wait()
        run.exit_code = proc.returncode

        if run.status == "running":
            if run.exit_code == 0:
                run.status = "completed"
                # Reload trained weights if output file exists
                if os.path.isfile(output_bin):
                    try:
                        manager.load_brain(run.brain_id, output_bin)
                        manager.save_brain_async(run.brain_id)
                        _log.info("Reloaded trained brain %d from script output", run.brain_id)
                    except Exception as exc:
                        _log.error("Failed to reload brain after script: %s", exc)
            else:
                run.status = "failed"

        _log.info("Script %s finished (exit=%d) for brain %d", run.script_id, run.exit_code, run.brain_id)

    except Exception as exc:
        run.status = "failed"
        run.stdout_lines.append(f"[ERROR] {exc}")
        _log.error("Script runner error: %s", exc, exc_info=True)


async def stop_script() -> bool:
    global _current_run
    if _current_run is None or _current_run.status not in ("starting", "running"):
        return False
    run = _current_run
    run.status = "stopped"
    if run._process and run._process.returncode is None:
        try:
            run._process.send_signal(signal.SIGTERM)
            _log.info("Sent SIGTERM to script pid=%d", run._process.pid)
        except ProcessLookupError:
            pass
    return True


def get_status() -> Optional[dict]:
    if _current_run is None:
        return None
    return _current_run.to_dict()
