#!/bin/bash
# Persistent SSH tunnel to RunPod with auto-reconnect.
# Forwards port 9900 (brain daemon TCP proxy).
# Checks every 30s and reconnects if dropped.

RHOST="root@74.2.96.55"
RPORT=11008
RKEY="$HOME/.ssh/id_ed25519_runpod"
LOCAL_PORT=9900
REMOTE_PORT=9900

while true; do
    # Check if tunnel is alive
    if ! ss -tlnp 2>/dev/null | grep -q ":${LOCAL_PORT}.*ssh"; then
        echo "[$(date)] Tunnel down — reconnecting..."
        pkill -f "ssh -f -N -L ${LOCAL_PORT}" 2>/dev/null
        sleep 1
        ssh -f -N \
            -L ${LOCAL_PORT}:127.0.0.1:${REMOTE_PORT} \
            -i "$RKEY" \
            -p ${RPORT} \
            -o StrictHostKeyChecking=no \
            -o ServerAliveInterval=15 \
            -o ServerAliveCountMax=3 \
            -o ExitOnForwardFailure=yes \
            -o TCPKeepAlive=yes \
            "$RHOST" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "[$(date)] Tunnel established on port ${LOCAL_PORT}"
        else
            echo "[$(date)] Tunnel failed — retry in 30s"
        fi
    fi
    sleep 30
done
