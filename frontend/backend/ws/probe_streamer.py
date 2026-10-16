"""Background probe loop that pushes data to connected WebSocket clients."""
import asyncio
import json

import nimcp_logger
from brain_manager import manager
from config import PROBE_INTERVAL_MS

_log = nimcp_logger.get("ws.probe_streamer")

# brain_id -> set of asyncio.Queue
_probe_subscribers: dict[int, set[asyncio.Queue]] = {}


def subscribe_probe(brain_id: int) -> asyncio.Queue:
    q: asyncio.Queue = asyncio.Queue(maxsize=50)
    _probe_subscribers.setdefault(brain_id, set()).add(q)
    return q


def unsubscribe_probe(brain_id: int, q: asyncio.Queue):
    subs = _probe_subscribers.get(brain_id)
    if subs:
        subs.discard(q)
        if not subs:
            del _probe_subscribers[brain_id]


async def probe_loop():
    """Runs forever, probing all subscribed brains at PROBE_INTERVAL_MS."""
    interval = PROBE_INTERVAL_MS / 1000.0
    while True:
        for brain_id in list(_probe_subscribers.keys()):
            subs = _probe_subscribers.get(brain_id)
            if not subs:
                continue
            try:
                probe = await asyncio.to_thread(manager.probe_brain, brain_id)
            except Exception as exc:
                _log.debug("Probe failed for brain %d: %s", brain_id, exc)
                continue
            if probe is None:
                continue
            msg = json.dumps({"type": "probe", "brain_id": brain_id, **probe})
            for q in list(subs):
                try:
                    q.put_nowait(msg)
                except asyncio.QueueFull:
                    pass
        await asyncio.sleep(interval)
