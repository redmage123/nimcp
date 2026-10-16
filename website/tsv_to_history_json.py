#!/usr/bin/env python3
"""Convert metrics_history.tsv → metrics_history.json (last N rows).

Reads the TSV produced by metrics_runpod.py on the pod (rsynced over by
pull_metrics_from_runpod.sh) and emits a JSON array of objects, one per
row, ordered oldest-first. The dashboard's index.html fetches this file
on page load and replays it through updateMetrics() so the in-browser
rolling history starts populated instead of blank.

Best-effort: any error logs to stderr and exits 0 — never breaks the
dashboard for a missing/malformed source file.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TSV_IN = os.path.join(HERE, 'metrics_history.tsv')
JSON_OUT = os.path.join(HERE, 'metrics_history.json')

# How many rows to keep in the bootstrap payload. The dashboard's MAX_POINTS
# is currently 60; we send 120 so it has 2× headroom (the front-end will
# trim down on display). Keeping this small (~60 KB at current schema)
# means the JSON file fits in any HTTP response without paging.
MAX_ROWS = 120


def main():
    if not os.path.exists(TSV_IN):
        return 0
    try:
        with open(TSV_IN) as f:
            lines = f.readlines()
    except OSError as e:
        print(f'tsv_to_history_json: cannot read {TSV_IN}: {e}', file=sys.stderr)
        return 0
    if len(lines) < 2:
        return 0  # header-only or empty
    header = lines[0].rstrip('\n').split('\t')
    rows = []
    for line in lines[-MAX_ROWS:]:
        line = line.rstrip('\n')
        if not line or line.startswith('timestamp\t'):
            continue
        cols = line.split('\t')
        if len(cols) != len(header):
            continue
        obj = {}
        for k, v in zip(header, cols):
            if v == '':
                obj[k] = None
                continue
            # Try numeric coercion so the front-end doesn't have to parseFloat
            # everything. Booleans were stored as 0/1 — leave as int.
            try:
                if '.' in v or 'e' in v.lower():
                    obj[k] = float(v)
                else:
                    obj[k] = int(v)
            except ValueError:
                obj[k] = v
        rows.append(obj)
    tmp = JSON_OUT + '.tmp'
    try:
        with open(tmp, 'w') as f:
            json.dump(rows, f)
        os.replace(tmp, JSON_OUT)
    except OSError as e:
        print(f'tsv_to_history_json: cannot write {JSON_OUT}: {e}', file=sys.stderr)
        return 0
    return 0


if __name__ == '__main__':
    sys.exit(main())
