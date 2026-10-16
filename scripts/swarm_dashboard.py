#!/usr/bin/env python3
"""
Swarm Telemetry Dashboard — lightweight browser-based monitor for NIMCP swarm.

Reads JSON telemetry from a file (written by the master runtime) and serves
an auto-refreshing HTML page.  Stdlib only, no pip dependencies.

Usage:
    python3 scripts/swarm_dashboard.py --telemetry /tmp/nimcp_swarm.json --port 8080
"""

import argparse
import html
import http.server
import json
import os
import time
import socketserver
from urllib.parse import urlparse, parse_qs

# ---------------------------------------------------------------------------
# Telemetry reader
# ---------------------------------------------------------------------------

def read_telemetry(path: str) -> dict:
    """Read and parse the JSON telemetry file.  Returns empty dict on error."""
    try:
        if not os.path.exists(path):
            return {}
        with open(path, "r") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return {}

# ---------------------------------------------------------------------------
# HTML rendering
# ---------------------------------------------------------------------------

STATE_COLORS = {
    "active":    "#27ae60",
    "alive":     "#27ae60",
    "suspected": "#f39c12",
    "dead":      "#e74c3c",
    "byzantine": "#8e44ad",
    "joining":   "#3498db",
    "unknown":   "#95a5a6",
}

def _state_badge(state: str) -> str:
    s = state.lower() if state else "unknown"
    color = STATE_COLORS.get(s, STATE_COLORS["unknown"])
    return (f'<span style="background:{color};color:#fff;padding:2px 8px;'
            f'border-radius:4px;font-size:0.85em;">{html.escape(state)}</span>')

def _time_ago(ts) -> str:
    if not ts:
        return "N/A"
    try:
        delta = time.time() - float(ts)
        if delta < 0:
            return "future?"
        if delta < 60:
            return f"{delta:.0f}s ago"
        if delta < 3600:
            return f"{delta/60:.1f}m ago"
        return f"{delta/3600:.1f}h ago"
    except (ValueError, TypeError):
        return "N/A"

def render_dashboard(data: dict, telemetry_path: str) -> str:
    """Render the full HTML dashboard page from telemetry data."""

    # --- Extract top-level stats ---
    peers = data.get("peers", [])
    sync = data.get("sync", {})
    swarm_name = data.get("swarm_name", "NIMCP Swarm")
    timestamp = data.get("timestamp", "")

    total      = len(peers)
    active     = sum(1 for p in peers if p.get("state", "").lower() in ("active", "alive"))
    suspected  = sum(1 for p in peers if p.get("state", "").lower() == "suspected")
    dead       = sum(1 for p in peers if p.get("state", "").lower() == "dead")
    byzantine  = sum(1 for p in peers if p.get("state", "").lower() == "byzantine")

    # --- Peer table rows ---
    peer_rows = ""
    for p in peers:
        did = html.escape(str(p.get("device_id", "?")))
        state = p.get("state", "unknown")
        addr = html.escape(str(p.get("address", "")))
        hb = _time_ago(p.get("last_heartbeat"))
        grad = p.get("gradient_norm_ema", 0.0)
        anom = p.get("anomaly_count", 0)
        peer_rows += f"""
        <tr>
            <td style="font-family:monospace;">{did}</td>
            <td>{_state_badge(state)}</td>
            <td>{addr}</td>
            <td>{hb}</td>
            <td>{grad:.4f}</td>
            <td>{anom}</td>
        </tr>"""

    if not peer_rows:
        peer_rows = '<tr><td colspan="6" style="text-align:center;color:#888;">No peers detected</td></tr>'

    # --- Sync info ---
    sync_round    = sync.get("round", "?")
    sync_phase    = html.escape(str(sync.get("phase", "?")))
    grads_recv    = sync.get("gradients_received", 0)
    grads_expect  = sync.get("gradients_expected", 0)

    file_age = ""
    try:
        mtime = os.path.getmtime(telemetry_path)
        file_age = _time_ago(mtime)
    except OSError:
        file_age = "unknown"

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta http-equiv="refresh" content="5">
<title>{html.escape(swarm_name)} — Swarm Dashboard</title>
<style>
  * {{ margin:0; padding:0; box-sizing:border-box; }}
  body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
         background: #1a1a2e; color: #e0e0e0; padding: 20px; }}
  h1 {{ color: #00d2ff; margin-bottom: 6px; }}
  .subtitle {{ color: #888; font-size: 0.85em; margin-bottom: 20px; }}
  .cards {{ display: flex; gap: 16px; flex-wrap: wrap; margin-bottom: 24px; }}
  .card {{ background: #16213e; border-radius: 8px; padding: 16px 24px; min-width: 140px;
           border-left: 4px solid #00d2ff; }}
  .card .label {{ font-size: 0.8em; color: #888; text-transform: uppercase; }}
  .card .value {{ font-size: 2em; font-weight: bold; color: #fff; }}
  .card.alert .value {{ color: #e74c3c; }}
  .card.warn .value {{ color: #f39c12; }}
  .card.ok .value {{ color: #27ae60; }}
  table {{ width: 100%; border-collapse: collapse; background: #16213e;
           border-radius: 8px; overflow: hidden; margin-bottom: 24px; }}
  th {{ background: #0f3460; text-align: left; padding: 10px 14px; color: #00d2ff;
        font-size: 0.85em; text-transform: uppercase; }}
  td {{ padding: 10px 14px; border-bottom: 1px solid #1a1a2e; }}
  tr:hover {{ background: #1a2744; }}
  .sync-box {{ background: #16213e; border-radius: 8px; padding: 16px 24px;
               display: inline-block; }}
  .sync-box span {{ margin-right: 24px; }}
  .sync-label {{ color: #888; font-size: 0.8em; text-transform: uppercase; }}
  .sync-val {{ font-size: 1.2em; font-weight: bold; }}
  .footer {{ color: #555; font-size: 0.75em; margin-top: 20px; }}
</style>
</head>
<body>
  <h1>{html.escape(swarm_name)}</h1>
  <p class="subtitle">Telemetry: {html.escape(telemetry_path)} &mdash; file updated {file_age} &mdash; page refreshes every 5s</p>

  <div class="cards">
    <div class="card"><div class="label">Total Peers</div><div class="value">{total}</div></div>
    <div class="card ok"><div class="label">Active</div><div class="value">{active}</div></div>
    <div class="card warn"><div class="label">Suspected</div><div class="value">{suspected}</div></div>
    <div class="card alert"><div class="label">Dead</div><div class="value">{dead}</div></div>
    <div class="card{'alert' if byzantine else ''}"><div class="label">Byzantine</div><div class="value">{byzantine}</div></div>
  </div>

  <h2 style="color:#00d2ff;margin-bottom:10px;">Peer Table</h2>
  <table>
    <thead>
      <tr>
        <th>Device ID</th><th>State</th><th>Address</th>
        <th>Last Heartbeat</th><th>Grad Norm EMA</th><th>Anomalies</th>
      </tr>
    </thead>
    <tbody>
      {peer_rows}
    </tbody>
  </table>

  <h2 style="color:#00d2ff;margin-bottom:10px;">Sync Round</h2>
  <div class="sync-box">
    <span><span class="sync-label">Round</span><br><span class="sync-val">{sync_round}</span></span>
    <span><span class="sync-label">Phase</span><br><span class="sync-val">{sync_phase}</span></span>
    <span><span class="sync-label">Gradients</span><br><span class="sync-val">{grads_recv} / {grads_expect}</span></span>
  </div>

  <p class="footer">NIMCP Swarm Dashboard &mdash; stdlib only, no external dependencies</p>
</body>
</html>"""

# ---------------------------------------------------------------------------
# HTTP handler
# ---------------------------------------------------------------------------

class DashboardHandler(http.server.BaseHTTPRequestHandler):
    telemetry_path = "/tmp/nimcp_swarm.json"

    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path == "/api/telemetry":
            data = read_telemetry(self.telemetry_path)
            payload = json.dumps(data, indent=2).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return

        # Default: serve HTML dashboard
        data = read_telemetry(self.telemetry_path)
        page = render_dashboard(data, self.telemetry_path).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(page)))
        self.end_headers()
        self.wfile.write(page)

    def log_message(self, format, *args):
        # Quieter logging
        pass

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="NIMCP Swarm Telemetry Dashboard")
    parser.add_argument("--telemetry", default="/tmp/nimcp_swarm.json",
                        help="Path to JSON telemetry file (default: /tmp/nimcp_swarm.json)")
    parser.add_argument("--port", type=int, default=8080,
                        help="HTTP port (default: 8080)")
    args = parser.parse_args()

    DashboardHandler.telemetry_path = args.telemetry

    with socketserver.TCPServer(("", args.port), DashboardHandler) as httpd:
        httpd.allow_reuse_address = True
        print(f"[NIMCP Swarm Dashboard] Serving on http://localhost:{args.port}")
        print(f"[NIMCP Swarm Dashboard] Telemetry file: {args.telemetry}")
        print(f"[NIMCP Swarm Dashboard] API endpoint: http://localhost:{args.port}/api/telemetry")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n[NIMCP Swarm Dashboard] Shutting down.")

if __name__ == "__main__":
    main()
