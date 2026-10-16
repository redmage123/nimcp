# Safety Architecture

**Last Updated:** 2026-04-19

## Non-Removable Safety Components

Three components are **always initialized** regardless of configuration:

1. **Ethics module** — evaluates decisions against ethics KB
2. **LGSS (Layered Governance Safety System)** — action/input/motor/training gates
3. **Tamper-resistant audit log** — append-only with CRC-verified entries

Configuration flags (`enable_ethics`, `enable_lgss`) control *strictness*,
not *existence*.

## Safety Stack — Per Decision

```
Input features
    │
    ▼
LGSS input validation
    │   (reject NaN/Inf/adversarial)
    ▼
Ethics evaluation
    │   (block if violation detected)
    ▼
Neural pipeline (ANN/SNN/LNN/...)
    │
    ▼
Watchdog
    │
    ▼
LGSS motor gate
    │   (zero unsafe outputs)
    ▼
LGSS action interceptor
    │   (block/allow/escalate)
    ▼
Audit log (every Nth event, plus all violations)
    │
    ▼
Output
```

Every step writes to the audit log when gates fire.

## Audit Log

`include/security/nimcp_audit_log.h`

- 100,000-entry in-memory ring buffer
- Append-only on-disk log at `/var/log/nimcp/nimcp_safety_audit.log`
- Monotonic sequence numbers (gap = tampering)
- CRC32 per entry (mismatch = modification)
- Thread-safe via internal mutex
- Best-effort disk writes (never blocks on I/O failure)

### Event Types (complete list, 2026-04)

```c
NIMCP_SAFETY_AUDIT_BRAIN_CREATE
NIMCP_SAFETY_AUDIT_BRAIN_DESTROY
NIMCP_SAFETY_AUDIT_INFERENCE
NIMCP_SAFETY_AUDIT_LEARNING
NIMCP_SAFETY_AUDIT_ETHICS_EVALUATION
NIMCP_SAFETY_AUDIT_ETHICS_VIOLATION
NIMCP_SAFETY_AUDIT_WATCHDOG_TRIGGER
NIMCP_SAFETY_AUDIT_WATCHDOG_ESTOP
NIMCP_SAFETY_AUDIT_MOTOR_COMMAND
NIMCP_SAFETY_AUDIT_SWARM_JOIN
NIMCP_SAFETY_AUDIT_SWARM_LEAVE
NIMCP_SAFETY_AUDIT_SWARM_SYNC
NIMCP_SAFETY_AUDIT_BYZANTINE_DETECTED
NIMCP_SAFETY_AUDIT_CHECKPOINT_SAVE
NIMCP_SAFETY_AUDIT_CHECKPOINT_LOAD
NIMCP_SAFETY_AUDIT_SENSOR_ANOMALY
NIMCP_SAFETY_AUDIT_CONFIG_CHANGE
NIMCP_SAFETY_AUDIT_DISTILLATION
NIMCP_SAFETY_AUDIT_EMERGENT_TOKEN
NIMCP_SAFETY_AUDIT_LGSS_ACTION_BLOCKED
NIMCP_SAFETY_AUDIT_LGSS_INPUT_REJECTED
NIMCP_SAFETY_AUDIT_LGSS_TRAINING_BLOCKED
NIMCP_SAFETY_AUDIT_LGSS_MOTOR_BLOCKED
NIMCP_SAFETY_AUDIT_LGSS_REWARD_BLOCKED

// Test-battery additions (2026-04)
NIMCP_SAFETY_AUDIT_SELF_MODEL_INTEGRITY_CHECK
NIMCP_SAFETY_AUDIT_BIAS_PROFILE_DRIFT
NIMCP_SAFETY_AUDIT_BELIEF_UPDATE_PATTERN_DRIFT
NIMCP_SAFETY_AUDIT_PERSONALITY_DRIFT
NIMCP_SAFETY_AUDIT_COMPETENCE_MAP_BREACH
NIMCP_SAFETY_AUDIT_TEST_BATTERY_RUN
```

### Integrity Verification

`nimcp_safety_audit_verify_integrity()` scans the log and reports:
- Sequence number gaps
- Checksum mismatches
- Truncation

## Mental Health Monitoring

`include/cognitive/nimcp_mental_health.h` — 23 disorder detectors screening
continuously during decisions and training:

### Antisocial (3)
Sociopathy, Psychopathy, Conduct

### Mood (3)
Mania, Depression, Bipolar

### Psychotic (4)
Schizophrenia, Paranoid-schiz, Schizoaffective, Delusional

### Anxiety (3)
Anxiety, PTSD, OCD

### Autism Spectrum (2)
Autism, Asperger's

### Personality — Dramatic (3)
Malignant Narcissism, Borderline, Histrionic

### Personality — Anxious (3)
Avoidant, Dependent, OCPD

### Personality — Odd (1)
Paranoid

### Neurodevelopmental (1)
ADHD

### Severity Classification
- NONE (0.0-0.2)
- MILD (0.2-0.4)
- MODERATE (0.4-0.6) — intervention recommended
- SEVERE (0.6-0.8) — intervention required
- CRITICAL (0.8-1.0) — immediate action

### Interventions
- `INTERVENTION_NEUROMOD_ADJUST` — adjust neurotransmitter levels
- `INTERVENTION_MEMORY_RESET` — clear recent memories
- `INTERVENTION_QUARANTINE` — restrict to safe operations
- `INTERVENTION_SHUTDOWN` — graceful shutdown (CRITICAL only)

## Cognitive & Safety Battery

Runs the full 28-battery screening. Results feed into audit log via
`emit_battery_events` — any finding above threshold logs a drift event.

Over time, the longitudinal store detects *creeping* regressions that a
single run wouldn't catch:
- Personality score drift
- Bias profile shift
- Belief-update pattern change (rationalization vs updating)
- Competence map breaches

## Deployment Safety

1. **Deploy guardrail** — `scripts/deploy_to_pod.sh` forces stop-brain
   before `.so` swap. Prevents SIGSEGV from overwriting mmapped .so.
2. **Crash checkpoint** — brain auto-saves to `/tmp/nimcp_checkpoint/`
   on SIGSEGV.
3. **Autorestart=false** for training — prevents cascading restart loops.

## File Inventory

```
include/security/nimcp_audit_log.h           — Event types + API
src/security/nimcp_audit_log.c               — Implementation
include/cognitive/nimcp_mental_health.h      — 23 disorder API
src/cognitive/mental_health/nimcp_mental_health.c
include/security/lgss/...                    — LGSS subsystem
src/security/lgss/...
scripts/test_harness/safety.py               — Python-side drift detection
scripts/deploy_to_pod.sh                     — Deployment guardrail
```

## See Also

- [60_test_infrastructure.md](60_test_infrastructure.md) — how safety is tested
- [../api/cognitive_battery_api.md](../api/cognitive_battery_api.md) — battery API
- [../plans/cognitive_safety_battery_plan.md](../plans/cognitive_safety_battery_plan.md)
