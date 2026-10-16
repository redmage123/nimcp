"""Thread-safe multi-brain registry.

Adapts to the actual nimcp Python API:
  - brain.predict(features) -> (label_str, confidence)
  - brain.learn(features, label_str, confidence) -> loss_float
  - brain.get_neuron_count() -> int
  - brain.get_utilization_metrics() -> (utilization, saturation)
  - brain.broadcast_probe() -> bool
  - brain.snapshot_cow() -> capsule
  - brain.restore_cow(capsule) -> bool
  - brain.save(path) / brain.load(path)
  - brain.probe() -> dict with full C-level metrics
"""
import json
import os
import shutil
import threading
import time
from collections import deque
from typing import Optional

import nimcp

import nimcp_logger
from cognitive_response import CognitiveInterpreter
from config import MAX_BRAIN_COUNT, PROBE_HISTORY_SIZE, BRAIN_STORAGE_DIR

# Default avatar identity — Athena's self-chosen appearance
DEFAULT_IDENTITY = {
    "skin_hue": 28,           # HSL hue (0-360)
    "skin_saturation": 0.55,  # HSL saturation (0-1)
    "skin_lightness": 0.72,   # HSL lightness (0-1)
    "eye_color_r": 0.29,      # RGB (0-1)
    "eye_color_g": 0.48,
    "eye_color_b": 0.71,
    "hair_color_r": 0.22,
    "hair_color_g": 0.15,
    "hair_color_b": 0.10,
    "hair_length": 0.6,       # 0=bald, 1=long
    "hair_style": "straight", # straight, wavy, curly
    "face_width": 0.5,        # 0=narrow, 1=wide
    "face_height": 0.5,       # 0=short, 1=tall
    "lip_fullness": 0.5,      # 0=thin, 1=full
    "lip_color_r": 0.80,
    "lip_color_g": 0.45,
    "lip_color_b": 0.45,
    "nose_width": 0.5,        # 0=narrow, 1=wide
    "brow_thickness": 0.5,    # 0=thin, 1=thick
    "brow_color_r": 0.30,
    "brow_color_g": 0.22,
    "brow_color_b": 0.15,
    "cheek_roundness": 0.5,   # 0=angular, 1=round
    "chin_shape": 0.5,        # 0=pointed, 1=rounded
    "freckles": 0.0,          # 0=none, 1=heavy
    "voice_pitch": 0.6,       # 0=low, 1=high (feminine default for Athena)
    "voice_warmth": 0.7,      # 0=cold, 1=warm
}

_log = nimcp_logger.get("brain_manager")

# Athena pretrained baseline — auto-applied to new brains when available
_ATHENA_SEARCH_PATHS = [
    os.path.join(os.path.dirname(__file__), "..", "..", "models", "pretrained", "athena", "v1.0", "athena.bin"),
    os.path.join(os.path.dirname(__file__), "..", "..", "models", "pretrained", "athena", "v1.0",
                 "nimcp_athena_foundation_v1.0.nimcp"),
    os.path.expanduser("~/.nimcp/models/pretrained/athena/v1.0/athena.bin"),
]
_ATHENA_BASELINE_PATH: Optional[str] = None
for _p in _ATHENA_SEARCH_PATHS:
    _p = os.path.normpath(_p)
    if os.path.isfile(_p):
        _ATHENA_BASELINE_PATH = _p
        break

SIZE_LABELS = {0: "Tiny", 1: "Small", 2: "Medium", 3: "Large"}
TASK_LABELS = {0: "Classification", 1: "Regression", 2: "Pattern Matching", 3: "Sequence", 4: "Association"}


class BrainStats:
    """Tracks metrics that the C API doesn't expose as a probe dict."""
    __slots__ = ("total_inferences", "total_learning_steps", "total_correct",
                 "last_loss", "losses", "inference_times")

    def __init__(self):
        self.total_inferences = 0
        self.total_learning_steps = 0
        self.total_correct = 0
        self.last_loss = 0.0
        self.losses: deque = deque(maxlen=200)
        self.inference_times: deque = deque(maxlen=200)

    def to_dict(self) -> dict:
        return {
            "total_inferences": self.total_inferences,
            "total_learning_steps": self.total_learning_steps,
            "total_correct": self.total_correct,
            "last_loss": self.last_loss,
            "losses": list(self.losses),
        }

    def load_dict(self, d: dict):
        self.total_inferences = d.get("total_inferences", 0)
        self.total_learning_steps = d.get("total_learning_steps", 0)
        self.total_correct = d.get("total_correct", 0)
        self.last_loss = d.get("last_loss", 0.0)
        for v in d.get("losses", []):
            self.losses.append(v)


class BrainMetadata:
    __slots__ = ("name", "created_at", "dataset", "parent_id", "num_inputs", "num_outputs", "size", "task", "owner")

    def __init__(self, name: str, num_inputs: int = 0, num_outputs: int = 0,
                 size: int = 1, task: int = 0,
                 dataset: Optional[str] = None, parent_id: Optional[int] = None,
                 created_at: Optional[str] = None, owner: Optional[str] = None):
        self.name = name
        self.created_at = created_at or time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        self.dataset = dataset
        self.parent_id = parent_id
        self.num_inputs = num_inputs
        self.num_outputs = num_outputs
        self.size = size
        self.task = task
        self.owner = owner  # None = shared/system, str = personal brain

    def to_dict(self) -> dict:
        d = {
            "name": self.name,
            "created_at": self.created_at,
            "dataset": self.dataset,
            "parent_id": self.parent_id,
            "num_inputs": self.num_inputs,
            "num_outputs": self.num_outputs,
            "size": self.size,
            "task": self.task,
        }
        if self.owner is not None:
            d["owner"] = self.owner
        return d


class BrainManager:
    """Thread-safe singleton managing brain instances."""

    def __init__(self):
        self._lock = threading.Lock()
        self._brains: dict[int, nimcp.Brain] = {}
        self._metadata: dict[int, BrainMetadata] = {}
        self._stats: dict[int, BrainStats] = {}
        self._probe_history: dict[int, deque] = {}
        self._next_id = 0
        # Per-user interpreters: keyed by (brain_id, username)
        self._interpreters: dict[tuple[int, str], CognitiveInterpreter] = {}
        self._snapshots: dict[int, dict] = {}  # brain_id -> {name: path}
        self._cow_snapshots: dict[int, dict] = {}  # brain_id -> {name: capsule}
        self._identities: dict[int, dict] = {}  # brain_id -> identity params

    # --- Persistence ---

    def _brain_dir(self, bid: int) -> str:
        return os.path.join(BRAIN_STORAGE_DIR, str(bid))

    def _save_brain_to_disk(self, bid: int):
        """Save brain weights + metadata to disk. Must be called with _lock held."""
        brain = self._brains.get(bid)
        if brain is None:
            return
        bdir = self._brain_dir(bid)
        os.makedirs(bdir, exist_ok=True)
        try:
            brain.save(os.path.join(bdir, "brain.bin"))
            meta_dict = self._metadata[bid].to_dict()
            meta_dict["stats"] = self._stats[bid].to_dict()
            identity = self._identities.get(bid)
            if identity:
                meta_dict["identity"] = identity
            with open(os.path.join(bdir, "meta.json"), "w") as f:
                json.dump(meta_dict, f, indent=2)
            _log.debug("Saved brain %d to disk", bid)
        except Exception as exc:
            _log.error("Failed to save brain %d to disk: %s", bid, exc)

    def save_brain_async(self, bid: int):
        """Public wrapper for saving a brain to disk (acquires lock)."""
        with self._lock:
            self._save_brain_to_disk(bid)

    def _delete_brain_from_disk(self, bid: int):
        bdir = self._brain_dir(bid)
        if os.path.isdir(bdir):
            shutil.rmtree(bdir, ignore_errors=True)
            _log.debug("Deleted brain %d from disk", bid)

    def load_all_from_disk(self):
        """Scan storage dir and restore all saved brains."""
        if not os.path.isdir(BRAIN_STORAGE_DIR):
            _log.info("No brain storage dir found at %s", BRAIN_STORAGE_DIR)
            return
        loaded = 0
        max_id = -1
        for entry in sorted(os.listdir(BRAIN_STORAGE_DIR)):
            bdir = os.path.join(BRAIN_STORAGE_DIR, entry)
            meta_path = os.path.join(bdir, "meta.json")
            bin_path = os.path.join(bdir, "brain.bin")
            if not os.path.isfile(meta_path) or not os.path.isfile(bin_path):
                continue
            try:
                bid = int(entry)
            except ValueError:
                continue
            try:
                with open(meta_path) as f:
                    meta_dict = json.load(f)
                brain = nimcp.Brain(
                    name=meta_dict["name"],
                    size=meta_dict.get("size", 1),
                    task=meta_dict.get("task", 0),
                    num_inputs=meta_dict.get("num_inputs", 4),
                    num_outputs=meta_dict.get("num_outputs", 3),
                )
                brain.load(bin_path)
                meta = BrainMetadata(
                    name=meta_dict["name"],
                    num_inputs=meta_dict.get("num_inputs", 4),
                    num_outputs=meta_dict.get("num_outputs", 3),
                    size=meta_dict.get("size", 1),
                    task=meta_dict.get("task", 0),
                    dataset=meta_dict.get("dataset"),
                    parent_id=meta_dict.get("parent_id"),
                    created_at=meta_dict.get("created_at"),
                    owner=meta_dict.get("owner"),
                )
                stats = BrainStats()
                stats_dict = meta_dict.get("stats")
                if stats_dict:
                    stats.load_dict(stats_dict)
                with self._lock:
                    self._brains[bid] = brain
                    self._metadata[bid] = meta
                    self._stats[bid] = stats
                    self._probe_history[bid] = deque(maxlen=PROBE_HISTORY_SIZE)
                    self._snapshots[bid] = {}
                    self._cow_snapshots[bid] = {}
                    self._identities[bid] = meta_dict.get("identity", dict(DEFAULT_IDENTITY))
                    if bid > max_id:
                        max_id = bid
                loaded += 1
                _log.info("Restored brain %d (%s) from disk", bid, meta_dict["name"])
            except Exception as exc:
                _log.error("Failed to restore brain %d: %s", bid, exc)
        with self._lock:
            if max_id >= self._next_id:
                self._next_id = max_id + 1
        _log.info("Loaded %d brains from disk", loaded)

    # --- CRUD ---

    def create_brain(self, name: str, size: int, task: int,
                     num_inputs: int, num_outputs: int,
                     dataset: Optional[str] = None,
                     num_neurons: Optional[int] = None,
                     owner: Optional[str] = None) -> int:
        with self._lock:
            if len(self._brains) >= MAX_BRAIN_COUNT:
                raise ValueError(f"Maximum brain count ({MAX_BRAIN_COUNT}) reached")
            brain = nimcp.Brain(
                name=name, size=size, task=task,
                num_inputs=num_inputs, num_outputs=num_outputs
            )
            if num_neurons is not None:
                brain.resize(num_neurons)
            # Auto-apply Athena pretrained baseline if available
            if _ATHENA_BASELINE_PATH:
                try:
                    brain.load(_ATHENA_BASELINE_PATH)
                    _log.info("Applied Athena baseline from %s", _ATHENA_BASELINE_PATH)
                except Exception as exc:
                    _log.debug("Athena baseline load skipped: %s", exc)
            bid = self._next_id
            self._next_id += 1
            self._brains[bid] = brain
            self._metadata[bid] = BrainMetadata(
                name=name, num_inputs=num_inputs, num_outputs=num_outputs,
                size=size, task=task, dataset=dataset, owner=owner,
            )
            self._stats[bid] = BrainStats()
            self._probe_history[bid] = deque(maxlen=PROBE_HISTORY_SIZE)
            self._snapshots[bid] = {}
            self._cow_snapshots[bid] = {}
            self._identities[bid] = dict(DEFAULT_IDENTITY)
            self._save_brain_to_disk(bid)
            _log.info("Created brain %d (%s) neurons=%s inputs=%d outputs=%d owner=%s",
                      bid, name, num_neurons or "preset", num_inputs, num_outputs, owner or "shared")
            return bid

    def resize_brain(self, bid: int, num_neurons: int) -> bool:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return False
            ok = brain.resize(num_neurons)
            if ok:
                self._save_brain_to_disk(bid)
                _log.info("Resized brain %d to %d neurons", bid, num_neurons)
            return ok

    def destroy_brain(self, bid: int) -> bool:
        with self._lock:
            if bid not in self._brains:
                return False
            del self._brains[bid]
            del self._metadata[bid]
            del self._stats[bid]
            del self._probe_history[bid]
            self._interpreters.pop(bid, None)
            self._snapshots.pop(bid, None)
            self._cow_snapshots.pop(bid, None)
            self._identities.pop(bid, None)
            self._delete_brain_from_disk(bid)
            _log.info("Destroyed brain %d", bid)
            return True

    def get_brain(self, bid: int) -> Optional[nimcp.Brain]:
        with self._lock:
            return self._brains.get(bid)

    def get_metadata(self, bid: int) -> Optional[BrainMetadata]:
        with self._lock:
            return self._metadata.get(bid)

    def has_brain(self, bid: int) -> bool:
        with self._lock:
            return bid in self._brains

    # --- Rename ---

    def rename_brain(self, bid: int, new_name: str) -> bool:
        with self._lock:
            meta = self._metadata.get(bid)
            if meta is None:
                return False
            meta.name = new_name
            self._save_brain_to_disk(bid)
            _log.info("Renamed brain %d to '%s'", bid, new_name)
            return True

    # --- Detail view ---

    def get_brain_detail(self, bid: int) -> Optional[dict]:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            meta = self._metadata[bid]
            stats = self._stats[bid]
            probe = self._build_probe(bid)
            accuracy = (stats.total_correct / stats.total_inferences) if stats.total_inferences > 0 else 0.0
            return {
                "id": bid,
                "name": meta.name,
                "created_at": meta.created_at,
                "size": meta.size,
                "size_label": SIZE_LABELS.get(meta.size, "Unknown"),
                "task": meta.task,
                "task_label": TASK_LABELS.get(meta.task, "Unknown"),
                "num_inputs": meta.num_inputs,
                "num_outputs": meta.num_outputs,
                "dataset": meta.dataset,
                "parent_id": meta.parent_id,
                "probe": probe,
                "total_inferences": stats.total_inferences,
                "total_learning_steps": stats.total_learning_steps,
                "accuracy": accuracy,
                "last_loss": stats.last_loss,
                "loss_history": list(stats.losses),
            }

    # --- Probe ---

    def _build_probe(self, bid: int) -> Optional[dict]:
        """Get probe from C brain.probe() and overlay Python-tracked stats."""
        brain = self._brains.get(bid)
        if brain is None:
            return None
        stats = self._stats[bid]

        try:
            probe = brain.probe()
        except Exception as exc:
            _log.debug("brain.probe() failed for brain %d: %s", bid, exc)
            return None

        # utilization not in C probe struct — fetch separately
        try:
            utilization, _saturation = brain.get_utilization_metrics()
        except Exception:
            utilization = 0.0
        probe["utilization"] = utilization

        # Fields not in C probe struct — always set from Python stats
        probe["last_loss"] = stats.last_loss

        # Overlay Python-tracked stats that accumulate across session
        if stats.total_inferences > 0:
            probe["total_inferences"] = stats.total_inferences
            probe["accuracy"] = stats.total_correct / stats.total_inferences
        if stats.total_learning_steps > 0:
            probe["total_learning_steps"] = stats.total_learning_steps
        if stats.inference_times:
            probe["avg_inference_time_us"] = sum(stats.inference_times) / len(stats.inference_times)

        return probe

    def list_brains(self) -> list[dict]:
        with self._lock:
            result = []
            for bid, meta in self._metadata.items():
                info = {
                    "id": bid,
                    "name": meta.name,
                    "created_at": meta.created_at,
                    "dataset": meta.dataset,
                    "parent_id": meta.parent_id,
                    "probe": self._build_probe(bid),
                }
                result.append(info)
            return result

    def probe_brain(self, bid: int) -> Optional[dict]:
        with self._lock:
            probe = self._build_probe(bid)
            if probe is None:
                return None
            self._probe_history[bid].append({
                "timestamp": time.time(),
                **probe,
            })
            return probe

    def get_probe_history(self, bid: int) -> list[dict]:
        with self._lock:
            hist = self._probe_history.get(bid)
            return list(hist) if hist else []

    # --- Inference / Learning ---

    def predict(self, bid: int, features: list[float]) -> Optional[tuple[str, float]]:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            stats = self._stats[bid]
            t0 = time.monotonic()
            result = brain.predict(features)
            elapsed_us = (time.monotonic() - t0) * 1_000_000
            stats.total_inferences += 1
            stats.inference_times.append(elapsed_us)
            return result  # (label_str, confidence)

    def decide(self, bid: int, features: list[float]) -> Optional[dict]:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            stats = self._stats[bid]
            t0 = time.monotonic()
            result = brain.decide(features)
            elapsed_us = (time.monotonic() - t0) * 1_000_000
            stats.total_inferences += 1
            stats.inference_times.append(elapsed_us)
            return result  # dict with label, confidence, explanation, sparsity, etc.

    def _get_interpreter(self, bid: int, username: str = "") -> Optional["CognitiveInterpreter"]:
        """Get or create per-user cognitive interpreter for a brain, syncing baselines."""
        meta = self._metadata.get(bid)
        if meta is None:
            return None
        key = (bid, username)
        interpreter = self._interpreters.get(key)
        if interpreter is None:
            stats = self._stats[bid]
            interpreter = CognitiveInterpreter(brain_name=meta.name)
            # Sync baselines so pre-existing training isn't mistaken for new growth
            interpreter._last_learning_steps = stats.total_learning_steps
            interpreter._last_inferences = stats.total_inferences
            self._interpreters[key] = interpreter
        return interpreter

    def converse(self, bid: int, text: str, username: str = "") -> Optional[dict]:
        """Run full cognitive pipeline and return conversational response."""
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            meta = self._metadata[bid]
            stats = self._stats[bid]
            interpreter = self._get_interpreter(bid, username)
            if interpreter is None:
                return None

            features = interpreter.encode_text(text, meta.num_inputs, interpreter.history)
            cognitive_state = interpreter.interpret(brain, features, text, interpreter.history)
            stats_dict = {
                "total_inferences": stats.total_inferences,
                "total_learning_steps": stats.total_learning_steps,
            }
            response = interpreter.generate_response(cognitive_state, text, interpreter.history, stats_dict)

            # Handle appearance description sentinel
            identity = self._identities.get(bid, DEFAULT_IDENTITY)
            if response == "__DESCRIBE_APPEARANCE__":
                response = interpreter.describe_identity(identity)

            # Check for identity-learning phrases ("my eyes are blue", etc.)
            identity_updates = interpreter.parse_identity_update(text)
            identity_learned = False
            if identity_updates:
                current = dict(identity)
                for key, value in identity_updates:
                    if key in DEFAULT_IDENTITY:
                        current[key] = value
                self._identities[bid] = current
                self._save_brain_to_disk(bid)
                identity_learned = True
                _log.info("Identity updated from conversation: %s", identity_updates)

            interpreter.add_to_history("user", text)
            interpreter.add_to_history("brain", response)

            stats.total_inferences += 1
            result = {
                "message": response,
                "cognitive_state": cognitive_state,
            }
            if identity_learned:
                result["identity_updated"] = True
                result["identity"] = self._identities[bid]
            return result

    def teach_conversational(self, bid: int, text: str, label: str, username: str = "") -> Optional[dict]:
        """Teach the brain via the cognitive interpreter."""
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            stats = self._stats[bid]
            interpreter = self._get_interpreter(bid, username)
            if interpreter is None:
                return None

            result = interpreter.teach(brain, text, label)
            stats.total_learning_steps += 1
            if result.get("loss") is not None:
                stats.last_loss = result["loss"]
                stats.losses.append(result["loss"])
            return result

    def introspect(self, bid: int, username: str = "") -> Optional[dict]:
        """Ask the brain to describe its own state."""
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            stats = self._stats[bid]
            interpreter = self._get_interpreter(bid, username)
            if interpreter is None:
                return None

            # Build cognitive_state dict from brain metrics
            try:
                neuron_count = brain.get_neuron_count()
            except Exception:
                neuron_count = 0
            try:
                utilization, _ = brain.get_utilization_metrics()
            except Exception:
                utilization = 0.0

            cognitive_state = {
                "neuron_count": neuron_count,
                "utilization": utilization,
            }
            stats_dict = {
                "total_inferences": stats.total_inferences,
                "total_learning_steps": stats.total_learning_steps,
                "accuracy": (stats.total_correct / stats.total_inferences) if stats.total_inferences > 0 else 0.0,
                "last_loss": stats.last_loss,
            }
            response = interpreter.generate_introspection(cognitive_state, stats_dict)
            return {"message": response}

    def ensure_athena(self):
        """Ensure brain_id=0 exists as the shared Athena instance."""
        with self._lock:
            if 0 in self._brains:
                _log.info("Athena (brain_id=0) already loaded")
                return
        # Create Athena as brain 0 outside lock (create_brain acquires lock)
        _log.info("Creating shared Athena brain (brain_id=0)")
        # Use reasonable defaults for the shared assistant
        bid = self.create_brain(
            name="Athena", size=2, task=4,
            num_inputs=1024, num_outputs=64,
        )
        if bid != 0:
            _log.warning("Athena created as brain_id=%d (expected 0)", bid)

    def list_brains_for_user(self, username: str, role: str) -> list[dict]:
        """List brains visible to a user. Admins see all, users see shared + their own."""
        with self._lock:
            result = []
            for bid, meta in self._metadata.items():
                # Admins see everything; users see shared (no owner) + their own
                if role != "admin" and meta.owner is not None and meta.owner != username:
                    continue
                info = {
                    "id": bid,
                    "name": meta.name,
                    "created_at": meta.created_at,
                    "dataset": meta.dataset,
                    "parent_id": meta.parent_id,
                    "owner": meta.owner,
                    "probe": self._build_probe(bid),
                }
                result.append(info)
            return result

    def learn(self, bid: int, features: list[float], label: str, confidence: float = 1.0) -> Optional[float]:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            stats = self._stats[bid]
            loss = brain.learn(features, label, confidence)
            stats.total_learning_steps += 1
            stats.last_loss = float(loss) if loss is not None else stats.last_loss
            stats.losses.append(stats.last_loss)
            return stats.last_loss

    def track_correct(self, bid: int, correct: bool):
        """Track prediction correctness for accuracy calculation."""
        with self._lock:
            if correct and bid in self._stats:
                self._stats[bid].total_correct += 1

    def update_training_loss(self, bid: int, loss: float):
        """Update loss during training so dashboard charts show it live."""
        with self._lock:
            stats = self._stats.get(bid)
            if stats is None:
                return
            stats.last_loss = loss
            stats.losses.append(loss)

    # --- Save/Load (snapshots) ---

    def save_brain(self, bid: int, path: str) -> bool:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return False
            try:
                brain.save(path)
                return True
            except Exception as exc:
                _log.error("Failed to save brain %d to %s: %s", bid, path, exc)
                return False

    def load_brain(self, bid: int, path: str) -> bool:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return False
            try:
                brain.load(path)
                return True
            except Exception as exc:
                _log.error("Failed to load brain %d from %s: %s", bid, path, exc)
                return False

    def snapshot_cow(self, bid: int) -> bool:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return False
            try:
                brain.snapshot_cow()
                return True
            except Exception as exc:
                _log.error("COW snapshot failed for brain %d: %s", bid, exc)
                return False

    def restore_cow(self, bid: int) -> bool:
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return False
            try:
                brain.restore_cow()
                return True
            except Exception as exc:
                _log.error("COW restore failed for brain %d: %s", bid, exc)
                return False

    # --- Snapshot accessors (thread-safe) ---

    def register_snapshot(self, bid: int, name: str, path: str) -> None:
        with self._lock:
            self._snapshots.setdefault(bid, {})[name] = path

    def list_snapshot_names(self, bid: int) -> list[dict]:
        with self._lock:
            return [{"name": n, "path": p} for n, p in self._snapshots.get(bid, {}).items()]

    def get_snapshot_path(self, bid: int, name: str) -> Optional[str]:
        with self._lock:
            return self._snapshots.get(bid, {}).get(name)

    def remove_snapshot(self, bid: int, name: str) -> bool:
        with self._lock:
            snaps = self._snapshots.get(bid, {})
            if name not in snaps:
                return False
            del snaps[name]
            return True

    # --- Speech & Avatar ---

    def speak(self, bid: int, semantic_vector: list[float] | None = None) -> dict | None:
        """Generate spoken text from brain's language production pipeline.

        Args:
            bid: Brain ID
            semantic_vector: Optional semantic vector (uses last decide output if None)

        Returns:
            dict with 'text', 'confidence', 'fluency' or None on failure
        """
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            try:
                if semantic_vector:
                    result = brain.speak(semantic_vector)
                else:
                    # Fallback: use a zero vector to trigger language production
                    meta = self._metadata[bid]
                    result = brain.speak([0.0] * min(meta.num_outputs, 256))
                return result
            except Exception as exc:
                _log.debug("brain.speak() failed for brain %d: %s", bid, exc)
                return None

    def get_avatar_state(self, bid: int) -> dict | None:
        """Get avatar visual state (FACS AUs, visemes, gaze, emotion, voice).

        Returns:
            dict with all avatar fields or None on failure
        """
        with self._lock:
            brain = self._brains.get(bid)
            if brain is None:
                return None
            try:
                return brain.get_avatar_state()
            except Exception as exc:
                _log.debug("brain.get_avatar_state() failed for brain %d: %s", bid, exc)
                return None

    # --- Identity / Self-Image ---

    def get_identity(self, bid: int) -> dict | None:
        """Get avatar identity (appearance parameters) for a brain."""
        with self._lock:
            if bid not in self._brains:
                return None
            return dict(self._identities.get(bid, DEFAULT_IDENTITY))

    def set_identity(self, bid: int, identity: dict) -> dict | None:
        """Set avatar identity parameters. Merges with defaults."""
        with self._lock:
            if bid not in self._brains:
                return None
            current = dict(self._identities.get(bid, DEFAULT_IDENTITY))
            # Only accept known keys
            for key in DEFAULT_IDENTITY:
                if key in identity:
                    current[key] = identity[key]
            self._identities[bid] = current
            self._save_brain_to_disk(bid)
            _log.info("Updated identity for brain %d", bid)
            return current

    def update_identity_from_text(self, bid: int, key: str, value) -> bool:
        """Update a single identity parameter (used by conversational learning)."""
        with self._lock:
            if bid not in self._brains or key not in DEFAULT_IDENTITY:
                return False
            identity = self._identities.setdefault(bid, dict(DEFAULT_IDENTITY))
            identity[key] = value
            self._save_brain_to_disk(bid)
            _log.info("Identity %s=%s for brain %d (learned from conversation)", key, value, bid)
            return True

    def destroy_all(self):
        with self._lock:
            _log.info("Destroying all %d brains", len(self._brains))
            self._brains.clear()
            self._metadata.clear()
            self._stats.clear()
            self._probe_history.clear()
            self._interpreters.clear()
            self._snapshots.clear()
            self._cow_snapshots.clear()
            self._identities.clear()


manager = BrainManager()
