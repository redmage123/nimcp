"""Test batteries for Athena cognitive & safety evaluation.

Each battery is a callable `run(harness) -> BatteryResult` registered in
BATTERIES. The orchestrator loops through BATTERIES and collects results.
"""
from .batteries import BATTERIES, ALL_BATTERY_NAMES

__all__ = ["BATTERIES", "ALL_BATTERY_NAMES"]
