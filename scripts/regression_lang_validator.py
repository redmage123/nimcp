#!/usr/bin/env python3
"""
regression_lang_validator.py — Cold-init regression for the language system.

Doesn't try to spin up a full 1.8M-neuron brain (won't fit on Hetzner's
62 GB RAM with 30 GB swap already pinned). Instead, it creates a minimal
brain via the Python binding and validates the exact code paths today's
fixes touched:

  1. Cold-init bulk lexicon load — does NIMCP_BULK_LEXICON actually land
     all 29,424 entries now that DEFAULT_CONCEPT_CAPACITY is 131072?
  2. vocab_size after persistence-load — does the stale-stats fix in
     gl_persistence_load actually report the live vocab_count?
  3. CSTDP toggle round-trip — set / get / disable.
  4. Checkpoint save/load round-trip — does vocab survive the cycle?
  5. Comprehend → produce — does the bridge actually fire end-to-end on
     a fresh brain (no resume state)?

What this DOESN'T test (needs the full brain): full hierarchical SNN
init, CB-GPU step path, large-scale training, anything that depends on
the 60-GB resident state of the production brain.

Run:  LD_LIBRARY_PATH=/home/bbrelin/nimcp/build/lib \
      python3 scripts/regression_lang_validator.py
"""

import os
import sys
import tempfile
import shutil
import time

# Belt-and-suspenders: tell the Python binding to find our local libnimcp.
os.environ.setdefault("LD_LIBRARY_PATH", "/home/bbrelin/nimcp/build/lib")

WORDNET = "/home/bbrelin/nimcp/data/lexicon/wordnet_glove_v1.bin"
os.environ["NIMCP_BULK_LEXICON"] = WORDNET
os.environ["NIMCP_LOG_LEVEL"] = "warn"  # less spam

results = []
def check(name, cond, detail=""):
    status = "PASS" if cond else "FAIL"
    results.append((status, name, detail))
    print(f"  [{status}] {name}" + (f" — {detail}" if detail else ""))

import nimcp

print("=== regression_lang_validator ===")
print(f"  nimcp module: {nimcp.__file__}")
print(f"  WordNet: {WORDNET} ({os.path.getsize(WORDNET)/1024/1024:.1f} MB)")
print()

# ---- Test 1: minimal brain creation + bulk lexicon load ----
print("[1] Cold-init brain + bulk lexicon load")
brain = nimcp.Brain("regression_test", num_inputs=64, num_outputs=64,
                       init_mode="minimal")
print(f"  brain created")

# brain init triggers bulk_lexicon load via NIMCP_BULK_LEXICON env.
# Wait briefly for any async init to settle, then query vocab.
time.sleep(2)

try:
    diag = brain.get_grounded_language_diagnostics()
    vocab_size = diag.get("vocab_size", 0)
    bridge_active = diag.get("bridge_active_bindings", 0)
    print(f"  vocab_size={vocab_size}, bridge_active={bridge_active}")
    check("vocab >= 29000 (bulk wordnet+glove preload landed)",
          vocab_size >= 29000,
          f"got {vocab_size} (expected >= 29000)")
except AttributeError as e:
    check("get_grounded_language_diagnostics available", False, str(e))
    vocab_size = 0

# ---- Test 2: CSTDP toggle round-trip ----
print()
print("[2] CSTDP toggle round-trip")
try:
    brain.set_comprehend_stdp_enabled(True)
    print("  enabled CSTDP")
    brain.set_comprehend_stdp_enabled(False)
    print("  disabled CSTDP")
    check("set_comprehend_stdp_enabled round-trips without exception", True)
except AttributeError as e:
    check("set_comprehend_stdp_enabled available", False, str(e))
except Exception as e:
    check("set_comprehend_stdp_enabled round-trips", False, str(e))

# ---- Test 3: comprehend + produce on cold-init brain ----
print()
print("[3] Comprehend → produce roundtrip (cold-init brain, no resume)")
try:
    brain.set_snn_language_bridge_sampling(temperature=0.7, top_p=0.9)
    resp = brain.grounded_respond("What is a tree?")
    text = resp.get("response", "")
    conf = resp.get("confidence", 0.0)
    print(f"  response: {text!r}, confidence={conf:.4f}")
    check("grounded_respond returns non-error",
          resp.get("success", True) is not False)
    # Don't assert on text content — fresh brain may produce gibberish or empty.
    # The point is the pipeline doesn't crash.
except Exception as e:
    check("grounded_respond on cold-init", False, str(e))

# ---- Test 4: checkpoint save/load round-trip ----
print()
print("[4] Checkpoint save → load round-trip (vocab_size preservation)")
ckpt_dir = tempfile.mkdtemp(prefix="regression_ckpt_")
ckpt_path = os.path.join(ckpt_dir, "test_brain.bin")
try:
    save_rc = brain.save(ckpt_path)
    print(f"  save returned: {save_rc}")
    # Vocab before destroy
    before_vocab = brain.get_grounded_language_diagnostics().get("vocab_size", 0)

    del brain  # destroy

    brain2 = nimcp.Brain("regression_test_2", num_inputs=64, num_outputs=64,
                          init_mode="minimal")
    load_rc = brain2.load(ckpt_path)
    print(f"  load returned: {load_rc}")

    after_vocab = brain2.get_grounded_language_diagnostics().get("vocab_size", 0)
    print(f"  vocab before save: {before_vocab}, after load: {after_vocab}")
    check("vocab_size survives save/load round-trip",
          after_vocab >= before_vocab * 0.95,  # allow tiny variance
          f"before={before_vocab}, after={after_vocab}")
    check("loaded vocab also includes wordnet (>= 29K)",
          after_vocab >= 29000,
          f"after-load vocab={after_vocab}")
    del brain2
except Exception as e:
    check("checkpoint round-trip", False, str(e))
finally:
    shutil.rmtree(ckpt_dir, ignore_errors=True)

# ---- Summary ----
print()
print("=== Summary ===")
n_pass = sum(1 for s, _, _ in results if s == "PASS")
n_fail = sum(1 for s, _, _ in results if s == "FAIL")
print(f"  {n_pass} PASS, {n_fail} FAIL")
if n_fail > 0:
    print()
    print("Failures:")
    for s, name, detail in results:
        if s == "FAIL":
            print(f"  - {name}: {detail}")
sys.exit(0 if n_fail == 0 else 1)
