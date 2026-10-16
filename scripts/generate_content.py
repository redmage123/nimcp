#!/usr/bin/env python3
"""Generate Claude parent content for Athena training.

Run this BEFORE immerse_athena.py — it generates all narrations, encouragements,
moral stories, etc. and caches them to JSON. Training then loads from cache
instead of calling Claude (which can't run when the brain uses 52GB+ RAM).

Usage:
    python3 scripts/generate_content.py [--force]
"""

import json
import os
import subprocess
import sys
import time

CACHE_FILE = "checkpoints/athena/claude_content_cache.json"

STAGE_DESC = {
    0: "newborn (0-3 months) — everything is new and wondrous",
    1: "infant (3-12 months) — learning names, recognizing objects",
    2: "toddler (1-2 years) — starting to babble and respond",
    3: "young child (2-4 years) — asking questions, forming opinions",
}

# Content types to generate per stage, each as a separate Claude call
# Each content type specifies count per batch and number of batches.
# Multiple batches avoid huge single responses; results are concatenated.
CONTENT_TYPES = {
    "narrations": {
        "count": 50,  # per batch
        "batches": 4,  # 200 total
        "desc": "unique 1-2 sentence narrations introducing sensory experiences to a {stage}. "
                "Express wonder at light, colors, sounds, textures, animals, nature. Vary widely.",
    },
    "encouragements": {
        "count": 50,
        "batches": 2,  # 100 total
        "desc": "encouraging/praising 1 sentence responses for when a {stage} is learning. "
                "Half for good progress, half for patience when struggling.",
    },
    "moral_stories": {
        "count": 25,
        "batches": 2,  # 50 total
        "desc": "very short moral stories (3-4 sentences each) about sharing, kindness, truth, "
                "helping, fairness. End each with a gentle question. Suitable for a {stage}.",
    },
    "speech_prompts": {
        "count": 40,
        "batches": 2,  # 80 total
        "desc": "encouraging prompts for teaching a {stage} to repeat words. "
                "Like 'Can you say dog? Doooog! Good try!' Each unique.",
    },
    "inspirations": {
        "count": 25,
        "batches": 2,  # 50 total
        "desc": "inspiring 2-3 sentence passages about natural wonders, beauty, curiosity. "
                "Suitable for a {stage}. Fill with genuine wonder.",
    },
    "dreams": {
        "count": 25,
        "batches": 1,  # 25 total
        "desc": "simple beautiful 'what if' questions that spark wonder in a {stage}. "
                "Just the questions, 1 sentence each.",
    },
    "conversations": {
        "count": 50,
        "batches": 2,  # 100 total
        "desc": "warm conversational responses a parent might give when their {stage} "
                "shows them something or tries to communicate. 1-2 sentences each.",
    },
}


def call_claude(prompt, max_tokens=2048, timeout=120):
    """Call Claude CLI with all optimization flags."""
    cmd = [
        "claude", "-p", prompt,
        "--model", "sonnet",
        "--output-format", "text",
        "--system-prompt", "",
        "--setting-sources", "",
        "--allowed-tools", "",
    ]
    env = {k: v for k, v in os.environ.items()
           if k not in ("CLAUDECODE", "CUDA_VISIBLE_DEVICES")}

    result = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=timeout, env=env,
                           stdin=subprocess.DEVNULL)  # don't wait 3s for stdin (rc=1 headless)
    if result.returncode != 0:
        raise RuntimeError(f"claude CLI failed (rc={result.returncode}): {result.stderr.strip()}")
    text = result.stdout.strip()
    if not text:
        raise RuntimeError("claude CLI returned empty output")
    return text


def _parse_json_array(raw):
    """Parse a JSON array from Claude's response, stripping markdown fences."""
    raw = raw.strip()
    if raw.startswith("```"):
        lines = raw.split("\n")
        lines = [l for l in lines if not l.strip().startswith("```")]
        raw = "\n".join(lines)
    items = json.loads(raw)
    if not isinstance(items, list):
        raise ValueError(f"Expected list, got {type(items)}")
    return items


def generate_stage(stage):
    """Generate all content types for a single stage, using batched calls."""
    stage_name = STAGE_DESC[stage]
    data = {}

    for content_type, spec in CONTENT_TYPES.items():
        count = spec["count"]
        batches = spec.get("batches", 1)
        desc = spec["desc"].format(stage=stage_name)
        total_target = count * batches

        print(f"    {content_type} ({total_target})...", end=" ", flush=True)
        all_items = []
        t0 = time.time()

        for batch_idx in range(batches):
            # For subsequent batches, ask for variety
            variety_hint = ""
            if batch_idx > 0:
                variety_hint = (
                    f"\nThis is batch {batch_idx + 1} of {batches}. "
                    f"Make these COMPLETELY DIFFERENT from previous batches. "
                    f"Cover different topics, objects, and scenarios."
                )

            prompt = (
                f"Generate exactly {count} items as a JSON array of strings.\n"
                f"Each item: {desc}{variety_hint}\n\n"
                f"Return ONLY a valid JSON array (no markdown fences, no commentary).\n"
                f"Example format: [\"first item\", \"second item\", ...]"
            )

            try:
                raw = call_claude(prompt, max_tokens=2048, timeout=120)
                items = _parse_json_array(raw)
                all_items.extend(items)
            except Exception as e:
                print(f"batch {batch_idx + 1} FAILED: {e}", end=" ", flush=True)

        data[content_type] = all_items
        elapsed = time.time() - t0
        print(f"{len(all_items)} items — {elapsed:.1f}s")

    return data


def main():
    force = "--force" in sys.argv

    if not force and os.path.exists(CACHE_FILE):
        try:
            with open(CACHE_FILE) as f:
                cache = json.load(f)
            if all(str(s) in cache for s in range(4)):
                print(f"Content cache already exists at {CACHE_FILE}")
                print("Use --force to regenerate.")
                return
        except Exception:
            pass

    # Connectivity test
    print("Testing Claude CLI connectivity...", flush=True)
    try:
        t0 = time.time()
        result = call_claude("Say OK", max_tokens=16, timeout=30)
        elapsed = time.time() - t0
        print(f"OK — {elapsed:.1f}s")
    except Exception as e:
        print(f"FAILED: {e}")
        print("Cannot reach Claude CLI. Aborting.")
        sys.exit(1)

    cache = {}
    total_t0 = time.time()

    for stage in range(4):
        print(f"\nStage {stage}: {STAGE_DESC[stage]}")
        data = generate_stage(stage)
        cache[str(stage)] = data

    os.makedirs(os.path.dirname(CACHE_FILE), exist_ok=True)
    with open(CACHE_FILE, "w") as f:
        json.dump(cache, f, indent=2)

    total_elapsed = time.time() - total_t0
    total_items = sum(
        len(v) for stage_data in cache.values()
        for v in stage_data.values() if isinstance(v, list)
    )
    print(f"\nDone! {total_items} items across 4 stages — {total_elapsed:.0f}s")
    print(f"Saved to {CACHE_FILE}")


if __name__ == "__main__":
    main()
