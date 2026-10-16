# Brain-Driven Fault Tolerance Architecture

## System Overview

```
┌───────────────────────────────────────────────────────────────────────┐
│                          APPLICATION LAYER                             │
│                                                                        │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │   Cognitive Recovery Coordinator (nimcp_cognitive_recovery.h)   │  │
│  │   • Workflow orchestration                                      │  │
│  │   • Subsystem coordination                                      │  │
│  │   • Unified high-level API                                      │  │
│  └───────────────────────┬────────────────────────────────────────┘  │
└────────────────────────────┼───────────────────────────────────────────┘
                             │
                             ▼
┌───────────────────────────────────────────────────────────────────────┐
│                    COGNITIVE INTEGRATION LAYER                         │
│                                                                        │
│  ┌──────────────────────────────────┐  ┌──────────────────────────┐  │
│  │ Brain Recovery Integration       │  │ Runtime Adaptation       │  │
│  │ (nimcp_brain_recovery_*.h)       │  │ (nimcp_runtime_*.h)      │  │
│  │                                  │  │                          │  │
│  │ • Strategy selection (Executive) │  │ • Parameter adjustment   │  │
│  │ • Pattern matching (Working Mem) │  │ • Feature toggling       │  │
│  │ • Outcome learning (Episodic)    │  │ • Policy application     │  │
│  │ • Success prediction (Reasoning) │  │ • No compilation needed  │  │
│  └─────────┬────────────────────────┘  └───────┬──────────────────┘  │
│            │                                    │                     │
│            └────────────┬───────────────────────┘                     │
└─────────────────────────┼─────────────────────────────────────────────┘
                          │
                          ▼
┌───────────────────────────────────────────────────────────────────────┐
│                      BRAIN COGNITIVE SYSTEMS                           │
│                                                                        │
│  ┌─────────────────┐  ┌──────────────────┐  ┌────────────────────┐  │
│  │  Executive      │  │  Working Memory  │  │  Episodic Memory   │  │
│  │  Function       │  │                  │  │                    │  │
│  │  • Task mgmt    │  │  • Pattern cache │  │  • Long-term store │  │
│  │  • Decision     │  │  • Recent fails  │  │  • Outcome history │  │
│  │  • Inhibition   │  │  • Active tasks  │  │  • Learning        │  │
│  └────────┬────────┘  └────────┬─────────┘  └────────┬───────────┘  │
│           │                    │                      │               │
│           │    ┌───────────────┴──────────────┐      │               │
│           │    │    Knowledge System           │      │               │
│           │    │    • Domain expertise         │      │               │
│           │    │    • Recovery strategies      │      │               │
│           │    └───────────────────────────────┘      │               │
│           │                                            │               │
└───────────┼────────────────────────────────────────────┼───────────────┘
            │                                            │
            ▼                                            ▼
┌───────────────────────────────────────────────────────────────────────┐
│                    FAULT TOLERANCE FOUNDATION                          │
│                                                                        │
│  ┌──────────────┐  ┌────────────────┐  ┌─────────────────────────┐  │
│  │  Health      │  │  Diagnostics   │  │  Recovery Engine        │  │
│  │  Monitor     │  │  System        │  │                         │  │
│  │              │  │                │  │  • Strategy execution   │  │
│  │  • Real-time │  │  • Error class │  │  • Tier management      │  │
│  │  • Anomalies │  │  • Root cause  │  │  • Verification         │  │
│  │  • Predict   │  │  • Stack trace │  │  • Circuit breakers     │  │
│  └──────────────┘  └────────────────┘  └─────────────────────────┘  │
└───────────────────────────────────────────────────────────────────────┘
```

## Cognitive Recovery Workflow

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ERROR DETECTED                                │
│  (NaN, Crash, Memory Leak, Performance Degradation, etc.)           │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             ▼
                   ┌──────────────────┐
                   │   DIAGNOSTICS    │
                   │ • Analyze error  │
                   │ • Root cause     │
                   │ • Severity       │
                   └────────┬─────────┘
                            │
                            ▼
              ┌─────────────────────────────────┐
              │     BRAIN ANALYSIS              │
              │                                 │
              │  ┌───────────────────────────┐ │
              │  │ Working Memory:           │ │
              │  │ "Seen this before?"       │ │
              │  │ → Search patterns         │ │
              │  └───────────────────────────┘ │
              │                                 │
              │  ┌───────────────────────────┐ │
              │  │ Executive Function:       │ │
              │  │ "What are my options?"    │ │
              │  │ → Evaluate strategies     │ │
              │  └───────────────────────────┘ │
              │                                 │
              │  ┌───────────────────────────┐ │
              │  │ Reasoning:                │ │
              │  │ "What will work?"         │ │
              │  │ → Predict success         │ │
              │  └───────────────────────────┘ │
              │                                 │
              │  ┌───────────────────────────┐ │
              │  │ Knowledge:                │ │
              │  │ "What do I know?"         │ │
              │  │ → Domain expertise        │ │
              │  └───────────────────────────┘ │
              └────────────┬────────────────────┘
                           │
                           ▼
                ┌────────────────────────┐
                │  STRATEGY SELECTION    │
                │                        │
                │  • Strategy choice     │
                │  • Confidence          │
                │  • Success probability │
                │  • Reasoning           │
                │  • Alternatives        │
                └──────────┬─────────────┘
                           │
                           ▼
               ┌──────────────────────────┐
               │  RUNTIME ADAPTATION      │
               │                          │
               │  • Adjust parameters     │
               │  • Toggle features       │
               │  • Apply policies        │
               │  • No code changes!      │
               └────────────┬─────────────┘
                            │
                            ▼
                  ┌──────────────────────┐
                  │  EXECUTE RECOVERY    │
                  │                      │
                  │  • Immediate (<1ms)  │
                  │  • Tactical (<100ms) │
                  │  • Strategic (<1s)   │
                  │  • Preventive        │
                  └──────────┬───────────┘
                             │
                             ▼
                  ┌──────────────────────┐
                  │  VERIFY SUCCESS      │
                  │                      │
                  │  • Check health      │
                  │  • Validate state    │
                  │  • Monitor stability │
                  └──────────┬───────────┘
                             │
                             ▼
               ┌──────────────────────────────┐
               │  LEARN FROM OUTCOME          │
               │                              │
               │  ┌────────────────────────┐ │
               │  │ Episodic Memory:       │ │
               │  │ "Store experience"     │ │
               │  │ → Save: Pattern→Result │ │
               │  └────────────────────────┘ │
               │                              │
               │  ┌────────────────────────┐ │
               │  │ Update Predictions:    │ │
               │  │ "Adjust confidence"    │ │
               │  │ → Success rates        │ │
               │  └────────────────────────┘ │
               │                              │
               │  ┌────────────────────────┐ │
               │  │ Generalize:            │ │
               │  │ "Extract patterns"     │ │
               │  │ → Similar situations   │ │
               │  └────────────────────────┘ │
               └──────────────────────────────┘
```

## Runtime-Only Adaptation

```
┌──────────────────────────────────────────────────────────────────┐
│                  ADJUSTABLE PARAMETERS                            │
│                  (No Compiler Required)                           │
│                                                                   │
│  ┌────────────────┐  ┌────────────────┐  ┌─────────────────┐   │
│  │   Learning     │  │ Regularization │  │   Gradient      │   │
│  │                │  │                │  │   Control       │   │
│  │ • LR           │  │ • Dropout      │  │ • Clipping      │   │
│  │ • Batch size   │  │ • L1/L2        │  │ • Norm limit    │   │
│  │ • Momentum     │  │ • Noise        │  │                 │   │
│  │ • Weight decay │  │                │  │                 │   │
│  └────────────────┘  └────────────────┘  └─────────────────┘   │
│                                                                   │
│  ┌────────────────┐  ┌────────────────┐  ┌─────────────────┐   │
│  │   Neuron       │  │  Plasticity    │  │ Neuromodulation │   │
│  │                │  │                │  │                 │   │
│  │ • Temperature  │  │ • STDP rate    │  │ • Dopamine      │   │
│  │ • Threshold    │  │ • Time window  │  │ • Serotonin     │   │
│  │ • Refractory   │  │ • Homeostasis  │  │ • ACh           │   │
│  │ • Leak factor  │  │                │  │ • NE            │   │
│  └────────────────┘  └────────────────┘  └─────────────────┘   │
│                                                                   │
│  ┌────────────────┐  ┌────────────────┐  ┌─────────────────┐   │
│  │   Memory       │  │  Performance   │  │   Numerical     │   │
│  │                │  │                │  │   Stability     │   │
│  │ • Capacity     │  │ • Threads      │  │ • Min weight    │   │
│  │ • Forgetting   │  │ • Cache size   │  │ • Max weight    │   │
│  │ • Consolidate  │  │ • Prefetch     │  │ • Epsilon       │   │
│  └────────────────┘  └────────────────┘  └─────────────────┘   │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│                     FEATURE TOGGLES                               │
│                     (Enable/Disable)                              │
│                                                                   │
│  ✓ Dropout            ✓ Plasticity        ✓ Memory Compaction   │
│  ✓ Batch Norm         ✓ Homeostasis       ✓ Prefetching         │
│  ✓ Grad Clipping      ✓ Neuromodulation   ✓ Caching             │
│  ✓ Weight Clipping    ✓ Debug Logging     ✓ Checkpointing       │
│  ✓ Layer Freezing     ✓ NaN Detection     ✓ Bounds Checking     │
└──────────────────────────────────────────────────────────────────┘
```

## Data Flow Diagram

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Error   │ ──▶ │ Diagnose │ ──▶ │  Brain   │ ──▶ │  Adapt   │
│ Detected │     │ Analyze  │     │ Analyze  │     │ Runtime  │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
                                        │
                                        ▼
                                  ┌──────────┐
                                  │  Select  │
                                  │ Strategy │
                                  └────┬─────┘
                                       │
                                       ▼
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Learn   │ ◀── │  Verify  │ ◀── │ Execute  │ ◀── │  Apply   │
│ Outcome  │     │ Success  │     │ Recovery │     │ Changes  │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
     │
     ▼
┌──────────────────────────────────────────────────────────────┐
│                    EPISODIC MEMORY                            │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Pattern 1: NaN in layer3 → Reduce LR 50% → Success    │ │
│  │ Pattern 2: Memory leak → Reduce batch → Success       │ │
│  │ Pattern 3: Gradient explosion → Clip → Success        │ │
│  │ ...                                                     │ │
│  └────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

## Module Dependencies

```
nimcp_cognitive_recovery.h
        │
        ├─► nimcp_brain_recovery_integration.h
        │           │
        │           ├─► nimcp_recovery.h
        │           ├─► nimcp_diagnostics.h
        │           ├─► nimcp_health_monitor.h
        │           └─► nimcp_brain.h
        │                   │
        │                   ├─► nimcp_executive.h
        │                   ├─► nimcp_working_memory.h
        │                   ├─► nimcp_episodic_memory.h (Phase M2)
        │                   └─► nimcp_knowledge.h
        │
        └─► nimcp_runtime_adaptation.h
                    │
                    └─► nimcp_brain.h
```

## File Organization

```
nimcp/
├── include/utils/fault_tolerance/
│   ├── nimcp_brain_recovery_integration.h  (Brain-driven recovery)
│   ├── nimcp_runtime_adaptation.h          (Parameter adaptation)
│   ├── nimcp_cognitive_recovery.h          (High-level coordinator)
│   ├── nimcp_recovery.h                    (Existing recovery)
│   ├── nimcp_diagnostics.h                 (Existing diagnostics)
│   └── nimcp_health_monitor.h              (Existing health monitor)
│
├── src/utils/fault_tolerance/
│   ├── nimcp_brain_recovery_integration.c
│   ├── nimcp_runtime_adaptation.c
│   ├── nimcp_cognitive_recovery.c          (To be implemented)
│   ├── nimcp_recovery.c
│   ├── nimcp_diagnostics.c
│   └── nimcp_health_monitor.c
│
└── docs/
    ├── BRAIN_FAULT_TOLERANCE_INTEGRATION_REPORT.md
    └── BRAIN_RECOVERY_ARCHITECTURE.md         (This file)
```

## Key Interfaces

### 1. Cognitive Recovery Coordinator → Brain Recovery Integration
```c
brain_recovery_decision_t* brain_recovery_select_strategy(
    brain_recovery_context_t* ctx,
    diagnostic_result_t* diagnosis,
    health_status_snapshot_t* current_health
);
```

### 2. Cognitive Recovery Coordinator → Runtime Adaptation
```c
bool runtime_adaptation_apply_batch(
    runtime_adaptation_context_t* ctx,
    parameter_change_t* changes,
    uint32_t num_changes,
    const char* reason
);
```

### 3. Brain Recovery Integration → Brain (Executive Function)
```c
// Brain's executive function evaluates recovery options
executive_controller_t* exec = brain_get_executive(brain);
uint32_t task_id = executive_add_task(exec, &recovery_task);
```

### 4. Brain Recovery Integration → Brain (Working Memory)
```c
// Brain's working memory stores recent failure patterns
working_memory_t* wm = brain_get_working_memory(brain);
bool found = working_memory_search(wm, failure_pattern);
```

### 5. Brain Recovery Integration → Brain (Episodic Memory)
```c
// Brain's episodic memory learns from outcomes
// Note: Phase M2 - Systems Consolidation
systems_consolidation_system_t* scs = brain_get_consolidation(brain);
consolidation_store_episode(scs, recovery_outcome);
```

## Performance Characteristics

| Operation | Time Complexity | Space Complexity |
|-----------|----------------|------------------|
| Strategy Selection | O(log n + k) | O(1) |
| Pattern Matching | O(log n) | O(n) |
| Parameter Adjustment | O(1) | O(1) |
| Learning Update | O(1) | O(1) |
| History Query | O(n) | O(n) |

Where:
- n = number of learned patterns
- k = number of strategy options

## Thread Safety

| Component | Thread Safety | Notes |
|-----------|--------------|-------|
| Brain Recovery Context | No | Requires mutex for concurrent access |
| Runtime Adaptation | No | Single-threaded by design |
| Cognitive Coordinator | No | Application manages threading |
| Learning Updates | No | Sequential learning assumed |

## Memory Management

```
Brain Recovery Context:
    Base: ~100 bytes
    History: 1000 entries × 500 bytes = 500KB
    Patterns: 500 patterns × 1KB = 500KB
    Total: ~1MB

Runtime Adaptation Context:
    Base: ~50 bytes
    Parameters: 50 × 4 bytes = 200 bytes
    Features: 15 × 1 byte = 15 bytes
    History: 500 entries × 200 bytes = 100KB
    Total: ~100KB

Cognitive Coordinator:
    Base: ~100 bytes
    Subsystems: Brain Recovery + Runtime Adaptation
    Total: ~1.1MB
```

## Summary

The brain-driven fault tolerance architecture represents a **cognitive approach to system recovery**, where the brain actively participates in:

1. **Analysis**: Understanding failures through cognitive reasoning
2. **Decision**: Selecting strategies through executive function
3. **Learning**: Improving from experience through episodic memory
4. **Adaptation**: Adjusting parameters through runtime modification
5. **Prediction**: Forecasting outcomes through pattern recognition

This creates a **self-improving, autonomous recovery system** that gets better over time, requires no code changes, and provides explainable decisions.
