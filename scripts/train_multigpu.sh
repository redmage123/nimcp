#!/bin/bash
# Multi-GPU Training Launcher for NIMCP
# GPU 0: Brain training (NIMCP C/CUDA kernels)
# GPU 1: Text encoder (SentenceTransformer)

set -e

# Expose both GPUs
export CUDA_VISIBLE_DEVICES=0,1

# Force encoder to use GPU 1
export ENCODER_GPU=cuda:1

# Optional: Set PyTorch to not pre-allocate all memory
export PYTORCH_CUDA_ALLOC_CONF=max_split_size_mb:512

echo "=== Multi-GPU Training Configuration ==="
echo "CUDA_VISIBLE_DEVICES: $CUDA_VISIBLE_DEVICES"
echo "ENCODER_GPU: $ENCODER_GPU"
echo "Brain will use: cuda:0"
echo "Encoder will use: cuda:1"
echo "========================================"

# Start encoder server in background on GPU 1
echo "Starting encoder server on GPU 1..."
python3 /root/nimcp/scripts/encoder_server.py > /tmp/encoder_server.log 2>&1 &
ENCODER_PID=$!
echo "Encoder server PID: $ENCODER_PID"

# Wait for encoder server to be ready
for i in {1..30}; do
    if [ -S /tmp/nimcp_encoder.sock ]; then
        echo "Encoder server ready"
        break
    fi
    sleep 1
done

if [ ! -S /tmp/nimcp_encoder.sock ]; then
    echo "ERROR: Encoder server failed to start"
    kill $ENCODER_PID 2>/dev/null
    exit 1
fi

# Launch training
cd /root/nimcp
python3 scripts/immerse_athena.py "$@"

# Cleanup on exit
kill $ENCODER_PID 2>/dev/null
