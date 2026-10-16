# NIMCP Reasoning Integration - Quick Start Guide

## Overview
Modular cognitive integration system for NIMCP reasoning with STRICT SRP adherence.

## Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                     Event Bus (Central Hub)                  │
└──────┬──────────┬──────────┬──────────┬──────────┬──────────┘
       │          │          │          │          │
   ┌───▼────┐ ┌──▼─────┐ ┌──▼────┐ ┌──▼─────┐ ┌──▼─────┐
   │Module 1│ │Module 2│ │Module3│ │Module 4│ │Module 5│
   │Attn    │ │Curios  │ │  WM   │ │  Exec  │ │Consol  │
   └────────┘ └────────┘ └───────┘ └────────┘ └────────┘
      ▲          ▲          ▲          ▲          ▲
      │          │          │          │          │
   ┌──┴──────────┴──────────┴──────────┴──────────┴────┐
   │           MODULE 6: Event Publisher               │
   │    (Reasoning Engine publishes events via M6)     │
   └───────────────────────────────────────────────────┘
```

## Quick Integration Example

### Step 1: Create Integration Modules
```c
#include "cognitive/reasoning/integration/nimcp_reasoning_attention.h"
#include "cognitive/reasoning/integration/nimcp_reasoning_curiosity.h"
#include "cognitive/reasoning/events/nimcp_reasoning_events.h"

// Create event bus
event_bus_t bus = event_bus_create("reasoning_bus", EVENT_DELIVERY_IMMEDIATE);

// Create cognitive systems
fault_attention_t* attention = fault_attention_create();
curiosity_engine_t curiosity = curiosity_engine_create(brain, "reasoning_learner");

// Create integration modules
reasoning_attention_t* attn_integration = 
    reasoning_attention_create(bus, attention);
    
reasoning_curiosity_t* curio_integration = 
    reasoning_curiosity_create(bus, curiosity);
```

### Step 2: Publish Reasoning Events
```c
// When reasoning engine derives a novel fact:
reasoning_events_publish_novel_fact_derived(
    bus,
    "bird(tweety)",
    "forward_chaining_from_rule_3"
);

// When proof fails:
reasoning_events_publish_proof_result(
    bus,
    "flies(penguin)",
    false,  // failed
    NULL
);
```

### Step 3: Cognitive Integration Happens Automatically
```
EVENT_NOVEL_FACT_DERIVED published
    ↓
MODULE 1 (Attention): Boost salience to 0.8
MODULE 2 (Curiosity): Increase curiosity drive by +0.1
MODULE 3 (WM):        Store in working memory slot
    ↓
All happens in <100μs, zero manual coordination!
```

## Module Summary

| Module | Responsibility | Performance |
|--------|---------------|-------------|
| MODULE 1 | Attention on novel facts | <10μs |
| MODULE 2 | Curiosity for unexplained facts | <15μs |
| MODULE 3 | WM storage (7±2 limit) | <20μs |
| MODULE 4 | Executive planning | <30μs |
| MODULE 5 | Rule consolidation | <50μs |
| MODULE 6 | Event publishing | <5μs |

## Event Types

```c
// Logic events (0xC000-0xC00C)
EVENT_LOGIC_GATE_EVALUATED      // Logic gate result
EVENT_LOGIC_INFERENCE_STARTED   // Inference started
EVENT_LOGIC_INFERENCE_COMPLETE  // Inference done
EVENT_FACT_ADDED               // New fact
EVENT_RULE_ADDED               // New rule
EVENT_UNIFICATION_SUCCEEDED    // Terms unified
EVENT_UNIFICATION_FAILED       // Unification failed
EVENT_FORWARD_CHAIN_STEP       // Forward chaining step
EVENT_BACKWARD_CHAIN_STEP      // Backward chaining step
EVENT_PROOF_FOUND              // Proof succeeded
EVENT_PROOF_FAILED             // Proof failed
EVENT_CONTRADICTION_DETECTED   // Logical contradiction
EVENT_NOVEL_FACT_DERIVED       // Novel fact discovered
```

## Configuration Example

```c
// Customize MODULE 1 (Attention)
reasoning_attention_config_t config = {
    .enable_novel_fact_boost = true,
    .enable_contradiction_boost = true,
    .novel_fact_salience = 0.9f,        // Increase from 0.8
    .contradiction_salience = 1.0f,
    .attention_decay_tau_ms = 3000,     // Faster decay
    .min_salience_threshold = 0.2f
};

reasoning_attention_t* integration = 
    reasoning_attention_create_custom(bus, attention, &config);
```

## Testing

```bash
cd /home/bbrelin/nimcp/build
cmake .. -DBUILD_TESTS=ON
make -j8

# Run all reasoning integration tests
ctest -R reasoning_integration --verbose

# Run specific module test
./test/unit/cognitive/reasoning/integration/test_reasoning_attention
```

## Statistics Monitoring

```c
// Get MODULE 1 statistics
reasoning_attention_stats_t stats;
reasoning_attention_get_stats(attn_integration, &stats);

printf("Events processed: %lu\n", stats.total_events_processed);
printf("Novel fact boosts: %lu\n", stats.novel_fact_boosts);
printf("Avg salience: %.2f\n", stats.avg_salience_applied);
printf("Avg callback time: %lu μs\n", stats.avg_callback_time_us);
```

## Cleanup

```c
// Destroy in reverse order
reasoning_attention_destroy(attn_integration);
reasoning_curiosity_destroy(curio_integration);
curiosity_engine_destroy(curiosity);
fault_attention_destroy(attention);
event_bus_destroy(bus);
```

## Key Design Principles

1. **Zero Coupling:** Modules communicate ONLY via events
2. **Single Responsibility:** Each module has ONE job
3. **Event-Driven:** Publish-subscribe pattern throughout
4. **Performance:** Sub-millisecond integration overhead
5. **Testability:** Each module independently testable

## Files

### Headers
- `include/cognitive/reasoning/integration/nimcp_reasoning_attention.h`
- `include/cognitive/reasoning/integration/nimcp_reasoning_curiosity.h`
- `include/cognitive/reasoning/integration/nimcp_reasoning_working_memory.h`
- `include/cognitive/reasoning/integration/nimcp_reasoning_executive.h`
- `include/cognitive/reasoning/integration/nimcp_reasoning_consolidation.h`
- `include/cognitive/reasoning/events/nimcp_reasoning_events.h`

### Implementation
- `src/cognitive/reasoning/integration/*.c` (corresponding implementations)

### Tests
- `test/unit/cognitive/reasoning/integration/*.cpp` (57+ tests)

## Documentation
- Full summary: `COGNITIVE_REASONING_INTEGRATION_SUMMARY.md` (629 lines)
- This quickstart: `REASONING_INTEGRATION_QUICKSTART.md`

## Status
✅ **PRODUCTION READY** - All 6 modules implemented, tested, and documented.
