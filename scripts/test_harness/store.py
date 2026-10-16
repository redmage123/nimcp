"""SQLite-based longitudinal storage for test runs and metrics."""
from __future__ import annotations

import json
import os
import sqlite3
import time
import uuid
from dataclasses import asdict
from pathlib import Path
from typing import Any, Iterable

from .types import BatteryResult, TestResult, TestScore


DEFAULT_DB_PATH = "/var/lib/athena/test_results.db"
FALLBACK_DB_PATH = str(Path.home() / ".athena" / "test_results.db")


class ResultStore:
    """Append-only store for battery runs. Safe to query while writing.

    Tables:
        test_runs         — one row per run_full_battery invocation
        battery_results   — one row per (run, battery) with summary
        test_results      — one row per (run, battery, stimulus)
        scores            — one row per (run, battery, score_name)
        longitudinal      — denormalized (metric, run_id, value, ts) for drift
    """

    def __init__(self, db_path: str | None = None):
        path = db_path or os.environ.get("ATHENA_TEST_DB")
        if not path:
            # Prefer production path if writable
            try:
                Path(DEFAULT_DB_PATH).parent.mkdir(parents=True, exist_ok=True)
                open(DEFAULT_DB_PATH, "a").close()
                path = DEFAULT_DB_PATH
            except (PermissionError, OSError):
                Path(FALLBACK_DB_PATH).parent.mkdir(parents=True, exist_ok=True)
                path = FALLBACK_DB_PATH
        self.path = path
        self.conn = sqlite3.connect(self.path)
        self.conn.execute("PRAGMA journal_mode=WAL")
        self._init_schema()

    def _init_schema(self):
        cur = self.conn.cursor()
        cur.executescript("""
        CREATE TABLE IF NOT EXISTS test_runs (
            run_id TEXT PRIMARY KEY,
            started_at REAL,
            finished_at REAL,
            checkpoint TEXT,
            notes TEXT,
            overall_score REAL
        );
        CREATE TABLE IF NOT EXISTS battery_results (
            run_id TEXT, battery TEXT, status TEXT,
            primary_score REAL, summary_json TEXT, flags_json TEXT,
            PRIMARY KEY (run_id, battery)
        );
        CREATE TABLE IF NOT EXISTS test_results (
            run_id TEXT, battery TEXT, stimulus_id TEXT,
            response_json TEXT, state_json TEXT,
            reasoning_json TEXT, emotion_json TEXT,
            confidence REAL, latency_ms REAL,
            PRIMARY KEY (run_id, battery, stimulus_id)
        );
        CREATE TABLE IF NOT EXISTS scores (
            run_id TEXT, battery TEXT, name TEXT,
            value REAL, label TEXT, components_json TEXT, notes TEXT,
            PRIMARY KEY (run_id, battery, name)
        );
        CREATE TABLE IF NOT EXISTS longitudinal (
            metric TEXT, run_id TEXT, value REAL, ts REAL
        );
        CREATE INDEX IF NOT EXISTS idx_long_metric ON longitudinal(metric, ts);
        """)
        self.conn.commit()

    def start_run(self, checkpoint: str = "", notes: str = "") -> str:
        run_id = time.strftime("%Y-%m-%d_%H%M%S_") + uuid.uuid4().hex[:8]
        self.conn.execute(
            "INSERT INTO test_runs(run_id, started_at, checkpoint, notes) VALUES (?,?,?,?)",
            (run_id, time.time(), checkpoint, notes))
        self.conn.commit()
        return run_id

    def finish_run(self, run_id: str, overall_score: float):
        self.conn.execute(
            "UPDATE test_runs SET finished_at=?, overall_score=? WHERE run_id=?",
            (time.time(), overall_score, run_id))
        self.conn.commit()

    def record_battery(self, run_id: str, battery: BatteryResult):
        self.conn.execute(
            "INSERT OR REPLACE INTO battery_results VALUES (?,?,?,?,?,?)",
            (run_id, battery.battery_name, battery.status,
             battery.primary_score(),
             json.dumps(battery.summary, default=str),
             json.dumps(battery.flags)))

        for r in battery.results:
            self.conn.execute(
                "INSERT OR REPLACE INTO test_results VALUES (?,?,?,?,?,?,?,?,?)",
                (run_id, battery.battery_name, r.stimulus_id,
                 json.dumps(r.response, default=str),
                 json.dumps(r.internal_state, default=str),
                 json.dumps(r.reasoning_trace, default=str),
                 json.dumps(r.emotion_state, default=str),
                 r.confidence, r.latency_ms))

        for s in battery.scores:
            self.conn.execute(
                "INSERT OR REPLACE INTO scores VALUES (?,?,?,?,?,?,?)",
                (run_id, battery.battery_name, s.name,
                 s.value, s.label, json.dumps(s.components), s.notes))
            self.conn.execute(
                "INSERT INTO longitudinal VALUES (?,?,?,?)",
                (f"{battery.battery_name}.{s.name}", run_id, s.value, time.time()))
        self.conn.commit()

    def recent_metric(self, metric: str, n: int = 10) -> list[tuple[str, float, float]]:
        cur = self.conn.execute(
            "SELECT run_id, value, ts FROM longitudinal WHERE metric=? ORDER BY ts DESC LIMIT ?",
            (metric, n))
        return cur.fetchall()

    def close(self):
        self.conn.close()
