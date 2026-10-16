#!/usr/bin/env python3
"""E2E tests for unified checkpoint format through Python API."""

import os
import sys
import struct
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

try:
    import nimcp
except ImportError:
    nimcp = None


@unittest.skipIf(nimcp is None, "nimcp Python bindings not available")
class TestUnifiedCheckpointE2E(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        nimcp.init()
        cls.brain = nimcp.Brain("test_ckpt",
                                num_inputs=64, num_outputs=64,
                                neuron_count=500, init_mode="minimal")

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, 'brain') and cls.brain:
            del cls.brain
        nimcp.shutdown()

    def test_save_creates_single_file(self):
        """Save should create exactly one file (no sidecars)."""
        with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as f:
            path = f.name
        try:
            self.brain.save(path)
            self.assertTrue(os.path.exists(path))
            # No sidecar files
            self.assertFalse(os.path.exists(path + '.snn'))
            self.assertFalse(os.path.exists(path + '.lnn'))
            self.assertFalse(os.path.exists(path + '.meta'))
        finally:
            os.unlink(path)

    def test_saved_file_has_nimv_magic(self):
        """Saved file should start with NIMV magic bytes."""
        with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as f:
            path = f.name
        try:
            self.brain.save(path)
            with open(path, 'rb') as f:
                magic = struct.unpack('<I', f.read(4))[0]
            self.assertEqual(magic, 0x4E494D56, "Expected NIMV magic")
        finally:
            os.unlink(path)

    def test_save_load_roundtrip(self):
        """Save then load should produce a valid brain."""
        with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as f:
            path = f.name
        try:
            self.brain.save(path)
            loaded = nimcp.Brain.load(path)
            self.assertIsNotNone(loaded)
            del loaded
        finally:
            os.unlink(path)

    def test_checkpoint_size_reasonable(self):
        """Checkpoint should be at least 1KB (not empty)."""
        with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as f:
            path = f.name
        try:
            self.brain.save(path)
            size = os.path.getsize(path)
            self.assertGreater(size, 1024, f"Checkpoint too small: {size} bytes")
        finally:
            os.unlink(path)


if __name__ == '__main__':
    unittest.main()
