#!/bin/bash
# Restore one checkpoint set from Hetzner to the vast pod's /dev/shm using
# BBR + parallel streams (scripts/parallel_xfer.py), then finalize (links,
# state file, watchdog) with scripts/finalize_restore.sh.
#
# Measured 2026-11-01: a single cubic stream Hetzner->pod ran at 0.07 MB/s;
# BBR on the sender raised it to 2.5 MB/s; 12 parallel streams to 15.9 MB/s.
#
# Usage: bash scripts/restore_checkpoint_pxfer.sh athena_s0_step50615.bin [--no-start]
set -u
S=${1:?snapshot name, e.g. athena_s0_step50615.bin}
NOSTART=${2:-}
H=bbrelin@176.9.99.103
D=/home/bbrelin/nimcp/checkpoints/athena
K=/root/.ssh/id_ed25519_hetzner
NIMCP_DIR=/root/nimcp
sysctl -q -w net.ipv4.tcp_congestion_control=bbr 2>/dev/null || true
mkdir -p /dev/shm/athena_ckpt && cd /dev/shm/athena_ckpt || exit 1
t0=$(date +%s)
rsync -a --partial-dir=.rsync-partial --exclude="*.snn" \
    -e "ssh -i $K -o StrictHostKeyChecking=no" "$H:$D/$S*" ./ || { echo "small-file pull failed"; exit 1; }
echo "small files: $(ls "$S"* | wc -l) in $(( $(date +%s) - t0 ))s"
python3 "$NIMCP_DIR/scripts/parallel_xfer.py" pull "$H:$D/$S.snn" "/dev/shm/athena_ckpt/$S.snn" \
    --streams 12 --chunk-mb 256 --ssh "-i $K" || { echo "snn pull failed"; exit 1; }
[ "$NOSTART" = "--no-start" ] && { echo "restored (not starting)"; exit 0; }
cd "$NIMCP_DIR" && bash scripts/finalize_restore.sh "$S"
