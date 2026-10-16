#!/bin/bash
# Monitor training and restart immerse_athena when Stage 1 completes.
# This ensures Stage 2 picks up the new curriculum wiring.
#
# Usage: nohup bash scripts/monitor_stage_completion.sh &

LOGFILE="/home/bbrelin/nimcp/nohup_training.log"
SCRIPT_DIR="/home/bbrelin/nimcp/scripts"

echo "[Monitor] Watching for Stage 1 completion..."

while true; do
    # Check if Stage 2 has started (meaning Stage 1 just finished)
    if grep -q "STAGE 2: Good Try" "$LOGFILE" 2>/dev/null; then
        echo "[Monitor] Stage 2 detected — Stage 1 has completed!"
        echo "[Monitor] Restarting immerse_athena to pick up new code..."

        # Kill the running immerse script
        pkill -f "immerse_athena.py" 2>/dev/null
        sleep 5

        # Restart with updated code
        cd /home/bbrelin/nimcp
        PYTHONUNBUFFERED=1 nohup python3 -u scripts/immerse_athena.py \
            --daemon --resume > nohup_training.log 2>&1 &

        echo "[Monitor] Restarted. PID: $!"
        echo "[Monitor] New code has curriculum wired into Stages 2 and 3."
        echo "[Monitor] Exiting monitor."
        exit 0
    fi

    sleep 60  # Check every minute
done
