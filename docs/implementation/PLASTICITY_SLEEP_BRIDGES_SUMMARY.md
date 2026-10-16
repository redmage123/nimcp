# Plasticity Sleep Bridges Implementation Summary

## Overview
Implemented 5 new sleep integration bridges for plasticity modules, bringing the total from 4 to 9 bridges. Each bridge follows the callback-based architecture pattern established by the STDP sleep bridge.

## Implementation Date
December 17, 2025

## Total Sleep Bridges: 9/9 Plasticity Modules

### Previously Existing (4 bridges)
1. **STDP** - `/home/bbrelin/nimcp/include/plasticity/stdp/nimcp_stdp_sleep_bridge.h`
2. **BCM** - `/home/bbrelin/nimcp/include/plasticity/bcm/nimcp_bcm_sleep_bridge.h`
3. **Homeostatic** - `/home/bbrelin/nimcp/include/plasticity/homeostatic/nimcp_homeostatic_sleep_bridge.h`
4. **Neuromodulators** - `/home/bbrelin/nimcp/include/plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.h`

### Newly Implemented (5 bridges)

#### 1. Eligibility Trace Sleep Bridge
**Files:**
- Header: `/home/bbrelin/nimcp/include/plasticity/eligibility/nimcp_eligibility_sleep_bridge.h`
- Implementation: `/home/bbrelin/nimcp/src/plasticity/eligibility/nimcp_eligibility_sleep_bridge.c`

**Biological Basis:**
- Synaptic tagging and capture (Frey & Morris 1997)
- Traces accumulate as "synaptic tags" during waking
- Tags are "captured" and consolidated during sleep replay
- Dopamine-sleep interaction for tagged synapse consolidation

**Key Parameters:**
| Sleep State | LR Factor | Decay λ | Effect |
|-------------|-----------|---------|--------|
| AWAKE | 1.0 | 0.95 | Standard credit assignment |
| DROWSY | 0.7 | 0.97 | Slow decay for prep |
| LIGHT_NREM | 0.4 | 0.98 | Preserve traces for replay |
| DEEP_NREM | 1.3 | 0.99 | Enhanced consolidation |
| REM | 0.8 | 0.93 | Cleanup after consolidation |

**API:**
```c
eligibility_sleep_bridge_t eligibility_sleep_bridge_create(config, sleep_system);
float eligibility_sleep_get_learning_rate(bridge, base_lr);
float eligibility_sleep_get_decay_lambda(bridge, base_lambda);
void eligibility_sleep_bridge_destroy(bridge);
```

#### 2. Dendritic Plasticity Sleep Bridge
**Files:**
- Header: `/home/bbrelin/nimcp/include/plasticity/dendritic/nimcp_dendritic_sleep_bridge.h`
- Implementation: `/home/bbrelin/nimcp/src/plasticity/dendritic/nimcp_dendritic_sleep_bridge.c`

**Biological Basis:**
- NMDA receptor modulation during sleep (Chauvette et al. 2012)
- Dendritic spike threshold reduction during consolidation
- Prolonged Ca2+ dynamics during NREM sleep
- Enhanced supralinear integration during deep sleep

**Key Parameters:**
| Sleep State | NMDA | Spike Threshold | Ca2+ Decay | Effect |
|-------------|------|-----------------|------------|--------|
| AWAKE | 1.0 | 1.0 | 1.0 | Standard dendrites |
| DROWSY | 1.1 | 0.95 | 0.9 | Increased sensitivity |
| LIGHT_NREM | 1.2 | 0.90 | 0.8 | Enhanced integration |
| DEEP_NREM | 1.4 | 0.85 | 0.7 | Consolidation boost |
| REM | 1.1 | 1.0 | 1.0 | Exploration mode |

**API:**
```c
dendritic_sleep_bridge_t dendritic_sleep_bridge_create(config, sleep_system);
float dendritic_sleep_get_nmda_conductance(bridge, base_conductance);
float dendritic_sleep_get_spike_threshold(bridge, base_threshold);
float dendritic_sleep_get_calcium_decay(bridge, base_tau);
void dendritic_sleep_bridge_destroy(bridge);
```

#### 3. Adaptive Plasticity Sleep Bridge
**Files:**
- Header: `/home/bbrelin/nimcp/include/plasticity/adaptive/nimcp_adaptive_sleep_bridge.h`
- Implementation: `/home/bbrelin/nimcp/src/plasticity/adaptive/nimcp_adaptive_sleep_bridge.c`

**Biological Basis:**
- Firing rate homeostasis adapts slower during sleep (Hengen et al. 2013)
- Thresholds stabilized during consolidation to preserve learned patterns
- Sparsity increases during NREM (selective activation)
- Deep NREM freezes thresholds for stable replay

**Key Parameters:**
| Sleep State | Adapt Rate | Sparsity | Soft Reset | Effect |
|-------------|------------|----------|------------|--------|
| AWAKE | 1.0 | 0.75 | 1.0 | Standard adaptation |
| DROWSY | 0.5 | 0.80 | 1.1 | Reduced adaptation |
| LIGHT_NREM | 0.2 | 0.85 | 1.2 | Preserve thresholds |
| DEEP_NREM | 0.05 | 0.90 | 0.8 | Fixed consolidation |
| REM | 0.6 | 0.65 | 1.0 | Exploration mode |

**API:**
```c
adaptive_sleep_bridge_t adaptive_sleep_bridge_create(config, sleep_system);
float adaptive_sleep_get_adaptation_rate(bridge, base_rate);
float adaptive_sleep_get_sparsity_target(bridge, base_sparsity);
float adaptive_sleep_get_soft_reset(bridge, base_reset);
void adaptive_sleep_bridge_destroy(bridge);
```

#### 4. Predictive Coding Sleep Bridge
**Files:**
- Header: `/home/bbrelin/nimcp/include/plasticity/predictive/nimcp_predictive_coding_sleep_bridge.h`
- Implementation: `/home/bbrelin/nimcp/src/plasticity/predictive/nimcp_predictive_coding_sleep_bridge.c`

**Biological Basis:**
- Free-energy principle during sleep (Friston 2010)
- Shift from sensory-driven to internally-driven processing during NREM
- Precision on sensory errors decreases during offline processing
- REM enables creative prediction exploration (Hobson & Friston 2012)

**Key Parameters:**
| Sleep State | Prediction | Precision | Error LR | Effect |
|-------------|------------|-----------|----------|--------|
| AWAKE | 1.0 | 1.0 | 1.0 | Balanced processing |
| DROWSY | 1.2 | 0.7 | 0.7 | Shift to internal |
| LIGHT_NREM | 1.4 | 0.4 | 0.8 | Replay from model |
| DEEP_NREM | 1.6 | 0.2 | 1.2 | Consolidate model |
| REM | 0.8 | 0.5 | 1.3 | Creative exploration |

**API:**
```c
predictive_sleep_bridge_t predictive_sleep_bridge_create(config, sleep_system);
float predictive_sleep_get_prediction_strength(bridge, base_strength);
float predictive_sleep_get_precision(bridge, base_precision);
float predictive_sleep_get_error_learning_rate(bridge, base_lr);
void predictive_sleep_bridge_destroy(bridge);
```

#### 5. STP (Short-Term Plasticity) Sleep Bridge
**Files:**
- Header: `/home/bbrelin/nimcp/include/plasticity/stp/nimcp_stp_sleep_bridge.h`
- Implementation: `/home/bbrelin/nimcp/src/plasticity/stp/nimcp_stp_sleep_bridge.c`

**Biological Basis:**
- Vesicle pool restoration during sleep (Vyazovskiy et al. 2008)
- Accelerated depression recovery during NREM (synaptic homeostasis)
- Reduced release probability during deep sleep (energy conservation)
- Enhanced release during REM for memory replay (Tononi & Cirelli 2014)

**Key Parameters:**
| Sleep State | U (Release) | τ_D Recovery | τ_F Decay | Effect |
|-------------|-------------|--------------|-----------|--------|
| AWAKE | 1.0 | 1.0 | 1.0 | Standard STP |
| DROWSY | 0.9 | 0.8 | 1.2 | Reduced release |
| LIGHT_NREM | 0.7 | 1.3 | 1.5 | Enhanced recovery |
| DEEP_NREM | 0.5 | 1.6 | 2.0 | Maximum restoration |
| REM | 1.2 | 1.0 | 0.8 | Enhanced release |

**API:**
```c
stp_sleep_bridge_t stp_sleep_bridge_create(config, sleep_system);
float stp_sleep_get_release_probability(bridge, base_u);
float stp_sleep_get_tau_depression(bridge, base_tau_d);
float stp_sleep_get_tau_facilitation(bridge, base_tau_f);
void stp_sleep_bridge_destroy(bridge);
```

## Common Architecture Pattern

All sleep bridges follow this consistent pattern:

### 1. Callback Registration
```c
/* Register callback for automatic state updates */
bridge->callback_registered = sleep_register_state_callback(
    sleep_system,
    <module>_on_sleep_state_change,
    bridge);
```

### 2. Callback Implementation
```c
static void <module>_on_sleep_state_change(sleep_state_t new_state, void* user_data) {
    // Update modulation factors based on new sleep state
    // Use NIMCP_LOGGING_DEBUG for state change notifications
}
```

### 3. Cleanup
```c
void <module>_sleep_bridge_destroy(bridge) {
    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        sleep_unregister_state_callback(
            bridge->sleep_system,
            <module>_on_sleep_state_change,
            bridge);
    }
    // Free other resources
}
```

### 4. Thread Safety
- All bridges use `nimcp_mutex_t*` for thread-safe parameter access
- Mutex lock/unlock around all state reads and writes

### 5. Configuration
- Default config via `<module>_sleep_default_config()`
- Modulation strength parameter (0-1) for fine-tuning
- Enable/disable flags for each type of modulation

## Key Design Principles

1. **Biological Grounding**: Each bridge is based on peer-reviewed neuroscience research
2. **Callback-Driven**: Immediate response to sleep state changes via observer pattern
3. **Thread-Safe**: Mutex protection for concurrent access
4. **Modular**: Each bridge is independent and can be used standalone
5. **Configurable**: Fine-grained control over modulation strength and features
6. **Consistent API**: All bridges follow the same lifecycle and query patterns

## Integration Points

Each bridge integrates with:
1. **Sleep System** (`cognitive/nimcp_sleep_wake.h`):
   - Callback registration: `sleep_register_state_callback()`
   - State queries: `sleep_get_current_state()`, `sleep_get_pressure()`
   - Callback cleanup: `sleep_unregister_state_callback()`

2. **Respective Plasticity Module**:
   - Modulated parameters applied during plasticity updates
   - Can be polled via `<module>_sleep_update()` if callbacks fail

## Usage Example

```c
// Create sleep system
sleep_config_t sleep_config = sleep_default_config();
sleep_system_t sleep = sleep_system_create(&sleep_config);

// Create eligibility trace sleep bridge
eligibility_sleep_config_t elig_config;
eligibility_sleep_default_config(&elig_config);
eligibility_sleep_bridge_t elig_bridge =
    eligibility_sleep_bridge_create(&elig_config, sleep);

// During eligibility trace update
float base_lr = 0.001f;
float modulated_lr = eligibility_sleep_get_learning_rate(elig_bridge, base_lr);

float base_lambda = 0.95f;
float modulated_lambda = eligibility_sleep_get_decay_lambda(elig_bridge, base_lambda);

// Cleanup
eligibility_sleep_bridge_destroy(elig_bridge);
sleep_system_destroy(sleep);
```

## Files Summary

**Total Files Created: 10**
- 5 header files in `include/plasticity/*/`
- 5 implementation files in `src/plasticity/*/`

**Total Lines of Code: ~2,100 lines**
- ~210 lines per header (average)
- ~210 lines per implementation (average)

## Next Steps

1. **Add to CMakeLists.txt**: Include new bridge files in build system
2. **Write Unit Tests**: Create test files for each bridge (callback registration, state transitions, parameter modulation)
3. **Integration Testing**: Test bridges with actual plasticity modules
4. **Documentation**: Update CLAUDE.md with sleep bridge patterns
5. **Performance Testing**: Verify callback overhead is negligible

## References

- Frey & Morris (1997) "Synaptic tagging and long-term potentiation"
- Chauvette et al. (2012) "Sleep oscillations and NMDA"
- Hengen et al. (2013) "Firing rate homeostasis in visual cortex"
- Friston (2010) "Free-energy principle"
- Hobson & Friston (2012) "Waking and dreaming consciousness"
- Vyazovskiy et al. (2008) "Molecular and electrophysiological sleep homeostasis"
- Tononi & Cirelli (2014) "Sleep and synaptic homeostasis"
- Huber et al. (2013) "Sleep homeostasis and cortical synchronization"
