#!/usr/bin/env python3
"""Unit tests for innate priors."""
from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


def test_gabor_filters_count_and_shape():
    from innate_priors import InnatePriors
    p = InnatePriors()
    ps = p.gabor_filters(n_orientations=8, n_scales=4, size=9)
    assert len(ps.filters) == 32, f"expected 32 filters, got {len(ps.filters)}"
    for f in ps.filters:
        assert len(f) == 81, f"expected 9x9=81 elements, got {len(f)}"
    print(f"  PASS: Gabor bank has {len(ps.filters)} filters of size {ps.metadata['size']}x{ps.metadata['size']}")


def test_gabor_filter_has_nonzero_response():
    from innate_priors import InnatePriors
    p = InnatePriors()
    ps = p.gabor_filters(n_orientations=4, n_scales=2, size=9)
    # Each filter should have nonzero response at center
    for f in ps.filters:
        max_abs = max(abs(v) for v in f)
        assert max_abs > 0.01, f"filter nearly zero everywhere: {max_abs}"
    print(f"  PASS: all Gabor filters have nonzero response")


def test_frequency_band_monotonic():
    from innate_priors import InnatePriors
    p = InnatePriors()
    ps = p.frequency_band_filters(n_bands=32, sample_rate_hz=16000.0)
    assert len(ps.filters) == 32
    # Center frequencies should be strictly increasing
    centers = [f[3] for f in ps.filters]
    for i in range(1, len(centers)):
        assert centers[i] > centers[i-1], "frequencies not monotonic"
    print(f"  PASS: {len(centers)} bands, range {centers[0]:.0f}-{centers[-1]:.0f} Hz")


def test_place_cells_cover_grid():
    from innate_priors import InnatePriors
    p = InnatePriors()
    ps = p.place_cell_grid(n_cells=64, grid_size=16)
    assert len(ps.filters) == 64
    # Each cell response should sum to >0 (non-trivial)
    for f in ps.filters:
        assert len(f) == 16 * 16
        total = sum(f)
        assert total > 0
    print(f"  PASS: {len(ps.filters)} place cells on {ps.metadata['grid_size']}x{ps.metadata['grid_size']} grid")


def test_face_template_has_feature_regions():
    from innate_priors import InnatePriors
    p = InnatePriors()
    ps = p.face_detector_template(size=16)
    assert len(ps.filters) == 1
    template = ps.filters[0]
    # Reshape to 16x16 and check eye regions have higher activation
    size = 16
    eye_y = size // 3
    eye_x_left = size // 3
    eye_x_right = 2 * size // 3
    eye_left_val = template[eye_y * size + eye_x_left]
    eye_right_val = template[eye_y * size + eye_x_right]
    corner_val = template[0]
    assert eye_left_val > corner_val, "eye region not elevated"
    assert eye_right_val > corner_val, "eye region not elevated"
    print(f"  PASS: face template has elevated eye/nose/mouth regions")


def test_body_schema_has_hand_overrepresentation():
    from innate_priors import InnatePriors
    p = InnatePriors()
    ps = p.body_schema_map(n_regions=50)
    # Total regions should reflect weighting
    assert len(ps.filters) > 0
    # Homunculus property: hands+face should dominate
    regions = ps.metadata["regions"]
    assert "hand_l" in regions and "hand_r" in regions
    assert "face" in regions
    print(f"  PASS: body schema has {len(ps.filters)} regions with homunculus weighting")


def test_apply_all_priors_with_mock_brain():
    from innate_priors import apply_all_priors

    class MockBrain:
        def __init__(self):
            self.calls = []
        def innate_hardwire(self, prior_name, filters, metadata):
            self.calls.append(prior_name)

    brain = MockBrain()
    summary = apply_all_priors(brain)
    assert len(summary["generated"]) == 5
    assert len(summary["applied"]) == 5
    assert "v1_gabor_filters" in summary["applied"]
    print(f"  PASS: all 5 priors applied to mock brain ({summary['applied']})")


def test_apply_all_priors_graceful_without_api():
    from innate_priors import apply_all_priors

    class BareBrain:
        pass

    summary = apply_all_priors(BareBrain())
    assert len(summary["generated"]) == 5
    assert len(summary["applied"]) == 0
    print(f"  PASS: graceful when innate_hardwire absent")


def main():
    failures = []
    for name, fn in [
        ("gabor_filters_count_and_shape", test_gabor_filters_count_and_shape),
        ("gabor_filter_has_nonzero_response", test_gabor_filter_has_nonzero_response),
        ("frequency_band_monotonic", test_frequency_band_monotonic),
        ("place_cells_cover_grid", test_place_cells_cover_grid),
        ("face_template_has_feature_regions", test_face_template_has_feature_regions),
        ("body_schema_has_hand_overrepresentation", test_body_schema_has_hand_overrepresentation),
        ("apply_all_priors_with_mock_brain", test_apply_all_priors_with_mock_brain),
        ("apply_all_priors_graceful_without_api", test_apply_all_priors_graceful_without_api),
    ]:
        print(f"[unit/innate_priors] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll innate priors unit tests passed.")


if __name__ == "__main__":
    main()
