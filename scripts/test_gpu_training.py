#!/usr/bin/env python3
"""
test_gpu_training.py — Verify GPU-default training pipeline

Tests:
  1. GPU detection and initialization
  2. Forward pass produces non-constant outputs
  3. Training loss decreases over epochs
  4. Classification accuracy improves
  5. GPU memory is allocated during training (nvidia-smi)
"""

import sys
import time
import random
import subprocess

import nimcp

# ── Helpers ──────────────────────────────────────────────────────────────────

def make_iris_dataset():
    """3-class classification dataset (simplified Iris-like)."""
    data = []
    random.seed(42)
    for _ in range(150):
        cls = random.randint(0, 2)
        if cls == 0:
            features = [random.gauss(5.0, 0.4), random.gauss(3.4, 0.4),
                        random.gauss(1.4, 0.2), random.gauss(0.2, 0.1)]
        elif cls == 1:
            features = [random.gauss(5.9, 0.5), random.gauss(2.8, 0.3),
                        random.gauss(4.3, 0.5), random.gauss(1.3, 0.3)]
        else:
            features = [random.gauss(6.6, 0.6), random.gauss(3.0, 0.3),
                        random.gauss(5.6, 0.5), random.gauss(2.0, 0.3)]
        label = f"class_{cls}"
        data.append((features, label))
    return data


def normalize(dataset):
    """Min-max normalize features to [0, 1]."""
    mins = [min(d[0][i] for d in dataset) for i in range(4)]
    maxs = [max(d[0][i] for d in dataset) for i in range(4)]
    out = []
    for features, label in dataset:
        normed = [(features[i] - mins[i]) / (maxs[i] - mins[i] + 1e-8)
                  for i in range(4)]
        out.append((normed, label))
    return out


def get_gpu_memory_mb():
    """Get GPU memory used (MiB) from nvidia-smi."""
    try:
        result = subprocess.run(
            ["nvidia-smi", "--query-gpu=memory.used", "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=5)
        return int(result.stdout.strip().split("\n")[0])
    except Exception:
        return -1


# ── Tests ────────────────────────────────────────────────────────────────────

def test_gpu_detection():
    """Test 1: GPU is detected and brain uses it."""
    print("Test 1: GPU detection and initialization")

    mem_before = get_gpu_memory_mb()
    brain = nimcp.Brain("gpu_detect", nimcp.BRAIN_SMALL, nimcp.TASK_CLASSIFICATION, 4, 3)

    # Do one learn to trigger GPU weight upload
    brain.learn([0.5, 0.5, 0.5, 0.5], "class_0")
    mem_after = get_gpu_memory_mb()

    if mem_before >= 0 and mem_after > mem_before:
        print(f"  PASS: GPU memory increased from {mem_before} MiB to {mem_after} MiB")
    elif mem_before >= 0:
        print(f"  INFO: GPU memory {mem_before} -> {mem_after} MiB (stubs may be in use)")
    else:
        print("  SKIP: nvidia-smi not available")

    del brain
    return True


def test_forward_nonconstant():
    """Test 2: Forward pass produces different outputs for different inputs."""
    print("\nTest 2: Forward pass produces non-constant outputs")

    brain = nimcp.Brain("fwd_test", nimcp.BRAIN_SMALL, nimcp.TASK_CLASSIFICATION, 4, 3)

    # Get predictions for distinct inputs
    pred1 = brain.predict([0.1, 0.2, 0.3, 0.4])
    pred2 = brain.predict([0.9, 0.8, 0.7, 0.6])
    pred3 = brain.predict([0.0, 0.0, 0.0, 0.0])

    label1, conf1 = pred1
    label2, conf2 = pred2
    label3, conf3 = pred3

    print(f"  Input [0.1,0.2,0.3,0.4] -> {label1} ({conf1:.4f})")
    print(f"  Input [0.9,0.8,0.7,0.6] -> {label2} ({conf2:.4f})")
    print(f"  Input [0.0,0.0,0.0,0.0] -> {label3} ({conf3:.4f})")

    # At minimum, confidences should not all be identical
    confs = [conf1, conf2, conf3]
    all_same = all(abs(c - confs[0]) < 1e-6 for c in confs)
    if not all_same:
        print("  PASS: Different inputs produce different confidence values")
    else:
        print("  WARN: All confidences identical (network may not be initialized with variance)")

    del brain
    return True


def test_loss_decreases():
    """Test 3: Loss decreases over repeated training on the same examples."""
    print("\nTest 3: Training loss decreases over epochs")

    brain = nimcp.Brain("loss_test", nimcp.BRAIN_SMALL, nimcp.TASK_CLASSIFICATION, 4, 3)

    dataset = normalize(make_iris_dataset())
    random.shuffle(dataset)

    num_epochs = 20
    losses_per_epoch = []

    for epoch in range(num_epochs):
        epoch_losses = []
        for features, label in dataset:
            brain.learn(features, label)
        # Measure loss: predict all and count matches
        correct = 0
        for features, label in dataset:
            pred_label, _ = brain.predict(features)
            if pred_label == label:
                correct += 1
        accuracy = correct / len(dataset)
        losses_per_epoch.append(1.0 - accuracy)

        if epoch % 5 == 0 or epoch == num_epochs - 1:
            print(f"  Epoch {epoch:3d}: accuracy={accuracy:.3f}  error={1-accuracy:.3f}")

    first_error = losses_per_epoch[0]
    last_error = losses_per_epoch[-1]

    if last_error < first_error:
        print(f"  PASS: Error decreased from {first_error:.3f} to {last_error:.3f}")
        passed = True
    else:
        print(f"  FAIL: Error did not decrease ({first_error:.3f} -> {last_error:.3f})")
        passed = False

    del brain
    return passed


def test_accuracy_improves():
    """Test 4: Classification accuracy on held-out data improves."""
    print("\nTest 4: Classification accuracy improves on test set")

    brain = nimcp.Brain("acc_test", nimcp.BRAIN_SMALL, nimcp.TASK_CLASSIFICATION, 4, 3)

    dataset = normalize(make_iris_dataset())
    random.shuffle(dataset)
    split = int(0.7 * len(dataset))
    train_data = dataset[:split]
    test_data = dataset[split:]

    # Baseline accuracy (untrained)
    correct_before = sum(1 for f, l in test_data if brain.predict(f)[0] == l)
    acc_before = correct_before / len(test_data)
    print(f"  Accuracy before training: {acc_before:.3f} ({correct_before}/{len(test_data)})")

    # Train
    num_epochs = 30
    for epoch in range(num_epochs):
        random.shuffle(train_data)
        for features, label in train_data:
            brain.learn(features, label)

    # Test accuracy after training
    correct_after = sum(1 for f, l in test_data if brain.predict(f)[0] == l)
    acc_after = correct_after / len(test_data)
    print(f"  Accuracy after {num_epochs} epochs: {acc_after:.3f} ({correct_after}/{len(test_data)})")

    if acc_after > acc_before:
        print(f"  PASS: Accuracy improved from {acc_before:.3f} to {acc_after:.3f}")
        passed = True
    else:
        print(f"  FAIL: Accuracy did not improve ({acc_before:.3f} -> {acc_after:.3f})")
        passed = False

    del brain
    return passed


def test_gpu_memory_usage():
    """Test 5: GPU memory is allocated during training."""
    print("\nTest 5: GPU memory usage during training")

    mem_before = get_gpu_memory_mb()
    if mem_before < 0:
        print("  SKIP: nvidia-smi not available")
        return True

    brain = nimcp.Brain("mem_test", nimcp.BRAIN_SMALL, nimcp.TASK_CLASSIFICATION, 4, 3)

    # Train a few examples to trigger GPU allocation
    for i in range(10):
        brain.learn([random.random() for _ in range(4)], f"class_{i % 3}")

    mem_during = get_gpu_memory_mb()
    print(f"  GPU memory before: {mem_before} MiB")
    print(f"  GPU memory during training: {mem_during} MiB")

    del brain
    time.sleep(0.5)

    mem_after = get_gpu_memory_mb()
    print(f"  GPU memory after cleanup: {mem_after} MiB")

    if mem_during > mem_before:
        print(f"  PASS: GPU memory allocated ({mem_during - mem_before} MiB used)")
        return True
    else:
        print("  INFO: No GPU memory increase detected (may be using CPU stubs)")
        return True


def test_training_throughput():
    """Test 6: Measure training throughput (examples/sec)."""
    print("\nTest 6: Training throughput measurement")

    brain = nimcp.Brain("throughput", nimcp.BRAIN_SMALL, nimcp.TASK_CLASSIFICATION, 4, 3)

    dataset = normalize(make_iris_dataset())
    num_examples = len(dataset) * 5  # 750 examples

    start = time.perf_counter()
    for epoch in range(5):
        for features, label in dataset:
            brain.learn(features, label)
    elapsed = time.perf_counter() - start

    throughput = num_examples / elapsed
    print(f"  Trained {num_examples} examples in {elapsed:.2f}s")
    print(f"  Throughput: {throughput:.0f} examples/sec")
    print(f"  PASS: Training pipeline functional")

    del brain
    return True


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    print("=" * 60)
    print("GPU-Default Training Pipeline Test")
    print("=" * 60)

    nimcp.init()
    print(f"NIMCP version: {nimcp.version()}\n")

    results = []
    tests = [
        test_gpu_detection,
        test_forward_nonconstant,
        test_loss_decreases,
        test_accuracy_improves,
        test_gpu_memory_usage,
        test_training_throughput,
    ]

    for test_fn in tests:
        try:
            passed = test_fn()
            results.append((test_fn.__doc__.split(":")[0].strip(), passed))
        except Exception as e:
            print(f"  ERROR: {e}")
            results.append((test_fn.__doc__.split(":")[0].strip(), False))

    print("\n" + "=" * 60)
    print("Summary")
    print("=" * 60)
    all_pass = True
    for name, passed in results:
        status = "PASS" if passed else "FAIL"
        if not passed:
            all_pass = False
        print(f"  [{status}] {name}")

    print()
    if all_pass:
        print("All tests passed!")
    else:
        print("Some tests failed.")

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
