# Training Pipeline Fixes (2026-02-24)

## Segfault: neural_network_create buffer overflow
- **Root cause**: `actual_neurons` (sum of layer sizes) exceeded `capacity` (capped at MAX_NEURONS=2M)
- **Location**: `src/core/neuralnet/nimcp_neuralnet.c:687`
- `actual_neurons` = 2,000,384 (input 256 + hidden layers + output 128)
- `capacity` = min(actual_neurons * 1.1, MAX_NEURONS) = 2,000,000
- Loop ran to 2,000,384, writing past 2,000,000 buffer
- **Fix**: `if (actual_neurons > capacity) actual_neurons = capacity;` after capacity capping
- **GDB command**: `gdb -batch -x cmds.txt --args python3 script.py` with `handle SIGSEGV stop print`

## brain.predict() returns tuple, not dict
- `brain.predict(features)` returns `(decision_label, confidence_float)`
- NOT a dict with `.get("decision")` — that was wrong assumption
- socratic_trainer.py and instructor_agent.py correctly do `pred, conf = brain.predict(...)`

## brain.learn() not brain.train()
- Python Brain API method is `brain.learn(features, label)`, not `brain.train()`
- Available methods: `learn`, `predict`, `predict_batch`, `decide_full`, `probe`, `save`, `load`, `resize`, `auto_resize`, `get_neuron_count`, `get_accuracy`, `get_utilization_metrics`, `broadcast_probe`, `snapshot_cow`, `restore_cow`, `destroy_cow_snapshot`

## Smoke test pattern (added to train_athena.py)
Runs in <30s after brain creation, before any long training:
1. Verify predict works with correct dimensions
2. Verify learn works
3. Verify encoding pipeline (text, QA, image all produce correct dims)
4. Verify domain labels are prefixed
5. Quick learn+predict cycle
If any fail → abort immediately with clear error message

## Health check pattern (runs after every phase)
- Predict sanity check
- Accuracy on 10 wine + 10 breast_cancer samples
- Neuron count sanity
- Logs metrics to JSONL for tracking

## Training speed concern
- 2M neurons → ~1 predict/train cycle per second on CPU
- Phase 0 (7 datasets × 5 epochs) takes ~4 hours
- Phase 1 (23 instructors × 23 hours) would take days
- Inferentia acceleration (planned for 2026-02-25) should help significantly
