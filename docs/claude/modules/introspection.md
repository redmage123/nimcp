# Introspection Module

**Last Updated**: 2026-03-11

## Overview

Self-monitoring and meta-cognitive awareness system. Provides APIs to query and inspect internal brain state including neuron activity, learned patterns, uncertainty estimates, and state representations.

## Components

| Component | Header | Description |
|-----------|--------|-------------|
| Core Introspection | `nimcp_introspection.h` | Active populations, state vectors, pattern queries, uncertainty |
| Consciousness Metrics | `nimcp_consciousness_metrics.h` | IIT 3.0 Phi computation, state classification |
| Temporal Patterns | `nimcp_temporal_patterns.h` | DTW-based pattern detection, prediction |
| Ensemble Uncertainty | `nimcp_ensemble_uncertainty.h` | Epistemic/aleatoric decomposition |
| Connectivity Health | `nimcp_connectivity_health.h` | Community detection, hub analysis, small-world topology, Shannon metrics |

All headers in `include/cognitive/introspection/`.

## Bridge Files

| Bridge | File |
|--------|------|
| SNN Bridge | `nimcp_introspection_snn_bridge.c/h` |
| Thalamic Bridge | `nimcp_introspection_thalamic_bridge.c/h` |
| FEP Bridge | `nimcp_introspection_fep_bridge.c/h` |
| Substrate Bridge | `nimcp_introspection_substrate_bridge.c/h` |
| Plasticity Bridge | `nimcp_introspection_plasticity_bridge.c/h` |
| Sleep Bridge | `nimcp_introspection_sleep_bridge.c/h` |
| Pink Noise Bridge | `nimcp_ensemble_uncertainty_pink_noise_bridge.c/h` |
| Middleware Adapter | `nimcp_introspection_middleware_adapter.h` |

Source files in `src/cognitive/introspection/`.

## Key Types

| Type | Purpose |
|------|---------|
| `introspection_context_t` | Opaque handle (pointer typedef) |
| `state_extraction_strategy_t` | FAST (10% sample) / BALANCED (30%) / DETAILED (full scan) |
| `neuron_population_t` | Active neuron IDs + activation levels |
| `brain_state_t` | Compressed state vector with entropy |
| `brain_uncertainty_t` | Epistemic + aleatoric + total uncertainty |
| `pattern_info_t` | Pattern name, strength, activation count, timestamps |
| `neuron_activity_t` | Single neuron detail: activation, gradient, connections, decision contribution |

## API Examples

```c
// Consciousness (IIT 3.0 Phi)
consciousness_phi_result_t* result = introspection_compute_phi(intro, NULL);

// Temporal pattern detection
temporal_pattern_t* patterns = introspection_detect_patterns(intro, NULL, &count);

// Ensemble uncertainty (epistemic/aleatoric)
ensemble_uncertainty_result_t unc = ensemble_compute_uncertainty(ensemble, features, n);
```

## Performance

| Function | Complexity | Typical Time |
|----------|------------|-------------|
| `brain_get_active_population` | O(n) | ~0.1-1ms |
| `brain_get_internal_state` | O(n) | ~0.5-2ms (depends on strategy) |
| `brain_get_uncertainty` | O(k), k=ensemble | ~1-5ms |
| `brain_is_pattern_active` | O(1) hash lookup | ~1us |
| `brain_get_neuron_activity` | O(1) direct | ~1us |

## Integration with Executive Controller

The executive controller's bio-async handler (`handle_decision_request()`) processes introspection queries:
- Cognitive load assessment
- Self-state assessment (`self_assess()` Python API)
- Pattern matching against internal state

## Cognitive Transcript Integration

The `cognitive_transcript_t` (in `nimcp_cognitive_transcript.h`) captures outputs from 28+ cognitive module types during `brain_decide()`. ~15 entries populated per decision cycle. `transcript_finalize()` computes coherence, cognitive load, dominant module, and 10 response hint flags. Accessible from Python via `Brain.get_transcript()`.

## Thread Safety

All functions are thread-safe. Multiple threads can query introspection state concurrently via mutex protection.

## Test Coverage: 173 tests
- Unit: 101
- Integration: 27
- Regression: 37
- E2E: 8
