#!/bin/bash
# NIMCP Dashboard — Launch Script
# Usage: cd /home/bbrelin/nimcp/frontend && bash start.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_LIB="$PROJECT_ROOT/build/lib"
PYTHON_LIB="$BUILD_LIB/python"
WEBDEMO_BACKEND="$PROJECT_ROOT/src/bindings/web-demo/backend"

# Environment
export LD_LIBRARY_PATH="$BUILD_LIB:${LD_LIBRARY_PATH:-}"
export PYTHONPATH="$PYTHON_LIB:$WEBDEMO_BACKEND:$SCRIPT_DIR/backend:${PYTHONPATH:-}"
export NIMCP_ROOT="$PROJECT_ROOT"
export NIMCP_DEV=1

echo "=== NIMCP Dashboard ==="
echo "Project root: $PROJECT_ROOT"
echo "Library path: $BUILD_LIB"
echo ""

# Check nimcp module
python3 -c "import nimcp; print(f'nimcp v{nimcp.version()} loaded')" 2>/dev/null || {
    echo "ERROR: Cannot import nimcp. Build the project first:"
    echo "  cd $PROJECT_ROOT/build && cmake .. && make nimcp nimcp_python -j4"
    exit 1
}

# Install Python deps if needed
if ! python3 -c "import fastapi" 2>/dev/null; then
    echo "Installing Python dependencies..."
    pip3 install -r "$SCRIPT_DIR/backend/requirements.txt"
fi

# Install Node deps and build client if needed
CLIENT_DIR="$SCRIPT_DIR/client"
if [ ! -d "$CLIENT_DIR/node_modules" ]; then
    echo "Installing Node dependencies..."
    (cd "$CLIENT_DIR" && npm install)
fi

# Check for production build
if [ -d "$CLIENT_DIR/dist" ]; then
    echo "Serving production build from client/dist/"
    export NIMCP_DEV=0
else
    echo "No production build found. Run 'cd client && npm run build' for production."
    echo "Starting Vite dev server on :5173..."
    (cd "$CLIENT_DIR" && npm run dev) &
    VITE_PID=$!
    trap "kill $VITE_PID 2>/dev/null; exit" INT TERM
fi

# TLS certificates
CERTS_DIR="$SCRIPT_DIR/backend/certs"
SSL_CERT="$CERTS_DIR/cert.pem"
SSL_KEY="$CERTS_DIR/key.pem"
if [ ! -f "$SSL_CERT" ] || [ ! -f "$SSL_KEY" ]; then
    echo "Generating self-signed TLS certificate..."
    mkdir -p "$CERTS_DIR"
    openssl req -x509 -newkey rsa:2048 \
        -keyout "$SSL_KEY" -out "$SSL_CERT" \
        -days 365 -nodes -subj '/CN=localhost' 2>/dev/null
fi

# Start backend (HTTPS)
echo "Starting FastAPI on https://0.0.0.0:8000 ..."
cd "$SCRIPT_DIR/backend"
python3 -m uvicorn main:app --host 0.0.0.0 --port 8000 \
    --ssl-certfile "$SSL_CERT" --ssl-keyfile "$SSL_KEY" --reload

# Clean up
[ -n "${VITE_PID:-}" ] && kill $VITE_PID 2>/dev/null
