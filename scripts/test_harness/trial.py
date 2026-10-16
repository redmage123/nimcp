"""Trial context manager using COW snapshots for state isolation."""
from __future__ import annotations

import logging
import time
from contextlib import contextmanager
from typing import Any

log = logging.getLogger("test_harness.trial")


class Trial:
    """Context that snapshots brain state on entry, restores on exit.

    Falls back gracefully if COW snapshots aren't available (no-op snapshot).
    """

    def __init__(self, client, isolate: bool = True, name: str = ""):
        self.client = client
        self.isolate = isolate
        self.name = name
        self._snapshot = None
        self._start_time = 0.0

    def __enter__(self):
        self._start_time = time.time()
        if self.isolate:
            self._snapshot = self._take_snapshot()
        return self

    def __exit__(self, exc_type, exc, tb):
        if self._snapshot is not None:
            self._restore_snapshot(self._snapshot)
        return False

    def _take_snapshot(self) -> Any:
        """Try COW snapshot; return None if unavailable."""
        try:
            return self.client._call("cow_trial_snapshot")
        except Exception as e:
            log.debug("COW snapshot unavailable (%s); trial runs without isolation", e)
            return None

    def _restore_snapshot(self, snap: Any):
        try:
            self.client._call("cow_trial_restore", snapshot=snap)
        except Exception as e:
            log.debug("COW restore failed: %s", e)

    def elapsed_ms(self) -> float:
        return (time.time() - self._start_time) * 1000.0
