#!/bin/bash
# Monitor for mode collapse — check every 60s
LOG=/home/bbrelin/nimcp/logs/collapse_monitor.log
echo "$(date) — Mode collapse monitor started" >> $LOG

while true; do
    if ! pgrep -f immerse_athena > /dev/null; then
        echo "$(date) — Training not running, waiting..." >> $LOG
        sleep 60
        continue
    fi

    METRICS=$(python3 -c "
from scripts.brain_client import BrainProxy
import json
try:
    brain = BrainProxy('/var/run/athena/brain.sock')
    nm = brain.get_network_metrics()
    # Check effective rank via decision
    d = brain.decide_full([0.1]*1024)
    ov = d.get('output_vector', []) if isinstance(d, dict) else []
    nonzero = sum(1 for v in ov if abs(v) > 1e-6)
    total = len(ov)
    # Diversity: std of outputs
    if ov:
        import statistics
        std = statistics.stdev(ov) if len(ov) > 1 else 0
    else:
        std = 0
    print(json.dumps({
        'ann_loss': nm.get('ann_loss',0),
        'cnn_loss': nm.get('cnn_loss',0),
        'snn_loss': nm.get('snn_loss',0),
        'lnn_loss': nm.get('lnn_loss',0),
        'ann_steps': nm.get('ann_steps',0),
        'cnn_steps': nm.get('cnn_steps',0),
        'nonzero': nonzero,
        'total': total,
        'output_std': std
    }))
except Exception as e:
    print(json.dumps({'error': str(e)}))
" 2>/dev/null)

    if echo "$METRICS" | python3 -c "
import sys, json
d = json.load(sys.stdin)
if 'error' in d:
    print(f'ERROR: {d[\"error\"]}')
    sys.exit(0)
steps = d['ann_steps']
nz = d['nonzero']
total = d['total']
std = d['output_std']
ann = d['ann_loss']
cnn = d['cnn_loss']
ratio = nz/total if total > 0 else 0
print(f'[{steps}stp] nonzero={nz}/{total} ({ratio:.1%}) std={std:.6f} ann={ann:.4f} cnn={cnn:.4f}')
if ratio < 0.1 and steps > 100:
    print('ALERT: MODE COLLAPSE — <10% nonzero outputs!')
    sys.exit(1)
if std < 0.0001 and steps > 100:
    print('ALERT: MODE COLLAPSE — output std near zero!')
    sys.exit(1)
" >> $LOG 2>&1; then
        : # OK
    else
        echo "$(date) — *** MODE COLLAPSE DETECTED ***" >> $LOG
        echo "$(date) MODE COLLAPSE: $(tail -2 $LOG)" >> /home/bbrelin/nimcp/TRAINING_ALERT.txt
    fi

    sleep 60
done
