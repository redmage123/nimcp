#!/bin/bash
#
# vast.ai Pod Bootstrap (minimal — survives everything)
#
# When a vast.ai pod reboots or resets, EVERYTHING is lost: /root/nimcp/,
# compiled nimcp.so, PyTorch install, pip cache, SSH keys, this script.
#
# This bootstrap is the smallest script you need to memorize / save
# externally (email it to yourself, keep it in password manager, etc).
# Its sole job: get the full recovery system back.
#
# USE: paste this whole script into the vast.ai web shell, or run via
#   curl -sL <this-url> | bash
#
# The one-liner to memorize:
#
#   curl -sL https://raw.githubusercontent.com/redmage123/nimcp/master/scripts/vast_pod_bootstrap.sh | bash
#
# After this finishes, the full recovery script is at:
#   /root/nimcp/scripts/vast_pod_recovery.sh
#
# Then run:
#   bash /root/nimcp/scripts/vast_pod_recovery.sh
#
# which handles the rest (PyTorch rebuild, nimcp rebuild, checkpoint restore).

set -u

GITHUB_REPO=https://github.com/redmage123/nimcp.git
NIMCP_DIR=/root/nimcp
LOG=/tmp/bootstrap.log

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$LOG"; }

log "=== Pod bootstrap starting ==="

# 1. Verify we have git + curl (should be preinstalled on vast GPU templates)
if ! command -v git >/dev/null; then
    log "git missing — installing..."
    apt-get update && apt-get install -y git
fi

# 2. Clone the repo if missing
if [ ! -d "$NIMCP_DIR/.git" ]; then
    log "Cloning nimcp repo..."
    if [ -d "$NIMCP_DIR" ]; then
        # Preserve existing checkpoints/scripts if partial
        mv "$NIMCP_DIR" "${NIMCP_DIR}.prebootstrap.$(date +%s)"
    fi
    git clone "$GITHUB_REPO" "$NIMCP_DIR" 2>&1 | tail -3 | tee -a "$LOG"
else
    log "Repo already present, pulling latest..."
    cd "$NIMCP_DIR" && git pull 2>&1 | tail -2 | tee -a "$LOG"
fi

# 3. Make scripts executable
chmod +x "$NIMCP_DIR"/scripts/*.sh 2>/dev/null

# 4. SSH key check — if missing, print instructions (we can't recover this)
HETZNER_KEY=/root/.ssh/id_ed25519_hetzner
if [ ! -f "$HETZNER_KEY" ]; then
    log "============================================================"
    log "WARNING: Hetzner SSH key missing at $HETZNER_KEY"
    log ""
    log "You MUST restore this key before training can checkpoint:"
    log ""
    log "  Option A — regenerate and re-authorize on Hetzner:"
    log "    mkdir -p /root/.ssh && chmod 700 /root/.ssh"
    log "    ssh-keygen -t ed25519 -f $HETZNER_KEY -N ''"
    log "    cat ${HETZNER_KEY}.pub"
    log "    # Then on Hetzner: append the public key to"
    log "    #   /home/bbrelin/.ssh/authorized_keys"
    log ""
    log "  Option B — restore from backup (preferred if available):"
    log "    scp -i /path/to/other-key /path/to/id_ed25519_hetzner \\"
    log "        root@vast-pod:$HETZNER_KEY"
    log "    chmod 600 $HETZNER_KEY"
    log ""
    log "After the SSH key is in place, run:"
    log "  bash $NIMCP_DIR/scripts/vast_pod_recovery.sh"
    log "============================================================"
    exit 1
else
    log "Hetzner SSH key present"
    chmod 600 "$HETZNER_KEY" 2>/dev/null
fi

# 5. Run the full recovery
log "Bootstrap complete. Running full recovery..."
log "This will handle: PyTorch Blackwell, huggingface-hub, nimcp.so rebuild,"
log "checkpoint restore from Hetzner, state sync, cleanup cron, watchdog start."
log ""

exec bash "$NIMCP_DIR/scripts/vast_pod_recovery.sh"
