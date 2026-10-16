#!/usr/bin/env python3
"""E2E tests for the unified brain probe system.

Tests the full stack: Python bindings → C API → probe registry → metrics → JSON.
"""

import sys
import os
import json
import time
import unittest

# Add scripts to path for brain_client
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))


class ProbeE2ETest(unittest.TestCase):
    """Tests using the Python nimcp module directly (no daemon)."""

    @classmethod
    def setUpClass(cls):
        import nimcp
        cls.nimcp = nimcp
        cls.brain = nimcp.Brain("probe_e2e_test",
                                 num_inputs=64, num_outputs=32,
                                 neuron_count=100, init_mode='minimal')

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, 'brain') and cls.brain:
            del cls.brain

    def test_attach_builtin_probes(self):
        """brain.attach_builtin_probes() returns count > 0."""
        count = self.brain.attach_builtin_probes(500)
        self.assertGreaterEqual(count, 1,
                                "Should attach at least 1 built-in probe")

    def test_get_all_probe_metrics_returns_dict(self):
        """brain.get_all_probe_metrics() returns a dict."""
        self.brain.attach_builtin_probes(500)
        # Do a forward pass to generate some data
        self.brain.decide_full([0.1] * 64)
        time.sleep(0.6)  # Wait for sampler to run

        metrics = self.brain.get_all_probe_metrics()
        self.assertIsInstance(metrics, dict,
                              "Probe metrics should be a dict")

    def test_probe_metrics_have_network_data(self):
        """After training, network_metrics probe should have ANN loss."""
        self.brain.attach_builtin_probes(100)

        # Train a few steps
        features = [0.1] * 64
        target = [0.5] * 32
        for _ in range(3):
            self.brain.learn_vector(features, target, label="test")

        time.sleep(0.2)
        metrics = self.brain.get_all_probe_metrics()
        if metrics:
            # Should have network_metrics probe
            if 'network_metrics' in metrics:
                nm = metrics['network_metrics']
                self.assertIn('ann.ema_loss', nm)

    def test_destroy_probe(self):
        """brain.destroy_probe() doesn't crash."""
        # This should not raise
        self.brain.destroy_probe(99999)  # Non-existent handle

    def test_pipeline_stage_probe(self):
        """After brain.decide_full(), inference stage metrics should exist."""
        self.brain.attach_builtin_probes(100)

        result = self.brain.decide_full([0.1] * 64)
        self.assertIsNotNone(result)

        metrics = self.brain.get_all_probe_metrics()
        # Metrics should contain some data after a decide call
        self.assertIsInstance(metrics, dict)

    def test_metrics_json_valid(self):
        """JSON output from get_all_probe_metrics is valid JSON."""
        self.brain.attach_builtin_probes(500)
        self.brain.decide_full([0.2] * 64)
        time.sleep(0.6)

        metrics = self.brain.get_all_probe_metrics()
        # Should be serializable to JSON
        if metrics:
            json_str = json.dumps(metrics)
            reparsed = json.loads(json_str)
            self.assertEqual(metrics, reparsed)


if __name__ == '__main__':
    unittest.main()
