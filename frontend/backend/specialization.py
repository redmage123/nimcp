"""Progressive brain specialization — auto-detect task, dimensions, and strategy.

Brains start as general-purpose (PENDING) and specialize through experience:
  PENDING -> INFANT -> DEVELOPING -> MATURE -> EXPERT
"""
from enum import IntEnum
from typing import Optional
import time

import nimcp_logger

_log = nimcp_logger.get("specialization")


# ---------------------------------------------------------------------------
# Specialization levels
# ---------------------------------------------------------------------------

class SpecializationLevel(IntEnum):
    PENDING = 0       # No C brain yet, waiting for first data
    INFANT = 1        # 0-50 learning steps, basic learning
    DEVELOPING = 2    # 50-500 steps
    MATURE = 3        # 500-2000 steps
    EXPERT = 4        # 2000+ steps AND accuracy > 70%


LEVEL_LABELS = {
    SpecializationLevel.PENDING: "Pending",
    SpecializationLevel.INFANT: "Infant",
    SpecializationLevel.DEVELOPING: "Developing",
    SpecializationLevel.MATURE: "Mature",
    SpecializationLevel.EXPERT: "Expert",
}

LEVEL_THRESHOLDS = [
    (SpecializationLevel.EXPERT, 2000, 0.70),
    (SpecializationLevel.MATURE, 500, 0.0),
    (SpecializationLevel.DEVELOPING, 50, 0.0),
    (SpecializationLevel.INFANT, 0, 0.0),
]


# ---------------------------------------------------------------------------
# Specialization state (per-brain)
# ---------------------------------------------------------------------------

class SpecializationState:
    __slots__ = (
        "level", "detected_task", "detected_num_inputs", "detected_num_outputs",
        "detected_domain", "recommended_strategy", "unique_labels_seen",
        "label_distribution", "total_data_points_seen", "reconfiguration_count",
        "milestones", "is_materialized",
    )

    def __init__(self):
        self.level: SpecializationLevel = SpecializationLevel.PENDING
        self.detected_task: Optional[int] = None
        self.detected_num_inputs: Optional[int] = None
        self.detected_num_outputs: Optional[int] = None
        self.detected_domain: Optional[str] = None
        self.recommended_strategy: Optional[str] = None
        self.unique_labels_seen: set[str] = set()
        self.label_distribution: dict[str, int] = {}
        self.total_data_points_seen: int = 0
        self.reconfiguration_count: int = 0
        self.milestones: list[dict] = []
        self.is_materialized: bool = False

    def to_dict(self) -> dict:
        return {
            "level": int(self.level),
            "level_label": LEVEL_LABELS.get(self.level, "Unknown"),
            "detected_task": self.detected_task,
            "detected_num_inputs": self.detected_num_inputs,
            "detected_num_outputs": self.detected_num_outputs,
            "detected_domain": self.detected_domain,
            "recommended_strategy": self.recommended_strategy,
            "unique_labels_seen": sorted(self.unique_labels_seen),
            "label_distribution": dict(self.label_distribution),
            "total_data_points_seen": self.total_data_points_seen,
            "reconfiguration_count": self.reconfiguration_count,
            "milestones": list(self.milestones),
            "is_materialized": self.is_materialized,
        }

    @classmethod
    def from_dict(cls, d: dict) -> "SpecializationState":
        s = cls()
        s.level = SpecializationLevel(d.get("level", 0))
        s.detected_task = d.get("detected_task")
        s.detected_num_inputs = d.get("detected_num_inputs")
        s.detected_num_outputs = d.get("detected_num_outputs")
        s.detected_domain = d.get("detected_domain")
        s.recommended_strategy = d.get("recommended_strategy")
        s.unique_labels_seen = set(d.get("unique_labels_seen", []))
        s.label_distribution = dict(d.get("label_distribution", {}))
        s.total_data_points_seen = d.get("total_data_points_seen", 0)
        s.reconfiguration_count = d.get("reconfiguration_count", 0)
        s.milestones = list(d.get("milestones", []))
        s.is_materialized = d.get("is_materialized", False)
        return s

    def check_level_transition(self, total_steps: int, accuracy: float) -> Optional[SpecializationLevel]:
        """Return new level if a threshold was crossed, else None."""
        if not self.is_materialized:
            return None
        for level, step_threshold, acc_threshold in LEVEL_THRESHOLDS:
            if total_steps >= step_threshold and accuracy >= acc_threshold and level > self.level:
                return level
        return None

    def record_milestone(self, event: str, details: Optional[dict] = None):
        entry = {
            "event": event,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "level": int(self.level),
            "level_label": LEVEL_LABELS.get(self.level, "Unknown"),
        }
        if details:
            entry.update(details)
        self.milestones.append(entry)
        _log.info("Milestone: %s (level=%s)", event, LEVEL_LABELS.get(self.level))


# ---------------------------------------------------------------------------
# Data analyzer (stateless utility)
# ---------------------------------------------------------------------------

class DataAnalyzer:
    """Analyzes training data to auto-detect task type, dimensions, and strategy."""

    @staticmethod
    def analyze_dataset(examples: list[dict]) -> dict:
        """Analyze a batch of examples and return detected configuration.

        Returns dict with: num_inputs, num_outputs, task_type, strategy, domain
        """
        if not examples:
            return {
                "num_inputs": 4, "num_outputs": 3,
                "task_type": 0, "strategy": "hebbian", "domain": "unknown",
            }

        # Detect input dimensions from first valid example
        num_inputs = 4
        for ex in examples:
            features = ex.get("features", ex.get("input", []))
            if features:
                num_inputs = len(features)
                break

        # Collect all labels
        labels = []
        for ex in examples:
            label = ex.get("label", ex.get("class"))
            if label is not None:
                labels.append(str(label))

        unique_labels = sorted(set(labels))
        task_type = DataAnalyzer.detect_task_type(labels, unique_labels)
        num_outputs = DataAnalyzer.compute_output_buffer(unique_labels)
        strategy = DataAnalyzer.recommend_strategy(
            len(examples), num_inputs, num_outputs, task_type
        )
        domain = DataAnalyzer.detect_domain(num_inputs, unique_labels)

        return {
            "num_inputs": num_inputs,
            "num_outputs": num_outputs,
            "task_type": task_type,
            "strategy": strategy,
            "domain": domain,
            "unique_labels": unique_labels,
        }

    @staticmethod
    def detect_task_type(labels: list[str], unique_labels: list[str]) -> int:
        """Detect task type from label characteristics.

        Returns: 0=classification, 1=regression, 3=sequence
        """
        if not labels:
            return 0  # classification default

        # Check if labels are all numeric (could be regression)
        all_numeric = True
        numeric_vals = []
        for lbl in unique_labels:
            try:
                numeric_vals.append(float(lbl))
            except (ValueError, TypeError):
                all_numeric = False
                break

        if all_numeric and len(unique_labels) > 20:
            # High cardinality numeric -> regression
            return 1

        # Check if labels look like sequential indices (0, 1, 2, ...)
        if all_numeric:
            int_vals = sorted(int(float(v)) for v in unique_labels)
            if int_vals == list(range(len(int_vals))):
                # Sequential indices with few classes -> classification
                if len(int_vals) <= 20:
                    return 0
                # Many sequential indices -> sequence prediction
                return 3

        # String labels -> classification
        return 0

    @staticmethod
    def recommend_strategy(num_examples: int, num_inputs: int,
                           num_outputs: int, task_type: int) -> str:
        """Recommend training strategy based on dataset characteristics."""
        if num_examples < 100:
            return "hebbian"
        elif num_examples < 1000:
            return "hybrid"
        else:
            return "gradient"

    @staticmethod
    def compute_output_buffer(unique_labels: list[str]) -> int:
        """Compute output buffer size with headroom to avoid frequent reconfiguration."""
        n = len(unique_labels) if unique_labels else 3
        return max(n * 2, n + 8, 10)

    @staticmethod
    def detect_domain(num_inputs: int, unique_labels: list[str]) -> str:
        """Heuristic domain detection from input dimensions and labels."""
        # Well-known input dimensions
        if num_inputs == 784:
            return "digits"
        if num_inputs == 3072:
            return "images"
        if num_inputs >= 100:
            return "high-dimensional"
        if num_inputs <= 10:
            return "tabular"
        return "general"


# ---------------------------------------------------------------------------
# Progressive activator (bio-blend by level)
# ---------------------------------------------------------------------------

class ProgressiveActivator:
    """Maps specialization level to biological modulation blend."""

    BIO_BLEND_MAP = {
        SpecializationLevel.PENDING: 0.0,
        SpecializationLevel.INFANT: 0.0,
        SpecializationLevel.DEVELOPING: 0.15,
        SpecializationLevel.MATURE: 0.3,
        SpecializationLevel.EXPERT: 0.5,
    }

    @classmethod
    def get_bio_blend_for_level(cls, level: SpecializationLevel) -> float:
        return cls.BIO_BLEND_MAP.get(level, 0.0)
