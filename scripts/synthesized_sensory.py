"""Synthesized multi-modal sensory enrichment.

Real sensors aren't available, but each training exposure can still be
amplified from "image + text" to a multi-channel tuple including synthesized
haptic, gaze, emotional, and audio context.

This closes a significant fraction of the per-exposure information gap vs
biological learning, without requiring additional hardware.

Usage:
    composer = SynthesizedSensoryComposer(base_composer)
    rich = composer.compose_rich(name="dog",
                                 description="a friendly furry animal")
    # rich is a dict with channels:
    #   rich["visual"]       — visual features (augmented)
    #   rich["text"]         — the parent narration
    #   rich["haptic"]       — synthesized tactile text
    #   rich["gaze"]         — joint attention cue
    #   rich["audio_tag"]    — synthesized audio context
    #   rich["emotion"]      — (valence, arousal) tuple
"""
from __future__ import annotations

import hashlib
import logging
import random
from typing import Any, Optional

log = logging.getLogger("synthesized_sensory")


# Simple lookup-based synthesis — for bootstrap. Later can be replaced
# by Claude-generated or LLM-generated richer descriptions.
HAPTIC_BY_CATEGORY = {
    "animal":   "soft fur warm breathing heartbeat",
    "plant":    "rough bark leaves rustle waxy smooth",
    "vehicle":  "hard metal cold smooth painted",
    "toy":      "plush squishy rattling plastic",
    "food":     "textured edible cool warm moist",
    "furniture":"solid wooden polished firm",
    "container":"hollow smooth rim weight",
    "sky":      "untouchable distant luminous",
    "default":  "tangible present here",
}

AUDIO_TAG_BY_CATEGORY = {
    "animal":   "breathing heartbeat paws soft sounds",
    "plant":    "rustling leaves silence wind",
    "vehicle":  "engine hum wheels road",
    "toy":      "squeak rattle laughter",
    "food":     "crunch soft chewing quiet",
    "furniture":"creak silence stability",
    "container":"clink tap hollow",
    "sky":      "silence distant wind",
    "default":  "ambient quiet",
}

EMOTION_BY_CATEGORY = {
    # (valence -1 to 1, arousal 0 to 1)
    "animal":   (+0.5, 0.4),   # warm, curious
    "plant":    (+0.3, 0.2),   # calm, neutral
    "vehicle":  (+0.2, 0.6),   # interesting, activating
    "toy":      (+0.7, 0.5),   # playful, joyful
    "food":     (+0.6, 0.3),   # pleasant
    "furniture":(+0.1, 0.1),   # neutral
    "container":(+0.0, 0.1),   # neutral
    "sky":      (+0.4, 0.3),   # wonder
    "default":  (+0.0, 0.2),
}


def _infer_category(name: str, description: str) -> str:
    # Word-boundary match to avoid "fur" matching "furniture" etc.
    import re
    text = (name + " " + description).lower()
    tokens = set(re.findall(r"[a-z]+", text))

    keyword_to_cat = {
        "furniture": ["chair", "table", "bed", "desk", "sofa", "furniture"],
        "container": ["cup", "bowl", "box", "jar", "bottle", "container"],
        "sky":       ["sun", "moon", "star", "cloud", "sky"],
        "vehicle":   ["car", "bike", "truck", "boat", "train", "plane",
                      "vehicle"],
        "toy":       ["ball", "bear", "doll", "block", "toy", "puzzle"],
        "food":      ["apple", "bread", "cheese", "fruit", "food", "drink",
                      "cookie"],
        "plant":     ["tree", "flower", "grass", "leaf", "plant", "bush",
                      "vine", "wooden"],
        "animal":    ["dog", "cat", "bird", "fish", "horse", "pet", "animal",
                      "fur", "furry"],
    }
    # Check in declared order; first match wins.
    for cat, keywords in keyword_to_cat.items():
        if any(k in tokens for k in keywords):
            return cat
    return "default"


class SynthesizedSensoryComposer:
    """Wraps a base composer (or operates standalone) producing multi-channel
    stimulus tuples.

    If a base composer is provided (one with `.compose(text, modality)`),
    its output is used for the vector channel. Otherwise the synthesis is
    text-only and downstream callers would need to encode.
    """

    def __init__(self, base_composer: Any = None, seed: int = 1):
        self.base = base_composer
        self._rng = random.Random(seed)
        self._n_composed = 0

    def compose_rich(self, *, name: str, description: str,
                     visual_variants: int = 1) -> dict:
        """Return a dict of synchronized channels for one exposure."""
        cat = _infer_category(name, description)
        haptic_text = HAPTIC_BY_CATEGORY.get(cat, HAPTIC_BY_CATEGORY["default"])
        audio_text = AUDIO_TAG_BY_CATEGORY.get(cat, AUDIO_TAG_BY_CATEGORY["default"])
        valence, arousal = EMOTION_BY_CATEGORY.get(cat,
                                                    EMOTION_BY_CATEGORY["default"])

        # Gaze cue — joint attention: "caregiver looking at X while saying X"
        gaze_cue = f"gaze_on:{name}"

        # Visual augmentation — if we have a real composer use it;
        # otherwise synthesize a marker.
        visual_features = None
        if self.base is not None and hasattr(self.base, "compose"):
            try:
                visual_features = self.base.compose(
                    text=f"{name} {description}",
                    modality="visual" if hasattr(self.base, "compose_visual")
                             else "text")
            except Exception as e:
                log.debug("base composer failed: %s", e)

        self._n_composed += 1
        return {
            "name": name,
            "category": cat,
            "text": f"That's a {name}! {description}",
            "haptic": haptic_text,
            "audio_tag": audio_text,
            "gaze": gaze_cue,
            "emotion": {"valence": valence, "arousal": arousal},
            "visual_features": visual_features,
            "visual_variants": visual_variants,
        }

    def multi_view(self, name: str, description: str,
                   n_variants: int = 4) -> list[dict]:
        """Produce N slightly-perturbed variants of the same exposure —
        analogous to seeing the same object from different angles."""
        base = self.compose_rich(name=name, description=description,
                                  visual_variants=n_variants)
        variants = []
        for i in range(n_variants):
            variant = dict(base)
            # Perturb text slightly to simulate view-dependent naming
            angle_words = ["front", "side", "above", "near", "far", "close"]
            variant["text"] = (f"That's a {name} from the "
                              f"{angle_words[i % len(angle_words)]}! "
                              f"{description}")
            variant["view"] = angle_words[i % len(angle_words)]
            variants.append(variant)
        return variants

    def stats(self) -> dict:
        return {"n_composed": self._n_composed}
