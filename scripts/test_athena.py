#!/usr/bin/env python3
"""
Athena Comprehensive Test Suite

5 testing methods:
  1. Automated regression — fixed inputs, track outputs over time
  2. Domain-specific accuracy — category discrimination tests
  3. Cross-modal binding — visual+audio co-occurrence strengthens associations
  4. Network health — SNN/LNN/HNN/FNO contributing meaningfully
  5. Interactive evaluation — conversation quality probes

Usage:
  python3 scripts/test_athena.py                 # Run all tests
  python3 scripts/test_athena.py --test regression
  python3 scripts/test_athena.py --test domain
  python3 scripts/test_athena.py --test crossmodal
  python3 scripts/test_athena.py --test health
  python3 scripts/test_athena.py --test interactive
  python3 scripts/test_athena.py --daemon        # Connect to running daemon
"""

import sys
import os
import json
import time
import argparse
import numpy as np
from datetime import datetime

sys.path.insert(0, os.path.dirname(__file__))

# ============================================================================
# Test infrastructure
# ============================================================================

class TestResult:
    def __init__(self, name, passed, score, details=""):
        self.name = name
        self.passed = passed
        self.score = score  # 0.0 to 1.0
        self.details = details

    def __str__(self):
        status = "PASS" if self.passed else "FAIL"
        return f"  [{status}] {self.name}: {self.score:.3f} — {self.details}"


class TestSuite:
    def __init__(self, brain, encoder=None):
        self.brain = brain
        self.encoder = encoder
        self.results = []
        self.history_file = "checkpoints/athena/test_history.jsonl"

    def add(self, result):
        self.results.append(result)
        print(result)

    def summary(self):
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        avg_score = np.mean([r.score for r in self.results]) if self.results else 0
        print(f"\n{'='*60}")
        print(f"  TEST SUMMARY: {passed}/{total} passed, avg_score={avg_score:.3f}")
        print(f"{'='*60}")
        return passed, total, avg_score

    def save_history(self):
        """Append results to history file for regression tracking."""
        os.makedirs(os.path.dirname(self.history_file), exist_ok=True)
        entry = {
            "timestamp": datetime.now().isoformat(),
            "tests": [
                {"name": r.name, "passed": r.passed, "score": r.score, "details": r.details}
                for r in self.results
            ],
            "total": len(self.results),
            "passed": sum(1 for r in self.results if r.passed),
            "avg_score": float(np.mean([r.score for r in self.results])) if self.results else 0,
        }
        with open(self.history_file, "a") as f:
            f.write(json.dumps(entry) + "\n")
        print(f"  Results saved to {self.history_file}")


def encode_text(text, encoder=None):
    """Encode text to feature vector using BERT or fallback."""
    if encoder:
        return encoder(text)
    # Simple hash-based encoding as fallback
    np.random.seed(hash(text) & 0xFFFFFFFF)
    return np.random.randn(1024).astype(np.float32).tolist()


# ============================================================================
# Test 1: Automated Regression Tests
# ============================================================================

def test_regression(suite):
    """Fixed inputs — check outputs are non-zero, diverse, and stable."""
    print("\n--- Test 1: Automated Regression ---")
    brain = suite.brain

    # Fixed probe stimuli that should produce distinct outputs
    probes = [
        ("warm red light",      "visual"),
        ("cold blue water",     "visual"),
        ("loud sharp thunder",  "auditory"),
        ("soft gentle whisper", "auditory"),
        ("rough sandpaper",     "tactile"),
    ]

    outputs = []
    norms = []
    for text, modality in probes:
        features = encode_text(text, suite.encoder)
        try:
            result = brain.decide_full(features)
            ov = np.array(result.get("output_vector", []))
            outputs.append(ov)
            norms.append(np.linalg.norm(ov))
        except Exception as e:
            outputs.append(np.zeros(10))
            norms.append(0.0)

    # Test 1a: Non-zero outputs
    nonzero_count = sum(1 for n in norms if n > 0.01)
    suite.add(TestResult(
        "regression/non_zero_outputs",
        nonzero_count == len(probes),
        nonzero_count / len(probes),
        f"{nonzero_count}/{len(probes)} probes produced non-zero output"
    ))

    # Test 1b: Output diversity (pairwise cosine dissimilarity)
    cos_sims = []
    for i in range(len(outputs)):
        for j in range(i + 1, len(outputs)):
            ni, nj = np.linalg.norm(outputs[i]), np.linalg.norm(outputs[j])
            if ni > 1e-8 and nj > 1e-8:
                cos = np.dot(outputs[i], outputs[j]) / (ni * nj)
                cos_sims.append(cos)
    avg_cos = np.mean(cos_sims) if cos_sims else 1.0
    diversity_score = max(0, 1.0 - avg_cos)  # Lower cos = more diverse
    suite.add(TestResult(
        "regression/output_diversity",
        avg_cos < 0.9,  # Outputs should NOT be nearly identical
        diversity_score,
        f"avg_cosine_sim={avg_cos:.4f} (lower is more diverse)"
    ))

    # Test 1c: Output stability (same input twice → similar output)
    features1 = encode_text("warm red light", suite.encoder)
    try:
        r1 = brain.decide_full(features1)
        r2 = brain.decide_full(features1)
        ov1 = np.array(r1.get("output_vector", []))
        ov2 = np.array(r2.get("output_vector", []))
        n1, n2 = np.linalg.norm(ov1), np.linalg.norm(ov2)
        if n1 > 1e-8 and n2 > 1e-8:
            stability = np.dot(ov1, ov2) / (n1 * n2)
        else:
            stability = 0.0
    except Exception:
        stability = 0.0
    suite.add(TestResult(
        "regression/output_stability",
        stability > 0.8,
        stability,
        f"same_input_cosine={stability:.4f} (higher is more stable)"
    ))

    # Test 1d: Gradient flow (learn produces non-zero gradient)
    try:
        features = encode_text("test gradient flow", suite.encoder)
        target = np.random.randn(2048).astype(np.float32).tolist()
        loss = brain.learn_vector(features, target, label="grad_test")
        grad_norm = brain.get_last_gradient_norm()
        has_gradient = grad_norm > 0.0 if isinstance(grad_norm, float) else False
    except Exception:
        loss = 0.0
        has_gradient = False
    suite.add(TestResult(
        "regression/gradient_flow",
        has_gradient,
        min(1.0, grad_norm / 10.0) if has_gradient else 0.0,
        f"grad_norm={grad_norm:.4f}" if has_gradient else "no gradient"
    ))


# ============================================================================
# Test 2: Domain-Specific Accuracy
# ============================================================================

def test_domain_accuracy(suite):
    """Test category discrimination — can Athena distinguish concepts?"""
    print("\n--- Test 2: Domain-Specific Accuracy ---")
    brain = suite.brain

    # Category pairs that should produce different outputs
    categories = {
        "animals_vs_objects": [
            (["cat", "dog", "bird", "fish", "horse"], "animal"),
            (["chair", "table", "lamp", "cup", "book"], "object"),
        ],
        "hot_vs_cold": [
            (["fire", "sun", "hot stove", "warm blanket", "boiling water"], "hot"),
            (["ice", "snow", "cold wind", "freezing water", "winter frost"], "cold"),
        ],
        "loud_vs_quiet": [
            (["thunder", "explosion", "shouting", "crash", "alarm"], "loud"),
            (["whisper", "silence", "gentle breeze", "soft hum", "tiptoe"], "quiet"),
        ],
    }

    for cat_name, (group_a, group_b) in categories.items():
        words_a, label_a = group_a
        words_b, label_b = group_b

        outputs_a = []
        outputs_b = []
        for word in words_a:
            features = encode_text(word, suite.encoder)
            try:
                r = brain.decide_full(features)
                outputs_a.append(np.array(r.get("output_vector", [])))
            except Exception:
                outputs_a.append(np.zeros(10))

        for word in words_b:
            features = encode_text(word, suite.encoder)
            try:
                r = brain.decide_full(features)
                outputs_b.append(np.array(r.get("output_vector", [])))
            except Exception:
                outputs_b.append(np.zeros(10))

        # Compute within-group similarity (should be HIGH)
        within_a = []
        for i in range(len(outputs_a)):
            for j in range(i + 1, len(outputs_a)):
                ni, nj = np.linalg.norm(outputs_a[i]), np.linalg.norm(outputs_a[j])
                if ni > 1e-8 and nj > 1e-8:
                    within_a.append(np.dot(outputs_a[i], outputs_a[j]) / (ni * nj))
        avg_within_a = np.mean(within_a) if within_a else 0.0

        # Compute between-group similarity (should be LOW)
        between = []
        for oa in outputs_a:
            for ob in outputs_b:
                na, nb = np.linalg.norm(oa), np.linalg.norm(ob)
                if na > 1e-8 and nb > 1e-8:
                    between.append(np.dot(oa, ob) / (na * nb))
        avg_between = np.mean(between) if between else 0.0

        # Score: within-group should be higher than between-group
        separation = avg_within_a - avg_between
        suite.add(TestResult(
            f"domain/{cat_name}",
            separation > 0.05,
            max(0, min(1.0, separation * 5)),
            f"within={avg_within_a:.4f} between={avg_between:.4f} separation={separation:.4f}"
        ))


# ============================================================================
# Test 3: Cross-Modal Binding
# ============================================================================

def test_crossmodal_binding(suite):
    """Test that multimodal co-occurrence creates stronger associations."""
    print("\n--- Test 3: Cross-Modal Binding ---")
    brain = suite.brain

    # Present paired stimuli (visual + audio for same concept)
    paired_concepts = [
        ("a dog barking loudly", ["visual", "audio", "speech"]),
        ("rain falling on a tin roof", ["visual", "audio"]),
        ("a baby laughing", ["audio", "speech"]),
    ]

    # Check if cortex CNNs exist (indicates multimodal processing)
    try:
        ccm = brain.get_cortex_cnn_metrics()
        active_cortices = list(ccm.keys())
    except Exception:
        active_cortices = []

    suite.add(TestResult(
        "crossmodal/cortex_count",
        len(active_cortices) >= 2,
        len(active_cortices) / 4.0,
        f"active cortices: {active_cortices if active_cortices else 'none'}"
    ))

    # Check if cortex embeddings differ by modality
    if len(active_cortices) >= 2:
        cortex_norms = {}
        for name, metrics in ccm.items():
            cortex_norms[name] = metrics.get("embedding_norm", 0.0)
        has_diverse_embeddings = len(set(round(n, 2) for n in cortex_norms.values())) > 1
        suite.add(TestResult(
            "crossmodal/diverse_embeddings",
            has_diverse_embeddings,
            1.0 if has_diverse_embeddings else 0.0,
            f"cortex norms: {cortex_norms}"
        ))
    else:
        suite.add(TestResult(
            "crossmodal/diverse_embeddings",
            False, 0.0,
            "insufficient cortex CNNs for comparison"
        ))

    # Test multimodal vs unimodal response strength
    try:
        # Multimodal stimulus (should trigger multiple cortices)
        features_multi = encode_text("a cat purring on a warm blanket in the sun", suite.encoder)
        r_multi = brain.decide_full(features_multi)
        norm_multi = np.linalg.norm(r_multi.get("output_vector", []))

        # Abstract stimulus (fewer sensory triggers)
        features_abstract = encode_text("the concept of infinity", suite.encoder)
        r_abstract = brain.decide_full(features_abstract)
        norm_abstract = np.linalg.norm(r_abstract.get("output_vector", []))

        # Multimodal should produce stronger (higher norm) response
        ratio = norm_multi / (norm_abstract + 1e-8)
        suite.add(TestResult(
            "crossmodal/multimodal_strength",
            ratio > 1.0,
            min(1.0, ratio / 2.0),
            f"multi_norm={norm_multi:.4f} abstract_norm={norm_abstract:.4f} ratio={ratio:.2f}"
        ))
    except Exception as e:
        suite.add(TestResult("crossmodal/multimodal_strength", False, 0.0, str(e)))


# ============================================================================
# Test 4: Network Health Tests
# ============================================================================

def test_network_health(suite):
    """Test that SNN/LNN/HNN/FNO are contributing meaningfully."""
    print("\n--- Test 4: Network Health ---")
    brain = suite.brain

    # 4a: SNN health
    try:
        snn = brain.snn_get_stats()
        if snn:
            spikes = snn.get("total_spikes", 0)
            rate = snn.get("mean_firing_rate", 0)
            sparsity = snn.get("sparsity", 0)
            silent = snn.get("silent_neurons", 0)
            hyper = snn.get("hyperactive_neurons", 0)

            # Healthy SNN: spikes > 0, rate 1-50Hz, sparsity 0.5-0.99, few hyperactive
            snn_healthy = spikes > 0 and 0.1 < rate < 50 and sparsity > 0.3
            snn_score = min(1.0, (
                (1.0 if spikes > 10 else spikes / 10.0) * 0.3 +
                (1.0 if 1.0 < rate < 30 else 0.5) * 0.3 +
                (1.0 if 0.5 < sparsity < 0.99 else 0.3) * 0.2 +
                (1.0 if hyper == 0 else 0.5) * 0.2
            ))
            suite.add(TestResult(
                "health/snn",
                snn_healthy, snn_score,
                f"spikes={spikes} rate={rate:.1f}Hz sparsity={sparsity:.2f} silent={silent} hyper={hyper}"
            ))
        else:
            suite.add(TestResult("health/snn", False, 0.0, "SNN not initialized"))
    except Exception as e:
        suite.add(TestResult("health/snn", False, 0.0, str(e)))

    # 4b: LNN health
    try:
        lnn = brain.lnn_get_stats()
        if lnn:
            fwd = lnn.get("forward_steps", 0)
            tau = lnn.get("avg_tau", 0)
            nan_count = lnn.get("nan_count", 0)
            inf_count = lnn.get("inf_count", 0)
            state_norm = lnn.get("state_norm", 0)

            lnn_healthy = fwd > 0 and nan_count == 0 and inf_count == 0 and state_norm > 0
            lnn_score = min(1.0, (
                (1.0 if fwd > 100 else fwd / 100.0) * 0.3 +
                (1.0 if 1.0 < tau < 20.0 else 0.3) * 0.2 +
                (1.0 if nan_count == 0 else 0.0) * 0.2 +
                (1.0 if inf_count == 0 else 0.0) * 0.15 +
                (1.0 if state_norm > 0.1 else 0.0) * 0.15
            ))
            suite.add(TestResult(
                "health/lnn",
                lnn_healthy, lnn_score,
                f"fwd={fwd} tau={tau:.2f} nans={nan_count} infs={inf_count} norm={state_norm:.2f}"
            ))
        else:
            suite.add(TestResult("health/lnn", False, 0.0, "LNN not initialized"))
    except Exception as e:
        suite.add(TestResult("health/lnn", False, 0.0, str(e)))

    # 4c: HNN health (Hamiltonian)
    try:
        nm = brain.get_network_metrics()
        hnn_active = nm.get("hnn_active", False)
        if hnn_active:
            energy = nm.get("hnn_energy", 0)
            deviation = nm.get("hnn_energy_deviation", 1.0)
            initial = nm.get("hnn_initial_energy", 0)

            # Good HNN: energy is finite, deviation < 50%
            hnn_healthy = abs(energy) > 1e-6 and deviation < 0.5
            hnn_score = max(0, 1.0 - deviation)
            suite.add(TestResult(
                "health/hnn",
                hnn_healthy, hnn_score,
                f"E={energy:.4f} deviation={deviation*100:.1f}% initial={initial:.4f}"
            ))
        else:
            suite.add(TestResult("health/hnn", False, 0.0, "HNN not active"))
    except Exception as e:
        suite.add(TestResult("health/hnn", False, 0.0, str(e)))

    # 4d: Per-network loss check
    try:
        nm = brain.get_network_metrics()
        for net_name in ["ann", "cnn", "snn", "lnn"]:
            loss_key = f"{net_name}_loss"
            steps_key = f"{net_name}_steps"
            loss = nm.get(loss_key, -1)
            steps = nm.get(steps_key, 0)
            active = steps > 0 and loss >= 0
            score = 1.0 if active else 0.0
            suite.add(TestResult(
                f"health/{net_name}_training",
                active, score,
                f"steps={steps} loss={loss:.4f}" if active else "not training"
            ))
    except Exception as e:
        suite.add(TestResult("health/network_training", False, 0.0, str(e)))

    # 4e: Cortex CNN health
    try:
        ccm = brain.get_cortex_cnn_metrics()
        for cortex_name in ["visual", "audio", "speech", "somato"]:
            if cortex_name in ccm:
                m = ccm[cortex_name]
                fwd = m.get("forward_steps", 0)
                bwd = m.get("backward_steps", 0)
                loss = m.get("ema_loss", -1)
                healthy = fwd > 0 and bwd > 0
                suite.add(TestResult(
                    f"health/cortex_{cortex_name}",
                    healthy, 1.0 if healthy else 0.0,
                    f"fwd={fwd} bwd={bwd} loss={loss:.4f}"
                ))
            else:
                suite.add(TestResult(
                    f"health/cortex_{cortex_name}",
                    False, 0.0, "not created"
                ))
    except Exception as e:
        suite.add(TestResult("health/cortex_cnns", False, 0.0, str(e)))

    # 4f: FNO check
    try:
        nm = brain.get_network_metrics()
        fno_audio = nm.get("fno_audio_steps", 0)
        fno_pop = nm.get("fno_pop_train_steps", 0)
        suite.add(TestResult(
            "health/fno_audio",
            fno_audio > 0, min(1.0, fno_audio / 10.0),
            f"steps={fno_audio}" if fno_audio > 0 else "not active"
        ))
        suite.add(TestResult(
            "health/fno_population",
            fno_pop > 0, min(1.0, fno_pop / 10.0),
            f"steps={fno_pop}" if fno_pop > 0 else "not active"
        ))
    except Exception as e:
        suite.add(TestResult("health/fno", False, 0.0, str(e)))

    # 4g: Biological subsystem health
    try:
        stats = brain.get_plasticity_stats()
        if stats:
            state = stats.get("plasticity_state", "UNKNOWN")
            dopamine = stats.get("dopamine", 0)
            rpe = stats.get("rpe", 0)
            bio_active = state != "UNKNOWN" and dopamine > 0
            suite.add(TestResult(
                "health/bio_plasticity",
                bio_active, 1.0 if bio_active else 0.0,
                f"state={state} DA={dopamine:.3f} RPE={rpe:.3f}"
            ))
        else:
            suite.add(TestResult("health/bio_plasticity", False, 0.0, "no plasticity stats"))
    except Exception as e:
        suite.add(TestResult("health/bio_plasticity", False, 0.0, str(e)))


# ============================================================================
# Test 5: Interactive Evaluation
# ============================================================================

def test_interactive(suite):
    """Test conversation-like probes — can Athena generate meaningful responses?"""
    print("\n--- Test 5: Interactive Evaluation ---")
    brain = suite.brain

    # 5a: Language comprehension — same meaning, different words → similar vectors
    try:
        r1 = brain.comprehend("the cat sat on the mat")
        r2 = brain.comprehend("a feline rested upon the rug")
        v1 = np.array(r1.get("semantic_vector", []))
        v2 = np.array(r2.get("semantic_vector", []))
        n1, n2 = np.linalg.norm(v1), np.linalg.norm(v2)
        if n1 > 1e-8 and n2 > 1e-8:
            sim = np.dot(v1, v2) / (n1 * n2)
        else:
            sim = 0.0
        suite.add(TestResult(
            "interactive/semantic_similarity",
            sim > 0.5,
            max(0, sim),
            f"cosine={sim:.4f} between synonym sentences"
        ))
    except Exception as e:
        suite.add(TestResult("interactive/semantic_similarity", False, 0.0, str(e)))

    # 5b: Text generation — produces non-empty output
    try:
        gen = brain.generate(prompt="hello")
        text = gen.get("text", "")
        conf = gen.get("confidence", 0)
        has_output = len(text.strip()) > 0
        suite.add(TestResult(
            "interactive/text_generation",
            has_output,
            conf if has_output else 0.0,
            f"'{text[:80]}' conf={conf:.3f}" if has_output else "empty output"
        ))
    except Exception as e:
        suite.add(TestResult("interactive/text_generation", False, 0.0, str(e)))

    # 5c: Deliberation — produces reasoning chain
    try:
        d = brain.deliberate("should you touch a hot stove?")
        has_reasoning = d.get("reasoning_confidence", 0) > 0 or d.get("total_turns", 0) > 0
        suite.add(TestResult(
            "interactive/deliberation",
            has_reasoning,
            d.get("reasoning_confidence", 0),
            f"conf={d.get('reasoning_confidence',0):.3f} turns={d.get('total_turns',0)}"
        ))
    except Exception as e:
        suite.add(TestResult("interactive/deliberation", False, 0.0, str(e)))

    # 5d: Self-assessment — brain can introspect
    try:
        sa = brain.self_assess("general")
        has_assessment = sa.get("assessment") == "active"
        suite.add(TestResult(
            "interactive/self_assessment",
            has_assessment,
            1.0 if has_assessment else 0.0,
            f"assessment={sa.get('assessment')} accuracy={sa.get('accuracy',0):.3f}"
        ))
    except Exception as e:
        suite.add(TestResult("interactive/self_assessment", False, 0.0, str(e)))

    # 5e: Curiosity — can detect knowledge gaps
    try:
        gaps = brain.curiosity_detect_gaps("quantum entanglement")
        has_curiosity = gaps.get("gap_size", 0) > 0
        suite.add(TestResult(
            "interactive/curiosity",
            has_curiosity,
            gaps.get("curiosity_intensity", 0),
            f"gap={gaps.get('gap_size',0):.3f} intensity={gaps.get('curiosity_intensity',0):.3f} questions={len(gaps.get('questions',[]))}"
        ))
    except Exception as e:
        suite.add(TestResult("interactive/curiosity", False, 0.0, str(e)))

    # 5f: Rubric quality
    try:
        features = encode_text("tell me about the sun", suite.encoder)
        brain.decide_full(features)
        rubric = brain.rubric()
        grade = rubric.get("grade", "?")
        overall = rubric.get("overall_score", 0)
        suite.add(TestResult(
            "interactive/rubric_quality",
            overall > 0.1,
            overall,
            f"grade={grade} score={overall:.3f}"
        ))
    except Exception as e:
        suite.add(TestResult("interactive/rubric_quality", False, 0.0, str(e)))


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="Athena Test Suite")
    parser.add_argument("--test", type=str, default="all",
                        choices=["all", "regression", "domain", "crossmodal", "health", "interactive"],
                        help="Which test to run (default: all)")
    parser.add_argument("--daemon", action="store_true",
                        help="Connect to running brain daemon")
    parser.add_argument("--checkpoint", type=str, default=None,
                        help="Load brain from checkpoint")
    args = parser.parse_args()

    # Connect to brain
    if args.daemon:
        from brain_client import BrainProxy
        brain = BrainProxy()
        print(f"Connected to daemon (neurons={brain.get_neuron_count():,})")
    elif args.checkpoint:
        import nimcp
        brain = nimcp.Brain.load(args.checkpoint)
        print(f"Loaded brain from {args.checkpoint}")
    else:
        # Default: try daemon first, fall back to small test brain
        try:
            from brain_client import BrainProxy
            brain = BrainProxy()
            print(f"Connected to daemon (neurons={brain.get_neuron_count():,})")
        except Exception:
            import nimcp
            brain = nimcp.Brain("test", neuron_count=1000, num_inputs=64,
                               num_outputs=64, init_mode="full")
            brain.enable_multi_network()
            print("Created small test brain (1K neurons)")

    # Try to set up BERT encoder
    encoder = None
    try:
        from sentence_transformers import SentenceTransformer
        model = SentenceTransformer("BAAI/bge-large-en-v1.5")
        def bert_encode(text):
            emb = model.encode(text, normalize_embeddings=True)
            return emb.tolist()
        encoder = bert_encode
        print("Using BERT encoder")
    except Exception:
        print("Using hash-based encoder (BERT not available)")

    suite = TestSuite(brain, encoder)
    print(f"\n{'='*60}")
    print(f"  ATHENA TEST SUITE — {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'='*60}")

    test_map = {
        "regression": test_regression,
        "domain": test_domain_accuracy,
        "crossmodal": test_crossmodal_binding,
        "health": test_network_health,
        "interactive": test_interactive,
    }

    if args.test == "all":
        for name, fn in test_map.items():
            try:
                fn(suite)
            except Exception as e:
                print(f"  [ERROR] {name} suite failed: {e}")
    else:
        test_map[args.test](suite)

    passed, total, avg = suite.summary()
    suite.save_history()

    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
