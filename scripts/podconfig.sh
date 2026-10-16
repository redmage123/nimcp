# Pod connection config — sourced by deploy/sync/tunnel scripts.
#
# RunPod pods get NEW SSH ports on every restart. Update POD_PORT below when
# the current pod restarts; all scripts that source this file pick up the new
# value. Everything else is stable (IP, key path, on-pod workspace path).
#
# Override any value via env: `POD_PORT=12345 ./deploy_to_pod.sh`

POD_HOST="${POD_HOST:-root@213.173.103.76}"
POD_PORT="${POD_PORT:-31527}"
POD_KEY="${POD_KEY:-$HOME/.ssh/id_ed25519_runpod}"
POD_DIR="${POD_DIR:-/workspace/nimcp}"
POD_PY_SITE="${POD_PY_SITE:-/usr/local/lib/python3.12/dist-packages}"
POD_SOCKET="${POD_SOCKET:-/var/run/athena/brain.sock}"
POD_LOGDIR="${POD_LOGDIR:-/var/log/athena}"
POD_CKPT="${POD_CKPT:-/workspace/nimcp/checkpoints/athena/athena_immersive.bin}"

# SNN language-bridge produce length control, applied via the
# set_length_control RPC right after the daemon comes up in a full deploy.
# min=3 is the floor that unblocks the cascade self-train trigram path
# (bigram needs >=2 tokens, trigram >=3) — without it an undertrained
# bridge collapses to a 1-word utterance. max=0 keeps the legacy implicit
# 32-word cap. Set POD_MIN_PRODUCE_WORDS=0 to skip activation entirely.
POD_MIN_PRODUCE_WORDS="${POD_MIN_PRODUCE_WORDS:-3}"
POD_MAX_PRODUCE_WORDS="${POD_MAX_PRODUCE_WORDS:-0}"

# Common ssh/scp commands with all the right flags — source this file and use
# directly as `$POD_SSH "remote command"` or `$POD_SCP local $POD_HOST:path`.
POD_SSH="ssh -o ConnectTimeout=10 -o StrictHostKeyChecking=no -i $POD_KEY -p $POD_PORT $POD_HOST"
POD_SCP="scp -o ConnectTimeout=10 -o StrictHostKeyChecking=no -i $POD_KEY -P $POD_PORT"
