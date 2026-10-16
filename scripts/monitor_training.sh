#!/bin/bash
#
# NIMCP Training Monitor - Periodic Status Updates
# Shows training progress every 30 seconds
#

# Change to nimcp root directory (parent of scripts)
cd "$(dirname "$0")/.."

LOGFILE="training_output.log"
STATUS_INTERVAL=30  # seconds

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "============================================================"
echo "🔍 NIMCP TRAINING MONITOR"
echo "============================================================"
echo ""

# Check if training is running
if [ ! -f training.pid ]; then
    echo "❌ No training.pid file found"
    exit 1
fi

TRAIN_PID=$(cat training.pid)

if ! ps -p $TRAIN_PID > /dev/null 2>&1; then
    echo "❌ Training process not running (PID: $TRAIN_PID)"
    exit 1
fi

echo "✓ Training process running (PID: $TRAIN_PID)"
echo ""

# Calculate session start time
SESSION_START=$(stat -c %Y training.pid)
CURRENT_TIME=$(date +%s)
ELAPSED=$((CURRENT_TIME - SESSION_START))
HOURS=$((ELAPSED / 3600))
MINUTES=$(((ELAPSED % 3600) / 60))

echo "Session uptime: ${HOURS}h ${MINUTES}m"
echo "Monitor interval: ${STATUS_INTERVAL}s"
echo "Press Ctrl+C to stop monitoring"
echo "============================================================"
echo ""

# Monitor loop
while true; do
    # Get latest progress line (handle carriage returns by taking last segment)
    # Look at last 10 lines, split by \r, get all progress lines, take the last one
    LATEST=$(tail -10 "$LOGFILE" | tr '\r' '\n' | grep -E "\[.*\/.*\]" | tail -1)

    if [ -n "$LATEST" ]; then
        # Extract numbers
        CURRENT=$(echo "$LATEST" | grep -oP '\[\K[0-9]+' | head -1)
        TOTAL=$(echo "$LATEST" | grep -oP '\/\K[0-9]+' | head -1)
        RATE=$(echo "$LATEST" | grep -oP 'Rate: \K[0-9.]+')

        # Calculate percentages and estimates
        if [ -n "$CURRENT" ] && [ -n "$TOTAL" ] && [ "$TOTAL" -gt 0 ]; then
            PERCENT=$(echo "scale=2; $CURRENT * 100 / $TOTAL" | bc)

            if [ -n "$RATE" ]; then
                RATE_CHECK=$(echo "$RATE > 0" | bc -l 2>/dev/null || echo "0")
                if [ "$RATE_CHECK" = "1" ]; then
                    REMAINING=$((TOTAL - CURRENT))
                    ETA_SEC=$(echo "scale=0; $REMAINING / $RATE" | bc -l)
                    ETA_MIN=$((ETA_SEC / 60))
                fi

                echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} Progress: ${GREEN}${CURRENT}/${TOTAL}${NC} (${PERCENT}%) | Rate: ${YELLOW}${RATE} ex/s${NC} | ETA: ~${ETA_MIN}m"
            else
                echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} Progress: ${GREEN}${CURRENT}/${TOTAL}${NC} (${PERCENT}%)"
            fi
        fi

        # Check for consolidation (handle carriage returns)
        CONSOLIDATION=$(tail -10 "$LOGFILE" | tr '\r' '\n' | grep "Consolidation" | tail -1)
        if [ -n "$CONSOLIDATION" ]; then
            echo -e "  ${YELLOW}💤 Last consolidation:${NC} $(echo "$CONSOLIDATION" | grep -oP 'at \K[0-9]+')"
        fi
    else
        echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} Waiting for progress data..."
    fi

    # Process info
    CPU_MEM=$(ps -p $TRAIN_PID -o %cpu,%mem --no-headers 2>/dev/null)
    if [ -n "$CPU_MEM" ]; then
        CPU=$(echo $CPU_MEM | awk '{print $1}')
        MEM=$(echo $CPU_MEM | awk '{print $2}')
        echo -e "  CPU: ${CPU}% | Memory: ${MEM}%"
    fi

    echo ""

    sleep $STATUS_INTERVAL
done
