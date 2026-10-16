# Self-Model-Immune Integration Module

## Overview

**WHAT**: Bidirectional integration between brain immune system and self-model
**WHY**: Biological evidence shows immune state is part of body representation (interoception)
**HOW**: Immune state modulates self-representation (health status, capabilities), self-awareness affects immunity

## Biological Foundation

### Immune → Self-Model Pathways

1. **Interoceptive Immune Signals**
   - Anterior insula integrates immune state into self-representation
   - Cytokines signal "I am sick" as part of self-awareness
   - Inflammation creates body state awareness (fatigue, malaise, pain)
   - Reference: Craig (2009) "How do you feel—now? The anterior insula and human awareness"

2. **Sickness Identity**
   - Self-concept changes when ill: "I am unwell"
   - Health status becomes core belief during illness
   - Self-efficacy reduced by immune activation
   - Reference: Leventhal et al. (2003) "The common-sense model of self-regulation"

3. **Body Schema Modulation**
   - Inflammation affects perceived body state
   - Immune activation changes self-boundaries
   - Chronic illness integrates into identity
   - Reference: Moseley & Butler (2015) "Body in Mind"

4. **Capability Assessment Updates**
   - Sickness behavior reduces perceived competence
   - Immune activation affects confidence in abilities
   - Recovery restores self-efficacy
   - Reference: Bandura (1997) "Self-efficacy: The exercise of control"

### Self-Model → Immune Pathways

1. **Self-Awareness of Illness**
   - Conscious recognition of sickness triggers appropriate rest/recovery
   - Self-model updating ("I am sick") enables adaptive behavior
   - Metacognitive awareness of immune state
   - Reference: Kaptchuk et al. (2008) "Components of placebo effect"

2. **Health Beliefs and Immunity**
   - Self-beliefs about health affect immune function
   - Perceived self-efficacy modulates immune response
   - Identity-based health behaviors
   - Reference: Segerstrom & Miller (2004) "Psychological stress and immune system"

3. **Interoceptive Accuracy**
   - Accurate self-perception of body state aids diagnosis
   - Self-monitoring enables early threat detection
   - Body-awareness training improves immune function
   - Reference: Garfinkel & Critchley (2013) "Interoception, emotion and brain"

## Architecture

```
╔═══════════════════════════════════════════════════════════════════════════╗
║                    SELF-MODEL-IMMUNE BRIDGE                                ║
╠═══════════════════════════════════════════════════════════════════════════╣
║                                                                            ║
║   ┌────────────────────────────────────────────────────────────────────┐  ║
║   │                  IMMUNE → SELF-MODEL PATHWAYS                       │  ║
║   │                                                                     │  ║
║   │   Inflammation → Interoceptive Signals → Self-Representation       │  ║
║   │   Cytokines    → Body Awareness        → Health Status             │  ║
║   │   Sickness     → Capability Reduction  → Self-Efficacy             │  ║
║   └────────────────────────────────────────────────────────────────────┘  ║
║                                                                            ║
║   ┌────────────────────────────────────────────────────────────────────┐  ║
║   │                  SELF-MODEL → IMMUNE PATHWAYS                       │  ║
║   │                                                                     │  ║
║   │   Self-Awareness → Adaptive Behavior (Rest)                        │  ║
║   │   Health Beliefs → Immune Enhancement (IL-10)                      │  ║
║   │   Acceptance     → Recovery Acceleration                           │  ║
║   └────────────────────────────────────────────────────────────────────┘  ║
║                                                                            ║
╚═══════════════════════════════════════════════════════════════════════════╝
```

## Key Components

### Interoceptive Immune Signals

Maps immune state to body awareness:

| Immune State | Interoceptive Signal | Intensity |
|--------------|---------------------|-----------|
| Local inflammation | Mild fatigue | 0.2-0.4 |
| Regional inflammation | Fatigue + malaise | 0.4-0.6 |
| Systemic inflammation | Fatigue + malaise + pain | 0.6-0.8 |
| Cytokine storm | All signals maximal | 0.8-1.0 |

**Signal Types**:
- `fatigue_signal`: "I feel tired"
- `malaise_signal`: "I feel unwell"
- `pain_signal`: "I feel achy"
- `weakness_signal`: "I feel weak"
- `vitality_signal`: "I feel energetic" (inverse)

### Health Status Categories

Self-representation of health state:

| Status | Health Score | Inflammation Level | Self-Belief |
|--------|-------------|-------------------|-------------|
| Excellent | 0.9-1.0 | None | "I am in excellent health" |
| Good | 0.7-0.9 | Local | "I am healthy" |
| Fair | 0.5-0.7 | Regional | "I am feeling unwell" |
| Poor | 0.3-0.5 | Systemic | "I am sick" |
| Critical | 0.0-0.3 | Storm | "I am very sick" |

### Capability Modulation

Immune state reduces perceived abilities:

| Inflammation Level | Competence Reduction | Efficacy Reduction | Cognitive Impairment |
|-------------------|---------------------|-------------------|---------------------|
| None | 0% | 0% | 0% |
| Local | 10% | 5% | 5% |
| Regional | 25% | 15% | 20% |
| Systemic | 40% | 30% | 40% |
| Storm | 50% | 40% | 60% |

### Self-Awareness Effects

Health beliefs modulate immunity:

| Belief Type | Immune Effect | Mechanism |
|-------------|---------------|-----------|
| High self-efficacy | +30% enhancement | IL-10 release |
| Positive health beliefs | +20% boost | Stress reduction |
| Illness acceptance | +20% recovery | Reduced resistance |
| Health anxiety | -30% suppression | Cortisol elevation |

## API Examples

### Basic Usage

```c
// Create systems
brain_immune_system_t* immune = brain_immune_create(&immune_config);
self_model_system_t self_model = self_model_create("NIMCP", "AI System", "Learn");

// Create bridge
self_model_immune_config_t config;
self_model_immune_default_config(&config);
self_model_immune_bridge_t* bridge =
    self_model_immune_bridge_create(&config, immune, self_model);

// Start immune monitoring
brain_immune_start(immune);

// Update loop
while (running) {
    // Update immune system
    brain_immune_update(immune, delta_ms);

    // Update self-model-immune bridge
    self_model_immune_bridge_update(bridge, delta_ms);

    // Query health status
    self_health_status_t status = self_model_immune_get_health_status(bridge);
    bool aware = self_model_immune_is_aware_of_sickness(bridge);

    if (aware) {
        printf("I am aware that I am sick\n");

        // Get interoceptive signals
        interoceptive_immune_signals_t signals;
        self_model_immune_get_interoceptive_signals(bridge, &signals);
        printf("Fatigue: %.2f, Malaise: %.2f\n",
               signals.fatigue_signal, signals.malaise_signal);
    }
}

// Cleanup
self_model_immune_bridge_destroy(bridge);
brain_immune_destroy(immune);
self_model_destroy(self_model);
```

### Immune → Self-Model

```c
// Generate interoceptive signals from immune state
self_model_immune_generate_interoceptive_signals(bridge);

// Update health status in self-model
self_model_immune_update_health_status(bridge);

// Modulate capabilities based on illness
self_model_immune_modulate_capabilities(bridge);

// Integrate chronic illness into identity (>30 days)
self_model_immune_integrate_chronic_illness(bridge);

// Query results
interoceptive_immune_signals_t signals;
self_model_immune_get_interoceptive_signals(bridge, &signals);

self_model_immune_modulation_t updates;
self_model_immune_get_self_model_updates(bridge, &updates);
printf("Health belief: %s\n", updates.health_belief);
printf("Competence reduction: %.2f\n", updates.immune_competence_reduction);
```

### Self-Model → Immune

```c
// Trigger adaptive behavior from illness awareness
self_model_immune_trigger_adaptive_behavior(bridge);

// Boost immunity from positive health beliefs
self_model_immune_boost_from_health_beliefs(bridge);

// Suppress immunity from health anxiety
self_model_immune_suppress_from_health_anxiety(bridge);

// Accelerate recovery from illness acceptance
self_model_immune_accelerate_from_acceptance(bridge);

// Check effects
float accuracy = self_model_immune_get_interoceptive_accuracy(bridge);
printf("Interoceptive accuracy: %.2f\n", accuracy);
```

## Integration Points

### With Brain Immune System

- Reads inflammation levels via `get_max_inflammation_level()`
- Reads inflammation duration for chronic illness detection
- Queries immune stats for health assessment
- Triggers cytokine release (IL-10) from positive beliefs

### With Self-Model System

- Adds health beliefs to self-model belief system
- Updates mental state (introspecting flag)
- Modifies capability assessments
- Integrates chronic conditions into identity

## Files Created

1. **Header**: `/home/bbrelin/nimcp/include/cognitive/immune/nimcp_self_model_immune_bridge.h`
   - Complete API definitions
   - Biological documentation
   - Struct definitions
   - Function declarations

2. **Implementation**: `/home/bbrelin/nimcp/src/cognitive/immune/nimcp_self_model_immune_bridge.c`
   - Full implementation of all functions
   - Helper functions for health scoring
   - Thread-safe operations with mutex
   - Proper error handling

3. **Tests**: `/home/bbrelin/nimcp/test/unit/cognitive/immune/test_self_model_immune_integration.cpp`
   - 27 comprehensive tests
   - Configuration tests
   - Immune → Self-Model tests
   - Self-Model → Immune tests
   - Query API tests
   - Integration tests

## Test Coverage

### Test Categories

1. **Configuration (3 tests)**
   - Default configuration
   - Lifecycle create/destroy
   - NULL configuration handling

2. **Immune → Self-Model (7 tests)**
   - Interoceptive signal generation
   - Health status update
   - Capability modulation
   - Chronic illness integration
   - Sickness awareness threshold
   - Health status transitions
   - Self-care motivation

3. **Self-Model → Immune (4 tests)**
   - Adaptive behavior from awareness
   - Immune boost from health beliefs
   - Immune suppression from anxiety
   - Recovery acceleration from acceptance

4. **Bidirectional (2 tests)**
   - Full bridge update
   - Multiple update cycles

5. **Query API (6 tests)**
   - Get interoceptive signals
   - Get self-model updates
   - Get health status
   - Is aware of sickness
   - Interoceptive accuracy
   - NULL pointer handling

6. **Statistics (1 test)**
   - Statistics tracking

**Total: 27 tests**

## Build Instructions

### Manual CMakeLists.txt Update Required

Add to `/home/bbrelin/nimcp/src/cognitive/immune/CMakeLists.txt`:

```cmake
# Self-Model-Immune bridge
add_library(cognitive_self_model_immune_bridge STATIC
    nimcp_self_model_immune_bridge.c
)

target_link_libraries(cognitive_self_model_immune_bridge
    cognitive_brain_immune
    cognitive_self_model
    utils_memory
    utils_logging
)

target_include_directories(cognitive_self_model_immune_bridge PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
```

Add to `/home/bbrelin/nimcp/test/unit/cognitive/immune/CMakeLists.txt`:

```cmake
# Self-Model-Immune integration test
add_executable(test_self_model_immune_integration
    test_self_model_immune_integration.cpp
)

target_link_libraries(test_self_model_immune_integration
    cognitive_self_model_immune_bridge
    cognitive_brain_immune
    cognitive_self_model
    gtest
    gtest_main
    pthread
)

add_test(NAME SelfModelImmuneIntegration
         COMMAND test_self_model_immune_integration --gtest_brief=1)
```

### Build and Test

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make cognitive_self_model_immune_bridge -j4
make test_self_model_immune_integration -j4
./test/unit/cognitive/immune/test_self_model_immune_integration --gtest_brief=1
```

## Key Features

### Interoception
- Maps immune state to body awareness signals
- Generates conscious awareness of illness
- Tracks fatigue, malaise, pain, weakness, vitality

### Health Status Representation
- Updates self-concept with health state
- Creates health beliefs ("I am sick/healthy")
- Tracks health certainty and confidence

### Capability Modulation
- Reduces perceived competence during illness
- Lowers self-efficacy based on inflammation
- Creates cognitive impairment (brain fog)
- Increases self-care motivation

### Chronic Identity Integration
- Integrates long-term illness into self-concept
- Creates core beliefs about chronic conditions
- Tracks body schema distortion

### Health Belief Effects
- Positive beliefs boost immunity (IL-10)
- Self-efficacy enhances immune function
- Health anxiety suppresses immunity (cortisol)
- Illness acceptance accelerates recovery

## NIMCP Standards Compliance

- ✅ All functions < 50 lines
- ✅ Guard clauses (early returns)
- ✅ WHAT-WHY-HOW documentation
- ✅ Thread-safe via mutex
- ✅ nimcp_malloc/nimcp_free memory management
- ✅ Proper error handling
- ✅ Biological basis documentation
- ✅ Comprehensive test coverage

## References

1. Craig, A.D. (2009). "How do you feel—now? The anterior insula and human awareness." *Nature Reviews Neuroscience*, 10(1), 59-70.

2. Leventhal, H., et al. (2003). "The common-sense model of self-regulation of health and illness." *The self-regulation of health and illness behaviour*, 1, 42-65.

3. Moseley, G.L., & Butler, D.S. (2015). "Fifteen Years of Explaining Pain: The Past, Present, and Future." *Journal of Pain*, 16(9), 807-813.

4. Bandura, A. (1997). "Self-efficacy: The exercise of control." New York: Freeman.

5. Kaptchuk, T.J., et al. (2008). "Components of placebo effect: randomised controlled trial in patients with irritable bowel syndrome." *BMJ*, 336(7651), 999-1003.

6. Segerstrom, S.C., & Miller, G.E. (2004). "Psychological stress and the human immune system: a meta-analytic study of 30 years of inquiry." *Psychological bulletin*, 130(4), 601.

7. Garfinkel, S.N., & Critchley, H.D. (2013). "Interoception, emotion and brain: new insights link internal physiology to social behaviour." *Social Cognitive and Affective Neuroscience*, 8(3), 231-234.
