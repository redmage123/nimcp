# Bio-Async Integration

Asynchronous messaging infrastructure for inter-module communication.

## Core Infrastructure
- 8-thread async messaging pool
- Bio-async router with module registration (800+ modules)
- Inbox capacity per module (16-256 messages)
- Promise-based completion: `nimcp_bio_promise_complete(promise, result)` -- 2 args only
- Handle tracker for safe handle management
- Predictive message routing (optional)

## Messaging Features
- Weight change broadcasts via plasticity bridge
- State broadcasts for sleep/wake transitions
- Bio-router re-registration is LOG_DEBUG (not LOG_WARN)

## Immune Bridge Integration (24+ bridges)

| Category | Bridges |
|----------|---------|
| Cognitive | attention, emotion, memory, reasoning, executive, introspection, curiosity, wellbeing, mental_health, tom, self_model, sleep |
| Perception | visual, audio, speech |
| Plasticity | stdp, bcm, homeostatic, synaptic_scaling, eligibility, dendritic |
| Middleware | routing, buffering, population_coding, feature_extractor, thalamic, sequence, training |
| Core | oscillations, cortical, broca |

## Bio-async Pattern
```c
int <prefix>_connect_bio_async(<bridge>_t* bridge);
int <prefix>_disconnect_bio_async(<bridge>_t* bridge);
bool <prefix>_is_bio_async_connected(const <bridge>_t* bridge);
```

## Module IDs: 0x0D00 - 0x0DFF

## GOTCHAs
- `nimcp_bio_promise_complete()` takes exactly 2 args, NOT 3
- Re-registration of already-registered modules logs at DEBUG level, not WARN
