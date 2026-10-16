"""Innate priors — evolution-equivalent structural initialization.

A newborn human isn't a blank slate. ~600M years of evolution is baked into
cortical structure: V1 orientation columns, face-detection primitives,
auditory frequency-band organization, hippocampal place-cell grid.

This module generates those priors as weight patterns that can be applied
at brain init time, skipping training equivalents of months of development.

Usage:
    priors = InnatePriors()
    v1_filters = priors.gabor_filters(n_orientations=8, n_scales=4)
    auditory = priors.frequency_band_filters(n_bands=32)
    place_cells = priors.place_cell_grid(n_cells=256, grid_size=16)
    # apply via brain.innate_hardwire(...) if exposed
"""
from __future__ import annotations

import logging
import math
from dataclasses import dataclass, field
from typing import Optional

log = logging.getLogger("innate_priors")


@dataclass
class PriorSet:
    name: str
    filters: list[list[float]] = field(default_factory=list)
    metadata: dict = field(default_factory=dict)

    def __len__(self):
        return len(self.filters)


class InnatePriors:
    """Generator for biologically-motivated structural priors."""

    # ---- Visual cortex V1 ----

    def gabor_filters(self, n_orientations: int = 8, n_scales: int = 4,
                       size: int = 9) -> PriorSet:
        """Generate Gabor filter bank matching V1 orientation columns.

        Parameters match typical primate V1:
        - n_orientations: 8 (every 22.5°)
        - n_scales: 4 (covering ~1-8 cycles per patch)
        """
        filters = []
        for s in range(n_scales):
            sigma = 1.5 * (1.5 ** s)
            freq = 0.5 / sigma
            for o in range(n_orientations):
                theta = math.pi * o / n_orientations
                kernel = self._make_gabor(size, theta, sigma, freq)
                filters.append(kernel)
        return PriorSet(
            name="v1_gabor_filters",
            filters=filters,
            metadata={
                "n_orientations": n_orientations,
                "n_scales": n_scales,
                "size": size,
                "description": "Oriented edge/bar detectors, biological V1",
            },
        )

    # ---- Auditory cortex ----

    def frequency_band_filters(self, n_bands: int = 32,
                                 sample_rate_hz: float = 16000.0) -> PriorSet:
        """Mel-scaled frequency band filters — approximates cochlear tonotopy."""
        # Mel scale: m = 2595 * log10(1 + f/700)
        def mel(f):
            return 2595.0 * math.log10(1.0 + f / 700.0)
        def inv_mel(m):
            return 700.0 * (10.0 ** (m / 2595.0) - 1.0)

        fmin_mel = mel(50.0)
        fmax_mel = mel(sample_rate_hz / 2)
        band_edges_mel = [
            fmin_mel + (fmax_mel - fmin_mel) * i / (n_bands + 1)
            for i in range(n_bands + 2)
        ]
        band_edges_hz = [inv_mel(m) for m in band_edges_mel]

        # Each band filter: triangular in frequency domain.
        # Represented as (low_hz, peak_hz, high_hz, center_freq).
        filters = []
        for i in range(n_bands):
            low = band_edges_hz[i]
            peak = band_edges_hz[i + 1]
            high = band_edges_hz[i + 2]
            filters.append([low, peak, high, peak])  # simplified representation

        return PriorSet(
            name="auditory_frequency_bands",
            filters=filters,
            metadata={
                "n_bands": n_bands,
                "sample_rate_hz": sample_rate_hz,
                "scale": "mel",
                "description": "Cochlear-inspired frequency band filters",
            },
        )

    # ---- Hippocampus place cells ----

    def place_cell_grid(self, n_cells: int = 256,
                         grid_size: int = 16) -> PriorSet:
        """Generate place-cell receptive fields on a 2D grid.

        Each place cell has a Gaussian response centered at some (x, y),
        with varying widths. Real hippocampal place cells show multi-scale
        tuning.
        """
        import random
        rng = random.Random(42)
        filters = []
        for i in range(n_cells):
            # Uniform coverage of grid
            cx = rng.uniform(0, grid_size)
            cy = rng.uniform(0, grid_size)
            # Width varies 0.5-2.5 cells
            sigma = rng.uniform(0.5, 2.5)
            # Build the 2D Gaussian response as a flattened grid
            response = []
            for y in range(grid_size):
                for x in range(grid_size):
                    dx = x - cx
                    dy = y - cy
                    response.append(math.exp(-0.5 * (dx * dx + dy * dy) / (sigma * sigma)))
            filters.append(response)
        return PriorSet(
            name="hippocampus_place_cells",
            filters=filters,
            metadata={
                "n_cells": n_cells,
                "grid_size": grid_size,
                "description": "Place-cell receptive fields, Gaussian",
            },
        )

    # ---- Fusiform face area ----

    def face_detector_template(self, size: int = 16) -> PriorSet:
        """Crude face template — eye pair + nose + mouth regions.

        Not a trained face detector, just a structural bias that makes the
        network sensitive to face-like topology.
        """
        # 2D template: higher activation in eye, nose, mouth regions
        template = [[0.0] * size for _ in range(size)]
        # Eyes at ~1/3 height, 1/3 and 2/3 width
        eye_y = size // 3
        for (ex, ey) in [(size // 3, eye_y), (2 * size // 3, eye_y)]:
            for y in range(size):
                for x in range(size):
                    d = (x - ex) ** 2 + (y - ey) ** 2
                    template[y][x] += math.exp(-0.5 * d / 1.5)
        # Nose at center
        nose_cx, nose_cy = size // 2, size // 2
        for y in range(size):
            for x in range(size):
                d = (x - nose_cx) ** 2 + (y - nose_cy) ** 2
                template[y][x] += 0.5 * math.exp(-0.5 * d / 2.0)
        # Mouth at ~2/3 height
        mouth_cx, mouth_cy = size // 2, 2 * size // 3
        for y in range(size):
            for x in range(size):
                d = (x - mouth_cx) ** 2 + (y - mouth_cy) ** 2
                template[y][x] += 0.7 * math.exp(-0.5 * d / 1.5)

        flat = [v for row in template for v in row]
        return PriorSet(
            name="fusiform_face_template",
            filters=[flat],
            metadata={
                "size": size,
                "description": "Coarse face-topology template (eyes+nose+mouth)",
            },
        )

    # ---- Somatosensory body schema ----

    def body_schema_map(self, n_regions: int = 32) -> PriorSet:
        """Simulated Penfield homunculus — body-region activation topology.

        Regions biased by cortical area ratio (hands/lips get more).
        """
        # Simplified weighting — hands 20%, face 15%, others distributed
        region_weights = {
            "face":      0.15,
            "lips":      0.10,
            "tongue":    0.05,
            "hand_l":    0.10,
            "hand_r":    0.10,
            "arm_l":     0.05,
            "arm_r":     0.05,
            "torso":     0.10,
            "leg_l":     0.07,
            "leg_r":     0.07,
            "foot_l":    0.03,
            "foot_r":    0.03,
            "back":      0.05,
            "other":     0.05,
        }
        filters = []
        region_names = list(region_weights.keys())
        for name, weight in region_weights.items():
            n_for_region = max(1, int(n_regions * weight))
            for _ in range(n_for_region):
                filters.append([weight, 0.0, 0.0])  # placeholder activation
        return PriorSet(
            name="somatosensory_body_schema",
            filters=filters,
            metadata={
                "n_regions": len(filters),
                "regions": region_names,
                "description": "Homunculus-weighted body region map",
            },
        )

    # ---- Helpers ----

    @staticmethod
    def _make_gabor(size: int, theta: float, sigma: float,
                     freq: float) -> list[float]:
        """Build one Gabor filter as a flat array."""
        out = []
        center = (size - 1) / 2.0
        cos_t = math.cos(theta)
        sin_t = math.sin(theta)
        for y in range(size):
            for x in range(size):
                dx = x - center
                dy = y - center
                # Rotate
                x_rot = dx * cos_t + dy * sin_t
                y_rot = -dx * sin_t + dy * cos_t
                # Gaussian envelope
                gaussian = math.exp(-0.5 * (x_rot ** 2 + y_rot ** 2) /
                                     (sigma ** 2))
                # Sinusoidal carrier
                sinusoid = math.cos(2 * math.pi * freq * x_rot)
                out.append(gaussian * sinusoid)
        return out


def apply_all_priors(brain) -> dict:
    """Generate all priors and attempt to apply via brain.innate_hardwire.

    Returns a summary dict of what was applied vs skipped.
    """
    priors = InnatePriors()
    all_sets = [
        priors.gabor_filters(),
        priors.frequency_band_filters(),
        priors.place_cell_grid(),
        priors.face_detector_template(),
        priors.body_schema_map(),
    ]
    summary = {"generated": [ps.name for ps in all_sets], "applied": []}
    for ps in all_sets:
        try:
            if hasattr(brain, "innate_hardwire"):
                brain.innate_hardwire(prior_name=ps.name,
                                        filters=ps.filters,
                                        metadata=ps.metadata)
                summary["applied"].append(ps.name)
        except Exception as e:
            log.debug("apply %s failed: %s", ps.name, e)
    return summary
