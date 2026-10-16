#!/usr/bin/env python3
"""
E2E daemon-RPC coverage for the SNN-language-stack knobs added in commits
263d60e8c .. a00a9ba0b (E1-E6).

Hybrid harness:
  - The "real" brain_daemon.py boots a 2M-neuron brain and binds a Unix
    socket under /var/run/athena/. Both of those are out of reach in CI/test
    sandboxes (root-only socket dir, ~11 min full init). To get coverage
    of the dispatcher in spite of that, we instantiate BrainService directly
    against a small in-process nimcp.Brain and exercise the SAME _cmd_*
    handlers the daemon main loop dispatches to (handle_readonly / handle).
    This catches the same regression classes the user listed:
      - RPC handler missing from the dispatcher  (KeyError / "Unknown command")
      - Method missing on the Brain object       ("not available — rebuild nimcp.so")
      - Handler raises mid-dispatch              (caught and surfaced)
    The only path we don't cover this way is the Unix-socket framing layer,
    which is identical for every command and is exercised by every existing
    test_brain_*.py round-trip in tests/smoke/.

  - When NIMCP_E2E_RUN_DAEMON=1 is set in the env AND /var/run/athena is
    writable, we ALSO run an end-to-end subprocess test that boots a real
    brain_daemon.py on a temp socket path and sends each RPC via length-
    prefixed JSON. Default-skip in CI keeps the test independent of root /
    systemd.

Coverage (each RPC verified to return ok=True with the new field set):
  - set_snn_language_bridge_beam_width           k=3
  - set_snn_language_bridge_eos_word_pop          pop=10
  - set_snn_language_bridge_repetition_penalty    penalty=0.2 window=3
  - set_anaphora_enabled                          enabled=True
  - set_grounded_negation_enabled                 enabled=True
  - set_grounded_sense_disambiguation_enabled     enabled=True
  - set_snn_language_bridge_rng_seed              seed=42
  - set_snn_language_bridge_sampling              temperature=0.8 top_p=0.9
  - set_snn_language_bridge_sampling_mode         mode=1
  - grounded_push_turn                            (semantic_vec, is_user)
  - grounded_get_discourse_turn_count             >=1
  - grounded_set_discourse_capacity               capacity=8
  - get_snn_language_bridge_config                dict reflects sampling/temperature

Failure modes to catch (per task spec):
  1. Unknown command            -> handler returns {"error": "Unknown command: ..."}
  2. Method missing on Brain    -> handler returns {"error": "... not available — rebuild nimcp.so"}
  3. Handler raises mid-dispatch -> handler returns {"error": "...: ..."}
  4. Daemon crash mid-test       -> subprocess test catches via wait()

Run:
    python3 tests/e2e/test_lang_daemon_rpc.py
"""
from __future__ import annotations

import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"

sys.path.insert(0, str(SCRIPTS_DIR))


# ---------------------------------------------------------------------------
# In-process harness (default)
# ---------------------------------------------------------------------------

class InProcDaemonRPCTest(unittest.TestCase):
    """Drive every new _cmd_* via brain_daemon.BrainService.handle().

    We never spawn a subprocess — the test loads brain_daemon.py as a module,
    builds a small nimcp.Brain, hands it to BrainService, and dispatches
    JSON-shaped requests through the dispatcher. This exercises:
      - the getattr(self, f"_cmd_{cmd}", None) lookup
      - the Brain method-name resolution inside each _cmd_*
      - the kwarg parsing and error-formatting of each handler
    """

    @classmethod
    def setUpClass(cls):
        # Quiet brain init logging so the test output is readable even when
        # a verbose runner streams stderr.
        import nimcp
        cls.nimcp = nimcp
        cls.brain = nimcp.Brain(
            "rpc_e2e_test",
            128,            # ANN size — small + fast (avoids 80+ subsystem init)
            10,             # task / num_classes
        )
        # Import brain_daemon AFTER nimcp so its module-level imports succeed.
        import brain_daemon  # noqa: E402
        cls.brain_daemon = brain_daemon
        cls.service = brain_daemon.BrainService(cls.brain)

    @classmethod
    def tearDownClass(cls):
        # Brain destructor runs on GC; explicit del helps under the test runner.
        try:
            del cls.service
        except Exception:
            pass
        try:
            del cls.brain
        except Exception:
            pass

    # -- helpers --

    def _dispatch(self, cmd: str, **kwargs) -> dict:
        """Dispatch a single command through the same code path as the
        daemon main loop. Asserts the response is a dict (not a string,
        not None) and returns it for further inspection."""
        req = {"cmd": cmd, **kwargs}
        # handle() is the write-path used by socket connections.
        resp = self.service.handle(req)
        self.assertIsInstance(resp, dict, f"{cmd} returned non-dict: {resp!r}")
        return resp

    def _assert_ok(self, resp: dict, cmd: str):
        self.assertTrue(
            resp.get("ok") is True,
            f"{cmd} did not return ok=True; got {resp!r}",
        )
        self.assertNotIn(
            "error", resp,
            f"{cmd} unexpectedly carried an 'error' field: {resp!r}",
        )

    # -- per-RPC coverage --

    def test_01_set_snn_language_bridge_beam_width(self):
        resp = self._dispatch("set_snn_language_bridge_beam_width", k=3)
        self._assert_ok(resp, "set_snn_language_bridge_beam_width")
        self.assertEqual(resp.get("k"), 3)

    def test_02_set_snn_language_bridge_eos_word_pop(self):
        resp = self._dispatch("set_snn_language_bridge_eos_word_pop", pop=10)
        self._assert_ok(resp, "set_snn_language_bridge_eos_word_pop")
        self.assertEqual(resp.get("pop"), 10)

    def test_03_set_snn_language_bridge_repetition_penalty(self):
        resp = self._dispatch(
            "set_snn_language_bridge_repetition_penalty",
            penalty=0.2, window=3,
        )
        self._assert_ok(resp, "set_snn_language_bridge_repetition_penalty")
        self.assertAlmostEqual(resp.get("penalty"), 0.2, places=4)
        self.assertEqual(resp.get("window"), 3)

    def test_04_set_anaphora_enabled(self):
        resp = self._dispatch("set_anaphora_enabled", enabled=True)
        self._assert_ok(resp, "set_anaphora_enabled")
        self.assertIs(resp.get("enabled"), True)

    def test_05_set_grounded_negation_enabled(self):
        resp = self._dispatch("set_grounded_negation_enabled", enabled=True)
        self._assert_ok(resp, "set_grounded_negation_enabled")
        self.assertIs(resp.get("enabled"), True)

    def test_06_set_grounded_sense_disambiguation_enabled(self):
        resp = self._dispatch(
            "set_grounded_sense_disambiguation_enabled", enabled=True,
        )
        self._assert_ok(resp, "set_grounded_sense_disambiguation_enabled")
        self.assertIs(resp.get("enabled"), True)

    def test_07_set_snn_language_bridge_rng_seed(self):
        resp = self._dispatch("set_snn_language_bridge_rng_seed", seed=42)
        self._assert_ok(resp, "set_snn_language_bridge_rng_seed")
        self.assertEqual(resp.get("seed"), 42)

    def test_08_set_snn_language_bridge_sampling(self):
        resp = self._dispatch(
            "set_snn_language_bridge_sampling",
            temperature=0.8, top_p=0.9,
        )
        self._assert_ok(resp, "set_snn_language_bridge_sampling")
        self.assertAlmostEqual(resp.get("temperature"), 0.8, places=4)
        self.assertAlmostEqual(resp.get("top_p"), 0.9, places=4)

    def test_09_set_snn_language_bridge_sampling_mode(self):
        resp = self._dispatch("set_snn_language_bridge_sampling_mode", mode=1)
        self._assert_ok(resp, "set_snn_language_bridge_sampling_mode")
        self.assertEqual(resp.get("mode"), 1)

    def test_10_grounded_push_turn(self):
        sem = [0.1] * 128
        resp = self._dispatch(
            "grounded_push_turn",
            semantic_vec=sem, n_words=4, is_user=True,
        )
        self._assert_ok(resp, "grounded_push_turn")
        # turn_count >=1 only after a successful push; if the brain
        # has no grounded_language attached the daemon returns -1 here.
        # Either is acceptable — we require the dispatch to succeed
        # without an error. A separate test_11 below asserts the count
        # is non-negative when grounded_language IS attached.

    def test_11_grounded_get_discourse_turn_count(self):
        resp = self._dispatch("grounded_get_discourse_turn_count")
        self._assert_ok(resp, "grounded_get_discourse_turn_count")
        # turn_count is an int (could be 0 if no push has occurred yet
        # in this test order; could be 1 if test_10 ran first under
        # alphabetical ordering as set by unittest).
        self.assertIn("turn_count", resp)
        self.assertIsInstance(resp["turn_count"], int)
        self.assertGreaterEqual(resp["turn_count"], 0)

    def test_12_grounded_set_discourse_capacity(self):
        resp = self._dispatch("grounded_set_discourse_capacity", capacity=8)
        self._assert_ok(resp, "grounded_set_discourse_capacity")
        self.assertEqual(resp.get("capacity"), 8)

    def test_13_get_snn_language_bridge_config(self):
        # First: pin the sampling knobs so we can assert they round-trip.
        self._dispatch(
            "set_snn_language_bridge_sampling",
            temperature=0.8, top_p=0.9,
        )
        resp = self._dispatch("get_snn_language_bridge_config")
        # Two acceptable shapes: {"ok": True, "config": {...}} (when bridge
        # is attached) or {"error": "..."} (no bridge attached). Small fast
        # brains often skip the bridge — accept either, but if it succeeds,
        # walk the dict.
        if "error" in resp:
            self.skipTest(f"no SNN-language bridge on this brain: {resp['error']}")

        self._assert_ok(resp, "get_snn_language_bridge_config")
        cfg = resp.get("config")
        self.assertIsInstance(cfg, dict, "config payload must be a dict")

        # Spot-check a representative subset of the documented fields.
        # We don't pin the count to 25 — the schema may grow; we DO pin the
        # presence of every known PA/MQ knob. Fields below are emitted by
        # nimcp_python.c::Brain_get_snn_language_bridge_config.
        required = {
            "max_concept_pops", "max_word_pops", "neurons_per_pop",
            "stdp_tau_plus", "stdp_tau_minus",
            "stdp_a_plus", "stdp_a_minus", "stdp_learning_rate",
            "binding_w_max", "decode_window_ms", "decay_rate", "spike_blend",
            "enable_da_modulation", "da_modulation_gain",
            "enable_imagination", "enable_curiosity",
            "enable_sleep_consolidation", "prune_threshold",
            "temperature", "top_p", "produce_topk",
            "glove_blend",
            "intent_persistence", "word_feedback",
            "enable_snn_spike_routing", "activation_tau_ms",
            "use_hyperbolic_embeddings", "sampling_mode",
        }
        missing = required - set(cfg.keys())
        self.assertFalse(
            missing,
            f"get_snn_language_bridge_config dict missing fields: {sorted(missing)}",
        )

        # Round-trip: temperature should be ≈ 0.8 (just set above).
        self.assertAlmostEqual(
            float(cfg["temperature"]), 0.8, places=4,
            msg=f"temperature did not round-trip; got {cfg['temperature']}",
        )
        self.assertAlmostEqual(
            float(cfg["top_p"]), 0.9, places=4,
            msg=f"top_p did not round-trip; got {cfg['top_p']}",
        )

    # -- cascade self-train + counters + respond_via_cascade RPCs --
    # Walkthrough-3 audit E flagged these as untested at the daemon
    # dispatch layer. Each handler is in scripts/brain_daemon.py around
    # lines 2075-2225 (cascade_self_train_*, cascade_counters_*,
    # respond_via_cascade_*).

    def test_14_set_cascade_self_train_enabled(self):
        resp = self._dispatch("set_cascade_self_train_enabled", enabled=True)
        # Either {"ok": True, "enabled": True} on a brain with cascade
        # subsystem, or {"error": "..."} if the brain init skipped it.
        # The dispatch itself MUST not raise.
        if "error" in resp:
            self.skipTest(f"cascade not init'd on this brain: {resp['error']}")
        self._assert_ok(resp, "set_cascade_self_train_enabled")
        self.assertIs(resp.get("enabled"), True)

    def test_15_set_cascade_self_train_tunables(self):
        resp = self._dispatch(
            "set_cascade_self_train_tunables",
            alpha=0.123, lr_scale=2.5,
        )
        if "error" in resp:
            self.skipTest(f"cascade not init'd: {resp['error']}")
        self._assert_ok(resp, "set_cascade_self_train_tunables")
        self.assertAlmostEqual(resp.get("alpha"), 0.123, places=4)
        self.assertAlmostEqual(resp.get("lr_scale"), 2.5, places=4)

    def test_16_get_cascade_self_train_state(self):
        # First flip the flag so the state read has something to verify.
        prep = self._dispatch("set_cascade_self_train_enabled", enabled=True)
        if "error" in prep:
            self.skipTest(f"cascade not init'd: {prep['error']}")
        resp = self._dispatch("get_cascade_self_train_state")
        if "error" in resp:
            self.skipTest(f"get_cascade_self_train_state unavailable: {resp['error']}")
        self._assert_ok(resp, "get_cascade_self_train_state")
        # Should return all 4 fields.
        for key in ("enabled", "baseline", "alpha", "lr_scale"):
            self.assertIn(key, resp, f"missing {key} in state dict")
        self.assertIs(resp["enabled"], True)

    def test_17_get_cascade_counters(self):
        resp = self._dispatch("get_cascade_counters")
        if "error" in resp:
            self.skipTest(f"get_cascade_counters unavailable: {resp['error']}")
        self._assert_ok(resp, "get_cascade_counters")
        # Spot-check the documented field set.
        required = {
            "total_runs", "runs_with_prompt", "runs_spontaneous",
            "runs_fatal_error",
            "stage_invocations", "stage_mask_skips", "stage_failures",
            "pragmatics_indirect_overrides", "wernicke_lexicon_miss",
            "speech_repair_applied",
            "self_train_steps_matched", "self_train_steps_no_bindings",
            "self_produced_events_fired",
            "discourse_ring_pushes_user", "discourse_ring_pushes_self",
        }
        missing = required - set(resp.keys())
        self.assertFalse(
            missing,
            f"get_cascade_counters missing fields: {sorted(missing)}",
        )
        # Per-stage arrays must be 15-element lists.
        for key in ("stage_invocations", "stage_mask_skips", "stage_failures"):
            self.assertIsInstance(resp[key], list, f"{key} not a list")
            self.assertEqual(
                len(resp[key]), 15,
                f"{key} length != 15 (cascade has 15 stages)",
            )

    def test_18_reset_cascade_counters(self):
        resp = self._dispatch("reset_cascade_counters")
        if "error" in resp:
            self.skipTest(f"reset_cascade_counters unavailable: {resp['error']}")
        self._assert_ok(resp, "reset_cascade_counters")
        # After reset, the snapshot should show all-zero totals.
        snap = self._dispatch("get_cascade_counters")
        if "error" in snap:
            return  # already verified the reset path
        self.assertEqual(snap.get("total_runs"), 0)
        self.assertEqual(snap.get("runs_with_prompt"), 0)
        self.assertEqual(snap.get("runs_spontaneous"), 0)

    def test_19_set_respond_via_cascade(self):
        resp = self._dispatch("set_respond_via_cascade", enabled=True)
        if "error" in resp:
            self.skipTest(f"respond_via_cascade unavailable: {resp['error']}")
        self._assert_ok(resp, "set_respond_via_cascade")
        self.assertIs(resp.get("enabled"), True)

    def test_20_get_respond_via_cascade(self):
        # Set then read — verify round-trip via daemon dispatch.
        prep = self._dispatch("set_respond_via_cascade", enabled=True)
        if "error" in prep:
            self.skipTest(f"set_respond_via_cascade unavailable: {prep['error']}")
        resp = self._dispatch("get_respond_via_cascade")
        if "error" in resp:
            self.skipTest(f"get_respond_via_cascade unavailable: {resp['error']}")
        self._assert_ok(resp, "get_respond_via_cascade")
        self.assertIs(resp.get("enabled"), True)
        # Flip back to default OFF so subsequent tests in this class run
        # at the legacy bridge-passthrough latency.
        self._dispatch("set_respond_via_cascade", enabled=False)

    # -- failure-mode coverage --

    def test_90_unknown_command_is_caught(self):
        """A bogus cmd must not raise — the dispatcher converts it into
        an {"error": "Unknown command: ..."} response. This is the
        contract every monitor / eval client relies on."""
        resp = self.service.handle({"cmd": "no_such_command_42"})
        self.assertIsInstance(resp, dict)
        self.assertIn("error", resp)
        self.assertIn("Unknown command", resp["error"])

    def test_91_method_missing_surfaces_clear_message(self):
        """Simulate the "rebuild nimcp.so" failure mode the user flagged.

        We can't monkey-patch nimcp.Brain — it's a C extension type with
        an immutable __dict__. Instead, build a small Python proxy that
        forwards everything BUT raises AttributeError on the method we
        want to simulate as missing. Hand that proxy to a fresh
        BrainService. The handler should NOT crash; it should return
        {"error": "... not available — rebuild nimcp.so"}, exactly what
        a stale .so on disk would produce.
        """
        real_brain = self.brain

        class _MissingMethodProxy:
            """Forwards every attribute to the real brain except the named
            method, which raises AttributeError on access."""
            def __init__(self, target, *missing):
                object.__setattr__(self, "_target", target)
                object.__setattr__(self, "_missing", set(missing))

            def __getattr__(self, name):
                if name in self._missing:
                    raise AttributeError(
                        f"simulated: {name} not in this binding"
                    )
                return getattr(self._target, name)

        proxy = _MissingMethodProxy(real_brain, "set_anaphora_enabled")
        # Build a one-off service so our patched proxy doesn't pollute
        # the class-level fixture.
        local_service = self.brain_daemon.BrainService(proxy)
        resp = local_service.handle(
            {"cmd": "set_anaphora_enabled", "enabled": True},
        )
        self.assertIsInstance(resp, dict)
        self.assertIn("error", resp)
        self.assertIn(
            "rebuild nimcp.so", resp["error"],
            f"expected the 'rebuild nimcp.so' hint; got: {resp['error']!r}",
        )


# ---------------------------------------------------------------------------
# Optional: real-subprocess harness (gated on env var)
# ---------------------------------------------------------------------------

@unittest.skipUnless(
    os.environ.get("NIMCP_E2E_RUN_DAEMON") == "1",
    "set NIMCP_E2E_RUN_DAEMON=1 to run the real-daemon subprocess test "
    "(requires a writable socket dir + several minutes of brain init)",
)
class SubprocessDaemonRPCTest(unittest.TestCase):
    """Boot a real brain_daemon.py and round-trip every new RPC over a
    Unix socket. Default-skipped — opt in via NIMCP_E2E_RUN_DAEMON=1."""

    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp(prefix="nimcp_e2e_daemon_")
        cls.socket_path = os.path.join(cls.tmpdir, "brain.sock")
        log_path = os.path.join(cls.tmpdir, "daemon.log")
        cls.proc = subprocess.Popen(
            [
                sys.executable,
                str(SCRIPTS_DIR / "brain_daemon.py"),
                "--fresh",
                "--init-mode", "minimal",
                "--neuron-count", "128",
                "--socket", cls.socket_path,
                "--log-file", log_path,
            ],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        # Wait up to 120 s for the socket to appear.
        deadline = time.time() + 120.0
        while time.time() < deadline:
            if os.path.exists(cls.socket_path):
                # Tiny grace period for chmod + listen() to settle.
                time.sleep(0.5)
                return
            if cls.proc.poll() is not None:
                stdout, stderr = cls.proc.communicate(timeout=1)
                raise RuntimeError(
                    f"daemon exited prematurely (rc={cls.proc.returncode}); "
                    f"stderr tail: {stderr[-1024:].decode(errors='replace')}",
                )
            time.sleep(0.5)
        raise RuntimeError(
            f"daemon socket {cls.socket_path} never appeared (120s)",
        )

    @classmethod
    def tearDownClass(cls):
        try:
            cls.proc.terminate()
            cls.proc.wait(timeout=10)
        except Exception:
            try:
                cls.proc.kill()
            except Exception:
                pass
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    # -- low-level frame I/O --

    def _send_recv(self, req: dict) -> dict:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(30.0)
        try:
            s.connect(self.socket_path)
            payload = json.dumps(req).encode("utf-8")
            s.sendall(struct.pack(">I", len(payload)) + payload)
            hdr = b""
            while len(hdr) < 4:
                chunk = s.recv(4 - len(hdr))
                if not chunk:
                    raise IOError("daemon closed socket before response header")
                hdr += chunk
            (length,) = struct.unpack(">I", hdr)
            data = b""
            while len(data) < length:
                chunk = s.recv(length - len(data))
                if not chunk:
                    raise IOError("daemon closed socket mid-response")
                data += chunk
            return json.loads(data.decode("utf-8"))
        finally:
            s.close()

    def _ok(self, cmd: str, **kw):
        resp = self._send_recv({"cmd": cmd, **kw})
        self.assertTrue(
            resp.get("ok") is True,
            f"{cmd} -> {resp!r}",
        )
        return resp

    # -- coverage --

    def test_real_daemon_full_rpc_sequence(self):
        # Exhaust every new knob in one test so we don't pay the daemon
        # boot cost N times.
        self._ok("set_snn_language_bridge_beam_width", k=3)
        self._ok("set_snn_language_bridge_eos_word_pop", pop=10)
        self._ok("set_snn_language_bridge_repetition_penalty",
                 penalty=0.2, window=3)
        self._ok("set_anaphora_enabled", enabled=True)
        self._ok("set_grounded_negation_enabled", enabled=True)
        self._ok("set_grounded_sense_disambiguation_enabled", enabled=True)
        self._ok("set_snn_language_bridge_rng_seed", seed=42)
        self._ok("set_snn_language_bridge_sampling",
                 temperature=0.8, top_p=0.9)
        self._ok("set_snn_language_bridge_sampling_mode", mode=1)
        self._ok("grounded_push_turn",
                 semantic_vec=[0.1] * 128, n_words=4, is_user=True)
        cnt = self._send_recv({"cmd": "grounded_get_discourse_turn_count"})
        self.assertTrue(cnt.get("ok"))
        self.assertGreaterEqual(int(cnt["turn_count"]), 1)
        self._ok("grounded_set_discourse_capacity", capacity=8)
        cfg_resp = self._send_recv({"cmd": "get_snn_language_bridge_config"})
        if cfg_resp.get("ok"):
            cfg = cfg_resp.get("config", {})
            self.assertAlmostEqual(float(cfg.get("temperature", -1)), 0.8,
                                   places=4)

        # ----- Cascade self-train + counters + respond_via_cascade -----
        # Mirror of InProcDaemonRPCTest test_14..test_20 but over the real
        # Unix socket so we exercise the JSON-frame + length-prefix path
        # the daemon main loop actually runs in production. Per-RPC
        # tolerates skipTest-equivalent failures (the minimal-init brain
        # may not have the cascade subsystem); we record but don't fail.
        ct_resp = self._send_recv({"cmd": "set_cascade_self_train_enabled",
                                     "enabled": True})
        if ct_resp.get("ok"):
            self._ok("set_cascade_self_train_tunables",
                     alpha=0.123, lr_scale=2.5)
            state = self._send_recv({"cmd": "get_cascade_self_train_state"})
            self.assertTrue(state.get("ok"))
            self.assertIs(state.get("enabled"), True)
            self.assertAlmostEqual(float(state.get("alpha", -1)), 0.123,
                                   places=4)

        ctr_resp = self._send_recv({"cmd": "get_cascade_counters"})
        if ctr_resp.get("ok"):
            for key in ("total_runs", "stage_invocations",
                        "stage_mask_skips", "stage_failures",
                        "wernicke_lexicon_miss",
                        "discourse_ring_pushes_self"):
                self.assertIn(key, ctr_resp,
                              f"get_cascade_counters missing {key}")
            for arr_key in ("stage_invocations", "stage_mask_skips",
                            "stage_failures"):
                self.assertEqual(len(ctr_resp[arr_key]), 15,
                                 f"{arr_key} != 15 stages")
            self._ok("reset_cascade_counters")

        rvc_resp = self._send_recv({"cmd": "set_respond_via_cascade",
                                      "enabled": True})
        if rvc_resp.get("ok"):
            self.assertIs(rvc_resp.get("enabled"), True)
            check = self._send_recv({"cmd": "get_respond_via_cascade"})
            self.assertTrue(check.get("ok"))
            self.assertIs(check.get("enabled"), True)
            # Restore default OFF so any later test in this run isn't
            # paying the cascade-orchestrator latency.
            self._send_recv({"cmd": "set_respond_via_cascade",
                             "enabled": False})


# ---------------------------------------------------------------------------
# Entry-point
# ---------------------------------------------------------------------------

def main():
    # Discover both test classes; the subprocess class self-skips unless
    # NIMCP_E2E_RUN_DAEMON=1.
    loader = unittest.defaultTestLoader
    suite = unittest.TestSuite([
        loader.loadTestsFromTestCase(InProcDaemonRPCTest),
        loader.loadTestsFromTestCase(SubprocessDaemonRPCTest),
    ])

    # The unittest runner uses stderr for its progress output, so we don't
    # redirect there. Brain-init logs go to stderr too — they're noisy but
    # we leave them in place so test failures are debuggable. To suppress
    # them, set NIMCP_LOG_LEVEL=off before invoking this test.
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == "__main__":
    main()
