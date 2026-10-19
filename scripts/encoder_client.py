#!/usr/bin/env python3
"""Client for encoder_server - calls remote GPU encoder via Unix socket."""

import socket
import struct
import numpy as np

SOCKET_PATH = '/tmp/nimcp_encoder.sock'

def encode_via_server(text):
    """Encode text via remote encoder server on GPU 1."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        sock.connect(SOCKET_PATH)

        # Send text length + text
        text_bytes = text.encode('utf-8')
        sock.sendall(struct.pack('I', len(text_bytes)))
        sock.sendall(text_bytes)

        # Receive embedding (1024 floats = 4096 bytes)
        embedding_bytes = b''
        while len(embedding_bytes) < 4096:
            chunk = sock.recv(4096 - len(embedding_bytes))
            if not chunk:
                raise ConnectionError("Server closed connection")
            embedding_bytes += chunk

        embedding = np.frombuffer(embedding_bytes, dtype=np.float32)
        return embedding
    finally:
        sock.close()
