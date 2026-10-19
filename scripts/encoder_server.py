#!/usr/bin/env python3
"""Standalone encoder server - runs in separate process on GPU 1.

Avoids dual-CUDA-context issues by running in separate process from NIMCP brain.
Communicates via Unix socket.
"""

import os
import sys
import socket
import struct
import numpy as np
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)

# Set GPU before importing torch
GPU_ID = int(os.environ.get('ENCODER_GPU', 'cuda:1').split(':')[1]) if ':' in os.environ.get('ENCODER_GPU', 'cuda:1') else 1
os.environ['CUDA_VISIBLE_DEVICES'] = str(GPU_ID)

from sentence_transformers import SentenceTransformer

SOCKET_PATH = '/tmp/nimcp_encoder.sock'
MODEL_NAME = 'BAAI/bge-large-en-v1.5'
# Use cached model path directly
CACHE_PATH = os.path.expanduser('~/.cache/huggingface/hub/models--BAAI--bge-large-en-v1.5/snapshots/default')

def main():
    # Load model on GPU from cache
    logger.info(f"Loading {MODEL_NAME} on GPU {GPU_ID}...")
    if os.path.exists(CACHE_PATH):
        logger.info(f"Using cached model from {CACHE_PATH}")
        model = SentenceTransformer(CACHE_PATH, device='cuda:0')  # cuda:0 because CUDA_VISIBLE_DEVICES filtered
    else:
        logger.warning(f"Cache not found at {CACHE_PATH}, trying online...")
        model = SentenceTransformer(MODEL_NAME, device='cuda:0')
    logger.info(f"Model loaded, actual device: {model.device}")

    # Remove old socket
    try:
        os.unlink(SOCKET_PATH)
    except FileNotFoundError:
        pass

    # Create Unix socket server
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCKET_PATH)
    server.listen(5)
    logger.info(f"Encoder server listening on {SOCKET_PATH}")

    while True:
        conn, _ = server.accept()
        try:
            # Read text length (4 bytes)
            len_data = conn.recv(4)
            if not len_data:
                continue
            text_len = struct.unpack('I', len_data)[0]

            # Read text
            text = conn.recv(text_len).decode('utf-8')

            # Encode
            embedding = model.encode(text, convert_to_numpy=True)

            # Send embedding (1024 floats = 4096 bytes)
            conn.sendall(embedding.astype(np.float32).tobytes())
        except Exception as e:
            logger.error(f"Encoding error: {e}")
        finally:
            conn.close()

if __name__ == '__main__':
    main()
