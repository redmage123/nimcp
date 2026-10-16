"""Configuration for NIMCP monitoring frontend backend."""
import os

# Load .env file if present
_env_path = os.path.join(os.path.dirname(__file__), ".env")
if os.path.isfile(_env_path):
    with open(_env_path) as _f:
        for _line in _f:
            _line = _line.strip()
            if _line and not _line.startswith("#") and "=" in _line:
                _k, _, _v = _line.partition("=")
                os.environ.setdefault(_k.strip(), _v.strip())

# Paths
PROJECT_ROOT = os.environ.get("NIMCP_ROOT", "/home/bbrelin/nimcp")
BUILD_LIB = os.path.join(PROJECT_ROOT, "build", "lib")
PYTHON_LIB = os.path.join(BUILD_LIB, "python")
WEBDEMO_BACKEND = os.path.join(PROJECT_ROOT, "src", "bindings", "web-demo", "backend")
CLIENT_DIST = os.path.join(os.path.dirname(os.path.dirname(__file__)), "client", "dist")
UPLOAD_DIR = os.path.join(os.path.dirname(__file__), "uploads")
BRAIN_STORAGE_DIR = os.environ.get("NIMCP_BRAIN_STORAGE_DIR",
                                   os.path.join(os.path.dirname(__file__), "brain_data"))

# Server
HOST = os.environ.get("NIMCP_HOST", "0.0.0.0")
PORT = int(os.environ.get("NIMCP_PORT", "8000"))
DEV_MODE = os.environ.get("NIMCP_DEV", "1") == "1"

# TLS
CERTS_DIR = os.path.join(os.path.dirname(__file__), "certs")
SSL_CERTFILE = os.environ.get("NIMCP_SSL_CERT", os.path.join(CERTS_DIR, "cert.pem"))
SSL_KEYFILE = os.environ.get("NIMCP_SSL_KEY", os.path.join(CERTS_DIR, "key.pem"))

# Authentication
AUTH_USER = os.environ.get("NIMCP_AUTH_USER", "admin")
AUTH_PASS_HASH = os.environ.get("NIMCP_AUTH_PASS_HASH", "")

# Probe
PROBE_INTERVAL_MS = int(os.environ.get("NIMCP_PROBE_INTERVAL", "750"))
PROBE_HISTORY_SIZE = 200

# Limits
MAX_BRAIN_COUNT = int(os.environ.get("NIMCP_MAX_BRAINS", "20"))
MAX_CSV_UPLOAD_BYTES = int(os.environ.get("NIMCP_MAX_CSV_BYTES", str(10 * 1024 * 1024)))
MAX_FEATURE_LENGTH = 10000
MAX_WS_MESSAGE_BYTES = 64 * 1024
C_API_TIMEOUT_SECONDS = float(os.environ.get("NIMCP_C_API_TIMEOUT", "30"))
MAX_AUDIO_SAMPLES = 441000  # 10s at 44.1kHz
MAX_VIDEO_PIXELS = 921600   # 640x480x3

# CORS
CORS_ALLOWED_ORIGINS = [o.strip() for o in os.environ.get("NIMCP_CORS_ORIGINS", "").split(",") if o.strip()]

# Logging
LOG_LEVEL = os.environ.get("NIMCP_LOG_LEVEL", "INFO")

# Defaults
DEFAULT_BRAIN_SIZE = 1  # BRAIN_SMALL
DEFAULT_TASK = 0  # TASK_CLASSIFICATION
