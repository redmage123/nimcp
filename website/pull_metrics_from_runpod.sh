#!/bin/bash
# Pull metrics.json from RunPod to Hetzner website dir.
#
# Sources podconfig.sh so when the pod's SSH port/host changes (RunPod
# re-issues these on every restart), this script tracks it automatically
# instead of going silently stale.
set -uo pipefail

NIMCP_DIR="/home/bbrelin/nimcp"
. "${NIMCP_DIR}/scripts/podconfig.sh"

$POD_SCP "${POD_HOST}:${POD_DIR}/website/metrics.json" \
         "${NIMCP_DIR}/website/metrics.json" 2>/dev/null

# History TSV — append-only on the pod. Pull alongside metrics.json so the
# dashboard can bootstrap its rolling history on page load instead of
# starting empty after every browser refresh.
$POD_SCP "${POD_HOST}:${POD_DIR}/website/metrics_history.tsv" \
         "${NIMCP_DIR}/website/metrics_history.tsv" 2>/dev/null

# Convert tail-N rows of TSV → metrics_history.json for the dashboard.
# Best-effort: if the TSV doesn't exist yet (first deploy) the script
# silently no-ops.
"${NIMCP_DIR}/website/tsv_to_history_json.py" 2>/dev/null || true
