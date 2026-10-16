#!/bin/bash
# Metrics exporter that uses SSH-tunneled socket from RunPod
# Tunnel must be running: ssh -f -N -L /tmp/athena/brain.sock:/var/run/athena/brain.sock ...

cd /home/bbrelin/nimcp

while true; do
    python3 -c "
import json, struct, socket, time, os

BRAIN_HOST = '127.0.0.1'
BRAIN_PORT = 9900
METRICS_FILE = 'website/metrics.json'

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
            if not c: return None
            hdr += c
        length = struct.unpack('>I', hdr)[0]
        body = b''
        while len(body) < length:
            c = s.recv(min(length - len(body), 65536))
            if not c: return None
            body += c
        return json.loads(body)
    except:
        return None
    finally:
        s.close()

metrics = {'timestamp': time.time(), 'ok': False}

r = query('status', 30)
if r and not r.get('error'):
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

    r = query('get_snn_stats', 15)
    if r:
        s = r.get('snn', r)
        metrics['snn_spikes'] = s.get('total_spikes', 0)
        metrics['snn_rate_hz'] = s.get('mean_firing_rate_hz', 0)
        metrics['snn_sparsity'] = s.get('sparsity', 0)

# Read cached step info (updated by background SSH fetch below)
try:
    with open('website/.step_cache.json') as f:
        sc = json.loads(f.read())
    metrics['current_step'] = sc.get('step', 0)
    metrics['current_stage'] = sc.get('stage', 0)
except:
    pass

with open(METRICS_FILE + '.tmp', 'w') as f:
    json.dump(metrics, f)
os.rename(METRICS_FILE + '.tmp', METRICS_FILE)
print(f'Metrics: ok={metrics[\"ok\"]} learn={metrics.get(\"learn_calls\",\"?\")} neurons={metrics.get(\"neuron_count\",\"?\")}')
"
    # Fetch step count from RunPod in background (non-blocking)
    ssh root@74.2.96.55 -p 11008 -i ~/.ssh/id_ed25519_runpod \
        -o ConnectTimeout=5 -o StrictHostKeyChecking=no \
        'cat /workspace/nimcp/checkpoints/athena/immersive_state.json 2>/dev/null' \
        > website/.step_cache.json 2>/dev/null &

    sleep 60
done
