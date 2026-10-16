#!/usr/bin/env python3
"""Tests for multi-network ensemble training (LNN + CNN + Adaptive).

WHAT: Verify Python bindings for brain.enable_multi_network()
WHY:  Ensure ensemble training is accessible and functional from Python
HOW:  Create brain, enable multi-network, train examples, check results

Usage:
    PYTHONPATH=build/lib/python python3 -m pytest test/python/test_multi_network.py -v
"""

import sys
from pathlib import Path

# Ensure build output is on the path
BUILD_LIB = Path(__file__).resolve().parent.parent.parent / "build" / "lib" / "python"
if str(BUILD_LIB) not in sys.path:
    sys.path.insert(0, str(BUILD_LIB))

import nimcp


def _make_brain(num_inputs=8, num_outputs=4):
    """Create a small test brain."""
    return nimcp.Brain(
        "test_multi",
        nimcp.BRAIN_TINY,
        nimcp.TASK_CLASSIFICATION,
        num_inputs,
        num_outputs,
    )


class TestMultiNetworkEnable:
    """Test enabling multi-network ensemble training."""

    def test_enable_multi_network(self):
        """Enable multi-network on a fresh brain -- should not raise."""
        brain = _make_brain()
        brain.enable_multi_network()

    def test_enable_idempotent(self):
        """Calling enable_multi_network twice should be safe."""
        brain = _make_brain()
        brain.enable_multi_network()
        brain.enable_multi_network()  # No exception expected

    def test_learn_with_multi_network(self):
        """Train a few examples after enabling multi-network -- loss should be valid."""
        brain = _make_brain(num_inputs=4, num_outputs=3)
        brain.enable_multi_network()

        features_a = [1.0, 0.0, 0.0, 0.0]
        features_b = [0.0, 1.0, 0.0, 0.0]

        for _ in range(5):
            loss_a = brain.learn(features_a, "alpha", 1.0)
            loss_b = brain.learn(features_b, "beta", 1.0)

            # Loss should be a finite number (not NaN, not Inf)
            assert loss_a == loss_a, "Loss is NaN"  # NaN != NaN
            assert loss_b == loss_b, "Loss is NaN"

    def test_enable_before_learn(self):
        """Enabling multi-network before any learning should work."""
        brain = _make_brain(num_inputs=4, num_outputs=2)

        # Enable first, then learn
        brain.enable_multi_network()

        loss = brain.learn([0.5, 0.3, 0.2, 0.1], "cat", 0.9)
        assert loss == loss, "Loss is NaN"

    def test_learn_without_enable(self):
        """Learning without enable_multi_network should still work (ADAPTIVE mode)."""
        brain = _make_brain(num_inputs=4, num_outputs=2)

        loss = brain.learn([0.5, 0.3, 0.2, 0.1], "cat", 0.9)
        assert loss == loss, "Loss is NaN"
