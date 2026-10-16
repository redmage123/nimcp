#!/bin/bash
# Training Watchdog — monitors immerse_athena.py for crashes and hangs.
#
# Checks every INTERVAL seconds:
#   1. Is the process still alive?
#   2. Has the log file been updated in the last STALE_THRESHOLD seconds?
#
# Alerts via: desktop notification (notify-send), terminal bell, and log file.
#
# Usage:
#   ./scripts/watchdog_training.sh              # monitor auto-detected PID
#   ./scripts/watchdog_training.sh 1453015      # monitor specific PID

set -euo pipefail

PID="${1:-}"
LOG="/home/bbrelin/nimcp/immerse_athena.log"
WATCHDOG_LOG="/home/bbrelin/nimcp/watchdog.log"
INTERVAL=120            # Check every 2 minutes
STALE_THRESHOLD=600     # Alert if log not updated in 10 minutes

# Auto-detect PID if not provided
if [ -z "$PID" ]; then
    PID=$(pgrep -f "immerse_athena" -n 2>/dev/null || true)
    if [ -z "$PID" ]; then
        echo "ERROR: No immerse_athena process found and no PID provided."
        exit 1
    fi
fi

echo "$(date '+%Y-%m-%d %H:%M:%S') Watchdog started for PID $PID" | tee -a "$WATCHDOG_LOG"
echo "  Log: $LOG"
echo "  Check interval: ${INTERVAL}s, stale threshold: ${STALE_THRESHOLD}s"

alert() {
    local msg="$1"
    local timestamp
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "$timestamp ALERT: $msg" | tee -a "$WATCHDOG_LOG"

    # Desktop notification
    notify-send -u critical "Training Watchdog" "$msg" 2>/dev/null || true

    # Terminal bell
    printf '\a'

    # Also write a prominent file the user will notice
    echo "$timestamp $msg" >> /home/bbrelin/nimcp/TRAINING_ALERT.txt
}

while true; do
    # Check 1: Is the process alive?
    if ! kill -0 "$PID" 2>/dev/null; then
        alert "CRASH: Training process PID $PID is dead!"
        # Check exit info
        if wait "$PID" 2>/dev/null; then
            alert "Process exited normally (code 0)"
        else
            alert "Process exited with error"
        fi
        # Show last log lines
        echo "--- Last 10 log lines ---" >> "$WATCHDOG_LOG"
        tail -10 "$LOG" >> "$WATCHDOG_LOG" 2>/dev/null
        exit 1
    fi

    # Check 2: Is the log file being updated?
    if [ -f "$LOG" ]; then
        last_mod=$(stat -c %Y "$LOG" 2>/dev/null || echo 0)
        now=$(date +%s)
        age=$(( now - last_mod ))

        if [ "$age" -gt "$STALE_THRESHOLD" ]; then
            alert "HANG: Log file not updated for ${age}s (threshold: ${STALE_THRESHOLD}s). Process may be stuck."
            # Check if process is using CPU
            cpu=$(ps -p "$PID" -o %cpu= 2>/dev/null || echo "0")
            mem=$(ps -p "$PID" -o %mem= 2>/dev/null || echo "0")
            alert "  Process stats: CPU=${cpu}%, MEM=${mem}%"
        fi
    fi

    sleep "$INTERVAL"
done
