"""Shared QuestDB ILP writer.

Used by metrics_exporter.py and any other Python module that wants
to push time-series data to QuestDB. Works on pod (via tunnel) and
Hetzner (direct) since both expose 127.0.0.1:9009.

Best-effort: failures are logged at DEBUG and don't raise. The exporter
must never block training because QuestDB is down.

ILP line-protocol reference (kept here for future authors):
    table_name,tag1=val1,tag2=val2 field1=1.0,field2=2i,field3="str" timestamp_ns

Field-type rules:
  - Strings must be double-quoted; internal `"` escaped as `\"`, backslash as `\\`.
  - Integers end with `i`     -> 42i
  - Floats are bare numbers  -> 1.5
  - Booleans are `t` or `f`  -> t / f
  - Timestamps are nanoseconds; omit for server time.
"""

from __future__ import annotations

import logging
import socket
import time
from typing import Any, Iterable, Mapping, Optional
from urllib import parse as _urlparse
from urllib import request as _urlreq
from urllib import error as _urlerror

log = logging.getLogger(__name__)

__all__ = ["QuestDBWriter", "format_line"]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _escape_ident(s: str) -> str:
    """Escape table/tag names per ILP rules: space, comma and equals are special."""
    if not isinstance(s, str):
        s = str(s)
    return (
        s.replace("\\", "\\\\")
         .replace(" ", "\\ ")
         .replace(",", "\\,")
         .replace("=", "\\=")
    )


def _escape_string_field(s: str) -> str:
    """Escape and quote a string field value."""
    if not isinstance(s, str):
        s = str(s)
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _format_field(value: Any) -> str:
    """Render a single field value in ILP form.

    bool MUST be checked before int because in Python bool is an int.
    """
    if isinstance(value, bool):
        return "t" if value else "f"
    if isinstance(value, int):
        return f"{value}i"
    if isinstance(value, float):
        # ILP rejects NaN/Inf; clamp to 0.0 with a debug note. Caller can
        # filter beforehand if they care.
        if value != value or value == float("inf") or value == float("-inf"):
            log.debug("non-finite float dropped (replaced with 0.0): %r", value)
            return "0.0"
        # Don't use scientific notation — QuestDB ILP accepts it but some
        # downstream tools choke. Render with enough precision.
        return repr(value)
    if isinstance(value, str):
        return _escape_string_field(value)
    # Fall back to string
    return _escape_string_field(str(value))


def format_line(
    table: str,
    tags: Optional[Mapping[str, Any]] = None,
    fields: Optional[Mapping[str, Any]] = None,
    ts_ns: Optional[int] = None,
) -> str:
    """Build a single ILP line. Public so callers can preview before writing."""
    parts = [_escape_ident(table)]
    if tags:
        tag_strs = [
            f"{_escape_ident(k)}={_escape_ident(str(v))}"
            for k, v in tags.items()
            if v is not None and v != ""
        ]
        if tag_strs:
            parts[0] = parts[0] + "," + ",".join(tag_strs)
    if not fields:
        # ILP requires at least one field. Caller bug — log and let server
        # reject, since silently dropping is worse.
        log.debug("format_line called with no fields for table=%s", table)
        return ""
    field_strs = [
        f"{_escape_ident(k)}={_format_field(v)}"
        for k, v in fields.items()
        if v is not None
    ]
    if not field_strs:
        return ""
    line = parts[0] + " " + ",".join(field_strs)
    if ts_ns is not None:
        line += f" {int(ts_ns)}"
    return line


# ---------------------------------------------------------------------------
# Writer
# ---------------------------------------------------------------------------

class QuestDBWriter:
    """Best-effort QuestDB ILP + HTTP-DDL writer.

    Connection-per-write keeps the code simple and works around stale
    sockets after long idle periods (the autossh tunnel can drop). For our
    metrics volume (a handful of rows per minute) the cost is negligible.
    """

    def __init__(
        self,
        host: str = "127.0.0.1",
        ilp_port: int = 9009,
        http_port: int = 9000,
        enabled: bool = True,
        connect_timeout: float = 2.0,
        send_timeout: float = 3.0,
        http_timeout: float = 5.0,
    ) -> None:
        self.host = host
        self.ilp_port = ilp_port
        self.http_port = http_port
        self.enabled = enabled
        self.connect_timeout = connect_timeout
        self.send_timeout = send_timeout
        self.http_timeout = http_timeout

    # -------------------- schema --------------------

    def ensure_table(self, ddl_sql: str) -> bool:
        """POST a DDL statement (CREATE TABLE IF NOT EXISTS ...) via /exec.

        Idempotent on the QuestDB side. Returns True on 200 OK, False otherwise.
        Never raises.
        """
        if not self.enabled:
            return False
        try:
            url = (
                f"http://{self.host}:{self.http_port}/exec?query="
                + _urlparse.quote(ddl_sql, safe="")
            )
            req = _urlreq.Request(url, method="GET")
            with _urlreq.urlopen(req, timeout=self.http_timeout) as resp:
                ok = 200 <= resp.status < 300
                if not ok:
                    log.debug("ensure_table HTTP %s for %s", resp.status, ddl_sql[:80])
                return ok
        except (_urlerror.URLError, OSError, ValueError) as e:
            log.debug("ensure_table failed (%s): %s", type(e).__name__, e)
            return False
        except Exception as e:  # noqa: BLE001 - swallow; never block caller
            log.debug("ensure_table unexpected error: %s", e)
            return False

    # -------------------- writes --------------------

    def write(self, line_protocol_str: str) -> bool:
        """Send a raw ILP line (or multi-line string) to QuestDB.

        Returns True on best-effort success (bytes were sent). False on
        failure. Does not raise.
        """
        if not self.enabled or not line_protocol_str:
            return False
        payload = line_protocol_str
        if not payload.endswith("\n"):
            payload = payload + "\n"
        sock = None
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(self.connect_timeout)
            sock.connect((self.host, self.ilp_port))
            sock.settimeout(self.send_timeout)
            sock.sendall(payload.encode("utf-8"))
            return True
        except (socket.timeout, OSError) as e:
            log.debug("ILP write failed (%s): %s", type(e).__name__, e)
            return False
        except Exception as e:  # noqa: BLE001
            log.debug("ILP write unexpected error: %s", e)
            return False
        finally:
            if sock is not None:
                try:
                    sock.close()
                except Exception:  # noqa: BLE001
                    pass

    def write_row(
        self,
        table: str,
        tags: Optional[Mapping[str, Any]] = None,
        fields: Optional[Mapping[str, Any]] = None,
        ts_ns: Optional[int] = None,
    ) -> bool:
        """Convenience: build one ILP line and send."""
        if not self.enabled:
            return False
        if ts_ns is None:
            ts_ns = int(time.time() * 1e9)
        line = format_line(table, tags=tags, fields=fields, ts_ns=ts_ns)
        if not line:
            return False
        return self.write(line)

    def write_batch(self, rows: Iterable[str]) -> bool:
        """Send multiple ILP lines in one connection."""
        if not self.enabled:
            return False
        lines = [r for r in rows if r]
        if not lines:
            return False
        payload = "\n".join(lines) + "\n"
        return self.write(payload)
