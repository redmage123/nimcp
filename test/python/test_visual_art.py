"""Unit tests for scripts/visual_art.py — CE-7 visual art procedural
composition generator + drip scoring.

Pure-Python tests with a fake brain stub. No daemon, no live brain. No
images persisted to disk.
"""
import os
import sys
import tempfile
import unittest

import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import visual_art as va  # noqa: E402


# ---------------------------------------------------------------------------
# Fake brain stub — same shape as test_storytelling.FakeBrain
# ---------------------------------------------------------------------------

class FakeBrain:
    """Minimal stand-in. Captures calls so tests can assert on the
    feedback path. No `visual_cortex_process` attribute by default —
    drip should fall back to train_language for the push step."""

    def __init__(self,
                 produced_text="A red circle on a white background.",
                 confidence=0.7):
        self._produced = produced_text
        self._confidence = confidence
        self.produce_calls = []
        self.learn_calls = []
        self.train_calls = []

    def produce_text(self, intent):
        self.produce_calls.append(list(intent) if intent is not None else None)
        return {"text": self._produced, "confidence": self._confidence,
                "success": True}

    def learn_language(self, text):
        self.learn_calls.append(text)

    def train_language(self, text, target_text):
        self.train_calls.append((text, target_text))


class FakeBrainWithVisual(FakeBrain):
    """FakeBrain that DOES expose visual_cortex_process."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.visual_calls = []

    def visual_cortex_process(self, pixels, width, height, channels=3):
        self.visual_calls.append({
            "n_pixels": len(pixels),
            "width": width,
            "height": height,
            "channels": channels,
        })
        return None


# ---------------------------------------------------------------------------
# generate_image_array — shape, dtype, range, determinism
# ---------------------------------------------------------------------------

class TestImageGeneration(unittest.TestCase):
    def test_shape_and_dtype_default(self):
        img = va.generate_image_array("single_red_circle")
        self.assertEqual(img.shape, (64, 64, 3))
        self.assertEqual(img.dtype, np.uint8)

    def test_value_range(self):
        for name in va.ART_COMPOSITIONS[1] + va.ART_COMPOSITIONS[2] + va.ART_COMPOSITIONS[3]:
            img = va.generate_image_array(name)
            self.assertTrue(img.min() >= 0)
            self.assertTrue(img.max() <= 255)

    def test_custom_size(self):
        img = va.generate_image_array("single_blue_square",
                                      height=32, width=48, channels=3)
        self.assertEqual(img.shape, (32, 48, 3))

    def test_determinism_same_input_same_pixels(self):
        a = va.generate_image_array("kandinsky_geometric")
        b = va.generate_image_array("kandinsky_geometric")
        self.assertTrue(np.array_equal(a, b),
                        "same composition_name must produce identical pixels")

    def test_determinism_pointillist(self):
        # Pointillist uses its own RNG — also deterministic.
        a = va.generate_image_array("pointillist_mosaic")
        b = va.generate_image_array("pointillist_mosaic")
        self.assertTrue(np.array_equal(a, b))

    def test_different_compositions_differ(self):
        a = va.generate_image_array("single_red_circle")
        b = va.generate_image_array("single_blue_square")
        self.assertFalse(np.array_equal(a, b))

    def test_unknown_name_raises(self):
        with self.assertRaises(ValueError):
            va.generate_image_array("definitely_not_a_real_composition")

    def test_red_circle_has_red_dominance(self):
        img = va.generate_image_array("single_red_circle")
        # The disk is centered and ~r=21 at 64x64. Sample within a small
        # central window; mean R should dominate B and G there.
        cy, cx = 32, 32
        patch = img[cy - 8:cy + 8, cx - 8:cx + 8, :]
        mean_r = float(patch[..., 0].mean())
        mean_g = float(patch[..., 1].mean())
        mean_b = float(patch[..., 2].mean())
        self.assertGreater(mean_r, mean_g + 50)
        self.assertGreater(mean_r, mean_b + 50)

    def test_checkerboard_diagonal_cells_match(self):
        img = va.generate_image_array("checkerboard_2x2")
        # Sample interior pixels of each quadrant (avoid edges).
        tl = img[8:24, 8:24, :].astype(np.int32).mean()
        br = img[40:56, 40:56, :].astype(np.int32).mean()
        tr = img[8:24, 40:56, :].astype(np.int32).mean()
        bl = img[40:56, 8:24, :].astype(np.int32).mean()
        # Diagonal cells (TL/BR) should be roughly equal in brightness;
        # likewise for the off-diagonal cells (TR/BL).
        self.assertAlmostEqual(tl, br, delta=5.0)
        self.assertAlmostEqual(tr, bl, delta=5.0)
        # And the two diagonals should DIFFER from each other.
        self.assertGreater(abs(tl - tr), 50.0)


# ---------------------------------------------------------------------------
# generate_composition_description
# ---------------------------------------------------------------------------

class TestDescriptions(unittest.TestCase):
    def test_returns_nonempty_string(self):
        desc = va.generate_composition_description("single_red_circle", 1)
        self.assertIsInstance(desc, str)
        self.assertTrue(len(desc) > 0)

    def test_circle_keyword_in_circle_description(self):
        desc = va.generate_composition_description("single_red_circle", 1).lower()
        self.assertIn("red", desc)
        self.assertIn("circle", desc)

    def test_house_description_mentions_roof_or_house(self):
        desc = va.generate_composition_description("house_shape", 2).lower()
        self.assertTrue("house" in desc or "roof" in desc)

    def test_unknown_composition_raises(self):
        with self.assertRaises(ValueError):
            va.generate_composition_description("not_real", 1)


# ---------------------------------------------------------------------------
# score_visual_response
# ---------------------------------------------------------------------------

class TestScoring(unittest.TestCase):
    def test_empty_response_is_zero(self):
        s = va.score_visual_response("", ["red", "circle"])
        self.assertEqual(s["composite"], 0.0)
        self.assertEqual(s["recall"], 0.0)
        self.assertEqual(s["specificity"], 0.0)

    def test_empty_keywords_is_zero(self):
        s = va.score_visual_response("a red circle", [])
        self.assertEqual(s["composite"], 0.0)

    def test_full_overlap_is_high(self):
        s = va.score_visual_response("red circle white background",
                                     ["red", "circle", "white", "background"])
        self.assertGreater(s["composite"], 0.9)
        self.assertEqual(s["recall"], 1.0)

    def test_partial_recall(self):
        s = va.score_visual_response(
            "I see a red circle in the image",
            ["red", "circle", "white", "background"])
        # 2 of 4 keywords present → recall 0.5
        self.assertAlmostEqual(s["recall"], 0.5, places=5)
        self.assertGreater(s["composite"], 0.0)
        self.assertLess(s["composite"], 1.0)

    def test_specificity_drops_with_noise(self):
        # Same recall but lots of off-topic tokens around it.
        clean = va.score_visual_response(
            "red circle", ["red", "circle"])
        noisy = va.score_visual_response(
            "red circle and a thousand other unrelated words drifting along here",
            ["red", "circle"])
        self.assertGreater(clean["specificity"], noisy["specificity"])
        self.assertGreater(clean["composite"], noisy["composite"])


# ---------------------------------------------------------------------------
# Drip — stage palettes + brain interaction
# ---------------------------------------------------------------------------

class TestDrip(unittest.TestCase):
    def setUp(self):
        # Reset the rotation counters for determinism.
        va._ROTATION_COUNTER[1] = 0
        va._ROTATION_COUNTER[2] = 0
        va._ROTATION_COUNTER[3] = 0

    def test_stage_1_has_4_compositions(self):
        self.assertEqual(len(va.ART_COMPOSITIONS[1]), 4)

    def test_stage_2_and_3_have_5_compositions(self):
        self.assertEqual(len(va.ART_COMPOSITIONS[2]), 5)
        self.assertEqual(len(va.ART_COMPOSITIONS[3]), 5)

    def test_stage_1_runs_and_produces_text(self):
        b = FakeBrain()
        results = va.run_visual_drip(b, stage=1, composer=None,
                                     num_compositions=4,
                                     composition_index=0)
        self.assertIsNotNone(results)
        self.assertEqual(len(results), 4)
        self.assertEqual(len(b.produce_calls), 4)

    def test_stage_2_default_num_compositions(self):
        b = FakeBrain()
        results = va.run_visual_drip(b, stage=2, composer=None,
                                     composition_index=0)
        # Default num_compositions=2.
        self.assertEqual(len(results), 2)
        self.assertEqual(len(b.produce_calls), 2)

    def test_stage_3_explicit_num_compositions(self):
        b = FakeBrain()
        results = va.run_visual_drip(b, stage=3, composer=None,
                                     num_compositions=3,
                                     composition_index=0)
        self.assertEqual(len(results), 3)

    def test_unknown_stage_returns_none(self):
        b = FakeBrain()
        out = va.run_visual_drip(b, stage=99, composer=None,
                                 num_compositions=1)
        self.assertIsNone(out)

    def test_visual_path_used_when_available(self):
        b = FakeBrainWithVisual()
        results = va.run_visual_drip(b, stage=1, composer=None,
                                     num_compositions=2,
                                     composition_index=0)
        self.assertEqual(len(b.visual_calls), 2,
                         "visual_cortex_process must be called when present")
        # And produce_text is still called after the visual push.
        self.assertEqual(len(b.produce_calls), 2)
        # Pixel count = 64 * 64 * 3 = 12288 by default.
        self.assertEqual(b.visual_calls[0]["n_pixels"], 64 * 64 * 3)
        self.assertEqual(b.visual_calls[0]["width"], 64)
        self.assertEqual(b.visual_calls[0]["height"], 64)
        self.assertEqual(b.visual_calls[0]["channels"], 3)
        # Each result records that the visual path was used.
        for r in results:
            self.assertEqual(r["path"], "visual")

    def test_fallback_to_train_language_when_no_visual_rpc(self):
        b = FakeBrain()  # no visual_cortex_process
        results = va.run_visual_drip(b, stage=1, composer=None,
                                     num_compositions=2,
                                     composition_index=0)
        # Each composition triggers either a fallback push (train_language
        # with description) or the corrective branch (also train_language).
        # In both cases train_language fires; high-score path also fires
        # learn_language. So we just check the path label.
        for r in results:
            self.assertEqual(r["path"], "language")
        # train_language has been called at least once per composition
        # (fallback push). May be called twice per composition if score is
        # below threshold (push + corrective).
        self.assertGreaterEqual(len(b.train_calls), 2)

    def test_high_score_takes_positive_path(self):
        # Brain reproduces all keywords for "single_red_circle"
        # — composite well above threshold → learn_language called.
        b = FakeBrainWithVisual(
            produced_text="A red circle on a white background.",
            confidence=0.9)
        results = va.run_visual_drip(b, stage=1, composer=None,
                                     num_compositions=1,
                                     composition_index=0)
        self.assertEqual(len(results), 1)
        self.assertGreaterEqual(results[0]["composite"],
                                va.VISUAL_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 1)
        # No corrective train_language since visual path was used for push.
        self.assertEqual(len(b.train_calls), 0)

    def test_low_score_takes_corrective_path(self):
        # Off-topic gibberish → composite below threshold → train_language
        # re-anchor on description.
        b = FakeBrainWithVisual(
            produced_text="zzz qqq xxx plimflam wibble",
            confidence=0.1)
        results = va.run_visual_drip(b, stage=1, composer=None,
                                     num_compositions=1,
                                     composition_index=0)
        self.assertLess(results[0]["composite"], va.VISUAL_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 0)
        self.assertEqual(len(b.train_calls), 1)
        # Re-anchor pair: (description, description)
        self.assertEqual(b.train_calls[0][0], b.train_calls[0][1])

    def test_produce_failure_is_caught(self):
        class BoomBrain(FakeBrain):
            def produce_text(self, intent):
                raise RuntimeError("simulated produce failure")

        b = BoomBrain()
        # Should swallow the failure and continue (returning the results
        # we got — which is an empty list because the only composition
        # failed).
        results = va.run_visual_drip(b, stage=1, composer=None,
                                     num_compositions=1,
                                     composition_index=0)
        self.assertEqual(results, [])

    def test_rotation_advances_when_no_explicit_index(self):
        b = FakeBrain()
        va._ROTATION_COUNTER[2] = 0
        va.run_visual_drip(b, stage=2, composer=None, num_compositions=1)
        va.run_visual_drip(b, stage=2, composer=None, num_compositions=1)
        self.assertEqual(va._ROTATION_COUNTER[2], 2)

    def test_explicit_index_does_not_bump_counter(self):
        b = FakeBrain()
        va._ROTATION_COUNTER[2] = 7
        va.run_visual_drip(b, stage=2, composer=None,
                           num_compositions=1, composition_index=99)
        self.assertEqual(va._ROTATION_COUNTER[2], 7)


# ---------------------------------------------------------------------------
# No disk persistence side-effect
# ---------------------------------------------------------------------------

class TestNoDiskPersistence(unittest.TestCase):
    def test_drip_creates_no_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            cwd = os.getcwd()
            try:
                os.chdir(tmp)
                # Run a multi-composition drip in an empty tempdir.
                b = FakeBrainWithVisual()
                va.run_visual_drip(b, stage=2, composer=None,
                                   num_compositions=3,
                                   composition_index=0)
                # Also exercise the language fallback path.
                b2 = FakeBrain()
                va.run_visual_drip(b2, stage=3, composer=None,
                                   num_compositions=2,
                                   composition_index=0)
                # No new files should have been created.
                contents = os.listdir(tmp)
                self.assertEqual(contents, [],
                                 f"drip created files: {contents!r}")
            finally:
                os.chdir(cwd)

    def test_image_generation_creates_no_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            cwd = os.getcwd()
            try:
                os.chdir(tmp)
                for name in va.ART_COMPOSITIONS[1]:
                    _ = va.generate_image_array(name)
                self.assertEqual(os.listdir(tmp), [])
            finally:
                os.chdir(cwd)


if __name__ == "__main__":
    unittest.main()
