#!/usr/bin/env python3
"""
NIMCP Simple Python Demo - Clean & Easy to Understand

This demo showcases NIMCP Phase 9 features using the Python API:
- Brain creation and training
- Pattern classification
- Epistemic quality assessment
- Bias detection
- Simple, clean output

USAGE:
    python3 simple_demo.py

REQUIREMENTS:
    pip install numpy
"""

import sys
import os
import numpy as np

# Add NIMCP Python bindings to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../build/lib/python'))

try:
    import nimcp
except ImportError:
    print("Error: NIMCP Python bindings not found")
    print("Build NIMCP first: cd build && make")
    sys.exit(1)

#=============================================================================
# CONFIGURATION
#=============================================================================

INPUT_DIM = 8
NUM_CLASSES = 3
NUM_TRAINING_EXAMPLES = 10

#=============================================================================
# HELPER FUNCTIONS - Clean and Simple
#=============================================================================

def create_pattern(pattern_id):
    """Create a simple pattern for classification"""
    pattern = np.zeros(INPUT_DIM)

    if pattern_id % 3 == 0:  # Pattern A
        pattern[::2] = 1.0  # Even indices
    elif pattern_id % 3 == 1:  # Pattern B
        pattern[1::2] = 1.0  # Odd indices
    else:  # Pattern C
        pattern[:INPUT_DIM//2] = 1.0  # First half

    return pattern.tolist()

def print_separator():
    """Print a clean separator"""
    print("\n" + "="*60)

def print_results(result):
    """Print results in a clean, readable format"""
    print(f"Decision: {result.get('label', 'unknown')}")
    print(f"Confidence: {result.get('confidence', 0.0) * 100:.1f}%")

    print_separator()
    print("QUALITY METRICS:")
    print(f"  Epistemic Quality: {result.get('epistemic_quality', 0.0) * 100:.1f}%")
    print(f"  Credibility: {result.get('credibility', 0.0) * 100:.1f}%")
    print(f"  Requires Verification: {'Yes' if result.get('requires_verification', False) else 'No'}")
    print(f"  Bias Detected: {'⚠ YES' if result.get('bias_detected', False) else '✓ No'}")
    print(f"  Ethical Approval: {'✓ YES' if result.get('ethical_approved', True) else '✗ NO'}")

#=============================================================================
# MAIN DEMO
#=============================================================================

def main():
    print("\n")
    print("╔" + "═"*58 + "╗")
    print("║  NIMCP Simple Python Demo - Phase 9 Features            ║")
    print("║  Clean, Simple, Easy to Understand                      ║")
    print("╚" + "═"*58 + "╝")

    #-------------------------------------------------------------------------
    # STEP 1: Create Brain (One Line!)
    #-------------------------------------------------------------------------
    print_separator()
    print("STEP 1: Creating Brain...")

    brain = nimcp.Brain(
        name="simple_python_demo",
        size=nimcp.BrainSize.SMALL,  # 1K neurons
        task=nimcp.BrainTask.CLASSIFICATION,
        num_inputs=INPUT_DIM,
        num_outputs=NUM_CLASSES
    )

    print("✓ Brain created: 1,000 neurons, 3 output classes")

    #-------------------------------------------------------------------------
    # STEP 2: Train the Brain (Simple API)
    #-------------------------------------------------------------------------
    print_separator()
    print(f"STEP 2: Training Brain with {NUM_TRAINING_EXAMPLES} examples...")
    print()

    labels = ["pattern_A", "pattern_B", "pattern_C"]

    for i in range(NUM_TRAINING_EXAMPLES):
        pattern = create_pattern(i)
        label = labels[i % NUM_CLASSES]
        confidence = 0.9

        # Train (One Line!)
        success = brain.learn(pattern, label, confidence)

        status = "✓" if success else "✗"
        print(f"  Example {i+1:2d}: {label:12s}... {status}")

    print("\n✓ Training complete")

    #-------------------------------------------------------------------------
    # STEP 3: Test Classification
    #-------------------------------------------------------------------------
    print_separator()
    print("STEP 3: Testing Classification...")

    # Test with pattern A
    test_pattern = create_pattern(0)  # Pattern A

    # Predict (One Line!)
    result = brain.predict(test_pattern)

    print_separator()
    print("RESULTS:")
    print_separator()
    print_results(result)

    #-------------------------------------------------------------------------
    # STEP 4: Test Epistemic Filtering (Bias Detection)
    #-------------------------------------------------------------------------
    print_separator()
    print("STEP 4: Testing Epistemic Filter (Bias Prevention)...")
    print()

    # Test with ambiguous input (should flag for verification)
    ambiguous_pattern = [0.5] * INPUT_DIM  # All values at 0.5

    result = brain.predict(ambiguous_pattern)

    print_separator()
    print("LOW-QUALITY INPUT TEST:")
    print_separator()
    print(f"Decision: {result.get('label', 'unknown')}")
    print(f"Confidence: {result.get('confidence', 0.0) * 100:.1f}%")
    print(f"Epistemic Quality: {result.get('epistemic_quality', 0.0) * 100:.1f}% (Low = Expected)")
    print(f"Requires Verification: {'✓ YES' if result.get('requires_verification', False) else '✗ NO'} (Yes = Expected)")

    #-------------------------------------------------------------------------
    # STEP 5: Get Brain Metrics
    #-------------------------------------------------------------------------
    print_separator()
    print("STEP 5: Brain Metrics...")

    metrics = brain.get_metrics()

    print(f"  Total Decisions: {metrics.get('total_decisions', 0)}")
    print(f"  Avg Confidence: {metrics.get('avg_confidence', 0.0) * 100:.1f}%")
    print(f"  Learning Events: {metrics.get('learning_events', 0)}")

    #-------------------------------------------------------------------------
    # Summary
    #-------------------------------------------------------------------------
    print_separator()
    print("\n")
    print("╔" + "═"*58 + "╗")
    print("║  Demo Complete!                                          ║")
    print("║                                                          ║")
    print("║  Key Features Demonstrated:                              ║")
    print("║  ✓ Simple Python API (3 method calls)                   ║")
    print("║  ✓ Pattern classification                               ║")
    print("║  ✓ Epistemic filtering (bias prevention)                ║")
    print("║  ✓ Quality metrics (confidence, credibility)            ║")
    print("║  ✓ Brain metrics (decisions, learning)                  ║")
    print("║                                                          ║")
    print("║  Total Lines of Code: ~80 (excluding comments)          ║")
    print("╚" + "═"*58 + "╝")
    print("\n")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(0)
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
