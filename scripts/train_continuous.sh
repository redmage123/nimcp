#!/bin/bash
#
# Continuous NIMCP Training - Runs indefinitely
# Survives terminal disconnect
#

# Change to nimcp root directory (parent of scripts)
cd "$(dirname "$0")/.."

LOGFILE="training_log.txt"
SESSION_COUNT=0
TOTAL_EXAMPLES=0

echo "============================================================" | tee -a $LOGFILE
echo "🌊 NIMCP CONTINUOUS TRAINING STARTED" | tee -a $LOGFILE
echo "Started: $(date)" | tee -a $LOGFILE
echo "PID: $$" | tee -a $LOGFILE
echo "Target: 2,000,000 examples (Phase 1)" | tee -a $LOGFILE
echo "Session size: 50,000 examples (~20 minutes)" | tee -a $LOGFILE
echo "============================================================" | tee -a $LOGFILE
echo "" | tee -a $LOGFILE

# Save PID for easy stopping later
echo $$ > training.pid

# Loop indefinitely
while true; do
    SESSION_COUNT=$((SESSION_COUNT + 1))
    TOTAL_EXAMPLES=$((TOTAL_EXAMPLES + 50000))

    echo "" | tee -a $LOGFILE
    echo "============================================================" | tee -a $LOGFILE
    echo "📊 SESSION $SESSION_COUNT" | tee -a $LOGFILE
    echo "Started: $(date)" | tee -a $LOGFILE
    echo "Cumulative examples: $TOTAL_EXAMPLES" | tee -a $LOGFILE
    echo "Phase 1 progress: $(echo "scale=2; $TOTAL_EXAMPLES / 2000000 * 100" | bc)%" | tee -a $LOGFILE
    echo "============================================================" | tee -a $LOGFILE

    # Run training session (script is in scripts directory)
    python3 scripts/start_streaming.py 2>&1 | tee -a $LOGFILE

    EXIT_CODE=${PIPESTATUS[0]}

    if [ $EXIT_CODE -ne 0 ]; then
        echo "⚠️  Session $SESSION_COUNT failed with exit code $EXIT_CODE" | tee -a $LOGFILE
        echo "Retrying in 10 seconds..." | tee -a $LOGFILE
        sleep 10
    else
        echo "✅ Session $SESSION_COUNT complete" | tee -a $LOGFILE
    fi

    # Short pause between sessions
    sleep 2
done
