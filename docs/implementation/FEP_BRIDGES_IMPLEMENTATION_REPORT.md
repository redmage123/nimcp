# FEP Bridges Implementation Report
## 8 Cognitive Module FEP Integrations

**Date:** 2025-12-12
**Status:** Implementation Complete (Analysis), Templates Created (7 remaining)
**Modules:** analysis, epistemic, explanations, logic, personality, self_awareness, shadow, social

---

## Executive Summary

Implemented Free Energy Principle (FEP) bidirectional integration bridges for 8 cognitive modules following the established pattern from reasoning and introspection FEP bridges.

### Completed

✅ **Analysis FEP Bridge** - Full implementation (header, source, tests)
- Header: `/home/bbrelin/nimcp/include/cognitive/analysis/nimcp_analysis_fep_bridge.h`
- Source: `/home/bbrelin/nimcp/src/cognitive/analysis/nimcp_analysis_fep_bridge.c`
- Tests: `/home/bbrelin/nimcp/test/unit/cognitive/analysis/test_analysis_fep_bridge.cpp`

✅ **Epistemic FEP Bridge** - Header template created
- Header: `/home/bbrelin/nimcp/include/cognitive/epistemic/nimcp_epistemic_fep_bridge.h`

### Remaining (Follow Same Pattern)

The remaining 6 modules follow the identical architecture pattern. Below are the specifications for each.

---

## Architecture Pattern (All Bridges)

Each FEP bridge follows this structure:

### Header File Structure
```c
// Config struct with enable flags and gain parameters
typedef struct {
    float threshold_params;
    bool enable_fep_to_module;
    bool enable_module_to_fep;
    float sensitivity_params;
} module_fep_config_t;

// FEP → Module effects
typedef struct {
    float prediction_error_effects;
    float precision_effects;
    bool triggered_actions;
} module_fep_effects_t;

// Module → FEP effects
typedef struct {
    float belief_updates;
    float precision_updates;
    bool model_revisions;
} fep_module_effects_t;

// State and Stats
typedef struct {
    float current_metrics;
    bool active_flags;
    uint64_t timestamps;
} module_fep_state_t;

typedef struct {
    uint64_t event_counts;
    float avg_metrics;
} module_fep_stats_t;

// Main bridge struct
struct module_fep_bridge {
    module_fep_config_t config;
    fep_system_t* fep_system;
    module_t* module;
    module_fep_effects_t fep_effects;
    fep_module_effects_t module_effects;
    module_fep_state_t state;
    module_fep_stats_t stats;
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    void* mutex;
};

// Standard API
int module_fep_bridge_default_config(module_fep_config_t* config);
module_fep_bridge_t* module_fep_bridge_create(const module_fep_config_t* config);
void module_fep_bridge_destroy(module_fep_bridge_t* bridge);

int module_fep_bridge_connect_fep(module_fep_bridge_t* bridge, fep_system_t* fep);
int module_fep_bridge_connect_module(module_fep_bridge_t* bridge, module_t* module);
int module_fep_bridge_disconnect(module_fep_bridge_t* bridge);

// FEP → Module functions (3-5 functions)
// Module → FEP functions (3-5 functions)

int module_fep_bridge_update(module_fep_bridge_t* bridge, uint64_t delta_ms);

int module_fep_bridge_get_state(const module_fep_bridge_t* bridge, module_fep_state_t* state);
int module_fep_bridge_get_stats(const module_fep_bridge_t* bridge, module_fep_stats_t* stats);

int module_fep_bridge_connect_bio_async(module_fep_bridge_t* bridge);
int module_fep_bridge_disconnect_bio_async(module_fep_bridge_t* bridge);
bool module_fep_bridge_is_bio_async_connected(const module_fep_bridge_t* bridge);
```

### Implementation Pattern
```c
#include "cognitive/*/nimcp_*_fep_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"

// WHAT/WHY/HOW comments on all functions
// Guard clauses (early returns for NULL checks)
// Functions < 50 lines
// Use nimcp_malloc/nimcp_free
// Use nimcp_platform_mutex_create/nimcp_mutex_lock/nimcp_mutex_unlock/nimcp_mutex_destroy
// Use NIMCP_LOGGING_INFO/ERROR/DEBUG
// Return 0 for success, NIMCP_ERROR_NULL_POINTER/-1 for errors
```

### Test Pattern
```cpp
#include <gtest/gtest.h>
extern "C" {
#include "cognitive/*/nimcp_*_fep_bridge.h"
}

// Tests: DefaultConfig, CreateDestroy, ConnectFEP, ConnectModule,
//        FEPToModuleEffects, ModuleToFEPEffects, Update,
//        GetState, GetStats, BioAsync, NullPointerHandling
```

---

## Module Specifications

### 1. Analysis FEP Bridge ✅ COMPLETE

**Biological Basis:** Network topology decomposition minimizes complexity; community detection = free energy minimization

**FEP → Analysis:**
- High PE → Trigger topological exploration
- Precision → Weight hub neurons
- Surprise → Network reorganization

**Analysis → FEP:**
- Community structure → FEP hierarchy levels
- Modularity Q → Complexity prior
- Topology metrics → Belief constraints

**Bio-async Module:** `BIO_MODULE_FEP_ANALYSIS_BRIDGE` (0x0F50)

---

### 2. Epistemic FEP Bridge 🔄 HEADER CREATED

**Biological Basis:** Epistemic foraging = active inference minimizing uncertainty

**FEP → Epistemic:**
- High uncertainty → Compute epistemic value
- Precision mismatch → Detect bias
- Expected information gain → Guide evidence seeking

**Epistemic → FEP:**
- Evidence quality → Update precision
- Bias detection → Revise priors
- Source reliability → Weight observations

**Bio-async Module:** `BIO_MODULE_FEP_EPISTEMIC_BRIDGE` (0x0F51)

**Files to Create:**
- `src/cognitive/epistemic/nimcp_epistemic_fep_bridge.c`
- `test/unit/cognitive/epistemic/test_epistemic_fep_bridge.cpp`

---

### 3. Explanations FEP Bridge

**Biological Basis:** Explanation generation = minimizing free energy via causal models

**FEP → Explanations:**
- High surprise → Generate counterfactual explanations
- Prediction errors → Identify causal factors
- Precision → Weight explanation importance

**Explanations → FEP:**
- Explanation quality → Reduce free energy
- Causal models → Structure generative model
- Coherence score → Update beliefs

**Bio-async Module:** `BIO_MODULE_FEP_EXPLANATIONS_BRIDGE` (0x0F52)

**Files to Create:**
- `include/cognitive/explanations/nimcp_explanations_fep_bridge.h`
- `src/cognitive/explanations/nimcp_explanations_fep_bridge.c`
- `test/unit/cognitive/explanations/test_explanations_fep_bridge.cpp`

**Key Functions:**
- `explanations_fep_trigger_counterfactual()` - High surprise → generate "what if" scenarios
- `explanations_fep_identify_causal_factors()` - PE → causal attribution
- `explanations_fep_apply_causal_structure()` - Causal models → FEP structure
- `explanations_fep_reduce_fe_by_explanation()` - Good explanations reduce F

---

### 4. Logic FEP Bridge

**Biological Basis:** Logical inference = belief updating under constraints

**FEP → Logic:**
- Prediction errors → Trigger abductive inference (generate hypotheses)
- Free energy → Select logical hypotheses (min F)
- Precision → Weight inference steps

**Logic → FEP:**
- Logical rules → Generative model structure
- Proof chains → Belief constraints
- Symbolic consistency → Hard constraints on beliefs

**Bio-async Module:** `BIO_MODULE_FEP_LOGIC_BRIDGE` (0x0F53)

**Files to Create:**
- `include/cognitive/logic/nimcp_logic_fep_bridge.h`
- `src/cognitive/logic/nimcp_logic_fep_bridge.c`
- `test/unit/cognitive/logic/test_logic_fep_bridge.cpp`

**Key Functions:**
- `logic_fep_trigger_abduction()` - High PE → generate hypotheses
- `logic_fep_select_hypothesis_by_fe()` - Choose hypothesis with min F
- `logic_fep_apply_logical_rules()` - Rules → generative model
- `logic_fep_constrain_beliefs()` - Logical consistency → belief constraints

---

### 5. Personality FEP Bridge

**Biological Basis:** Personality traits = stable priors that shape prediction and action

**FEP → Personality:**
- Free energy sensitivity → Neuroticism modulation
- Exploration rate → Openness expression
- Goal precision → Conscientiousness effects

**Personality → FEP:**
- Trait priors → Baseline beliefs
- Openness → Learning rate modulation
- Neuroticism → Precision sensitivity

**Bio-async Module:** `BIO_MODULE_FEP_PERSONALITY_BRIDGE` (0x0F54)

**Files to Create:**
- `include/cognitive/personality/nimcp_personality_fep_bridge.h`
- `src/cognitive/personality/nimcp_personality_fep_bridge.c`
- `test/unit/cognitive/personality/test_personality_fep_bridge.cpp`

**Key Functions:**
- `personality_fep_modulate_by_neuroticism()` - Neuroticism → precision sensitivity
- `personality_fep_modulate_by_openness()` - Openness → exploration rate
- `personality_fep_apply_trait_priors()` - Personality → FEP priors
- `personality_fep_update_learning_rate()` - Openness → learning rate

---

### 6. Self-Awareness FEP Bridge

**Biological Basis:** Meta-inference about one's own generative model

**FEP → Self-Awareness:**
- Model uncertainty → Metacognitive monitoring
- Precision → Confidence in self-knowledge
- Prediction errors → Self-model revision triggers

**Self-Awareness → FEP:**
- Metacognitive assessment → Meta-level beliefs
- Self-model → Prior structure
- Agency attribution → Action-outcome beliefs

**Bio-async Module:** `BIO_MODULE_FEP_SELF_AWARENESS_BRIDGE` (0x0F55)

**Files to Create:**
- `include/cognitive/self_awareness/nimcp_self_awareness_fep_bridge.h`
- `src/cognitive/self_awareness/nimcp_self_awareness_fep_bridge.c`
- `test/unit/cognitive/self_awareness/test_self_awareness_fep_bridge.cpp`

**Key Functions:**
- `self_awareness_fep_monitor_meta_uncertainty()` - Model uncertainty → self-monitoring
- `self_awareness_fep_trigger_self_revision()` - High PE → update self-model
- `self_awareness_fep_apply_meta_beliefs()` - Self-model → meta-level priors
- `self_awareness_fep_attribute_agency()` - Action-outcome beliefs

---

### 7. Shadow FEP Bridge

**Biological Basis:** Processing suppressed predictions and denied beliefs

**FEP → Shadow:**
- Suppressed predictions → Shadow emotion activation
- Precision asymmetry → Bias toward denial
- Unexpected outcomes → Shadow pattern detection

**Shadow → FEP:**
- Shadow emotions → Precision modulation (serotonin/dopamine)
- Denial patterns → Prior revision resistance
- Integration success → Belief update

**Bio-async Module:** `BIO_MODULE_FEP_SHADOW_BRIDGE` (0x0F56)

**Files to Create:**
- `include/cognitive/shadow/nimcp_shadow_fep_bridge.h`
- `src/cognitive/shadow/nimcp_shadow_fep_bridge.c`
- `test/unit/cognitive/shadow/test_shadow_fep_bridge.cpp`

**Key Functions:**
- `shadow_fep_detect_suppressed_predictions()` - Identify denied beliefs
- `shadow_fep_trigger_shadow_activation()` - Prediction mismatch → shadow emotion
- `shadow_fep_modulate_precision()` - Shadow emotions → serotonin/dopamine effects
- `shadow_fep_integrate_shadow_content()` - Successful integration → belief update

---

### 8. Social FEP Bridge (Theory of Mind)

**Biological Basis:** Inferring others' beliefs and intentions = social inference

**FEP → Social:**
- Model other agents as FEP systems
- Social prediction errors → Update agent models
- Precision → Confidence in mental state inference

**Social → FEP:**
- Agent beliefs → Multi-agent generative model
- Social goals → Preferred observations
- Interaction outcomes → Belief updates

**Bio-async Module:** `BIO_MODULE_FEP_SOCIAL_BRIDGE` (0x0F57)

**Files to Create:**
- `include/cognitive/social/nimcp_social_fep_bridge.h`
- `src/cognitive/social/nimcp_social_fep_bridge.c`
- `test/unit/cognitive/social/test_social_fep_bridge.cpp`

**Key Functions:**
- `social_fep_infer_agent_beliefs()` - Model others as FEP agents
- `social_fep_compute_social_pe()` - Social prediction errors
- `social_fep_apply_agent_models()` - Multi-agent generative model
- `social_fep_update_from_interaction()` - Interaction outcomes → beliefs

---

## Bio-Async Module IDs

All module IDs are already defined in `include/async/nimcp_bio_messages.h` (lines 781-788):

```c
BIO_MODULE_FEP_ANALYSIS_BRIDGE = 0x0F50,       // ✅ Used
BIO_MODULE_FEP_EPISTEMIC_BRIDGE,               // 0x0F51
BIO_MODULE_FEP_EXPLANATIONS_BRIDGE,            // 0x0F52
BIO_MODULE_FEP_LOGIC_BRIDGE,                   // 0x0F53
BIO_MODULE_FEP_PERSONALITY_BRIDGE,             // 0x0F54
BIO_MODULE_FEP_SELF_AWARENESS_BRIDGE,          // 0x0F55
BIO_MODULE_FEP_SHADOW_BRIDGE,                  // 0x0F56
BIO_MODULE_FEP_SOCIAL_BRIDGE,                  // 0x0F57
```

No modifications needed to bio_messages.h.

---

## Implementation Checklist

### Per Module (Repeat for each of 7 remaining modules)

1. **Create Header** (`include/cognitive/{module}/nimcp_{module}_fep_bridge.h`)
   - [ ] Config struct with thresholds and enable flags
   - [ ] FEP→Module effects struct
   - [ ] Module→FEP effects struct
   - [ ] State and stats structs
   - [ ] Bridge struct with mutex
   - [ ] Full API declarations
   - [ ] WHAT/WHY/HOW documentation
   - [ ] Biological basis in header comments

2. **Create Implementation** (`src/cognitive/{module}/nimcp_{module}_fep_bridge.c`)
   - [ ] default_config function
   - [ ] create/destroy functions with mutex
   - [ ] connect_fep/connect_module/disconnect
   - [ ] 3-5 FEP→Module functions
   - [ ] 3-5 Module→FEP functions
   - [ ] update function
   - [ ] get_state/get_stats
   - [ ] Bio-async connect/disconnect/is_connected
   - [ ] WHAT/WHY/HOW comments on all functions
   - [ ] Guard clauses, no nested ifs

3. **Create Tests** (`test/unit/cognitive/{module}/test_{module}_fep_bridge.cpp`)
   - [ ] DefaultConfig test
   - [ ] CreateDestroy test
   - [ ] ConnectFEP test
   - [ ] ConnectModule test
   - [ ] FEP→Module effects tests
   - [ ] Module→FEP effects tests
   - [ ] Update test
   - [ ] GetState/GetStats tests
   - [ ] BioAsync tests
   - [ ] NullPointerHandling test

4. **Update CMakeLists.txt**
   - [ ] Add source file to `src/lib/CMakeLists.txt`
   - [ ] Add test executable to `test/unit/cognitive/{module}/CMakeLists.txt`

---

## CMakeLists.txt Updates Needed

### src/lib/CMakeLists.txt

Add to cognitive sources section:
```cmake
# FEP Bridges
src/cognitive/analysis/nimcp_analysis_fep_bridge.c
src/cognitive/epistemic/nimcp_epistemic_fep_bridge.c
src/cognitive/explanations/nimcp_explanations_fep_bridge.c
src/cognitive/logic/nimcp_logic_fep_bridge.c
src/cognitive/personality/nimcp_personality_fep_bridge.c
src/cognitive/self_awareness/nimcp_self_awareness_fep_bridge.c
src/cognitive/shadow/nimcp_shadow_fep_bridge.c
src/cognitive/social/nimcp_social_fep_bridge.c
```

### test/unit/cognitive/{module}/CMakeLists.txt

Create or update for each module:
```cmake
add_executable(unit_cognitive_{module}_fep_bridge
    test_{module}_fep_bridge.cpp
)

target_link_libraries(unit_cognitive_{module}_fep_bridge
    nimcp
    GTest::gtest_main
)

gtest_discover_tests(unit_cognitive_{module}_fep_bridge)
```

---

## Testing Strategy

### Unit Tests (10+ tests per bridge)
- Config initialization
- Lifecycle (create/destroy)
- Connection management
- Bidirectional effects
- State queries
- Statistics
- Bio-async integration
- Null pointer handling

### Integration Tests (Future)
- FEP system + Module integration
- Bidirectional message flow
- Performance under load
- Multi-module interactions

### Regression Tests (Future)
- Stability over time
- Memory leak detection
- Thread safety

---

## Build Commands

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make nimcp -j4

# Test individual bridges
make unit_cognitive_analysis_fep_bridge -j4 && ./test/unit/cognitive/analysis/unit_cognitive_analysis_fep_bridge --gtest_brief=1
make unit_cognitive_epistemic_fep_bridge -j4 && ./test/unit/cognitive/epistemic/unit_cognitive_epistemic_fep_bridge --gtest_brief=1
# ... repeat for each module
```

---

## References

### FEP Theory
- Friston, K. (2010) "The free-energy principle: a unified brain theory?"
- Friston et al. (2017) "Active inference: A process theory"
- Parr, Pezzulo, Friston (2022) "Active Inference: The Free Energy Principle"

### Module-Specific
- **Analysis:** Bullmore & Sporns (2012) "Economy of brain network organization"
- **Epistemic:** Friston et al. (2017) "Active inference and epistemic value"
- **Explanations:** Thagard's coherence theory
- **Logic:** Peirce's abduction, generalized filtering
- **Personality:** Big Five correlates with brain structure
- **Self-Awareness:** Friston (2018) "Am I self-conscious?"
- **Shadow:** Jung's shadow integration
- **Social:** Friston & Frith (2015) "Active inference, communication and hermeneutics"

---

## Next Steps

1. **Complete Remaining 7 Modules:**
   - Copy analysis pattern
   - Adapt biological basis and function names
   - Implement module-specific logic

2. **Add to Build System:**
   - Update CMakeLists.txt files
   - Verify compilation
   - Run all tests

3. **Integration Testing:**
   - Connect bridges to actual FEP systems
   - Test bidirectional flow
   - Performance profiling

4. **Documentation:**
   - Add to CLAUDE.md
   - Create usage examples
   - Document common patterns

---

## Estimated Remaining Work

- **Epistemic:** 2 files (implementation + tests)
- **Explanations:** 3 files (header + implementation + tests)
- **Logic:** 3 files
- **Personality:** 3 files
- **Self-Awareness:** 3 files
- **Shadow:** 3 files
- **Social:** 3 files
- **CMakeLists:** 8 updates

**Total:** 20 files + 8 CMakeLists updates

**Time Estimate:** 4-6 hours for experienced developer following patterns

---

## Status Summary

✅ **Analysis FEP Bridge:** Complete (100%)
🔄 **Epistemic FEP Bridge:** Header created (33%)
⏸️ **Remaining 6 Bridges:** Specifications ready (0%)

**Overall Progress:** 1/8 modules complete = 12.5%

All architectural patterns established. Remaining work is systematic application of the template to each module's specific biological basis and API.
