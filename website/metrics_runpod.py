#!/usr/bin/env python3
"""Metrics exporter: queries brain daemon via TCP, writes metrics.json."""
import json, struct, socket, time, os, subprocess

BRAIN_HOST = '127.0.0.1'
BRAIN_PORT = 9900
METRICS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'metrics.json')
STEP_CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '.step_cache.json')
HISTORY_TSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'metrics_history.tsv')
INTERVAL = 60

# Stable column order for the history TSV. New metrics get appended to this
# list — never inserted in the middle, never removed — so awk/pandas readers
# can rely on positions across days. Missing values are written as empty
# strings; nested values (dicts) are flattened with dot-paths up front.
HISTORY_FIELDS = [
    'timestamp', 'ok', 'training_active',
    'uptime', 'learn_calls', 'infer_calls', 'errors', 'neuron_count',
    'current_stage', 'current_step',
    'ann_loss', 'cnn_loss', 'snn_loss', 'lnn_loss', 'fno_loss',
    'ann_steps', 'fno_steps', 'fno_params',
    'hnn_active', 'hnn_energy', 'hnn_energy_deviation',
    'snn_spikes', 'snn_rate_hz', 'snn_sparsity',
    'stage2.active_engines', 'stage2.fep_free_energy',
    'stage2.attention_strength', 'stage2.warm_start_complete',
    'stage2.inference_time_ms',
]


def _flatten(metrics):
    """Flatten nested dicts (e.g. metrics['stage2']['fep_free_energy']) to
    dot-path keys, so the TSV stays a flat 2-D table."""
    flat = {}
    for k, v in metrics.items():
        if isinstance(v, dict):
            for k2, v2 in v.items():
                flat[f'{k}.{k2}'] = v2
        else:
            flat[k] = v
    return flat


def write_history_row(metrics):
    """Append one row to the history TSV. Writes the header once, on first
    create. Best-effort — TSV write failures must never block the JSON path."""
    try:
        flat = _flatten(metrics)
        new_file = not os.path.exists(HISTORY_TSV)
        with open(HISTORY_TSV, 'a') as f:
            if new_file:
                f.write('\t'.join(HISTORY_FIELDS) + '\n')
            row = []
            for col in HISTORY_FIELDS:
                v = flat.get(col, '')
                if isinstance(v, bool):
                    row.append('1' if v else '0')
                elif isinstance(v, float):
                    row.append(f'{v:.6g}')
                else:
                    row.append(str(v))
            f.write('\t'.join(row) + '\n')
    except Exception as e:
        print(f'[metrics] history TSV write failed: {e}', flush=True)


def query(cmd, timeout=15):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    try:
        s.connect((BRAIN_HOST, BRAIN_PORT))
        data = json.dumps({'cmd': cmd}).encode()
        s.sendall(struct.pack('>I', len(data)) + data)
        hdr = b''
        while len(hdr) < 4:
            c = s.recv(4 - len(hdr))
            if not c:
                return None
            hdr += c
        length = struct.unpack('>I', hdr)[0]
        body = b''
        while len(body) < length:
            c = s.recv(min(length - len(body), 65536))
            if not c:
                return None
            body += c
        return json.loads(body)
    except Exception as e:
        print(f'[metrics] Query {cmd} failed: {e}', flush=True)
        return None
    finally:
        s.close()


def fetch_step_cache():
    """Fetch immersive_state.json from RunPod in background."""
    try:
        subprocess.Popen(
            ['ssh', 'root@74.2.96.55', '-p', '11008',
             '-i', os.path.expanduser('~/.ssh/id_ed25519_runpod'),
             '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=no',
             'cat /workspace/nimcp/checkpoints/athena/immersive_state.json 2>/dev/null'],
            stdout=open(STEP_CACHE, 'w'),
            stderr=subprocess.DEVNULL)
    except Exception:
        pass


def collect():
    metrics = {'timestamp': time.time(), 'ok': False}

    r = query('status', 30)
    if not r or r.get('error'):
        return metrics

    metrics['uptime'] = r.get('uptime', 0)
    metrics['learn_calls'] = r.get('learn_calls', 0)
    metrics['infer_calls'] = r.get('infer_calls', 0)
    metrics['errors'] = r.get('errors', 0)
    metrics['ok'] = True

    r = query('get_neuron_count', 15)
    if r:
        metrics['neuron_count'] = r.get('neuron_count', 0)

    r = query('get_network_metrics', 25)
    if r:
        m = r.get('metrics', r)
        metrics['ann_loss'] = m.get('ann_loss', 0)
        metrics['cnn_loss'] = m.get('cnn_loss', 0)
        metrics['snn_loss'] = m.get('snn_loss', 0)
        metrics['lnn_loss'] = m.get('lnn_loss', 0)
        metrics['ann_steps'] = m.get('ann_steps', 0)
        # FNO metrics
        metrics['fno_loss'] = m.get('fno_audio_ema_loss', 0)
        metrics['fno_steps'] = m.get('fno_audio_steps', 0)
        metrics['fno_params'] = m.get('fno_audio_params', 0)
        # HNN metrics
        metrics['hnn_active'] = m.get('hnn_active', False)
        metrics['hnn_energy'] = m.get('hnn_energy', 0)
        metrics['hnn_energy_deviation'] = m.get('hnn_energy_deviation', 0)

    r = query('get_snn_stats', 15)
    if r:
        s = r.get('snn', r)
        metrics['snn_spikes'] = s.get('total_spikes', 0)
        metrics['snn_rate_hz'] = s.get('mean_firing_rate_hz', 0)
        metrics['snn_sparsity'] = s.get('sparsity', 0)

    # Read cached step info
    try:
        with open(STEP_CACHE) as f:
            sc = json.loads(f.read())
        metrics['current_step'] = sc.get('step', 0)
        metrics['current_stage'] = sc.get('stage', 0)
    except Exception:
        pass

    # Training dashboard — all metrics from C-level brain struct via daemon API
    r = query('get_training_dashboard', 15)
    if r:
        d = r.get('dashboard', r)
        if isinstance(d, dict) and d.get('stage', 0) > 0:
            metrics['current_stage'] = d.get('stage', 0)
            metrics['current_step'] = d.get('step', 0)

        s2 = {}
        s2['active_engines'] = d.get('active_engines', 74) if d else 74
        if d:
            if d.get('current_domain'): s2['current_domain'] = d['current_domain']
            if d.get('fact_ratio', 0) > 0: s2['fact_ratio'] = d['fact_ratio']
            s2['warm_start_complete'] = d.get('warm_start_complete', False)
            if d.get('warm_start_step', 0) > 0: s2['warm_start_step'] = d['warm_start_step']
            if d.get('lr_physics', 0) > 0: s2['lr_physics'] = d['lr_physics']
            if d.get('lr_chemistry', 0) > 0: s2['lr_chemistry'] = d['lr_chemistry']
            if d.get('lr_biology', 0) > 0: s2['lr_biology'] = d['lr_biology']
            if d.get('wm_steps', 0) > 0: s2['wm_steps'] = d['wm_steps']
            if d.get('collapse_events', 0) > 0: s2['collapse_events'] = d['collapse_events']
            if d.get('surprises', 0) > 0: s2['surprises'] = d['surprises']
            if d.get('replays', 0) > 0: s2['replays'] = d['replays']
            if d.get('vocab_size', 0) > 0: s2['vocab_size'] = d['vocab_size']
            if d.get('lang_confidence', 0) > 0: s2['lang_confidence'] = d['lang_confidence']
            # FEP from HNN
            if d.get('inference_time_ms', 0) > 0: s2['inference_time_ms'] = d['inference_time_ms']
            if d.get('attention_strength', 0) > 0: s2['attention_strength'] = d['attention_strength']

        # FEP free energy from network metrics (more reliable)
        r2 = query('get_network_metrics', 15)
        if r2:
            m = r2.get('metrics', r2)
            if m.get('hnn_energy_deviation'):
                s2['fep_free_energy'] = m['hnn_energy_deviation']

        metrics['stage2'] = s2

    # Probe metrics — brain regions, neurons, synapses (for charts)
    r = query('get_probe_metrics', 20)
    if r:
        pm = r.get('probe_metrics', r)
        if isinstance(pm, dict) and len(pm) > 0:
            metrics['probe_metrics'] = pm

    return metrics


if __name__ == '__main__':
    print(f'[metrics] Starting — writing to {METRICS_FILE}', flush=True)
    _last_learn_calls = 0
    _last_learn_time = time.time()
    _stall_warned = False

    while True:
        try:
            m = collect()

            # Detect stalled training: if learn_calls hasn't increased in 5 min.
            # Also detect brain restart — when the daemon restarts, learn_calls
            # resets to 0 (or a low number). Without handling this, the cached
            # _last_learn_calls stays at the old high value and the `>` check
            # never fires; _last_learn_time ages past 300s; training_active
            # gets stuck at False forever even though learns ARE happening.
            current_learns = m.get('learn_calls', 0)
            now = time.time()
            if current_learns < _last_learn_calls:
                # Brain restart: counter went backwards. Re-anchor and treat
                # as active — the daemon is back up and processing.
                print(f'[metrics] Brain restart detected — learn_calls went '
                      f'{_last_learn_calls} -> {current_learns}; re-anchoring',
                      flush=True)
                _last_learn_calls = current_learns
                _last_learn_time = now
                _stall_warned = False
                m['training_active'] = True
            elif current_learns > _last_learn_calls:
                _last_learn_calls = current_learns
                _last_learn_time = now
                _stall_warned = False
                m['training_active'] = True
            elif m.get('ok') and (now - _last_learn_time) > 300:
                m['training_active'] = False
                if not _stall_warned:
                    print(f'[metrics] WARNING: Training stalled — no learns in '
                          f'{int(now - _last_learn_time)}s', flush=True)
                    _stall_warned = True
            else:
                m['training_active'] = True  # Within grace period

            tmp = METRICS_FILE + '.tmp'
            with open(tmp, 'w') as f:
                json.dump(m, f)
            os.rename(tmp, METRICS_FILE)
            write_history_row(m)
            print(f'[metrics] ok={m["ok"]} learn={m.get("learn_calls","?")} '
                  f'step={m.get("current_step","?")} active={m.get("training_active","?")}',
                  flush=True)
        except Exception as e:
            print(f'[metrics] Error: {e}', flush=True)

        # Fetch step count in background
        fetch_step_cache()

        time.sleep(INTERVAL)
