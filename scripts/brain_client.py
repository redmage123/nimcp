#!/usr/bin/env python3
"""
brain_client.py — Client library for the Brain Daemon

Provides a BrainProxy class that has the same API as nimcp.Brain but
forwards all calls over the Unix socket to the brain_daemon.py process.

Usage:
    from brain_client import BrainProxy, is_daemon_running

    if is_daemon_running():
        brain = BrainProxy()
    else:
        import nimcp
        brain = nimcp.Brain("athena", ...)

    # Same API either way:
    loss = brain.learn_vector(features, target, label="dog")
    result = brain.decide_full(features)
"""

import json
import os
import socket
import struct
import time

SOCKET_PATH = "/var/run/athena/brain.sock"
DEFAULT_TIMEOUT = 300.0  # 5 min — 2.5M neurons + SNN BPTT can take 60-90s per step


def is_daemon_running(socket_path=SOCKET_PATH):
    """Check if the brain daemon is running and responsive."""
    if not os.path.exists(socket_path):
        return False
    try:
        proxy = BrainProxy(socket_path=socket_path, timeout=5.0)
        resp = proxy._send({"cmd": "ping"})
        return resp.get("ok", False)
    except Exception:
        return False


class BrainProxy:
    """Drop-in replacement for nimcp.Brain that proxies to the daemon.

    All methods match the nimcp.Brain API. The daemon handles thread safety.
    Includes automatic retry with backoff for transient socket failures.
    """

    # Retry config
    MAX_RETRIES = 5
    INITIAL_BACKOFF = 1.0       # seconds
    MAX_BACKOFF = 30.0          # seconds
    # 30 min covers a full-init brain restart (~15-25 min checkpoint load on
    # a 2M neuron brain). The 2026-05-15 wedge was partially explained by the
    # old 5 min ceiling: a brain SIGTERM + autorestart cycle exceeded 5 min,
    # _wait_for_daemon raised ConnectionError, retries got burned, the
    # trainer's caller silently swallowed the final exception.
    DAEMON_WAIT_TIMEOUT = 1800

    # Transient errors worth retrying
    _TRANSIENT = (ConnectionError, BrokenPipeError,
                  FileNotFoundError, socket.timeout, OSError)

    def __init__(self, socket_path=SOCKET_PATH, timeout=DEFAULT_TIMEOUT):
        self.socket_path = socket_path
        self.timeout = timeout
        self._consecutive_failures = 0
        self._async_pool = None

    def _send_fire_and_forget(self, req):
        """Send without waiting for response — runs in background thread."""
        from concurrent.futures import ThreadPoolExecutor
        if self._async_pool is None:
            self._async_pool = ThreadPoolExecutor(max_workers=2)
        self._async_pool.submit(self._send_once, req)

    def _send_once(self, req):
        """Single send attempt — no retries."""
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(self.timeout)
        try:
            sock.connect(self.socket_path)
            data = json.dumps(req, default=_json_default).encode("utf-8")
            sock.sendall(struct.pack(">I", len(data)) + data)

            # Receive response
            hdr = b""
            while len(hdr) < 4:
                chunk = sock.recv(4 - len(hdr))
                if not chunk:
                    raise ConnectionError("Daemon closed connection")
                hdr += chunk
            length = struct.unpack(">I", hdr)[0]
            body = b""
            while len(body) < length:
                chunk = sock.recv(min(length - len(body), 65536))
                if not chunk:
                    raise ConnectionError("Daemon closed connection")
                body += chunk
            resp = json.loads(body.decode("utf-8"))
            if "error" in resp and resp["error"]:
                raise RuntimeError(f"Brain daemon error: {resp['error']}")
            return resp
        finally:
            sock.close()

    def _send(self, req):
        """Send with automatic retry and exponential backoff.

        Retries on transient socket errors (connection refused, reset, timeout,
        file not found). If the daemon socket disappears entirely, waits for
        it to come back (supervisord auto-restarts the daemon).

        Retry-counting policy: FileNotFoundError ("socket gone" — daemon
        restarting) does NOT count against MAX_RETRIES. Instead it triggers
        a single _wait_for_daemon call (up to DAEMON_WAIT_TIMEOUT). Counting
        socket-gone against MAX_RETRIES is what made the 2026-05-15 wedge
        unrecoverable: a brain SIGTERM + autorestart raced through 5
        retries' worth of socket-gone events before the brain finished
        loading. Now the retry budget is reserved for "brain is up but
        flaky" failures (ConnectionReset, recv-timeout, etc.).
        """
        last_exc = None
        backoff = self.INITIAL_BACKOFF
        attempt = 0

        while attempt < self.MAX_RETRIES:
            try:
                resp = self._send_once(req)
                # Success — reset failure counter
                if self._consecutive_failures > 0:
                    print(f"  [BrainProxy] Reconnected after "
                          f"{self._consecutive_failures} failures",
                          flush=True)
                self._consecutive_failures = 0
                return resp
            except self._TRANSIENT as e:
                last_exc = e
                self._consecutive_failures += 1
                cmd = req.get("cmd", "?")

                # Socket gone — daemon restarting. Wait for it without
                # consuming a retry slot.
                if isinstance(e, FileNotFoundError):
                    print(f"  [BrainProxy] Socket gone — waiting for "
                          f"daemon restart (up to "
                          f"{self.DAEMON_WAIT_TIMEOUT}s)...", flush=True)
                    try:
                        self._wait_for_daemon()
                        backoff = self.INITIAL_BACKOFF  # reset after wait
                        continue  # retry without bumping attempt
                    except ConnectionError:
                        # Daemon didn't come back within DAEMON_WAIT_TIMEOUT
                        # — raise and let caller decide.
                        raise

                # Brain reachable but flaky — count against MAX_RETRIES.
                attempt += 1
                if attempt < self.MAX_RETRIES:
                    print(f"  [BrainProxy] {type(e).__name__} on "
                          f"'{cmd}' — retry {attempt}/{self.MAX_RETRIES} "
                          f"in {backoff:.1f}s", flush=True)
                    time.sleep(backoff)
                    backoff = min(backoff * 2, self.MAX_BACKOFF)

        # All retries exhausted
        raise last_exc

    def _wait_for_daemon(self):
        """Block until the daemon socket reappears and responds to ping."""
        deadline = time.monotonic() + self.DAEMON_WAIT_TIMEOUT
        wait = 2.0
        while time.monotonic() < deadline:
            time.sleep(wait)
            if os.path.exists(self.socket_path):
                try:
                    self._send_once({"cmd": "ping"})
                    return  # Daemon is back
                except Exception:
                    pass  # Socket exists but not ready yet
            wait = min(wait * 1.5, 15.0)
        raise ConnectionError(
            f"Daemon did not restart within {self.DAEMON_WAIT_TIMEOUT}s")

    # -- Batched commands --

    def batch(self, commands):
        """Send multiple commands in a single round-trip.

        Args:
            commands: list of request dicts, e.g. [{"cmd": "ping"}, {"cmd": "status"}]
        Returns:
            list of response dicts, one per command
        """
        resp = self._send({"cmd": "batch", "commands": commands})
        return resp.get("results", [])

    def submit_sensory_batch(self, modalities):
        """Submit multiple sensory modalities in one round-trip.

        Args:
            modalities: dict of modality → data, e.g.
                {"visual": (pixels, w, h, ch), "audio": mel, "speech": samples}
        """
        commands = []
        for modality, data in modalities.items():
            req = {"cmd": "submit_sensory", "modality": modality}
            if modality == "visual" and isinstance(data, tuple):
                pixels, w, h, ch = data
                req["data"] = _to_list(pixels)
                req["width"] = w
                req["height"] = h
                req["channels"] = ch
            elif modality in ("somatosensory", "somato") and isinstance(data, tuple):
                vec, n_seg = data
                req["data"] = _to_list(vec)
                req["n_segments"] = n_seg
            else:
                req["data"] = _to_list(data)
            commands.append(req)
        if commands:
            self._send({"cmd": "batch", "commands": commands})

    # -- Core learning --

    def learn_vector(self, features, target, label=None, confidence=None,
                     learning_rate=None):
        # Fast path: encode float arrays as base64 binary within JSON.
        # JSON float list: 4096 floats → ~50KB text (12 bytes/float as string).
        # Base64 f32:     4096 floats → ~22KB text (4 bytes/float × 4/3 base64).
        # Saves ~3ms serialization + ~2ms network per call.
        import numpy as np
        from base64 import b64encode
        f_arr = np.asarray(features, dtype=np.float32)
        t_arr = np.asarray(target, dtype=np.float32)
        req = {"cmd": "learn_vector_bin",
               "f_b64": b64encode(f_arr.tobytes()).decode("ascii"),
               "t_b64": b64encode(t_arr.tobytes()).decode("ascii"),
               "f_len": len(f_arr), "t_len": len(t_arr)}
        if label is not None:
            req["label"] = label
        if confidence is not None:
            req["confidence"] = float(confidence)
        if learning_rate is not None:
            req["learning_rate"] = float(learning_rate)
        resp = self._send(req)
        return resp.get("loss", 0.0)

    def learn_vector_batch(self, pairs, learning_rate=None):
        import numpy as np
        from base64 import b64encode
        # Pack all feature/target pairs as concatenated base64 binary
        f_arrays = [np.asarray(f, dtype=np.float32) for f, t in pairs]
        t_arrays = [np.asarray(t, dtype=np.float32) for f, t in pairs]
        f_concat = np.concatenate(f_arrays)
        t_concat = np.concatenate(t_arrays)
        req = {"cmd": "learn_vector_batch_bin",
               "n_pairs": len(pairs),
               "f_dim": len(f_arrays[0]) if f_arrays else 0,
               "t_dim": len(t_arrays[0]) if t_arrays else 0,
               "f_b64": b64encode(f_concat.tobytes()).decode("ascii"),
               "t_b64": b64encode(t_concat.tobytes()).decode("ascii")}
        if learning_rate is not None:
            req["learning_rate"] = float(learning_rate)
        resp = self._send(req)
        return resp.get("avg_loss", 0.0)

    def train_batch_text(self, items, learning_rate=None):
        """Batch training from raw text — daemon ONNX-encodes + learns.

        items: list of {"text": "...", "label": "...", "target_text": "..."}
        Returns dict with avg_loss, n_items, ms_per_item.
        """
        req = {"cmd": "train_batch_text", "items": items}
        if learning_rate is not None:
            req["learning_rate"] = float(learning_rate)
        return self._send(req)

    # -- Inference --

    def decide_full(self, features):
        resp = self._send({"cmd": "decide_full",
                           "features": _to_list(features)})
        return resp.get("result", {})

    def predict(self, features):
        resp = self._send({"cmd": "predict",
                           "features": _to_list(features)})
        return resp.get("result", {})

    def speak(self, output_vector):
        resp = self._send({"cmd": "speak",
                           "output_vector": _to_list(output_vector)})
        return resp.get("result", {})

    def generate_text(self, output_vector):
        resp = self._send({"cmd": "generate_text",
                           "output_vector": _to_list(output_vector)})
        return resp.get("result", {})

    def grounded_respond(self, text):
        resp = self._send({"cmd": "grounded_respond", "text": text})
        return resp.get("result", {})

    # -- Developmental stage control --
    # These mirror the in-process nimcp.Brain methods of the same name so the
    # trainer (immerse_athena --daemon, which is a *socket client*, not an
    # in-process brain) can drive the daemon's developmental gate.
    #
    # Without them, TrainingIntegration.begin_stage()'s
    #   self.brain.set_active_stage(stage)
    # raised AttributeError on the proxy and was swallowed ("stage gate frozen
    # at default — rebuild nimcp.so"). The daemon's gl->current_stage then
    # stayed at its default (0) for the entire run while the curriculum
    # believed it had advanced — capping produce to the stage-0 confidence
    # floor (1 word) regardless of training progress. The .so was never the
    # problem: the trainer is a pure client, so the missing piece was this
    # 4th-layer client method, not a binding. (See feedback-committed-but-
    # unreachable: trace a default-OFF/idle feature OUTWARD to the daemon.)
    def set_active_stage(self, stage):
        """Set the brain's developmental stage. Drives the produce confidence
        floor + POS / function-word scaffolding gates (gl->current_stage)."""
        resp = self._send({"cmd": "set_active_stage", "stage": int(stage)})
        return resp.get("stage", resp)

    def get_active_stage(self):
        resp = self._send({"cmd": "get_active_stage"})
        return resp.get("stage")

    def set_stage_constraints(self, stage):
        """Slice-E alias — set_active_stage plus install the per-stage vocab
        mask / produce-word bounds in one call."""
        return self._send({"cmd": "set_stage_constraints", "stage": int(stage)})

    def get_stage_constraints(self, stage=None):
        # R4 (walkthrough fix): the daemon defaults stage=0 when the field is
        # absent, so the bare call only ever read stage-0 constraints. Default
        # to the daemon's ACTIVE stage (query it) so the trainer sees the
        # constraints actually in force; an explicit stage still overrides.
        req = {"cmd": "get_stage_constraints"}
        if stage is None:
            stage = self.get_active_stage()
        if stage is not None:
            req["stage"] = int(stage)
        resp = self._send(req)
        return resp.get("constraints", resp)

    def set_vocab_mask_for_stage(self, stage):
        return self._send({"cmd": "set_vocab_mask_for_stage",
                           "stage": int(stage)})

    def produce_cascade_recurrent(self, prompt=None, max_iters=8, self_match_eps=0.01):
        """Recurrent / biological-fidelity variant of produce_cascade.
        Iterates the full 15-stage cascade until utterance + self_match
        converge. Each iteration's stage_self_train STDP shifts the bridge
        slightly; the next iteration reads the shifted bridge; the system
        settles toward an attractor. Mimics real cortical settling
        dynamics (~200-400ms ≈ 4-8 iterations at our per-stage cost).

        Returns dict with utterance/word_count/confidence/settling_steps.
        settling_steps is the number of iterations actually taken before
        convergence (or max_iters if no convergence).

        Slice 1 of the recurrent-language-architecture rewrite (see
        docs/claude/recurrent-language-architecture.md)."""
        req = {
            "cmd": "produce_cascade_recurrent",
            "max_iters": int(max_iters),
            "self_match_eps": float(self_match_eps),
        }
        if prompt is not None:
            req["prompt"] = prompt
        resp = self._send(req)
        if resp.get("error"):
            return {}
        return resp

    def get_cascade_fep_metrics(self):
        """Slice 3: snapshot the FEP prediction-error trajectory of the
        most recent produce_cascade_recurrent call on the daemon.

        Returns dict with:
          iterations_run     — number of recurrent iters
          pe_total_trace     — list of per-iter pe_total values
          pe_total_initial / terminal / min / max / mean
          pe_decay_rate      — (initial-terminal)/initial; positive =
                               system reduced its surprise across iters
          converged          — True if the loop exited on convergence
                               (utterance + self_match stable), False if
                               it hit max_iters.

        Zero-initialized if produce_cascade_recurrent has never run.
        Used by monitoring + walkthrough scripts to diagnose whether
        the recurrent loop is actually settling vs wedged."""
        resp = self._send({"cmd": "get_cascade_fep_metrics"})
        if resp.get("error"):
            return {}
        return resp

    def produce_cascade(self, prompt=None):
        """Invoke the full 15-stage production cascade. Drives drive/goal/
        listener/episodic/content/lexical/syntactic(Broca)/self-comp(Wernicke)/
        phonological/motor/self-feedback/speech-repair/prosody/self-train.

        When cascade_self_train is enabled (set_cascade_self_train_enabled),
        stage 15 applies reward-modulated STDP to the bridge based on the
        self-comprehension match score and releases phasic DA. This is the
        path that trains all stages (not just the bridge).

        prompt=None runs spontaneous-speech mode (purely from internal state).
        Returns the full cascade diagnostic dict (~46 fields).
        """
        req = {"cmd": "produce_cascade"}
        if prompt is not None:
            req["prompt"] = prompt
        resp = self._send(req)
        if resp.get("error"):
            return {}
        return resp

    def set_cascade_self_train_enabled(self, enabled=True):
        """Toggle cascade-driven reinforcement learning. When True, every
        produce_cascade call's stage_self_train applies reward-modulated
        STDP to the bridge based on self_match (Wernicke re-parse of own
        output vs. content_intent) and releases phasic DA proportional
        to reward. Closes the self-supervised loop."""
        resp = self._send({
            "cmd": "set_cascade_self_train_enabled",
            "enabled": bool(enabled),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", enabled))

    def set_lateral_inhibition_enabled(self, enabled=True):
        """Slice 4 (recurrent-language-architecture): toggle competitive
        recurrent decode in the SNN language bridge. Default OFF preserves
        bit-for-bit legacy decode_spikes behavior. When ON, every
        bridge_produce call's per-word decode swaps the one-shot cosine
        argmax for a recurrent competition over the top-K candidates:
        candidates excite themselves AND inhibit each other; winner emerges
        from settling dynamics. Implements cohort-model (Marslen-Wilson
        1987) / interactive-activation (McClelland 1981) lexical selection.

        See docs/claude/recurrent-language-architecture.md for the design.
        Runtime-only — caller must re-apply after each daemon restart."""
        resp = self._send({
            "cmd": "set_lateral_inhibition_enabled",
            "enabled": bool(enabled),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", enabled))

    def get_lateral_inhibition_enabled(self):
        """Slice 4: read the lateral-inhibition runtime flag."""
        resp = self._send({"cmd": "get_lateral_inhibition_enabled"})
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", False))

    def set_lateral_inhibition_params(self, gain_self=1.5, gain_inhibit=0.026,
                                       micro_steps=20):
        """Slice 4: tune the recurrent-competition dynamics.

        gain_self    — self-excitation gain per micro-step (default 1.5).
        gain_inhibit — per-other inhibition coefficient (default ~0.026,
                        chosen so 31 * gain_inhibit ≈ 0.81 < gain_self for
                        K=32, keeping the competition bounded).
        micro_steps  — settling iterations (default 20).

        Validation: each value > 0; gains <= 100; steps in [1, 200]."""
        resp = self._send({
            "cmd": "set_lateral_inhibition_params",
            "gain_self":    float(gain_self),
            "gain_inhibit": float(gain_inhibit),
            "micro_steps":  int(micro_steps),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return {
            "gain_self":    float(resp.get("gain_self", gain_self)),
            "gain_inhibit": float(resp.get("gain_inhibit", gain_inhibit)),
            "micro_steps":  int(resp.get("micro_steps", micro_steps)),
        }

    def get_lateral_inhibition_params(self):
        """Slice 4: read lateral-inhibition tunables."""
        resp = self._send({"cmd": "get_lateral_inhibition_params"})
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return {
            "gain_self":    float(resp.get("gain_self", 0.0)),
            "gain_inhibit": float(resp.get("gain_inhibit", 0.0)),
            "micro_steps":  int(resp.get("micro_steps", 0)),
        }

    def set_respond_via_cascade(self, enabled=True):
        """Toggle whether grounded_respond routes through the 15-stage
        cascade (True) or just the bridge (False). Cascade adds Broca's
        syntax + agreement validation, phonological + motor planning,
        prosody, and self-comprehension. Default off in the daemon."""
        resp = self._send({
            "cmd": "set_respond_via_cascade",
            "enabled": bool(enabled),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", enabled))

    def set_reason_in_content(self, enabled=True):
        """Tier 1 Step E: toggle reasoning-conclusion blending into the
        cascade's content intent. When True (and the reasoning engine is
        enabled), respond invokes the reasoning engine once per prompt and
        biases production toward the conclusion. Adds reasoning-pass
        latency; no effect unless respond_via_cascade is also on. Default
        off in the daemon."""
        resp = self._send({
            "cmd": "set_reason_in_content",
            "enabled": bool(enabled),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", enabled))

    def get_reason_in_content(self):
        """Read the current reason_in_content flag (Tier 1 Step E)."""
        resp = self._send({"cmd": "get_reason_in_content"})
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", False))

    def set_thalamic_gate_enabled(self, enabled=True):
        """Slice 6: toggle thalamic gating of cascade-stage bandwidth.

        When True, each cascade stage's scaleable contributions
        (content_intent magnitude, lexical fluency, prosodic intensity,
        self-train lr_scale) are multiplied by a per-stage gate in
        [0, 1] derived from arousal (NE) + attention (ACh) state. Models
        the pulvinar's role as central relay + gain controller. Default
        off — when disabled the cascade behaves byte-identically to
        legacy. Returns the bool the daemon actually set."""
        resp = self._send({
            "cmd": "set_thalamic_gate_enabled",
            "enabled": bool(enabled),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", enabled))

    # ----- Slice 7 — cerebellar prediction-correction -----

    def set_cerebellar_correction_enabled(self, enabled=True):
        """Slice 7: enable cerebellar prediction-correction in the
        cascade's motor + prosody stages. When ON, the existing cerebellum
        adapter (cerebellum_predict_outcome / cerebellum_update_forward_model
        / cerebellum_broadcast_error) is called before + after motor +
        prosody to predict the upcoming pattern, learn from the realised
        actual, and broadcast climbing-fiber error.

        Default OFF — when off, the stages run byte-identically to master.
        Requires brain->cerebellum (created at init); minimal-init brains
        silently no-op the cerebellar code path."""
        resp = self._send({
            "cmd": "set_cerebellar_correction_enabled",
            "enabled": bool(enabled),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", enabled))

    def set_thalamic_gate_for_stage(self, stage_idx, weight):
        """Slice 6: manual override of a single stage's gate weight.

        stage_idx is 0..14 in cascade_stage_mask_t bit order:
            0=wernicke 1=drive 2=goal 3=listener 4=episodic 5=content
            6=lexical 7=syntactic 8=self_comp 9=phonological 10=motor
            11=self_feedback 12=speech_repair 13=prosody 14=self_train

        weight < 0 clears the manual override (returns to auto-derived).
        weight in [0, 1] sets and locks the gate. Useful for ablation
        experiments (e.g. set motor=0 to silence articulator output).
        Returns the (stage_idx, weight) dict the daemon recorded."""
        resp = self._send({
            "cmd": "set_thalamic_gate_for_stage",
            "stage_idx": int(stage_idx),
            "weight":    float(weight),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return {
            "stage_idx": int(resp.get("stage_idx", stage_idx)),
            "weight":    float(resp.get("weight",    weight)),
        }

    def get_thalamic_gates(self):
        """Slice 6: read the current thalamic gate weights + override flags.

        Returns dict with keys:
          'enabled':     bool — master flag
          'gates':       dict[str, float] — stage-name keyed weights
                         (e.g. {'drive': 0.5, 'content': 0.8, ...})
          'weights':     [float] × 15 — same values, ordered by stage idx
          'overrides':   [bool]  × 15 — true if manually overridden
          'stage_names': [str]   × 15 — names matching the index ordering
        When the daemon was started against a pre-Slice-6 nimcp.so the
        call returns {} (RPC raises on AttributeError → returns empty)."""
        resp = self._send({"cmd": "get_thalamic_gates"})
        if resp.get("error"):
            return {}
        return resp

    # ----- Slice 5 — phonological loop working memory buffer -----

    def set_phonological_loop_enabled(self, enabled=True):
        """Slice 5: enable Baddeley's phonological-loop working memory.

        When enabled, the recurrent cascade decays traces between
        iterations, merges lexical output into the buffer, and feeds the
        buffer's surface form (words with trace >= 0.3) to stage_syntactic
        + stage_self_comp instead of the one-shot lexical output. Default
        OFF: recurrent cascade behaves byte-identically to Slice 1+2.
        """
        resp = self._send({
            "cmd": "set_phonological_loop_enabled",
            "enabled": bool(enabled),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return bool(resp.get("enabled", enabled))

    def set_phonological_loop_decay(self, decay=0.15):
        """Slice 5: configure per-iteration phonological-loop trace decay.

        Clamped to [0.0, 0.5]; NaN/Inf coerce to default 0.15."""
        resp = self._send({
            "cmd": "set_phonological_loop_decay",
            "decay": float(decay),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return float(resp.get("decay", decay))

    def clear_phonological_loop(self):
        """Slice 5: full reset of the phonological loop (drops every
        trace + word; clears surface buffer). Recurrent cascade also
        clears the loop on entry — this is primarily admin/diagnostic."""
        resp = self._send({"cmd": "clear_phonological_loop"})
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return True

    def get_phonological_loop_state(self):
        """Slice 5: read-only inspection — returns dict('buffer': str,
        'trace_count': int). buffer is the active surface form (words
        with trace >= 0.3, space-separated)."""
        resp = self._send({"cmd": "get_phonological_loop_state"})
        if resp.get("error"):
            return {}
        return {
            "buffer":      str(resp.get("buffer", "")),
            "trace_count": int(resp.get("trace_count", 0)),
        }

    def get_phonological_loop_diag(self):
        """Slice 5: full diagnostic snapshot — enabled/buffer_len/
        trace_count/trace_capacity/max_words/decay_rate/
        avg_trace_strength/last_refresh_ms."""
        resp = self._send({"cmd": "get_phonological_loop_diag"})
        if resp.get("error"):
            return {}
        return resp

    def set_cerebellar_correction_strength(self, strength=0.5):
        """Slice 7: bias mix factor when the recurrent loop flags
        correction_pending. Clamped to [0,1]. Default 0.5."""
        resp = self._send({
            "cmd": "set_cerebellar_correction_strength",
            "strength": float(strength),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return float(resp.get("strength", strength))

    def set_cerebellar_pe_threshold(self, threshold=0.20):
        """Slice 7 (S7-C2): PE-norm threshold above which the recurrent
        cascade flags correction_pending. The cascade PE-norm is the sum of
        two cosine-distance norms (motor + prosody), each in [0, 1], so the
        meaningful range is [0, 2]. Clamped to [0, 2]. Default 0.20."""
        resp = self._send({
            "cmd": "set_cerebellar_pe_threshold",
            "threshold": float(threshold),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return float(resp.get("threshold", threshold))

    def get_cerebellar_diag(self):
        """Slice 7: snapshot of cerebellar prediction-correction
        diagnostics. Returns a dict with keys enabled / strength /
        correction_pending / pe_threshold / predictions_made /
        corrections_applied / last_pe_norm. Cheap to call (no atomics
        — cascade is single-caller-at-a-time)."""
        resp = self._send({"cmd": "get_cerebellar_diag"})
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        # Drop the wrapper "ok" key — callers want the diag scalars.
        resp.pop("ok", None)
        return resp

    def echo_and_correct(self, parent_text, target_word, lr_scale=1.0):
        """Supervised production-training pulse for the SNN language bridge.

        Strengthens (active_concept_pop → target_word_pop) bindings inside
        the bridge so produce stops emitting uniform-random WordNet words.
        Returns the number of pairs strengthened (0 if target_word not in
        bridge's mirrored word_pops yet).
        """
        resp = self._send({
            "cmd": "echo_and_correct",
            "parent_text": parent_text,
            "target_word": target_word,
            "lr_scale": float(lr_scale),
        })
        return int(resp.get("pairs_strengthened", 0))

    def learn_text_bigrams(self, text, lr=0.02):
        """Walk text and apply next-token bigram (and trigram, if flag is on)
        STDP updates on the SNN language bridge. Returns the bigram count.

        This is the ONLY path that drives `total_trigram_updates` — the
        bridge's trigram counter stays at 0 unless this fires regularly on
        real multi-word text. Trigram LTD-on-top-1-false-winner is the only
        mechanism that pulls saturated dominant word_pops back down.
        """
        resp = self._send({
            "cmd": "learn_text_bigrams",
            "text": text,
            "lr": float(lr),
        })
        if resp.get("error"):
            return 0
        return int(resp.get("count", 0))

    def reset_lexicon_distributional(self, zero_and_mark_uninit=True, jitter=0.01):
        """Reset every lexicon entry's distributional embedding. zero_and_mark_uninit=True
        zeros and flags as uninitialized (clean reset; vectors re-learn from next
        comprehend). False re-randomizes uniformly in [-jitter, +jitter] keeping
        initialized=True. Returns the number of lexicon entries touched. Use when
        comprehend(p_i) cosine ≈ 1.0 across distinct prompts."""
        resp = self._send({
            "cmd": "reset_lexicon_distributional",
            "zero_and_mark_uninit": bool(zero_and_mark_uninit),
            "jitter": float(jitter),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return int(resp.get("touched", 0))

    def reset_concept_grounding(self):
        """Reset concept grounding for a clean re-grounding pass: clears all
        lexicon concept bindings AND wipes the semantic_memory concept store
        (frees capacity). Returns the number of bindings cleared. Use after the
        grounding-feature fix to recover from concept collapse (comprehend
        cosine ≈ 1.0)."""
        resp = self._send({"cmd": "reset_concept_grounding"})
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return int(resp.get("bindings_cleared", 0))

    def classify_toxicity(self, text):
        """Classify text via the toxicity classifier. Returns a dict
        {predicted_harm, fairness_violation, anti_toxic_signal, would_block,
        category}, or None if the classifier is not attached."""
        resp = self._send({"cmd": "classify_toxicity", "text": str(text)})
        if resp.get("error"):
            return None
        return resp.get("result")

    def reset_lang_bridge_weights(self, w_min=0.001, w_max=0.05):
        """Break rank-1 collapse by re-randomizing every bridge binding
        weight to uniform(w_min, w_max). Returns the number of bindings
        reset. Use after diagnosing comprehend cosines ≈ 1.0 across
        distinct prompts. The vocab structure (which bindings exist)
        is preserved; only weights change."""
        resp = self._send({
            "cmd": "reset_lang_bridge_weights",
            "w_min": float(w_min),
            "w_max": float(w_max),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return int(resp.get("reset_count", 0))

    def set_ltd_margin(self, margin):
        """Set the SNN bridge's next-token LTD margin gate. LTD only fires
        when topK[0].confidence >= margin * topK[target_rank].confidence
        AND target appears in topK. margin=1.0 reproduces the legacy
        unconditional rule (collapsed step-3900); 1.5 is the calibrated
        default; >=10 effectively disables LTD. Clamped server-side to
        [1.0, 100.0]; out-of-range raises RuntimeError via the daemon
        error path. Runtime-only; not persisted across saves."""
        resp = self._send({
            "cmd": "set_ltd_margin",
            "margin": float(margin),
        })
        if resp.get("error"):
            raise RuntimeError(resp["error"])
        return float(resp.get("margin", margin))

    def ground_word(self, word, features, modality=5, attention=0.7,
                     valence=0.0, arousal=0.0):
        """Ground a word in sensory features. Returns True on success."""
        resp = self._send({
            "cmd": "ground_word",
            "word": word,
            "features": _to_list(features),
            "modality": int(modality),
            "attention": float(attention),
            "valence": float(valence),
            "arousal": float(arousal),
        })
        if resp.get("error"):
            return False
        return bool(resp.get("ok", False))

    def set_grounding_emotion(self, valence, arousal):
        """Set the default valence/arousal applied to subsequent ground_word
        calls when the caller does not pass explicit values."""
        resp = self._send({
            "cmd": "set_grounding_emotion",
            "valence": float(valence),
            "arousal": float(arousal),
        })
        return bool(resp.get("ok", False))

    def learn_language_pair(self, text, target_text, learning_rate=0.05):
        """Train a single input→target text pair via grounded_language."""
        resp = self._send({
            "cmd": "learn_language_pair",
            "text": text,
            "target_text": target_text,
            "learning_rate": float(learning_rate),
        })
        return resp

    # -- LNN --

    def lnn_forward_step(self, features):
        resp = self._send({"cmd": "lnn_forward_step",
                           "features": _to_list(features)})
        return resp.get("result")

    def lnn_get_state(self):
        resp = self._send({"cmd": "lnn_get_state"})
        return resp.get("state")

    # -- Checkpoint --

    def save(self, path):
        self._send({"cmd": "save", "path": path})

    # -- Monitoring --

    def get_stats(self):
        resp = self._send({"cmd": "stats"})
        return resp.get("stats", {})

    def get_accuracy(self):
        resp = self._send({"cmd": "get_accuracy"})
        return resp.get("accuracy", 0.0)

    def get_last_gradient_norm(self):
        resp = self._send({"cmd": "get_last_gradient_norm"})
        return resp.get("gradient_norm", 0.0)

    def get_neuron_count(self):
        resp = self._send({"cmd": "get_neuron_count"})
        return resp.get("neuron_count", 0)

    def get_transcript(self):
        resp = self._send({"cmd": "get_transcript"})
        return resp.get("transcript")

    def get_cognitive_stats(self):
        resp = self._send({"cmd": "get_cognitive_stats"})
        return resp.get("stats", {})

    def probe(self):
        resp = self._send({"cmd": "probe"})
        return resp.get("probe", {})

    def repair_nan_weights(self):
        """Zero out NaN/Inf weights in the adaptive network."""
        resp = self._send({"cmd": "repair_nan_weights"})
        return resp

    def get_network_metrics(self):
        resp = self._send({"cmd": "get_network_metrics"})
        return resp.get("metrics", {})

    def get_cortex_cnn_metrics(self):
        resp = self._send({"cmd": "get_cortex_cnn_metrics"})
        return resp.get("metrics", {})

    # -- Phi-3 Language Cortex --

    def phi3_generate(self, text, max_tokens=256):
        """Generate a response using Phi-3 conditioned on brain state."""
        resp = self._send({"cmd": "phi3_generate", "text": text,
                           "max_tokens": max_tokens})
        return resp

    # -- Identity --

    def get_identity(self):
        """Get Athena's current identity state (personality, voice, accent)."""
        return self._send({"cmd": "get_identity"})

    # -- TTS --

    def tts_speak(self, text, accent=None, output_path=None):
        """Synthesize speech with brain-state modulation."""
        resp = self._send({"cmd": "tts_speak", "text": text,
                           "accent": accent, "output_path": output_path})
        return resp

    def tts_register_accent(self, name, audio_path):
        """Register a new accent from reference audio."""
        return self._send({"cmd": "tts_register_accent", "name": name,
                           "audio_path": audio_path})

    def tts_list_accents(self):
        """List available accents."""
        return self._send({"cmd": "tts_list_accents"})

    # -- Biological state --

    def substrate_get_health(self):
        resp = self._send({"cmd": "substrate_get_health"})
        return resp.get("health", "UNKNOWN")

    def substrate_get_metabolic(self):
        resp = self._send({"cmd": "substrate_get_metabolic"})
        return resp.get("metabolic", {})

    def medulla_get_arousal(self):
        resp = self._send({"cmd": "medulla_get_arousal"})
        return resp.get("arousal", 0.5)

    def medulla_get_circadian_efficiency(self):
        resp = self._send({"cmd": "medulla_get_circadian_efficiency"})
        return resp.get("efficiency", 1.0)

    def sleep_get_pressure(self):
        resp = self._send({"cmd": "sleep_get_pressure"})
        return resp.get("pressure", 0.0)

    def sleep_get_state(self):
        resp = self._send({"cmd": "sleep_get_state"})
        return resp.get("state", 0)

    def sleep_is_needed(self):
        resp = self._send({"cmd": "sleep_is_needed"})
        return resp.get("needed", False)

    def sleep_run_cycle(self, duration=2):
        self._send({"cmd": "sleep_run_cycle", "duration": duration})

    def update_medulla(self, dt=0.1):
        self._send({"cmd": "update_medulla", "dt": dt})

    def bg_get_dopamine(self):
        resp = self._send({"cmd": "bg_get_dopamine"})
        return resp.get("dopamine", 0.0)

    def bg_get_rpe(self):
        resp = self._send({"cmd": "bg_get_rpe"})
        return resp.get("rpe", 0.0)

    def bg_get_conflict(self):
        resp = self._send({"cmd": "bg_get_conflict"})
        return resp.get("conflict", 0.0)

    def bg_get_mode(self):
        resp = self._send({"cmd": "bg_get_mode"})
        return resp.get("mode", 0)

    def bg_update_reward(self, reward, rpe=0.0):
        self._send_fire_and_forget({"cmd": "bg_update_reward", "reward": reward, "rpe": rpe})

    def apply_reward_learning(self, reward):
        """Slice C: drive reward-modulated (three-factor STDP) plasticity.
        Distinct from set_last_external_reward — this actually propagates the
        reward through the DA machinery (negative reward → anti-Hebbian LTD)."""
        self._send_fire_and_forget({"cmd": "apply_reward_learning", "reward": reward})

    def set_last_external_reward(self, reward):
        """Slice D: stamp the most-recent external reward so the cascade
        self-train gate sees a FRESH reward (freshness TTL + threshold)."""
        self._send_fire_and_forget({"cmd": "set_last_external_reward", "reward": reward})

    # -- Training config --

    def set_plasticity_state(self, state):
        self._send({"cmd": "set_plasticity_state", "state": state})

    def set_task_type(self, task_type):
        self._send({"cmd": "set_task_type", "task_type": task_type})

    def set_fast_training(self, enabled):
        self._send({"cmd": "set_fast_training", "enabled": enabled})

    def reinit_weights(self):
        self._send({"cmd": "reinit_weights"})

    def enable_biological_plasticity(self, enabled):
        self._send({"cmd": "enable_biological_plasticity", "enabled": enabled})

    def consolidate(self, mode="auto"):
        self._send({"cmd": "consolidate", "mode": mode})

    def cerebellum_process_error(self, error):
        self._send({"cmd": "cerebellum_process_error", "error": error})

    # -- UTM --

    def utm_get_training_health(self):
        resp = self._send({"cmd": "utm_get_training_health"})
        return resp.get("health", {})

    def utm_forward_only(self, features):
        resp = self._send({"cmd": "utm_forward_only",
                           "features": _to_list(features)})
        return resp.get("result")

    # -- Experience --

    def experience(self, modality, data, confidence=None):
        req = {"cmd": "experience", "modality": modality, "data": data}
        if confidence is not None:
            req["confidence"] = confidence
        self._send(req)

    # -- Sensory cortex --

    def audio_cortex_process(self, samples):
        resp = self._send({"cmd": "audio_cortex_process",
                           "samples": _to_list(samples)})
        return resp.get("result")

    def visual_cortex_process(self, pixels, width, height, channels=3):
        resp = self._send({"cmd": "visual_cortex_process",
                           "pixels": _to_list(pixels),
                           "width": width, "height": height,
                           "channels": channels})
        return resp.get("result")

    def speech_cortex_process(self, samples):
        resp = self._send({"cmd": "speech_cortex_process",
                           "samples": _to_list(samples)})
        return resp.get("result")

    # -- Sensory input --

    def submit_sensory(self, modality, data, **kwargs):
        req = {"cmd": "submit_sensory", "modality": modality,
               "data": _to_list(data)}
        req.update(kwargs)
        # Symmetric logging across all modalities — prior code only logged
        # visual which created the illusion that audio/speech/somato streams
        # were not running. Modality → tag mapping keeps log volume sane.
        tag = {
            "visual":         "VISUAL-CLIENT",
            "audio":          "AUDIO-CLIENT",
            "speech":         "SPEECH-CLIENT",
            "somatosensory":  "SOMATO-CLIENT",
            "somato":         "SOMATO-CLIENT",
        }.get(modality, f"{modality.upper()}-CLIENT")
        import sys
        print(f"  [{tag}] sending {len(req['data'])} elements",
              file=sys.stderr, flush=True)
        # Visual remains blocking (its data is large + needs sync confirmation
        # for the visual cortex staging). Non-visual stays fire-and-forget
        # so audio/speech/somato don't stall the training loop waiting for
        # per-frame ACKs.
        if modality == "visual":
            resp = self._send(req)
            print(f"  [{tag}] response: {resp}",
                  file=sys.stderr, flush=True)
        else:
            self._send_fire_and_forget(req)

    # -- Arousal control --

    def medulla_boost_arousal(self, amount=0.1):
        self._send({"cmd": "medulla_boost_arousal", "amount": amount})

    def medulla_reduce_arousal(self, amount=0.1):
        self._send({"cmd": "medulla_reduce_arousal", "amount": amount})

    # -- Reward / novelty --

    def edp_process_reward(self, reward):
        self._send({"cmd": "edp_process_reward", "reward": reward})

    def edp_process_novelty(self, novelty):
        self._send({"cmd": "edp_process_novelty", "novelty": novelty})

    # -- Language / cognitive training --

    def train_cognitive(self, **kwargs):
        req = {"cmd": "train_cognitive"}
        req.update(kwargs)
        resp = self._send(req)
        return resp.get("result")

    def train_language(self, text, target_text=None):
        self._send({"cmd": "train_language", "text": text,
                     "target_text": target_text or text})

    def learn_language(self, text):
        self._send({"cmd": "learn_language", "text": text})

    # -- Reasoning --

    def ti_init_reasoning(self):
        self._send({"cmd": "ti_init_reasoning"})

    def ti_add_fact(self, fact, confidence=0.5):
        self._send_fire_and_forget({"cmd": "ti_add_fact", "fact": fact,
                     "confidence": confidence})

    def ti_add_rule(self, rule, confidence=0.5):
        self._send({"cmd": "ti_add_rule", "rule": rule,
                     "confidence": confidence})

    def ti_forward_chain(self):
        resp = self._send({"cmd": "ti_forward_chain"})
        return resp.get("result")

    # -- Brain config --

    def enable_multi_network(self):
        self._send({"cmd": "enable_multi_network"})

    def init_cortex_cnns(self):
        self._send({"cmd": "init_cortex_cnns"})

    def enable_world_model(self, enabled=True):
        self._send({"cmd": "enable_world_model", "enabled": enabled})

    def enable_world_model_bridge(self, enabled=True):
        self._send({"cmd": "enable_world_model_bridge", "enabled": enabled})

    def set_training_dashboard(self, **kwargs):
        req = {"cmd": "set_training_dashboard"}
        req.update(kwargs)
        self._send(req)

    def get_training_dashboard(self):
        resp = self._send({"cmd": "get_training_dashboard"})
        return resp.get("dashboard", {})

    def attach_builtin_probes(self, interval_ms=1000):
        resp = self._send({"cmd": "attach_builtin_probes", "interval_ms": interval_ms})
        return resp.get("count", 0) if isinstance(resp, dict) else 0

    def get_all_probe_metrics(self):
        resp = self._send({"cmd": "get_probe_metrics"})
        return resp.get("probe_metrics", {}) if isinstance(resp, dict) else {}

    def enable_mixed_precision(self, enabled=True):
        self._send({"cmd": "enable_mixed_precision", "enabled": enabled})

    def enable_gradient_checkpointing(self, enabled=True, interval=None):
        req = {"cmd": "enable_gradient_checkpointing", "enabled": enabled}
        if interval is not None:
            req["interval"] = interval
        self._send(req)

    # -- LNN / SNN / CNN --

    def lnn_create(self, *args, **kwargs):
        req = {"cmd": "lnn_create"}
        if args:
            req["args"] = list(args)
        req.update(kwargs)
        self._send(req)

    def lnn_get_stats(self):
        resp = self._send({"cmd": "lnn_get_stats"})
        return resp.get("stats", {})

    def snn_get_stats(self):
        resp = self._send({"cmd": "snn_get_stats"})
        return resp.get("stats", {})

    def cnn_get_stats(self):
        resp = self._send({"cmd": "cnn_get_stats"})
        return resp.get("stats", {})

    # -- Plasticity / pruning --

    def get_plasticity_stats(self):
        resp = self._send({"cmd": "get_plasticity_stats"})
        return resp.get("stats", {})

    def prune_synapses(self, threshold=0.01):
        self._send({"cmd": "prune_synapses", "threshold": threshold})

    # -- Curiosity --

    def curiosity_detect_gaps(self, domain=None):
        req = {"cmd": "curiosity_detect_gaps"}
        if domain is not None:
            req["domain"] = domain
        resp = self._send(req)
        return resp.get("result")

    # -- UTM EMA --

    def utm_swap_to_ema(self):
        self._send({"cmd": "utm_swap_to_ema"})

    def utm_swap_from_ema(self):
        self._send({"cmd": "utm_swap_from_ema"})

    # -- Language / interactive --

    def comprehend(self, text):
        resp = self._send({"cmd": "comprehend", "text": text})
        return resp.get("result", {})

    def get_top_phrases(self, top_k: int = 20) -> list:
        """Return the top-K most-frequent learned phrases as a list of dicts.

        Each dict carries {"form": str, "frequency": int, "component_words": int}.
        Returns an empty list if grounded_language is not yet initialised on the
        daemon brain or if the daemon is on an older nimcp.so without the wrapper.
        """
        resp = self._send({"cmd": "get_top_phrases", "top_k": int(top_k)})
        return resp.get("phrases", [])

    def get_modality_counts(self) -> dict:
        """Return per-modality binding counts as a dict.

        Keys: "visual", "auditory", "motor", "emotional", "spatial", "linguistic".
        Values: number of bindings across the lexicon with non-zero
        modality_strength in that channel. Returns an empty dict if
        grounded_language is not initialised or the daemon's nimcp.so is older
        than this RPC. Curriculum tests use this to verify modality coverage.
        """
        resp = self._send({"cmd": "get_modality_counts"})
        return resp.get("counts", {})

    def generate(self, prompt=None, semantic_input=None):
        req = {"cmd": "generate"}
        if prompt is not None:
            req["prompt"] = prompt
        if semantic_input is not None:
            req["semantic_input"] = _to_list(semantic_input)
        resp = self._send(req)
        return resp.get("result", {})

    def produce_text(self, intent):
        resp = self._send({"cmd": "produce_text", "intent": _to_list(intent)})
        return resp.get("result", {})

    def deliberate(self, topic):
        resp = self._send({"cmd": "deliberate", "topic": topic})
        return resp.get("result", {})

    def self_assess(self, domain):
        resp = self._send({"cmd": "self_assess", "domain": domain})
        return resp.get("result", {})

    def curiosity_detect_gaps(self, topic):
        resp = self._send({"cmd": "curiosity_detect_gaps", "topic": topic})
        return resp.get("result", {})

    def rubric(self):
        resp = self._send({"cmd": "rubric"})
        return resp.get("result", {})

    def get_last_gradient_norm(self):
        resp = self._send({"cmd": "get_last_gradient_norm"})
        return resp.get("result", 0.0)

    def focus_attention(self, modality):
        self._send({"cmd": "focus_attention", "modality": modality})

    # -- Cognitive & Safety Test Battery wrappers --

    def get_mental_health_report(self):
        resp = self._send({"cmd": "get_mental_health_report"})
        return resp.get("report", {})

    def get_mental_health_check(self, disorder):
        resp = self._send({"cmd": "get_mental_health_check", "disorder": disorder})
        return resp.get("score", 0.0)

    def get_emotion_state(self):
        resp = self._send({"cmd": "get_emotion_state"})
        return resp.get("emotion", {})

    def get_internal_state(self, strategy=1):
        resp = self._send({"cmd": "get_internal_state", "strategy": strategy})
        return resp.get("state", {})

    def predict_with_confidence(self, features):
        resp = self._send({"cmd": "predict_with_confidence", "features": _to_list(features)})
        return resp.get("result", {})

    def predict_with_deadline(self, features, deadline_ms=100.0):
        resp = self._send({"cmd": "predict_with_deadline",
                           "features": _to_list(features),
                           "deadline_ms": deadline_ms})
        return resp.get("result", {})

    def perturb_weights(self, magnitude=0.01, target="global", tag="mark_test"):
        resp = self._send({"cmd": "perturb_weights",
                           "magnitude": magnitude, "target": target, "tag": tag})
        return resp.get("result", {})

    def enter_idle_with_telemetry(self, duration_ms=2000):
        resp = self._send({"cmd": "enter_idle_with_telemetry",
                           "duration_ms": duration_ms})
        return resp.get("result", {})

    def get_inner_speech_trace(self, n=10):
        resp = self._send({"cmd": "get_inner_speech_trace", "n": n})
        return resp.get("trace", [])

    def get_hypothesis_log(self, n=10):
        resp = self._send({"cmd": "get_hypothesis_log", "n": n})
        return resp.get("log", [])

    def cow_trial_snapshot(self):
        resp = self._send({"cmd": "cow_trial_snapshot"})
        return resp.get("handle")

    def cow_trial_restore(self, handle):
        resp = self._send({"cmd": "cow_trial_restore", "handle": handle})
        return resp.get("ok", False)

    def _call(self, cmd, **kwargs):
        """Generic RPC invocation — used by the test harness for optional APIs."""
        payload = {"cmd": cmd}
        payload.update(kwargs)
        resp = self._send(payload)
        for key in ("result", "report", "state", "emotion", "trace", "log",
                    "score", "handle", "metrics"):
            if key in resp:
                return resp[key]
        return resp


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _to_list(obj):
    """Convert numpy arrays, bytes, and other iterables to plain lists."""
    if obj is None:
        return None
    if isinstance(obj, (bytes, bytearray)):
        return list(obj)  # bytes → list of ints [0-255]
    if hasattr(obj, "tolist"):
        return obj.tolist()
    if isinstance(obj, (list, tuple)):
        return list(obj)
    return obj


def _json_default(obj):
    """Handle numpy types."""
    try:
        import numpy as np
        if isinstance(obj, (np.integer,)):
            return int(obj)
        if isinstance(obj, (np.floating,)):
            return float(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
    except ImportError:
        pass
    return str(obj)
