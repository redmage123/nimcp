#!/usr/bin/env python3
"""Smoke test for scripts/stages_4_7.py with a mock brain and a stand-in for
the immerse_athena module. Exercises every episode generator directly, the
generic runner for each stage (gate disabled, small minimum), the stage-7
conversation inbox, and the probe sets.

Run: python3 tests/python/test_stages_4_7.py
"""
import json
import os
import random
import sys
import tempfile
import types

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
import stages_4_7 as S  # noqa: E402

failures = 0


def expect(cond, msg):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", msg)


class MockBrain:
    """Answers with a canned, task-aware response so scores are non-trivial."""
    def __init__(self):
        self.learned = []
        self.rewards = []

    def grounded_respond(self, text):
        t = text.lower()
        if "why" in t:
            return {"response": "because the sun is hot and heat melts it", "confidence": 0.7, "success": True}
        if "how many" in t:
            return {"response": "there are three of them", "confidence": 0.6, "success": True}
        if "where will" in t or "where does" in t:
            return {"response": "in the box", "confidence": 0.5, "success": True}
        if "spell" in t:
            return {"response": "c a t", "confidence": 0.5, "success": True}
        if "does not belong" in t:
            return {"response": "the car does not belong", "confidence": 0.5, "success": True}
        return {"response": "the fox went to the stream to drink water in the morning", "confidence": 0.4, "success": True}

    def decide_full(self, feats):
        return {"output_vector": [0.1] * 16}

    def learn_vector(self, f, t, label="", confidence=0.5, learning_rate=None):
        self.learned.append(label)
        return 0.9

    def learn_language(self, text):
        return 0

    def bg_update_reward(self, r, c):
        self.rewards.append(r)

    def curiosity_detect_gaps(self, topic):
        return {"novelty_score": 0.7, "suggested_topic": topic, "questions": [f"why is {topic} like that?"]}

    def set_plasticity_state(self, s):
        pass

    def prune_synapses(self, thr):
        return 0

    def ti_reason(self, q):
        return {"ok": True}

    def snn_get_stats(self):
        return {"mean_firing_rate": 2.0, "sparsity": 0.2, "hyperactive_neurons": 3}


class MockTrainer:
    def __init__(self, brain, batch_size=8):
        self.brain = brain
        self.n = 0

    def learn(self, features, target, label="", confidence=0.5, learning_rate=None):
        self.n += 1
        return 0.8

    def flush(self):
        pass


class MockLR:
    def __init__(self, **kw):
        self.lr = 1.0

    def get_lr(self):
        return self.lr

    def step(self):
        pass


class MockMastery:
    def __init__(self):
        self.m = {}

    def load(self, p):
        pass

    def save(self, p):
        pass

    def record(self, d, loss, conf):
        self.m[d] = conf

    def get_mastery(self, d):
        return self.m.get(d, 0.0)

    def select_domain(self, ds):
        return random.choice(ds)


class MockComposer:
    def compose(self, text=None, modality="text", **kw):
        return np.ones(8, dtype=np.float32)


class MockParent:
    teacher = None

    def __init__(self):
        self.critic_calls = 0

    def pre_generate_content(self, stage, num_stimuli):
        pass

    def observe_response(self, result):
        return "words"

    def run_critic_eval(self, brain, prompt, stage=None):
        self.critic_calls += 1
        return None


class MockClock:
    def tick(self, brain):
        return None

    def do_sleep(self, brain, parent):
        pass

    def do_consolidate(self, brain):
        pass


class MockSource:
    ADVANCED_TOPICS = {"science": [], "mathematics": []}

    def get_fact(self, preferred_domain=None):
        return ("Plants make their food from sunlight and water.", "science")

    def get_advanced(self, domain=None):
        return ("Energy from the sun drives the water cycle on Earth.", domain or "science")

    def get_question(self):
        return "What do you like?"


def make_ia(tmpdir):
    ia = types.SimpleNamespace()
    ia.TRAINING_PROGRESS = [0, 0]
    ia._checkpoint_pending = False
    ia.saved = []
    ia.MiniBatchTrainer = MockTrainer
    ia.CosineAnnealingLR = MockLR
    ia.MasteryTracker = MockMastery
    ia.MASTERY_FILE = os.path.join(tmpdir, "mastery.json")
    ia.CHECKPOINT_DIR = tmpdir
    ia.COGNITIVE_TRAIN_INTERVAL = 5
    ia.STAGE_GATE_ENABLED = False
    ia.STAGE_GATE_LOG = os.path.join(tmpdir, "gate.jsonl")
    ia._CHAT_LOG_FILE = os.path.join(tmpdir, "chat.jsonl")
    ia._CANONICAL_CFG = None
    ia._MATH_CFG = None
    ia._STREAMING_CFG = None
    ia._PROBE_DRIPS_ENABLED = False
    ia.submit_multimodal = lambda brain, text, target_word=None: ["V"]
    ia.make_semantic_target = lambda text: np.ones(8, dtype=np.float32)
    ia.encode_text = lambda text: np.array([hash(w) % 7 for w in text.lower().split()][:8] + [0] * 8, dtype=np.float32)[:8]
    ia._save_checkpoint = lambda brain, decoder, stage, step, wait=False: ia.saved.append((stage, step))
    ia._inject_cognitive_training = lambda *a, **k: 0.5
    ia._inject_idk_unanswerable = lambda *a, **k: None
    ia.evaluate_performance = lambda *a, **k: None
    ia.chat_eval = lambda *a, **k: None
    ia._print_utm_health = lambda b: None
    ia._print_bio_stats = lambda b: None
    return ia


with tempfile.TemporaryDirectory() as d:
    random.seed(7)
    brain, comp, parent, clock, src = MockBrain(), MockComposer(), MockParent(), MockClock(), MockSource()
    ia = make_ia(d)
    B = S._Brain(brain, ia, comp, None, parent)
    tr = MockTrainer(brain)

    # Every episode generator runs and returns a scored Episode
    gens = {
        "causal": lambda: S.ep_causal(B, tr, 4, 1.0),
        "asks": lambda: S.ep_athena_asks(B, tr, 4, 1.0, src),
        "narrative": lambda: S.ep_narrative(B, tr, 4, 1.0),
        "other_minds": lambda: S.ep_other_minds(B, tr, 4, 1.0),
        "number/category": lambda: S.ep_number_category(B, tr, 4, 1.0),
        "read": lambda: S.ep_read_aloud(B, tr, 5, 1.0, ia),
        "phonics": lambda: S.ep_phonics(B, tr, 5, 1.0),
        "write": lambda: S.ep_write(B, tr, 5, 1.0),
        "retell": lambda: S.ep_retell_passage(B, tr, 5, 1.0, ia),
        "vocabulary": lambda: S.ep_vocabulary(B, tr, 5, 1.0, src),
        "socratic": lambda: S.ep_socratic(B, tr, 6, 1.0, src, MockMastery(), ["science"]),
        "worksheet": lambda: S.ep_worksheet(B, tr, 6, 1.0, src, MockMastery(), ["science"]),
        "math/logic": lambda: S.ep_math_logic(B, tr, 6, 1.0),
        "explain_back": lambda: S.ep_explain_back(B, tr, 6, 1.0, src),
        "research": lambda: S.ep_research(B, tr, 6, 1.0, src, ia),
        "self_directed": lambda: S.ep_self_directed(B, tr, 7, 1.0, src, ia, MockMastery()),
        "teach_back": lambda: S.ep_teach_back(B, tr, 7, 1.0, src),
        "reflect": lambda: S.ep_reflect(B, tr, 7, 1.0),
    }
    for name, g in gens.items():
        try:
            ep = g()
            expect(isinstance(ep, S.Episode) and 0.0 <= ep.score <= 1.0, f"{name}: score in range ({ep.score if isinstance(ep, S.Episode) else ep})")
        except Exception as e:
            expect(False, f"{name} raised {e!r}")

    # Task scoring sanity with the canned brain
    expect(S.ep_causal(B, tr, 4, 1.0).details["connective"], "causal: connective detected")
    cats = [S.ep_number_category(B, tr, 4, 1.0) for _ in range(12)]
    expect(any(e.kind == "number" and e.score == 1.0 for e in cats) or any(e.kind == "number" for e in cats),
           "number episodes produced")

    # Stage 7 conversation inbox: consumed and transcribed
    os.makedirs(os.path.join(d, "checkpoints", "athena"), exist_ok=True)
    cwd = os.getcwd(); os.chdir(d)
    try:
        with open(S.INBOX_FILE, "w") as fh:
            fh.write(json.dumps({"user": "Hello Athena, what did you learn today?"}) + "\n")
        ep = S.ep_conversation(B, tr, 7, 1.0, ia)
        expect(ep is not None and ep.kind == "conversation", "conversation episode consumed inbox")
        expect(os.path.getsize(S.INBOX_FILE) == 0, "inbox drained")
        expect(os.path.exists(S.TRANSCRIPT_FILE), "transcript written")
        expect(S.ep_conversation(B, tr, 7, 1.0, ia) is None, "empty inbox -> None")
    finally:
        os.chdir(cwd)

    # Stage-4 video episode: None without a cache, scored with one
    class FakeBank:
        def __init__(self): self.q = []
        def get_sequence(self): return {"item": "f", "title": "Film F", "summary": "A film about a fox who learns to swim across a river.",
                                        "description": "a moving scene from the film 'f'",
                                        "frames": [bytes([i]) * 3072 for i in range(8)]}
        def get_film(self, n=4): return [dict(self.get_sequence(), start_s=float(k)) for k in range(n)]
        @staticmethod
        def sequence_text(seq): return seq.get("summary") or seq["description"]
        def queue_frames(self, fr): self.q = list(fr)
        def pop_frame(self): return (self.q.pop(0), 32, 32, 3, "video") if self.q else None
    ia_nov = types.SimpleNamespace(**vars(ia))
    ia_nov._get_real_sensory = lambda: None
    expect(S.ep_video_next(B, tr, 4, 1.0, ia_nov) is None, "video_next without cache -> None")
    ia_vid = types.SimpleNamespace(**vars(ia))
    ia_vid._get_real_sensory = lambda: FakeBank()
    epv = S.ep_video_next(B, tr, 4, 1.0, ia_vid)
    expect(epv is not None and epv.kind == "video_next" and 0.0 <= epv.score <= 1.0 and epv.turns == 6,
           f"video_next episode {epv}")
    for name, fn in (("video_describe", lambda: S.ep_video_describe(B, tr, 5, 1.0, ia_vid)),
                     ("video_explain", lambda: S.ep_video_explain(B, tr, 6, 1.0, ia_vid)),
                     ("watch_film", lambda: S.ep_watch_film(B, tr, 7, 1.0, ia_vid, MockMastery()))):
        e = fn()
        expect(e is not None and e.kind == name and 0.0 <= e.score <= 1.0 and e.turns >= 9, f"{name}: {e}")
        expect(fn.__call__ is not None and S.ep_video_describe(B, tr, 5, 1.0, ia_nov) is None, "video episodes None without cache")

    # Probe sets return floats in range
    for name, pf in (("p4", lambda: S.probe_stage4(B)), ("p5", lambda: S.probe_stage5(B, ia)),
                     ("p6", lambda: S.probe_stage6(B))):
        m = pf()
        expect(m and all(0.0 <= v <= 1.0 for v in m.values()), f"{name} metrics {m}")

    # Generic runner for each stage: gate disabled, small minimum, checkpoint cadence
    for st, run in ((4, S.run_stage_4), (5, S.run_stage_5), (6, S.run_stage_6)):
        ia.saved.clear()
        losses = run(ia, brain, comp, parent, clock, src, None, num_episodes=60, start_from=0)
        expect(len(losses) >= 60, f"stage {st}: {len(losses)} losses recorded")
        expect((st, 50) in ia.saved, f"stage {st}: checkpoint at 50 ({ia.saved})")
        expect(ia.TRAINING_PROGRESS[0] == st, f"stage {st}: progress stage set")
    # Stage 7 with a bounded count for the test (0 = unbounded in production)
    ia.saved.clear()
    losses = S.run_stage_7(ia, brain, comp, parent, clock, src, None, num_episodes=30, start_from=0,
                           max_episodes=30)
    expect(len(losses) >= 30, f"stage 7: {len(losses)} losses")
    expect(parent.critic_calls > 0, "critic consulted")
    expect(len(brain.rewards) > 0, "rewards delivered")

if failures:
    print(f"test_stages_4_7: {failures} failure(s)")
    sys.exit(1)
print("test_stages_4_7: all passed")
