#!/usr/bin/env python3
"""Standalone test for the curated film list (scripts/video_allowlist.txt).

Checks that the allowlist parses, contains every film the video probe holds
out, and contains none of the films excluded from the curriculum.

Run: python3 tests/python/test_video_allowlist.py
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.join(HERE, "..", "..", "scripts")
sys.path.insert(0, SCRIPTS)
from build_video_cache import read_allowlist  # noqa: E402
from real_sensory import RealSensoryBank  # noqa: E402

failures = 0


def expect(cond, msg):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", msg)


def main():
    ids = read_allowlist(os.path.join(SCRIPTS, "video_allowlist.txt"))
    expect(len(ids) >= 40, f"allowlist has {len(ids)} films, expected at least 40")
    expect(len(ids) == len(set(ids)), "no duplicate identifiers")
    expect(all(" " not in i and "#" not in i for i in ids), "identifiers are bare tokens")
    missing = RealSensoryBank.HELDOUT_FILMS - set(ids)
    expect(not missing, f"held-out films missing from the allowlist: {sorted(missing)}")
    banned = RealSensoryBank.EXCLUDED_FILMS & set(ids)
    expect(not banned, f"excluded films present in the allowlist: {sorted(banned)}")

    # Parser: comments, blank lines, duplicates and inline comments.
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as fh:
        fh.write("# header\n\nalpha   # first\nbeta\nalpha\n  # indented comment\ngamma#x\n")
        path = fh.name
    try:
        expect(read_allowlist(path) == ["alpha", "beta", "gamma"], f"parse: {read_allowlist(path)}")
    finally:
        os.remove(path)

    if failures:
        print(f"test_video_allowlist: {failures} failure(s)")
        sys.exit(1)
    print("test_video_allowlist: all passed")


if __name__ == "__main__":
    main()
