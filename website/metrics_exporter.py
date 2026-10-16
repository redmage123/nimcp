#!/usr/bin/env python3
"""Exports brain metrics to a JSON file for the website to consume.

Runs as a daemon, queries the brain daemon every 10 seconds, writes
metrics.json to the website directory (served as static file by nginx).

Also dual-writes a subset of the metrics to QuestDB on each scrape.
QuestDB writes are best-effort: a tunnel/server outage NEVER breaks
the JSON path that the website relies on.
"""
import json
import sys
import time
import os
import logging

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts'))

# Optional QuestDB writer — exporter still runs if the module is missing.
try:
    from questdb_writer import QuestDBWriter
    _QDB_AVAILABLE = True
except Exception as _qdb_exc:  # noqa: BLE001
    QuestDBWriter = None  # type: ignore[assignment]
    _QDB_AVAILABLE = False
    print(f"[metrics_exporter] questdb_writer unavailable ({_qdb_exc}); "
          f"continuing with JSON-only output", flush=True)

METRICS_FILE = os.path.join(os.path.dirname(__file__), 'metrics.json')
IMMERSIVE_STATE = '/workspace/nimcp/checkpoints/athena/immersive_state.json'
INTERVAL = 60  # seconds (5 queries × 25-30s worst case = allow full minute)

# QuestDB feature gate — env var lets ops kill the dual-write without
# editing code. Default ON when the module is importable.
QUESTDB_ENABLED = os.environ.get('QUESTDB_ENABLED', '1').lower() not in (
    '0', 'false', 'no', ''
)
QUESTDB_HOST = os.environ.get('QUESTDB_HOST', '127.0.0.1')
QUESTDB_ILP_PORT = int(os.environ.get('QUESTDB_ILP_PORT', '9009'))
QUESTDB_HTTP_PORT = int(os.environ.get('QUESTDB_HTTP_PORT', '9000'))

# DDL for tables we own. CREATE TABLE IF NOT EXISTS — safe to run on every
# startup. Schema names match the spec in the QuestDB persistence task.
QUESTDB_SCHEMA_DDL = [
    # training_metrics: per-scrape rollup of cross-network state
    "CREATE TABLE IF NOT EXISTS training_metrics ("
    "  timestamp TIMESTAMP,"
    "  stage INT,"
    "  step INT,"
    "  ann_loss DOUBLE,"
    "  cnn_loss DOUBLE,"
    "  snn_loss DOUBLE,"
    "  lnn_loss DOUBLE,"
    "  ann_steps LONG,"
    "  cnn_steps LONG,"
    "  snn_steps LONG,"
    "  lnn_steps LONG,"
    "  hnn_energy DOUBLE,"
    "  hnn_deviation DOUBLE,"
    "  fno_visual_loss DOUBLE,"
    "  fno_audio_loss DOUBLE,"
    "  fno_speech_loss DOUBLE,"
    "  fno_somato_loss DOUBLE,"
    "  fno_visual_steps LONG,"
    "  fno_audio_steps LONG,"
    "  fno_speech_steps LONG,"
    "  fno_somato_steps LONG,"
    "  snn_rate_hz DOUBLE,"
    "  snn_sparsity DOUBLE,"
    "  trn_mean_rate_hz DOUBLE,"
    "  arousal DOUBLE,"
    "  sleep_pressure DOUBLE,"
    "  gl_vocab_size LONG,"
    "  gl_total_bindings LONG,"
    "  gl_total_groundings LONG,"
    "  gl_total_productions LONG,"
    "  gl_templates_learned LONG,"
    "  gl_snn_bridge_blend DOUBLE"
    ") timestamp(timestamp) PARTITION BY DAY",

    # cortex_cnn_metrics: one row per modality per scrape
    "CREATE TABLE IF NOT EXISTS cortex_cnn_metrics ("
    "  timestamp TIMESTAMP,"
    "  modality SYMBOL,"
    "  last_loss DOUBLE,"
    "  ema_loss DOUBLE,"
    "  forward_steps LONG,"
    "  backward_steps LONG,"
    "  embedding_norm DOUBLE,"
    "  confidence DOUBLE,"
    "  embedding_dim INT,"
    "  num_params INT"
    ") timestamp(timestamp) PARTITION BY DAY",

    # immune_state: best-effort — daemon may not expose all columns yet
    "CREATE TABLE IF NOT EXISTS immune_state ("
    "  timestamp TIMESTAMP,"
    "  antibody_count LONG,"
    "  t_cell_count LONG,"
    "  b_cell_count LONG,"
    "  antigen_count LONG,"
    "  inflammation_level DOUBLE,"
    "  cytokine_il1 DOUBLE,"
    "  cytokine_il6 DOUBLE,"
    "  cytokine_il10 DOUBLE,"
    "  cytokine_tnf DOUBLE,"
    "  cytokine_ifn DOUBLE,"
    "  cytokine_il4 DOUBLE,"
    "  treg_storm_engaged BOOLEAN,"
    "  cytokines_damped LONG"
    ") timestamp(timestamp) PARTITION BY DAY",

    # bbb_threats: future-facing; populated when daemon exposes BBB telemetry
    "CREATE TABLE IF NOT EXISTS bbb_threats ("
    "  timestamp TIMESTAMP,"
    "  threat_type SYMBOL,"
    "  severity SYMBOL,"
    "  layer SYMBOL,"
    "  source SYMBOL,"
    "  sample STRING,"
    "  blocked BOOLEAN"
    ") timestamp(timestamp) PARTITION BY DAY",
]

# Whitelist of training_metrics columns that we recognise. Used to filter
# the metrics dict so we never push unknown keys (would cause schema drift
# the moment anyone adds a new metric).
TRAINING_METRICS_COLUMNS = {
    'stage': int, 'step': int,
    'ann_loss': float, 'cnn_loss': float, 'snn_loss': float, 'lnn_loss': float,
    'ann_steps': int, 'cnn_steps': int, 'snn_steps': int, 'lnn_steps': int,
    'hnn_energy': float, 'hnn_deviation': float,
    'fno_visual_loss': float, 'fno_audio_loss': float,
    'fno_speech_loss': float, 'fno_somato_loss': float,
    'fno_visual_steps': int, 'fno_audio_steps': int,
    'fno_speech_steps': int, 'fno_somato_steps': int,
    'snn_rate_hz': float, 'snn_sparsity': float,
    'trn_mean_rate_hz': float, 'arousal': float, 'sleep_pressure': float,
    'gl_vocab_size': int, 'gl_total_bindings': int,
    'gl_total_groundings': int, 'gl_total_productions': int,
    'gl_templates_learned': int, 'gl_snn_bridge_blend': float,
}

CORTEX_FIELD_TYPES = {
    'last_loss': float, 'ema_loss': float,
    'forward_steps': int, 'backward_steps': int,
    'embedding_norm': float, 'confidence': float,
    'embedding_dim': int, 'num_params': int,
}

IMMUNE_FIELD_TYPES = {
    'antibody_count': int, 't_cell_count': int, 'b_cell_count': int,
    'antigen_count': int, 'inflammation_level': float,
    'cytokine_il1': float, 'cytokine_il6': float, 'cytokine_il10': float,
    'cytokine_tnf': float, 'cytokine_ifn': float, 'cytokine_il4': float,
    'treg_storm_engaged': bool, 'cytokines_damped': int,
}


# Module-level QuestDB state (initialised in main()).
_qdb = None
_qdb_init_done = False
_qdb_unreachable_since = 0.0  # epoch seconds; 0 means "no failure pending"
_qdb_unreachable_log_interval = 60.0


def _qdb_init():
    """Construct the writer + push DDL once. Safe to call repeatedly."""
    global _qdb, _qdb_init_done
    if _qdb_init_done:
        return
    _qdb_init_done = True
    if not (_QDB_AVAILABLE and QUESTDB_ENABLED):
        print(f"[metrics_exporter] QuestDB push disabled "
              f"(available={_QDB_AVAILABLE}, enabled={QUESTDB_ENABLED})",
              flush=True)
        return
    try:
        _qdb = QuestDBWriter(
            host=QUESTDB_HOST,
            ilp_port=QUESTDB_ILP_PORT,
            http_port=QUESTDB_HTTP_PORT,
            enabled=True,
        )
        ddl_ok = sum(1 for ddl in QUESTDB_SCHEMA_DDL if _qdb.ensure_table(ddl))
        print(f"[metrics_exporter] QuestDB writer ready "
              f"(host={QUESTDB_HOST}:{QUESTDB_ILP_PORT}, "
              f"DDL ok {ddl_ok}/{len(QUESTDB_SCHEMA_DDL)})", flush=True)
    except Exception as e:  # noqa: BLE001
        print(f"[metrics_exporter] QuestDB init failed: {e}", flush=True)
        _qdb = None


def _qdb_log_unreachable():
    """Throttle the 'QuestDB unreachable' log to once per minute."""
    global _qdb_unreachable_since
    now = time.time()
    if now - _qdb_unreachable_since >= _qdb_unreachable_log_interval:
        _qdb_unreachable_since = now
        print("[metrics_exporter] QuestDB unreachable — continuing JSON-only",
              flush=True)


def _coerce(value, target_type):
    """Best-effort cast to the column type. Drop on failure."""
    if value is None:
        return None
    try:
        if target_type is bool:
            return bool(value)
        if target_type is int:
            return int(value)
        if target_type is float:
            f = float(value)
            if f != f or f in (float('inf'), float('-inf')):
                return None
            return f
    except (TypeError, ValueError):
        return None
    return None


def _push_to_questdb(metrics, ts_ns):
    """Best-effort dual-write of one scrape into QuestDB."""
    if _qdb is None:
        return
    sent = False
    try:
        # training_metrics — flatten + filter by whitelist
        train_fields = {}
        # `metrics` keys we care about. Some come as nested dicts; flatten.
        # `current_stage`/`current_step` map to `stage`/`step`.
        if isinstance(metrics.get('current_stage'), int):
            train_fields['stage'] = metrics['current_stage']
        if isinstance(metrics.get('current_step'), int):
            train_fields['step'] = metrics['current_step']
        for col, typ in TRAINING_METRICS_COLUMNS.items():
            if col in ('stage', 'step'):
                continue  # already handled above
            if col in metrics:
                v = _coerce(metrics[col], typ)
                if v is not None:
                    train_fields[col] = v
        if train_fields:
            sent |= _qdb.write_row('training_metrics', fields=train_fields,
                                   ts_ns=ts_ns)

        # cortex_cnn_metrics — one row per modality
        cortex = metrics.get('cortex')
        if isinstance(cortex, dict):
            lines = []
            for modality, c in cortex.items():
                if not isinstance(c, dict):
                    continue
                # The exporter today flattens c to {'loss', 'ema_loss', 'fwd', 'bwd'};
                # remap to schema names.
                remap = {
                    'last_loss': c.get('last_loss', c.get('loss')),
                    'ema_loss': c.get('ema_loss'),
                    'forward_steps': c.get('forward_steps', c.get('fwd')),
                    'backward_steps': c.get('backward_steps', c.get('bwd')),
                    'embedding_norm': c.get('embedding_norm'),
                    'confidence': c.get('confidence'),
                    'embedding_dim': c.get('embedding_dim'),
                    'num_params': c.get('num_params'),
                }
                fields = {}
                for k, typ in CORTEX_FIELD_TYPES.items():
                    v = _coerce(remap.get(k), typ)
                    if v is not None:
                        fields[k] = v
                if not fields:
                    continue
                from questdb_writer import format_line  # local import keeps gate
                line = format_line(
                    'cortex_cnn_metrics',
                    tags={'modality': modality},
                    fields=fields,
                    ts_ns=ts_ns,
                )
                if line:
                    lines.append(line)
            if lines:
                sent |= _qdb.write_batch(lines)

        # immune_state — daemon doesn't expose this yet, but if any of the
        # known keys ever appear in `metrics`, push them.
        immune_fields = {}
        for k, typ in IMMUNE_FIELD_TYPES.items():
            if k in metrics:
                v = _coerce(metrics[k], typ)
                if v is not None:
                    immune_fields[k] = v
        if immune_fields:
            sent |= _qdb.write_row('immune_state', fields=immune_fields,
                                   ts_ns=ts_ns)

    except Exception as e:  # noqa: BLE001
        # Belt-and-braces: writer methods already swallow, but defend the
        # main loop against any unexpected error from this path.
        logging.getLogger(__name__).debug("questdb push raised: %s", e)
        sent = False

    if not sent:
        _qdb_log_unreachable()


def _safe_query(b, cmd, timeout=10):
    """Query daemon with per-call timeout, return None on failure."""
    import socket as _socket
    sock = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect(b.socket_path)
        import json, struct
        data = json.dumps({'cmd': cmd}).encode('utf-8')
        sock.sendall(struct.pack('>I', len(data)) + data)
        hdr = b''
        while len(hdr) < 4:
            chunk = sock.recv(4 - len(hdr))
            if not chunk: return None
            hdr += chunk
        length = struct.unpack('>I', hdr)[0]
        body = b''
        while len(body) < length:
            chunk = sock.recv(min(length - len(body), 65536))
            if not chunk: return None
            body += chunk
        return json.loads(body.decode('utf-8'))
    except Exception:
        return None
    finally:
        sock.close()


def collect_metrics():
    """Query brain daemon and return metrics dict.

    Each query is independent — a slow or failed query doesn't block others.
    Uses _send_once (no retry) to avoid blocking for minutes on a dead daemon.
    """
    from brain_client import BrainProxy
    b = BrainProxy(timeout=10)
    metrics = {'timestamp': time.time(), 'ok': False}

    # Status — daemon under heavy load can take 20-30s to respond
    # because the single lock serializes all requests behind learn_vector calls
    r = _safe_query(b, 'status', timeout=30)
    if r and not r.get('error'):
        metrics['uptime'] = r.get('uptime', 0)
        metrics['learn_calls'] = r.get('learn_calls', 0)
        metrics['infer_calls'] = r.get('infer_calls', 0)
        metrics['errors'] = r.get('errors', 0)
        metrics['ok'] = True
    else:
        return metrics  # Daemon down — return partial (still written to file)

    # Neuron count
    r = _safe_query(b, 'get_neuron_count', timeout=15)
    if r:
        metrics['neuron_count'] = r.get('neuron_count', 0)

    # Network metrics — can be very slow under load (10-20s)
    r = _safe_query(b, 'get_network_metrics', timeout=25)
    if r:
        m = r.get('metrics', r)
        metrics['ann_loss'] = m.get('ann_loss', 0)
        metrics['cnn_loss'] = m.get('cnn_loss', 0)
        metrics['snn_loss'] = m.get('snn_loss', 0)
        metrics['lnn_loss'] = m.get('lnn_loss', 0)
        metrics['ann_steps'] = m.get('ann_steps', 0)

    # SNN stats
    r = _safe_query(b, 'get_snn_stats', timeout=15)
    if r:
        s = r.get('snn', r)
        metrics['snn_spikes'] = s.get('total_spikes', 0)
        metrics['snn_rate_hz'] = s.get('mean_firing_rate_hz', 0)
        metrics['snn_sparsity'] = s.get('sparsity', 0)
        metrics['snn_silent'] = s.get('silent_neurons', 0)
        metrics['snn_populations'] = s.get('n_populations', 0)

    # Cortex CNN — can be very slow under load (10-20s)
    r = _safe_query(b, 'get_cortex_cnn_metrics', timeout=25)
    if r:
        m = r.get('metrics', r)
        cortex = {}
        for name in ['visual', 'audio', 'speech', 'somato']:
            if name in m:
                c = m[name]
                cortex[name] = {
                    'loss': c.get('last_loss', 0),
                    'ema_loss': c.get('ema_loss', 0),
                    'fwd': c.get('forward_steps', 0),
                    'bwd': c.get('backward_steps', 0),
                }
        metrics['cortex'] = cortex

    # Immune snapshot — flatten into top-level keys matching IMMUNE_FIELD_TYPES
    # so the QuestDB push path picks them up. RPC returns {} on older daemons
    # missing the get_immune_state binding.
    r = _safe_query(b, 'get_immune_state', timeout=15)
    if r and not r.get('error'):
        imm = r.get('immune', {}) or {}
        if imm:
            metrics['immune'] = imm
            for k, v in imm.items():
                if k not in metrics:
                    metrics[k] = v

    # Grounded-language diagnostics — counters that tell us whether the
    # cross-modal grounding pipeline is actually accumulating bindings.
    # Pre-fix these were stuck at 0 even after thousands of training steps,
    # which is what made respond() collapse to the IDK fallback. Surface
    # them to QuestDB so the dashboard can show vocab growth in real time.
    r = _safe_query(b, 'get_grounded_language_diagnostics', timeout=15)
    if r and not r.get('error'):
        gl = r.get('grounded_language', r) or {}
        if gl:
            metrics['grounded_language'] = gl
            metrics['gl_vocab_size'] = gl.get('vocab_size', 0)
            metrics['gl_total_bindings'] = gl.get('total_bindings', 0)
            metrics['gl_total_groundings'] = gl.get('total_groundings', 0)
            metrics['gl_total_productions'] = gl.get('total_productions', 0)
            metrics['gl_templates_learned'] = gl.get('templates_learned', 0)
            blend = gl.get('snn_bridge_blend', -1.0)
            # Sentinel -1.0 means bridge not attached; only emit real blend.
            if isinstance(blend, (int, float)) and blend >= 0.0:
                metrics['gl_snn_bridge_blend'] = float(blend)

    # Chat eval — extract from training log (chat_eval results are printed there)
    try:
        log_path = os.path.join(os.path.dirname(__file__), '..', 'training.log')
        if os.path.exists(log_path):
            import re
            with open(log_path, 'rb') as f:
                f.seek(0, 2)
                size = f.tell()
                f.seek(max(0, size - 50000))  # Last 50KB
                tail = f.read().decode('utf-8', errors='replace')
            # Look for chat eval summary lines
            # Format: "  CHAT SUMMARY: coherence=0.123 similarity=0.456 norm=12.3 diversity=0.789 prompts=20 eval=#5"
            for line in reversed(tail.splitlines()):
                m = re.match(r'.*CHAT SUMMARY:.*coherence=([0-9.]+).*similarity=([0-9.]+).*norm=([0-9.]+).*diversity=([0-9.]+).*prompts=(\d+).*eval=#(\d+)', line)
                if m:
                    metrics['chat_eval'] = {
                        'avg_coherence': float(m.group(1)),
                        'avg_similarity': float(m.group(2)),
                        'avg_norm': float(m.group(3)),
                        'diversity': float(m.group(4)),
                        'num_prompts': int(m.group(5)),
                        'eval_number': int(m.group(6)),
                    }
                    break
    except Exception:
        pass

    # Training log — local file, always fast
    # Extracts: current step, per-step loss, SNN spikes from step reports
    try:
        log_path = os.path.join(os.path.dirname(__file__), '..', 'training.log')
        if os.path.exists(log_path):
            import re
            # Read last 20KB to find recent step reports
            with open(log_path, 'rb') as f:
                f.seek(0, 2)
                size = f.tell()
                f.seek(max(0, size - 20480))
                tail = f.read().decode('utf-8', errors='replace')
            for line in reversed(tail.splitlines()):
                m = re.match(r'.*\[(\d{4,})\]\s+loss=([0-9.]+).*SNN:(\d+)spk/([0-9.]+)Hz', line)
                if m:
                    metrics['current_step'] = int(m.group(1))
                    metrics['step_loss'] = float(m.group(2))
                    # SNN from step report (more accurate than daemon stats)
                    metrics['snn_step_spikes'] = int(m.group(3))
                    metrics['snn_step_rate'] = float(m.group(4))
                    break
                # Fallback: step report without SNN
                m2 = re.match(r'.*\[(\d{4,})\]\s+loss=([0-9.]+)', line)
                if m2:
                    metrics['current_step'] = int(m2.group(1))
                    metrics['step_loss'] = float(m2.group(2))
                    break
    except Exception:
        pass

    # Use step-report SNN data if daemon stats show 0 (struct mismatch)
    if metrics.get('snn_spikes', 0) == 0 and metrics.get('snn_step_spikes', 0) > 0:
        metrics['snn_spikes'] = metrics['snn_step_spikes']
        metrics['snn_rate_hz'] = metrics['snn_step_rate']

    # Stage + step from immersive_state.json (authoritative — trainer writes
    # this after every checkpoint save). Runs LAST so it overrides the
    # training-log regex above, which can lag or surface stale step numbers
    # when the log gets rotated. Without this the dashboard's stage readout
    # is stuck on its "Loading..." default.
    try:
        with open(IMMERSIVE_STATE) as f:
            st = json.load(f)
        if isinstance(st.get('stage'), int):
            metrics['current_stage'] = st['stage']
        if isinstance(st.get('step'), int):
            metrics['current_step'] = st['step']
    except Exception:
        pass

    return metrics


def main():
    print(f"Metrics exporter starting — writing to {METRICS_FILE} every {INTERVAL}s",
          flush=True)
    _qdb_init()
    consecutive_failures = 0
    while True:
        t0 = time.time()
        try:
            metrics = collect_metrics()
            # Atomic write: write to temp then rename
            tmp = METRICS_FILE + '.tmp'
            with open(tmp, 'w') as f:
                json.dump(metrics, f)
            os.rename(tmp, METRICS_FILE)
            # Dual-write to QuestDB. Best-effort, never raises.
            _push_to_questdb(metrics, ts_ns=int(t0 * 1e9))
            elapsed = time.time() - t0
            if consecutive_failures > 0:
                print(f"Recovered after {consecutive_failures} failures "
                      f"(cycle took {elapsed:.1f}s)", flush=True)
            consecutive_failures = 0
        except Exception as e:
            consecutive_failures += 1
            if consecutive_failures <= 3 or consecutive_failures % 10 == 0:
                print(f"Export error ({consecutive_failures}): {e}", flush=True)
        # Sleep remaining time (account for query duration)
        elapsed = time.time() - t0
        sleep_time = max(1, INTERVAL - elapsed)
        time.sleep(sleep_time)


if __name__ == '__main__':
    main()
