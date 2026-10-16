#!/bin/bash
#
# NIMCP Web Demo Startup Script
# =============================
#
# WHAT: Automated startup script for NIMCP web demo
# WHY:  Simplify the process of starting both backend and frontend
# HOW:  Start Flask backend and React frontend in separate processes

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}======================================================================${NC}"
echo -e "${BLUE}NIMCP Web Demo Startup${NC}"
echo -e "${BLUE}======================================================================${NC}"

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BACKEND_DIR="$SCRIPT_DIR/backend"
FRONTEND_DIR="$SCRIPT_DIR/frontend"

# Check if backend venv exists
if [ ! -d "$BACKEND_DIR/venv" ]; then
    echo -e "${YELLOW}Backend virtual environment not found. Creating...${NC}"
    cd "$BACKEND_DIR"
    python3 -m venv venv
    echo -e "${GREEN}✓ Virtual environment created${NC}"

    echo -e "${YELLOW}Installing backend dependencies...${NC}"
    ./venv/bin/pip install -q -r requirements.txt
    echo -e "${GREEN}✓ Backend dependencies installed${NC}"

    echo -e "${YELLOW}Installing NIMCP Python bindings...${NC}"
    cd "$SCRIPT_DIR/../../python"
    "$BACKEND_DIR/venv/bin/pip" install -q -e .
    echo -e "${GREEN}✓ NIMCP Python bindings installed${NC}"
fi

# Check if NIMCP library exists
NIMCP_LIB="$SCRIPT_DIR/../../../bin/libnimcp.so"
if [ ! -f "$NIMCP_LIB" ]; then
    echo -e "${RED}✗ NIMCP library not found at: $NIMCP_LIB${NC}"
    echo -e "${YELLOW}Please build NIMCP first:${NC}"
    echo -e "  cd $SCRIPT_DIR/../../../build"
    echo -e "  cmake .."
    echo -e "  make"
    exit 1
fi

# Check if frontend node_modules exists
if [ ! -d "$FRONTEND_DIR/node_modules" ]; then
    echo -e "${YELLOW}Frontend dependencies not found. Installing...${NC}"
    cd "$FRONTEND_DIR"
    npm install
    echo -e "${GREEN}✓ Frontend dependencies installed${NC}"
fi

# Start backend
echo -e "\n${BLUE}Starting Flask Backend...${NC}"
cd "$BACKEND_DIR"
./venv/bin/python app.py &
BACKEND_PID=$!
echo -e "${GREEN}✓ Backend started (PID: $BACKEND_PID)${NC}"
echo -e "  URL: ${BLUE}http://localhost:5000${NC}"

# Wait for backend to start
echo -e "${YELLOW}Waiting for backend to be ready...${NC}"
for i in {1..30}; do
    if curl -s http://localhost:5000/api/status > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Backend is ready${NC}"
        break
    fi
    sleep 1
done

# Start frontend
echo -e "\n${BLUE}Starting React Frontend...${NC}"
cd "$FRONTEND_DIR"
PORT=3005 npm start &
FRONTEND_PID=$!
echo -e "${GREEN}✓ Frontend started (PID: $FRONTEND_PID)${NC}"
echo -e "  URL: ${BLUE}http://localhost:3005${NC}"

# Save PIDs to file for cleanup
echo "$BACKEND_PID" > /tmp/nimcp-demo-backend.pid
echo "$FRONTEND_PID" > /tmp/nimcp-demo-frontend.pid

echo -e "\n${BLUE}======================================================================${NC}"
echo -e "${GREEN}NIMCP Web Demo is starting!${NC}"
echo -e "${BLUE}======================================================================${NC}"
echo -e "Backend:  ${BLUE}http://localhost:5000${NC}"
echo -e "Frontend: ${BLUE}http://localhost:3005${NC}"
echo -e "\n${YELLOW}To stop the demo, run:${NC}"
echo -e "  $SCRIPT_DIR/stop-demo.sh"
echo -e "\n${YELLOW}Or press Ctrl+C and run:${NC}"
echo -e "  kill $BACKEND_PID $FRONTEND_PID"
echo -e "${BLUE}======================================================================${NC}\n"

# Wait for both processes
wait
