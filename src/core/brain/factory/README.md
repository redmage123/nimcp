# NIMCP Brain Factory Module

## Overview

The Brain Factory module provides a clean, modular interface for creating and configuring NIMCP brain instances. Extracted from `nimcp_brain.c` to improve code organization and maintainability.

## Quick Start

```c
#include "core/brain/factory/nimcp_brain_factory.h"

// Create a brain configuration
brain_config_t config;
nimcp_brain_factory_init_brain_config(&config, "my_brain", 
                                       BRAIN_SIZE_MEDIUM,
                                       BRAIN_TASK_CLASSIFICATION,
                                       784,  // inputs (e.g., 28x28 image)
                                       10,   // outputs (e.g., 10 classes)
                                       strategy);

// Allocate brain structure
brain_t brain = nimcp_brain_factory_allocate_brain();

// Create neural network
brain->network = nimcp_brain_factory_create_brain_network(
    config.num_inputs,
    config.num_outputs, 
    nimcp_brain_factory_get_neuron_count(config.size),
    config.sparsity_target,
    config.neuron_integration
);

// Initialize subsystems
nimcp_brain_factory_init_glial_subsystem(brain);
nimcp_brain_factory_init_multimodal_subsystems(brain);
nimcp_brain_factory_init_working_memory_subsystem(brain);
// ... more subsystems as needed
```

## Module Structure

```
src/core/brain/factory/
├── nimcp_brain_factory.h        # Public API (714 lines)
├── nimcp_brain_factory.c        # Implementation (3,219 lines)
├── EXTRACTION_REPORT.md         # Detailed extraction report
└── README.md                    # This file
```

## Function Categories

### 1. Configuration Builders (7 functions)
Build brain configurations with sensible defaults:
- `nimcp_brain_factory_get_neuron_count()` - Map size to neuron count
- `nimcp_brain_factory_get_default_sparsity()` - Get sparsity for size
- `nimcp_brain_factory_build_spike_params()` - Create spike config
- `nimcp_brain_factory_build_base_network_config()` - Base network setup
- `nimcp_brain_factory_build_network_config()` - Full adaptive config
- `nimcp_brain_factory_init_brain_config()` - Initialize brain config
- `nimcp_brain_factory_init_brain_stats()` - Initialize statistics

### 2. Brain Factory (4 functions)
Core creation and validation:
- `nimcp_brain_factory_validate_creation_params()` - Validate inputs
- `nimcp_brain_factory_allocate_brain()` - Allocate brain structure
- `nimcp_brain_factory_create_brain_network()` - Create network
- `nimcp_brain_factory_init_output_labels()` - Setup output labels

### 3. Decision Caching (3 functions)
Performance optimization through caching:
- `nimcp_brain_factory_is_cached_input()` - Check cache
- `nimcp_brain_factory_cache_decision()` - Store decision
- `nimcp_brain_factory_clear_cache()` - Invalidate cache

### 4. Subsystem Initializers (28 functions)
Initialize cognitive and biological subsystems:

**Biological Realism:**
- `init_glial_subsystem()` - Astrocyte modulation
- `init_pink_noise_subsystem()` - 1/f noise in neuromodulation
- `init_neuromodulator_system()` - Dopamine, serotonin, etc.
- `init_spatial_neuromod_system()` - Volume transmission

**Perception:**
- `init_multimodal_subsystems()` - Visual, audio, speech cortices

**Attention & Focus:**
- `init_attention_subsystem()` - Multihead attention
- `init_salience_subsystem()` - Bottom-up attention

**Memory Systems:**
- `init_working_memory_subsystem()` - Miller's 7±2
- `init_consolidation_subsystem()` - Sleep-dependent consolidation
- `init_autobiographical_memory_subsystem()` - Episodic self-memory

**Reasoning & Logic:**
- `init_symbolic_logic_subsystem()` - Propositional logic
- `init_symbolic_reasoning_subsystem()` - Knowledge graphs
- `init_epistemic_subsystem()` - Bias detection

**Executive Functions:**
- `init_executive_subsystem()` - Planning, task switching
- `init_predictive_subsystem()` - Free energy minimization
- `init_meta_learning_subsystem()` - Learning-to-learn

**Social Cognition:**
- `init_theory_of_mind_subsystem()` - Mental state inference
- `init_mirror_neurons()` - Observational learning
- `init_empathy_network_subsystem()` - Emotional resonance
- `init_empathetic_response_subsystem()` - Active empathy

**Self & Identity:**
- `init_self_model_subsystem()` - Identity representation
- `init_introspection_subsystem()` - Metacognition
- `init_mental_health_subsystem()` - Wellbeing monitoring

**Values & Ethics:**
- `init_ethics_engine_subsystem()` - Moral reasoning
- `init_natural_explanations_subsystem()` - Interpretability

**Motivation:**
- `init_curiosity_subsystem()` - Intrinsic motivation

**Integration:**
- `init_brain_regions_subsystem()` - Cortical organization
- `init_global_workspace_subsystem()` - Conscious access

## Design Patterns

### Factory Pattern
Encapsulates brain creation logic:
```c
brain_t brain = nimcp_brain_factory_allocate_brain();
// vs manually allocating and initializing all fields
```

### Builder Pattern
Step-by-step configuration:
```c
network_config_t base = nimcp_brain_factory_build_base_network_config(...);
adaptive_spike_params_t spike = nimcp_brain_factory_build_spike_params(...);
adaptive_network_config_t full = nimcp_brain_factory_build_network_config(...);
```

### Template Method
Initialization follows standard pattern:
```c
if (!brain->config.enable_<subsystem>) return true;  // Disabled
if (brain-><subsystem>) return true;                  // Already initialized
brain-><subsystem> = <subsystem>_create(&config);    // Create
if (!brain-><subsystem>) return false;                // Handle error
return true;                                          // Success
```

## Error Handling

All functions follow consistent error handling:

```c
// Validation functions return bool
if (!nimcp_brain_factory_validate_creation_params(name, in, out)) {
    // Error message already set via set_error()
    return NULL;
}

// Allocation functions return NULL on error
brain_t brain = nimcp_brain_factory_allocate_brain();
if (!brain) {
    // Error message already set
    return NULL;
}

// Init functions return bool
if (!nimcp_brain_factory_init_working_memory_subsystem(brain)) {
    // Cleanup and propagate error
    brain_destroy(brain);
    return NULL;
}
```

## Thread Safety

Functions are thread-safe where documented:
- `clear_cache()` - Uses mutex protection
- `cache_decision()` - Caller must hold mutex
- `allocate_brain()` - Initializes cache mutex

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Configuration | O(1) | Constant time |
| Validation | O(1) | Simple range checks |
| Allocation | O(1) | Single struct malloc |
| Network creation | O(n) | n = num_neurons |
| Subsystem init | Varies | See subsystem docs |

## Memory Management

All functions use NIMCP memory allocators:
- `nimcp_calloc()` - Zero-initialized allocation
- `nimcp_malloc()` - Standard allocation
- `nimcp_free()` - Deallocation

Callers are responsible for:
- Freeing returned brain structures via `brain_destroy()`
- Freeing layer_sizes from `build_base_network_config()`

## Biological Realism

Many functions incorporate biological principles:

**Neuron counts** mimic real brain scales:
- TINY: 100 (C. elegans)
- SMALL: 500 (simple circuits)
- MEDIUM: 1,000 (cortical column)
- LARGE: 5,000 (multi-column)

**Sparsity** reflects cortical activity:
- 70-90% inactive neurons (1-4% active firing)

**Subsystems** model brain regions:
- Visual cortex: V1 orientation filters
- Audio cortex: Tonotopic organization
- Working memory: Prefrontal-like buffer
- Neuromodulators: Volume transmission

## Testing

Example unit test:
```c
void test_brain_factory_creation(void) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    assert(brain != NULL);
    assert(brain->owns_network == true);
    assert(brain->is_cow_clone == false);
    brain_destroy(brain);
}

void test_neuron_count_mapping(void) {
    assert(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY) == 100);
    assert(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL) == 500);
    assert(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM) == 1000);
}
```

## Migration Guide

### Before (nimcp_brain.c)
```c
// Static functions, internal use only
static uint32_t get_neuron_count(brain_size_t size);
static brain_t allocate_brain(void);
```

### After (nimcp_brain_factory.h)
```c
// Public API, reusable
uint32_t nimcp_brain_factory_get_neuron_count(brain_size_t size);
brain_t nimcp_brain_factory_allocate_brain(void);
```

### Update Existing Code
```bash
# Replace function calls in nimcp_brain.c
sed -i 's/get_neuron_count(/nimcp_brain_factory_get_neuron_count(/g' src/core/brain/nimcp_brain.c
sed -i 's/allocate_brain(/nimcp_brain_factory_allocate_brain(/g' src/core/brain/nimcp_brain.c
# ... repeat for all 42 functions
```

## Integration Checklist

- [ ] Add `nimcp_brain_factory.c` to CMakeLists.txt
- [ ] Include `nimcp_brain_factory.h` in `nimcp_brain.c`
- [ ] Replace all static function calls with factory calls
- [ ] Update external function declarations
- [ ] Compile and verify no missing symbols
- [ ] Run full test suite
- [ ] Update documentation references

## References

- **Main Brain API:** `src/core/brain/nimcp_brain.h`
- **Extraction Report:** `EXTRACTION_REPORT.md`
- **Original Code:** `nimcp_brain.c` lines 723-3800
- **NIMCP Docs:** `docs/api/brain_factory.md` (if available)

## License

Same as NIMCP project (see root LICENSE file)

## Maintainers

NIMCP Development Team

## Version

1.0.0 (2025-11-19) - Initial extraction from nimcp_brain.c
