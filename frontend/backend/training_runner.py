"""Background training orchestrator using asyncio tasks."""
import asyncio
import math
import time
from typing import Optional

import nimcp_logger
from brain_manager import manager
import dataset_manager

_log = nimcp_logger.get("training")
_lock = asyncio.Lock()


class TrainingRun:
    __slots__ = ("brain_id", "dataset_id", "epochs", "batch_size",
                 "running", "epoch", "step", "total_steps", "loss",
                 "accuracy", "learning_rate", "elapsed", "_task",
                 "_subscribers")

    def __init__(self, brain_id: int, dataset_id: str, epochs: int, batch_size: int):
        self.brain_id = brain_id
        self.dataset_id = dataset_id
        self.epochs = epochs
        self.batch_size = batch_size
        self.running = False
        self.epoch = 0
        self.step = 0
        self.total_steps = 0
        self.loss = 0.0
        self.accuracy = 0.0
        self.learning_rate = 0.001
        self.elapsed = 0.0
        self._task: Optional[asyncio.Task] = None
        self._subscribers: list[asyncio.Queue] = []

    def subscribe(self) -> asyncio.Queue:
        q: asyncio.Queue = asyncio.Queue(maxsize=100)
        self._subscribers.append(q)
        return q

    def unsubscribe(self, q: asyncio.Queue):
        try:
            self._subscribers.remove(q)
        except ValueError:
            pass

    def _notify(self, msg: dict):
        for q in list(self._subscribers):
            try:
                q.put_nowait(msg)
            except asyncio.QueueFull:
                pass

    def progress_dict(self) -> dict:
        return {
            "brain_id": self.brain_id, "running": self.running,
            "epoch": self.epoch, "step": self.step,
            "total_steps": self.total_steps, "loss": self.loss,
            "accuracy": self.accuracy, "learning_rate": self.learning_rate,
            "elapsed_seconds": round(self.elapsed, 2),
        }


_active_runs: dict[int, TrainingRun] = {}


def get_run(brain_id: int) -> Optional[TrainingRun]:
    return _active_runs.get(brain_id)


async def start_training(brain_id: int, dataset_id: str, epochs: int, batch_size: int) -> TrainingRun:
    async with _lock:
        if brain_id in _active_runs and _active_runs[brain_id].running:
            raise RuntimeError("Training already in progress")

        _log.info("Starting training brain=%d dataset=%s epochs=%d batch=%d",
                   brain_id, dataset_id, epochs, batch_size)
        run = TrainingRun(brain_id, dataset_id, epochs, batch_size)
        _active_runs[brain_id] = run
        run._task = asyncio.create_task(_train_loop(run))
        return run


async def stop_training(brain_id: int) -> bool:
    async with _lock:
        run = _active_runs.get(brain_id)
        if run is None or not run.running:
            return False
        _log.info("Stopping training for brain %d", brain_id)
        run.running = False
        if run._task:
            run._task.cancel()
        return True


async def _train_loop(run: TrainingRun):
    examples = await asyncio.to_thread(dataset_manager.get_examples, run.dataset_id)
    if not examples:
        run._notify({"type": "training_error", "message": "No examples found"})
        return

    run.total_steps = run.epochs * len(examples)
    run.running = True
    start = time.monotonic()

    correct = 0
    total = 0
    loss_sum = 0.0
    loss_count = 0

    try:
        for epoch in range(run.epochs):
            if not run.running:
                break
            run.epoch = epoch + 1
            for ex in examples:
                if not run.running:
                    break
                run.step += 1
                features = ex.get("features", ex.get("input", []))
                label = str(ex.get("label", ex.get("class", "0")))
                confidence = ex.get("confidence", 1.0)

                await asyncio.to_thread(
                    manager.learn, run.brain_id, features, label, float(confidence)
                )
                run.elapsed = time.monotonic() - start

                # Predict to compute loss and track accuracy
                result = await asyncio.to_thread(
                    manager.predict, run.brain_id, features
                )
                if result is not None:
                    pred_label, pred_conf = result
                    total += 1
                    if pred_label == label:
                        correct += 1
                    # Cross-entropy-style loss with running average for smooth trend
                    sample_loss = -math.log(max(pred_conf, 1e-7))
                    loss_sum += sample_loss
                    loss_count += 1
                    run.loss = loss_sum / loss_count
                    run.accuracy = correct / total if total > 0 else 0.0

                    manager.track_correct(run.brain_id, pred_label == label)
                    manager.update_training_loss(run.brain_id, run.loss)

                run._notify({"type": "training_progress", **run.progress_dict()})

        run.running = False
        _log.info("Training complete brain=%d steps=%d loss=%.4f", run.brain_id, run.step, run.loss)
        manager.save_brain_async(run.brain_id)
        run._notify({
            "type": "training_complete",
            "final_loss": run.loss, "total_steps": run.step,
            "duration": round(run.elapsed, 2),
        })
    except asyncio.CancelledError:
        run.running = False
        _log.info("Training cancelled for brain %d", run.brain_id)
    except Exception as exc:
        run.running = False
        _log.error("Training error for brain %d: %s", run.brain_id, exc, exc_info=True)
        run._notify({"type": "training_error", "message": "Training failed due to an internal error"})
