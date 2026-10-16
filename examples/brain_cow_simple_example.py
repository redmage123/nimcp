#!/usr/bin/env python3
"""
Simple Brain COW Clone Example

This matches the example requested in the task specification.
Shows basic usage of clone_cow() for memory-efficient brain cloning.
"""

import sys
sys.path.insert(0, '/home/bbrelin/nimcp/build/lib/python')

import nimcp

# Create original brain
original = nimcp.Brain("model", size=1, task=0, inputs=10, outputs=3)

# Train the brain
features = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0]
original.learn(features, "class_A", confidence=1.0)

# Create COW clone - shares network, 86% memory saved
clone = original.clone_cow()

# Read-only inference works on clone
result = clone.decide(features)
print(f"Prediction: {result[0]}, Confidence: {result[1]:.3f}")

# Show memory savings
stats = clone.probe()
print(f"\nCOW Statistics:")
print(f"  Is COW clone: {stats['is_cow_clone']}")
print(f"  Shared memory: {stats['cow_shared_bytes'] / 1024:.2f} KB")
print(f"  Private memory: {stats['cow_private_bytes'] / 1024:.2f} KB")
savings = (stats['cow_shared_bytes'] / (stats['cow_shared_bytes'] + stats['cow_private_bytes'])) * 100
print(f"  Memory savings: {savings:.1f}%")
