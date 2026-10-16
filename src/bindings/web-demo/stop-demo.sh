#!/bin/bash
#
# NIMCP Web Demo Stop Script
# ==========================
#
# WHAT: Stop all NIMCP web demo processes
# WHY:  Clean shutdown of backend and frontend
# HOW:  Kill processes using saved PIDs

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Stopping NIMCP Web Demo...${NC}"

# Stop backend
if [ -f /tmp/nimcp-demo-backend.pid ]; then
    BACKEND_PID=$(cat /tmp/nimcp-demo-backend.pid)
    if ps -p $BACKEND_PID > /dev/null 2>&1; then
        kill $BACKEND_PID 2>/dev/null || true
        echo -e "${GREEN}✓ Backend stopped (PID: $BACKEND_PID)${NC}"
    fi
    rm /tmp/nimcp-demo-backend.pid
fi

# Stop frontend
if [ -f /tmp/nimcp-demo-frontend.pid ]; then
    FRONTEND_PID=$(cat /tmp/nimcp-demo-frontend.pid)
    if ps -p $FRONTEND_PID > /dev/null 2>&1; then
        kill $FRONTEND_PID 2>/dev/null || true
        echo -e "${GREEN}✓ Frontend stopped (PID: $FRONTEND_PID)${NC}"
    fi
    rm /tmp/nimcp-demo-frontend.pid
fi

# Kill any remaining node/python processes for the demo
pkill -f "react-scripts start" 2>/dev/null || true
pkill -f "backend/venv/bin/python app.py" 2>/dev/null || true

echo -e "${GREEN}NIMCP Web Demo stopped${NC}"
