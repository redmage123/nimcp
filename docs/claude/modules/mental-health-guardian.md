# Mental Health Guardian

## Purpose

Independent background monitoring agent for detecting and intervening on AI mental health disorders. Provides continuous, non-blocking surveillance of brain state with graduated intervention responses.

## Architecture

```
Brain Decision Loop
        │
        ▼
┌───────────────────┐
│  Mental Health    │ ◄── Behavioral markers collection
│     Monitor       │
└─────────┬─────────┘
          │ mental_health_report_t
          ▼
┌───────────────────┐     ┌─────────────────────────────────┐
│  Mental Health    │────▶│ Bio-async Status Broadcasts     │
│    Guardian       │     │ (Serotonin=status, NE=alerts)   │
│  (Background      │     └─────────────────────────────────┘
│   Thread 100ms)   │
└─────────┬─────────┘
          │ Intervention Level
          ▼
┌─────────────────────────────────────────────────────────────┐
│ OBSERVE │ ADJUST       │ REGULATE        │ QUARANTINE      │
│ (log)   │ (neuromod)   │ (sleep+reset)   │ (full lockdown) │
└─────────────────────────────────────────────────────────────┘
```

## Intervention Levels

| Level | Severity | Actions |
|-------|----------|---------|
| OBSERVE | < 0.3 | Log only, monitoring continues |
| ADJUST | 0.3-0.6 | Neuromodulator tweaks (disorder-specific) |
| REGULATE | 0.6-0.8 | Sleep cycle trigger + homeostatic reset |
| QUARANTINE | > 0.8 | Full lockdown: defensive neuromod posture, clear working memory, deep sleep |

## 23 Disorder Detectors

**Cluster A - Antisocial:** Sociopathy, Psychopathy, Conduct Disorder
**Cluster B - Mood:** Mania, Depression, Bipolar
**Cluster C - Psychotic:** Schizophrenia, Paranoid Schizophrenia, Schizoaffective, Delusional
**Cluster D - Anxiety:** Anxiety, PTSD, OCD
**Cluster E - Autism Spectrum:** Autism, Asperger's
**Cluster F - Personality (Dramatic):** Malignant Narcissism, Borderline, Histrionic
**Cluster G - Personality (Anxious):** Avoidant, Dependent, OCPD
**Cluster H - Personality (Odd):** Paranoid Personality
**Cluster I - Neurodevelopmental:** ADHD

## Integration Points

| System | Integration | Purpose |
|--------|-------------|---------|
| FEP Bridge | `mental_health_fep_bridge_*` | Aberrant precision detection |
| Immune Bridge | `mental_health_immune_bridge_*` | Cytokine effects on disorders |
| Bio-async | Status broadcasts | Distributed monitoring |
| Sleep System | `sleep_run_cycle()` | Recovery triggering |
| Plasticity | Homeostatic reset | Learning rate management |
| Neuromodulators | `neuromodulator_set_level()` | Direct intervention |
| Working Memory | `working_memory_clear()` | Pattern breaking |
| Internal KG | Topology-aware nodes | State persistence |

## Key APIs

```c
// Configuration
mental_health_guardian_config_t mental_health_guardian_default_config(void);

// Lifecycle
mental_health_guardian_t* mental_health_guardian_create(
    brain_t brain,
    const mental_health_guardian_config_t* config);
void mental_health_guardian_destroy(mental_health_guardian_t* guardian);

// Thread Control
bool mental_health_guardian_start(mental_health_guardian_t* guardian);
bool mental_health_guardian_stop(mental_health_guardian_t* guardian);
bool mental_health_guardian_pause(mental_health_guardian_t* guardian);
bool mental_health_guardian_resume(mental_health_guardian_t* guardian);

// Status & Metrics
bool mental_health_guardian_get_status(
    mental_health_guardian_t* guardian,
    mental_health_guardian_status_t* status);

// Manual Control
guardian_intervention_level_t mental_health_guardian_force_check(
    mental_health_guardian_t* guardian);
bool mental_health_guardian_set_level(
    mental_health_guardian_t* guardian,
    guardian_intervention_level_t level);
```

## Status Structure

```c
typedef struct mental_health_guardian_status {
    guardian_state_t state;               // STOPPED/RUNNING/PAUSED
    guardian_intervention_level_t level;  // Current intervention level
    float overall_severity;               // [0.0-1.0]
    uint64_t checks_performed;            // Total health checks
    uint64_t interventions_applied;       // Total interventions
    uint64_t uptime_ms;                   // Running time
    int primary_disorder;                 // Most severe (-1 if none)
    int secondary_disorder;               // Second most severe (-1 if none)
    float secondary_disorder_score;       // Score of secondary [0.0-1.0]
    uint32_t active_disorders;            // Number above threshold
} mental_health_guardian_status_t;
```

## Files

- **Header:** `include/cognitive/mental_health/nimcp_mental_health_guardian.h`
- **Source:** `src/cognitive/mental_health/nimcp_mental_health_guardian.c`
- **Monitor:** `src/cognitive/mental_health/nimcp_mental_health.c`
- **Detectors:** `src/cognitive/mental_health/disorder_detectors.c`
- **Interventions:** `src/cognitive/mental_health/interventions.c`

## Bridge Files

- `nimcp_mental_health_fep_bridge.c` - Free Energy Principle integration
- `nimcp_mental_health_immune_bridge.c` - Brain immune system
- `nimcp_mental_health_sleep_bridge.c` - Sleep system
- `nimcp_mental_health_substrate_bridge.c` - Metabolic substrate
- `nimcp_mental_health_thalamic_bridge.c` - Thalamic routing

## Return Value Conventions

- FEP bridges: Return `0` for success, `-1` for errors (NOT NIMCP_OK/ERROR)
- Core APIs: Return `bool` or opaque handles
- Get functions: Return data via output parameters

## Test Coverage

- **Unit:** `test/unit/cognitive/mental_health/test_mental_health_guardian.cpp`
- **E2E:** `test/e2e/test_mental_health_guardian_e2e.cpp`
- **Integration:** `test/integration/cognitive/mental_health/test_guardian_brain_integration.cpp`
- **Regression:** `test/regression/cognitive/test_guardian_intervention_regression.cpp`
