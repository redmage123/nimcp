#!/usr/bin/env python3
"""Unit tests for synthesized sensory composer."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


def test_category_inference():
    from synthesized_sensory import _infer_category
    assert _infer_category("dog", "a furry animal") == "animal"
    assert _infer_category("oak tree", "a tall plant") == "plant"
    assert _infer_category("red car", "a fast vehicle") == "vehicle"
    assert _infer_category("teddy bear", "a soft toy") == "toy"
    assert _infer_category("apple", "a sweet food") == "food"
    assert _infer_category("zqrk", "unknown thing") == "default"
    print("  PASS: category inference correct for known categories")


def test_compose_rich_has_all_channels():
    from synthesized_sensory import SynthesizedSensoryComposer
    comp = SynthesizedSensoryComposer()
    rich = comp.compose_rich(name="dog", description="friendly furry animal")
    # All expected channels present
    required = {"name", "category", "text", "haptic", "audio_tag",
                "gaze", "emotion", "visual_features", "visual_variants"}
    missing = required - set(rich.keys())
    assert not missing, f"missing channels: {missing}"
    # Emotion has valence + arousal
    assert "valence" in rich["emotion"]
    assert "arousal" in rich["emotion"]
    # Category is correctly inferred as animal
    assert rich["category"] == "animal"
    # Haptic text mentions fur / warm
    assert "fur" in rich["haptic"] or "warm" in rich["haptic"]
    print(f"  PASS: all channels present with expected content")


def test_multi_view_produces_variants():
    from synthesized_sensory import SynthesizedSensoryComposer
    comp = SynthesizedSensoryComposer()
    variants = comp.multi_view("cat", "a purring house pet", n_variants=4)
    assert len(variants) == 4
    # Each variant has a 'view' tag and different text
    views = {v["view"] for v in variants}
    assert len(views) >= 3, f"variants share views: {views}"
    texts = {v["text"] for v in variants}
    assert len(texts) == len(variants), "texts not unique across variants"
    print(f"  PASS: multi_view produces {len(views)} distinct views")


def test_emotion_valence_varies_by_category():
    from synthesized_sensory import SynthesizedSensoryComposer
    comp = SynthesizedSensoryComposer()
    animal = comp.compose_rich(name="dog", description="furry pet animal")
    furniture = comp.compose_rich(name="chair", description="wooden furniture")
    toy = comp.compose_rich(name="ball", description="soft toy")
    # Animal + toy should have higher valence than furniture
    assert animal["emotion"]["valence"] > furniture["emotion"]["valence"]
    assert toy["emotion"]["valence"] > furniture["emotion"]["valence"]
    print(f"  PASS: valence correctly differentiated by category")


def test_stats_counter():
    from synthesized_sensory import SynthesizedSensoryComposer
    comp = SynthesizedSensoryComposer()
    for _ in range(5):
        comp.compose_rich(name="item", description="a thing")
    assert comp.stats()["n_composed"] == 5
    print(f"  PASS: stats counter tracks composition calls")


def main():
    failures = []
    for name, fn in [
        ("category_inference", test_category_inference),
        ("compose_rich_has_all_channels", test_compose_rich_has_all_channels),
        ("multi_view_produces_variants", test_multi_view_produces_variants),
        ("emotion_valence_varies_by_category", test_emotion_valence_varies_by_category),
        ("stats_counter", test_stats_counter),
    ]:
        print(f"[unit/synthesized_sensory] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll synthesized sensory unit tests passed.")


if __name__ == "__main__":
    main()
