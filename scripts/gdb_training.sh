#!/bin/bash
# Run training under gdb to catch heap corruption
cd /home/bbrelin/nimcp

export NIMCP_NO_COW_SIGNAL=1
export TOKENIZERS_PARALLELISM=false
export TQDM_DISABLE=1
export MALLOC_CHECK_=3

gdb -batch \
    -ex "set pagination off" \
    -ex "set logging file /home/bbrelin/nimcp/gdb_crash.log" \
    -ex "set logging enabled on" \
    -ex "run" \
    -ex "bt full" \
    -ex "info registers" \
    -ex "thread apply all bt" \
    -ex "quit" \
    --args python3 scripts/immerse_athena.py --fresh \
    > /home/bbrelin/nimcp/gdb_output.log 2>&1
