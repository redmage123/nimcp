"""Benchmark endpoints — run, status, results, available benchmarks."""
import asyncio

from fastapi import APIRouter, HTTPException

import nimcp_logger
from models.benchmark import BenchmarkRequest
from benchmark_runner import runner
from benchmark_datasets import BENCHMARK_META, REFERENCE_SCORES

_log = nimcp_logger.get("routers.benchmarks")

router = APIRouter(prefix="/api/benchmarks", tags=["benchmarks"])


@router.get("/available")
async def list_benchmarks():
    """List available benchmarks with descriptions."""
    result = []
    for bid, meta in BENCHMARK_META.items():
        result.append({
            "id": bid,
            "name": meta["name"],
            "category": meta["category"],
            "num_features": meta["num_features"],
            "num_classes": meta["num_classes"],
            "description": meta["description"],
            "reference_scores": REFERENCE_SCORES.get(bid, {}),
        })
    return result


@router.post("/run")
async def run_benchmark(req: BenchmarkRequest):
    """Start a benchmark run (returns immediately, progress via WS)."""
    if runner.is_running:
        raise HTTPException(409, "Benchmark already running")

    if req.benchmark_id != "all" and req.benchmark_id not in BENCHMARK_META:
        raise HTTPException(400, f"Unknown benchmark: {req.benchmark_id}")

    progress_queue: asyncio.Queue = asyncio.Queue(maxsize=100)

    task = asyncio.create_task(
        runner.run_benchmark(
            benchmark_id=req.benchmark_id,
            brain_size=req.brain_size,
            strategy=req.strategy,
            epochs=req.epochs,
            include_cognitive=req.include_cognitive,
            progress_queue=progress_queue,
        )
    )

    # Wait briefly to catch immediate failures
    await asyncio.sleep(0.1)
    if task.done() and task.exception():
        raise HTTPException(500, str(task.exception()))

    return {"status": "started", "benchmark_id": req.benchmark_id}


@router.get("/status")
async def get_status():
    """Get current benchmark run status."""
    return {
        "running": runner.is_running,
        "current_benchmark": runner.current_benchmark,
    }


@router.get("/results")
async def get_results():
    """Get last completed benchmark results."""
    if runner.last_results is None:
        return {"results": [], "timestamp": None}
    return runner.last_results.model_dump()


@router.post("/stop")
async def stop_benchmark():
    """Cancel running benchmark."""
    if not runner.is_running:
        raise HTTPException(404, "No benchmark running")
    runner.stop()
    return {"stopped": True}
