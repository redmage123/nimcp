#!/usr/bin/env python3
"""
NIMCP Simple Terminal UI - Interactive & Clean

An interactive terminal UI that's easy to use with colored output,
clear menus, and step-by-step guidance.

USAGE:
    python3 simple_terminal_ui.py

REQUIREMENTS:
    pip install colorama numpy
"""

import sys
import os
import numpy as np
from colorama import init, Fore, Back, Style

# Initialize colorama
init(autoreset=True)

# Add NIMCP to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../build/lib/python'))

try:
    import nimcp
except ImportError:
    print(f"{Fore.RED}Error: NIMCP Python bindings not found")
    print(f"{Fore.YELLOW}Build NIMCP first: cd build && make")
    sys.exit(1)

#=============================================================================
# GLOBAL STATE
#=============================================================================

brain = None
training_examples = []
prediction_history = []

#=============================================================================
# UI HELPERS - Clean and Simple
#=============================================================================

def clear_screen():
    """Clear the terminal screen"""
    os.system('cls' if os.name == 'nt' else 'clear')

def print_header():
    """Print a nice header"""
    print(f"\n{Fore.CYAN}{'='*70}")
    print(f"{Fore.CYAN}{'  NIMCP Simple Terminal UI - Phase 9 Features':^70}")
    print(f"{Fore.CYAN}{'='*70}\n")

def print_separator():
    """Print a separator"""
    print(f"{Fore.CYAN}{'-'*70}")

def print_menu():
    """Print the main menu"""
    print(f"\n{Fore.GREEN}MAIN MENU:")
    print(f"{Fore.YELLOW}  1. {Fore.WHITE}Initialize Brain")
    print(f"{Fore.YELLOW}  2. {Fore.WHITE}Train with Example")
    print(f"{Fore.YELLOW}  3. {Fore.WHITE}Make Prediction")
    print(f"{Fore.YELLOW}  4. {Fore.WHITE}View Metrics")
    print(f"{Fore.YELLOW}  5. {Fore.WHITE}View Training History")
    print(f"{Fore.YELLOW}  6. {Fore.WHITE}Reset Brain")
    print(f"{Fore.YELLOW}  7. {Fore.WHITE}Help")
    print(f"{Fore.YELLOW}  0. {Fore.WHITE}Exit")
    print()

def get_input(prompt, default=None):
    """Get user input with optional default"""
    if default:
        user_input = input(f"{Fore.CYAN}{prompt} [{Fore.YELLOW}{default}{Fore.CYAN}]: {Fore.WHITE}")
        return user_input if user_input.strip() else default
    else:
        return input(f"{Fore.CYAN}{prompt}: {Fore.WHITE}")

def print_success(message):
    """Print success message"""
    print(f"{Fore.GREEN}✓ {message}")

def print_error(message):
    """Print error message"""
    print(f"{Fore.RED}✗ {message}")

def print_warning(message):
    """Print warning message"""
    print(f"{Fore.YELLOW}⚠ {message}")

def print_info(message):
    """Print info message"""
    print(f"{Fore.CYAN}ℹ {message}")

def pause():
    """Pause and wait for user"""
    input(f"\n{Fore.CYAN}Press Enter to continue...")

#=============================================================================
# MENU ACTIONS
#=============================================================================

def action_initialize():
    """Initialize the brain"""
    global brain

    clear_screen()
    print_header()
    print(f"{Fore.GREEN}INITIALIZE BRAIN\n")

    input_dim = int(get_input("Input dimension", "8"))
    num_classes = int(get_input("Number of classes", "3"))

    try:
        brain = nimcp.Brain(
            name="terminal_ui",
            size=nimcp.BrainSize.SMALL,
            task=nimcp.BrainTask.CLASSIFICATION,
            num_inputs=input_dim,
            num_outputs=num_classes
        )

        print()
        print_success(f"Brain initialized successfully!")
        print_info(f"  Neurons: 1,000")
        print_info(f"  Input dimension: {input_dim}")
        print_info(f"  Output classes: {num_classes}")

    except Exception as e:
        print()
        print_error(f"Failed to initialize brain: {e}")

    pause()

def action_train():
    """Train with an example"""
    global brain, training_examples

    if brain is None:
        print_error("Brain not initialized! Please initialize first.")
        pause()
        return

    clear_screen()
    print_header()
    print(f"{Fore.GREEN}TRAIN WITH EXAMPLE\n")

    print_info("Enter features as comma-separated values")
    features_str = get_input("Features", "1,0,1,0,1,0,1,0")
    label = get_input("Label", "pattern_A")
    confidence = float(get_input("Confidence (0.0-1.0)", "0.9"))

    try:
        features = [float(x.strip()) for x in features_str.split(',')]

        success = brain.learn(features, label, confidence)

        print()
        if success:
            training_examples.append({'features': features, 'label': label})
            print_success(f"Training successful!")
            print_info(f"  Label: {label}")
            print_info(f"  Confidence: {confidence * 100:.1f}%")
            print_info(f"  Total examples: {len(training_examples)}")
        else:
            print_error("Training failed")

    except Exception as e:
        print()
        print_error(f"Training error: {e}")

    pause()

def action_predict():
    """Make a prediction"""
    global brain, prediction_history

    if brain is None:
        print_error("Brain not initialized! Please initialize first.")
        pause()
        return

    clear_screen()
    print_header()
    print(f"{Fore.GREEN}MAKE PREDICTION\n")

    print_info("Enter features as comma-separated values")
    features_str = get_input("Features", "1,0,1,0,1,0,1,0")

    try:
        features = [float(x.strip()) for x in features_str.split(',')]

        result = brain.predict(features)

        prediction_history.append(result)

        print()
        print_separator()
        print(f"{Fore.GREEN}PREDICTION RESULTS:")
        print_separator()

        print(f"{Fore.WHITE}Decision: {Fore.CYAN}{result.get('label', 'unknown')}")
        print(f"{Fore.WHITE}Confidence: {Fore.CYAN}{result.get('confidence', 0.0) * 100:.1f}%")

        print()
        print(f"{Fore.YELLOW}QUALITY METRICS:")

        quality = result.get('epistemic_quality', 0.0)
        quality_color = Fore.GREEN if quality > 0.7 else Fore.YELLOW if quality > 0.4 else Fore.RED
        print(f"{Fore.WHITE}  Epistemic Quality: {quality_color}{quality * 100:.1f}%")

        credibility = result.get('credibility', 0.0)
        cred_color = Fore.GREEN if credibility > 0.7 else Fore.YELLOW if credibility > 0.4 else Fore.RED
        print(f"{Fore.WHITE}  Credibility: {cred_color}{credibility * 100:.1f}%")

        requires_verification = result.get('requires_verification', False)
        verify_text = f"{Fore.YELLOW}⚠ Yes" if requires_verification else f"{Fore.GREEN}✓ No"
        print(f"{Fore.WHITE}  Requires Verification: {verify_text}")

        bias_detected = result.get('bias_detected', False)
        bias_text = f"{Fore.YELLOW}⚠ Yes" if bias_detected else f"{Fore.GREEN}✓ No"
        print(f"{Fore.WHITE}  Bias Detected: {bias_text}")

        ethical = result.get('ethical_approved', True)
        ethical_text = f"{Fore.GREEN}✓ Yes" if ethical else f"{Fore.RED}✗ No"
        print(f"{Fore.WHITE}  Ethical Approval: {ethical_text}")

        print_separator()

    except Exception as e:
        print()
        print_error(f"Prediction error: {e}")

    pause()

def action_metrics():
    """View metrics"""
    global brain, training_examples, prediction_history

    if brain is None:
        print_error("Brain not initialized! Please initialize first.")
        pause()
        return

    clear_screen()
    print_header()
    print(f"{Fore.GREEN}BRAIN METRICS\n")

    try:
        metrics = brain.get_metrics()

        print_separator()
        print(f"{Fore.CYAN}USAGE STATISTICS:")
        print_separator()
        print(f"{Fore.WHITE}  Total Training Examples: {Fore.YELLOW}{len(training_examples)}")
        print(f"{Fore.WHITE}  Total Predictions: {Fore.YELLOW}{len(prediction_history)}")
        print(f"{Fore.WHITE}  Learning Events: {Fore.YELLOW}{metrics.get('learning_events', 0)}")
        print(f"{Fore.WHITE}  Total Decisions: {Fore.YELLOW}{metrics.get('total_decisions', 0)}")

        if prediction_history:
            avg_confidence = sum(p.get('confidence', 0) for p in prediction_history) / len(prediction_history)
            avg_quality = sum(p.get('epistemic_quality', 0) for p in prediction_history) / len(prediction_history)

            print()
            print_separator()
            print(f"{Fore.CYAN}QUALITY AVERAGES:")
            print_separator()
            print(f"{Fore.WHITE}  Avg Confidence: {Fore.YELLOW}{avg_confidence * 100:.1f}%")
            print(f"{Fore.WHITE}  Avg Epistemic Quality: {Fore.YELLOW}{avg_quality * 100:.1f}%")

        print_separator()

    except Exception as e:
        print_error(f"Error retrieving metrics: {e}")

    pause()

def action_history():
    """View training history"""
    global training_examples

    clear_screen()
    print_header()
    print(f"{Fore.GREEN}TRAINING HISTORY\n")

    if not training_examples:
        print_warning("No training examples yet")
    else:
        print(f"{Fore.CYAN}Total examples: {Fore.YELLOW}{len(training_examples)}\n")

        for i, example in enumerate(training_examples, 1):
            print(f"{Fore.WHITE}{i:3d}. {Fore.CYAN}{example['label']:20s} {Fore.WHITE}| Features: {Fore.YELLOW}{example['features'][:5]}{'...' if len(example['features']) > 5 else ''}")

    pause()

def action_reset():
    """Reset the brain"""
    global brain, training_examples, prediction_history

    clear_screen()
    print_header()
    print(f"{Fore.RED}RESET BRAIN\n")

    print_warning("This will delete the brain and all training data!")
    confirm = get_input("Are you sure? (yes/no)", "no")

    if confirm.lower() == 'yes':
        brain = None
        training_examples = []
        prediction_history = []
        print()
        print_success("Brain reset successfully")
    else:
        print()
        print_info("Reset cancelled")

    pause()

def action_help():
    """Show help"""
    clear_screen()
    print_header()
    print(f"{Fore.GREEN}HELP & GUIDE\n")

    print(f"{Fore.CYAN}QUICK START:")
    print(f"{Fore.WHITE}  1. Initialize Brain - Set input dimension and number of output classes")
    print(f"{Fore.WHITE}  2. Train - Provide examples with features and labels")
    print(f"{Fore.WHITE}  3. Predict - Test the brain with new features")
    print(f"{Fore.WHITE}  4. View Metrics - See performance statistics")
    print()

    print(f"{Fore.CYAN}PHASE 9 FEATURES:")
    print(f"{Fore.WHITE}  • {Fore.GREEN}Epistemic Filtering{Fore.WHITE} - Bias prevention and quality assessment")
    print(f"{Fore.WHITE}  • {Fore.GREEN}Credibility Scoring{Fore.WHITE} - Evaluate claim credibility")
    print(f"{Fore.WHITE}  • {Fore.GREEN}Bias Detection{Fore.WHITE} - Identify cognitive biases")
    print(f"{Fore.WHITE}  • {Fore.GREEN}Ethical Reasoning{Fore.WHITE} - Golden Rule evaluation")
    print()

    print(f"{Fore.CYAN}EXAMPLE WORKFLOW:")
    print(f"{Fore.WHITE}  1. Initialize: 8 inputs, 3 classes")
    print(f"{Fore.WHITE}  2. Train: 1,0,1,0,1,0,1,0 → pattern_A")
    print(f"{Fore.WHITE}  3. Train: 0,1,0,1,0,1,0,1 → pattern_B")
    print(f"{Fore.WHITE}  4. Predict: 1,0,1,0,1,0,1,0 → Should predict pattern_A")

    pause()

#=============================================================================
# MAIN LOOP
#=============================================================================

def main():
    """Main interactive loop"""
    while True:
        clear_screen()
        print_header()

        # Show brain status
        if brain is not None:
            print(f"{Fore.GREEN}Status: Brain Initialized ✓")
            print(f"{Fore.CYAN}Training examples: {Fore.YELLOW}{len(training_examples)}")
            print(f"{Fore.CYAN}Predictions made: {Fore.YELLOW}{len(prediction_history)}")
        else:
            print(f"{Fore.YELLOW}Status: No Brain Initialized")

        print_menu()

        choice = get_input("Choose an option", "0")

        if choice == '1':
            action_initialize()
        elif choice == '2':
            action_train()
        elif choice == '3':
            action_predict()
        elif choice == '4':
            action_metrics()
        elif choice == '5':
            action_history()
        elif choice == '6':
            action_reset()
        elif choice == '7':
            action_help()
        elif choice == '0':
            print(f"\n{Fore.CYAN}Goodbye! 👋\n")
            break
        else:
            print_error("Invalid choice!")
            pause()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n\n{Fore.CYAN}Interrupted by user. Goodbye! 👋\n")
        sys.exit(0)
    except Exception as e:
        print(f"\n{Fore.RED}Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
