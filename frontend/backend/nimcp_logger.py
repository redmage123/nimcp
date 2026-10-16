"""Centralized structured logging with per-request ID tracking."""
import contextvars
import logging
import uuid

_request_id: contextvars.ContextVar[str] = contextvars.ContextVar("request_id", default="-")


def new_request_id() -> str:
    """Generate a 12-char hex request ID and store in context."""
    rid = uuid.uuid4().hex[:12]
    _request_id.set(rid)
    return rid


def get_request_id() -> str:
    return _request_id.get()


class RequestIDFilter(logging.Filter):
    """Injects request_id into every log record."""

    def filter(self, record: logging.LogRecord) -> bool:
        record.request_id = _request_id.get()
        return True


def setup_logging(level: str = "INFO") -> None:
    """Configure root logger with structured format."""
    fmt = "[%(asctime)s] %(levelname)s [%(request_id)s] %(name)s — %(message)s"
    handler = logging.StreamHandler()
    handler.setFormatter(logging.Formatter(fmt, datefmt="%Y-%m-%d %H:%M:%S"))
    handler.addFilter(RequestIDFilter())

    root = logging.getLogger()
    root.setLevel(getattr(logging, level.upper(), logging.INFO))
    # Remove existing handlers to avoid duplicates on reload
    root.handlers.clear()
    root.addHandler(handler)


def get(name: str) -> logging.Logger:
    """Return a namespaced logger."""
    return logging.getLogger(f"nimcp.{name}")
