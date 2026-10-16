#!/usr/bin/env python3
"""
Monkey-patch: Inject CurriculumEscalator into the running immerse_athena process.

This creates a trigger file that _check_hp_overrides reads and executes,
patching the _inject_cognitive_training function to always use a curriculum.

Usage: python3 scripts/inject_curriculum_patch.py
"""

import json
import os

# The patch: create a global curriculum and wrap _inject_cognitive_training
# to always pass it. This works because Python closures capture by reference.
PATCH_CODE = '''
import sys

# Get the immerse_athena module from the running process
mod = sys.modules.get('__main__')
if mod is None:
    print("    [PATCH] Cannot find __main__ module")
else:
    # Import CurriculumEscalator from the module
    CurriculumEscalator = getattr(mod, 'CurriculumEscalator', None)
    if CurriculumEscalator is None:
        print("    [PATCH] CurriculumEscalator not found in module")
    else:
        # Create a global curriculum with all tiers unlocked
        _global_curriculum = CurriculumEscalator(unlock_threshold=200.0)
        _global_curriculum.current_tier = 2  # Reasoning tier unlocked

        # Save the original function
        _orig_inject = getattr(mod, '_inject_cognitive_training', None)
        if _orig_inject is None:
            print("    [PATCH] _inject_cognitive_training not found")
        else:
            # Wrap it to always pass curriculum
            def _patched_inject(brain, composer, step, learning_rate,
                                spectral_splitter=None, fold_idx=0,
                                curriculum=None):
                # Use our curriculum if none was passed
                if curriculum is None:
                    curriculum = _global_curriculum
                return _orig_inject(brain, composer, step, learning_rate,
                                    spectral_splitter=spectral_splitter,
                                    fold_idx=fold_idx,
                                    curriculum=curriculum)

            # Replace the function in the module
            mod._inject_cognitive_training = _patched_inject
            # Also update in globals so the training loop picks it up
            mod.__dict__['_inject_cognitive_training'] = _patched_inject

            print(f"    [PATCH] CurriculumEscalator injected (tier={_global_curriculum.current_tier}, "
                  f"threshold={_global_curriculum.unlock_threshold})")
            print(f"    [PATCH] _inject_cognitive_training wrapped with curriculum")
'''

# Write as a trigger file that _check_hp_overrides will pick up
TRIGGER_PATH = "/tmp/athena_exec_patch.py"

with open(TRIGGER_PATH, 'w') as f:
    f.write(PATCH_CODE)

print(f"Patch written to {TRIGGER_PATH}")
print("The running immerse_athena.py needs to check for this file.")
print("Since _check_hp_overrides doesn't exec arbitrary .py files yet,")
print("we need to add that capability first.")
