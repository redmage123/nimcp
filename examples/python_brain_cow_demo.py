#!/usr/bin/env python3
"""
NIMCP Brain Copy-on-Write (COW) Cloning Demo

This example demonstrates the efficient COW brain cloning feature which
provides 86% memory savings for inference-only clones.

Key Benefits:
- Clone time: <10ms (vs ~1000ms for full copy)
- Memory overhead: ~1MB (vs ~50MB for full copy)
- Memory savings: 86% for read-only inference

Use Cases:
1. Parallel inference on multiple inputs
2. Creating checkpoints before training
3. A/B testing different training strategies
"""

import nimcp
import random

def main():
    print("=" * 70)
    print("NIMCP Brain COW Cloning Demo")
    print("=" * 70)

    # Step 1: Create original brain
    print("\n1. Creating original brain...")
    original = nimcp.Brain(
        name="classifier",
        size=1,      # SMALL (1K neurons, ~10MB)
        task=0,      # CLASSIFICATION
        inputs=10,
        outputs=3
    )
    print("   Brain created successfully")

    # Step 2: Train the original brain with some examples
    print("\n2. Training original brain...")
    training_data = [
        ([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0], "class_A"),
        ([0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.0], "class_B"),
        ([0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5], "class_C"),
    ]

    for features, label in training_data:
        original.learn(features, label, confidence=1.0)
        print(f"   Learned: {label}")

    # Step 3: Probe original brain statistics
    print("\n3. Original brain statistics:")
    stats = original.probe()
    print(f"   Task: {stats['task_name']}")
    print(f"   Neurons: {stats['num_neurons']:,}")
    print(f"   Synapses: {stats['num_synapses']:,}")
    print(f"   Memory: {stats['memory_bytes'] / 1024 / 1024:.2f} MB")
    print(f"   Learning steps: {stats['total_learning_steps']}")

    # Step 4: Create COW clone
    print("\n4. Creating COW clone...")
    clone = original.clone_cow()
    print("   Clone created (shares network with original)")

    # Step 5: Probe clone statistics to show memory sharing
    print("\n5. Clone statistics (COW sharing):")
    clone_stats = clone.probe()
    print(f"   Is COW clone: {clone_stats['is_cow_clone']}")
    print(f"   Shared memory: {clone_stats['cow_shared_bytes'] / 1024 / 1024:.2f} MB")
    print(f"   Private memory: {clone_stats['cow_private_bytes'] / 1024:.2f} KB")
    print(f"   Memory savings: {(clone_stats['cow_shared_bytes'] / stats['memory_bytes'] * 100):.1f}%")

    # Step 6: Use clone for inference (read-only operations work)
    print("\n6. Running inference on clone...")
    test_features = [0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 0.1]
    label, confidence = clone.decide(test_features)
    print(f"   Prediction: {label} (confidence: {confidence:.3f})")

    # Step 7: Create multiple clones for parallel inference
    print("\n7. Creating multiple clones for parallel inference...")
    clones = [original.clone_cow() for _ in range(5)]
    print(f"   Created {len(clones)} clones")

    # Simulate parallel inference
    print("\n   Running parallel inference:")
    for i, brain_clone in enumerate(clones):
        # Generate random test features
        test = [random.random() for _ in range(10)]
        label, conf = brain_clone.decide(test)
        print(f"   Clone {i}: {label} (conf: {conf:.3f})")

    # Step 8: Show total memory efficiency
    print("\n8. Memory efficiency analysis:")
    print(f"   Original brain memory: {stats['memory_bytes'] / 1024 / 1024:.2f} MB")
    print(f"   Total clones: {len(clones) + 1}")
    print(f"   Without COW (estimated): {(stats['memory_bytes'] * (len(clones) + 2)) / 1024 / 1024:.2f} MB")

    # Estimate COW savings (assuming ~86% sharing)
    cow_shared = clone_stats['cow_shared_bytes']
    cow_private = clone_stats['cow_private_bytes']
    total_cow_memory = stats['memory_bytes'] + (cow_private * (len(clones) + 1))
    print(f"   With COW (estimated): {total_cow_memory / 1024 / 1024:.2f} MB")
    print(f"   Total savings: {((1 - (total_cow_memory / (stats['memory_bytes'] * (len(clones) + 2)))) * 100):.1f}%")

    print("\n" + "=" * 70)
    print("Demo completed successfully!")
    print("=" * 70)

if __name__ == "__main__":
    main()
