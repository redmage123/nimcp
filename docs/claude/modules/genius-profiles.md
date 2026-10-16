# Genius Profiles Module

**Version**: 1.0.0
**Location**: `include/core/brain/genius/`, `src/core/brain/genius/`

---

## Overview

The Genius Profiles system creates specialized brain configurations modeling different types of cognitive excellence based on neuroscience research. It leverages NIMCP's lateralization system, brain region adapters, and hemispheric architecture to create brains with domain-specific cognitive enhancements.

---

## Genius Types

| Type | Enum | Exemplars | Key Features |
|------|------|-----------|--------------|
| **Mathematical** | `GENIUS_TYPE_MATHEMATICAL` | Gauss, Newton, Ramanujan | Enhanced parietal (2x), strong prefrontal-parietal connectivity |
| **Visual/Artistic** | `GENIUS_TYPE_VISUAL_ARTISTIC` | Rembrandt, Van Gogh, Da Vinci | Enhanced V4/V8 color, right hemisphere dominance |
| **Musical** | `GENIUS_TYPE_MUSICAL` | Mozart, Beethoven, Bach | Enlarged planum temporale, enhanced cerebellum timing |
| **Literary** | `GENIUS_TYPE_LITERARY` | Shakespeare, Tolstoy | Enhanced Broca's/Wernicke's, strong semantic networks |
| **Scientific** | `GENIUS_TYPE_SCIENTIFIC` | Tesla, Darwin, Curie | Eidetic visual cortex, cross-domain association |
| **Athletic** | `GENIUS_TYPE_ATHLETIC` | Jordan, Gretzky | Enhanced motor cortex, superior cerebellum |
| **Strategic** | `GENIUS_TYPE_STRATEGIC` | Napoleon, Churchill | Enhanced social cognition, pattern recognition |
| **Financial** | `GENIUS_TYPE_FINANCIAL` | Buffett, Soros, Simons | Risk assessment, temporal discounting |
| **Polymath** | `GENIUS_TYPE_POLYMATH` | Da Vinci, Leibniz | Combines multiple profiles with weighted blending |

---

## Quick Start

### Create a Genius Brain

```c
#include "core/brain/genius/nimcp_genius_profiles.h"

// Create a mathematical genius brain
brain_t math_brain = genius_brain_create(GENIUS_TYPE_MATHEMATICAL);

// Create with hemispheric asymmetry
hemispheric_brain_t* artist = genius_hemispheric_brain_create(GENIUS_TYPE_VISUAL_ARTISTIC);
```

### Using the Bridge Pattern

```c
// Create bridge with configuration
genius_profiles_config_t config;
genius_profiles_config_default(&config);
config.enable_bio_async = true;
config.enable_eidetic = true;

genius_profiles_bridge_t* bridge = genius_profiles_bridge_create(&config);

// Activate a profile
genius_profiles_activate(bridge, GENIUS_TYPE_SCIENTIFIC, 1.0f);

// Enter flow state
genius_profiles_enter_flow(bridge, 0.8f, 0.9f);  // challenge, skill

// Clean up
genius_profiles_bridge_destroy(bridge);
```

### Create a Polymath

```c
// Da Vinci-style: 60% artistic, 40% scientific
genius_profiles_bridge_t* bridge = genius_profiles_bridge_create(NULL);
genius_profiles_create_polymath(bridge,
    GENIUS_TYPE_VISUAL_ARTISTIC,  // primary
    GENIUS_TYPE_SCIENTIFIC,       // secondary
    0.4f);                        // blend factor
```

---

## API Reference

### Profile Retrieval

```c
// Get static profile definition
const genius_profile_t* genius_profile_get(genius_type_t type);

// Get type name/description
const char* genius_type_name(genius_type_t type);
```

### Brain Creation

```c
// Create brain with genius profile
nimcp_brain_t* genius_brain_create(genius_type_t type);

// Create hemispheric brain with lateralization
hemispheric_brain_t* genius_hemispheric_brain_create(genius_type_t type);
```

### Bridge Lifecycle

```c
// Configuration
genius_error_t genius_profiles_config_default(genius_profiles_config_t* config);

// Create/destroy
genius_profiles_bridge_t* genius_profiles_bridge_create(const genius_profiles_config_t* config);
void genius_profiles_bridge_destroy(genius_profiles_bridge_t* bridge);

// Reset state
genius_error_t genius_profiles_bridge_reset(genius_profiles_bridge_t* bridge);
```

### Profile Activation

```c
// Activate single profile
genius_error_t genius_profiles_activate(
    genius_profiles_bridge_t* bridge,
    genius_type_t type,
    float strength);  // 0.0-1.0

// Deactivate
genius_error_t genius_profiles_deactivate(genius_profiles_bridge_t* bridge);

// Blend multiple profiles
genius_error_t genius_profiles_blend(
    genius_profiles_bridge_t* bridge,
    const genius_type_t* types,
    const float* weights,
    uint32_t count);

// Create polymath combination
genius_error_t genius_profiles_create_polymath(
    genius_profiles_bridge_t* bridge,
    genius_type_t primary,
    genius_type_t secondary,
    float blend_factor);  // 0.0-0.5
```

### Flow State

```c
// Enter flow state (requires ACTIVE or BLENDED state)
genius_error_t genius_profiles_enter_flow(
    genius_profiles_bridge_t* bridge,
    float challenge_level,  // 0.0-1.0
    float skill_level);     // 0.0-1.0

// Exit flow state
genius_error_t genius_profiles_exit_flow(genius_profiles_bridge_t* bridge);
```

### System Integration

```c
// Connect to brain region adapters
genius_error_t genius_profiles_connect_regions(
    genius_profiles_bridge_t* bridge,
    parietal_adapter_t* parietal,
    occipital_adapter_t* occipital,
    temporal_adapter_t* temporal,
    prefrontal_adapter_t* prefrontal,
    cerebellum_adapter_t* cerebellum,
    hippocampus_adapter_t* hippocampus,
    motor_adapter_t* motor);

// Connect to memory systems
genius_error_t genius_profiles_connect_memory_systems(
    genius_profiles_bridge_t* bridge,
    working_memory_t* working_memory,
    semantic_memory_system_t* semantic_memory,
    engram_system_t* engram_system,
    systems_consolidation_system_t* consolidation);

// Bio-async messaging
genius_error_t genius_profiles_connect_bio_async(genius_profiles_bridge_t* bridge);
genius_error_t genius_profiles_disconnect_bio_async(genius_profiles_bridge_t* bridge);

// Knowledge graph wiring
genius_error_t genius_profiles_register_kg_wiring(genius_profiles_bridge_t* bridge);
```

---

## Eidetic Memory

Eidetic (photographic) memory is a cross-cutting enhancement applicable to any genius type.

### Eidetic Presets

| Preset | Function | Characteristics |
|--------|----------|-----------------|
| **Tesla** | `eidetic_config_tesla()` | Visual-spatial dominant, complete machine visualization |
| **Mozart** | `eidetic_config_mozart()` | Auditory dominant, replay entire symphonies |
| **von Neumann** | `eidetic_config_vonneumann()` | Numerical/verbal, instant calculation |
| **Kim Peek** | `eidetic_config_kim_peek()` | Encyclopedic factual recall |
| **Wiltshire** | `eidetic_config_wiltshire()` | Visual-artistic, cityscape from memory |

### Eidetic Configuration

```c
typedef struct {
    // Modality strengths (0.0 = normal, 3.0 = exceptional)
    float visual_eidetic;
    float auditory_eidetic;
    float spatial_eidetic;
    float verbal_eidetic;

    // Global characteristics
    float encoding_speed;       // How fast memories form
    float decay_resistance;     // Resistance to forgetting
    float retrieval_accuracy;   // Recall precision
    float detail_granularity;   // Fine-grained detail

    // Per-system configs
    eidetic_working_memory_config_t working_memory;
    eidetic_hippocampus_config_t hippocampus;
    eidetic_semantic_config_t semantic;
    // ... etc
} eidetic_memory_config_t;
```

### Apply Eidetic Enhancement

```c
// Get eidetic config from active profile
const eidetic_memory_config_t* config = genius_profiles_get_eidetic_config(bridge);

// Apply to bridge (uses profile's eidetic settings)
genius_profiles_apply_eidetic(bridge);
```

---

## Profile Structure

Each genius profile contains:

```c
typedef struct {
    genius_type_t type;
    const char* name;
    const char* description;
    const char* exemplars;

    // Cognitive traits
    genius_traits_t traits;

    // Brain region configurations
    genius_region_config_t parietal;
    genius_region_config_t occipital;
    genius_region_config_t temporal;
    genius_region_config_t prefrontal;
    genius_region_config_t cerebellum;
    genius_region_config_t hippocampus;
    genius_region_config_t motor;

    // Inter-region connectivity
    genius_connectivity_t connectivity;

    // Hemispheric lateralization
    lateralization_profile_t lateralization;

    // Eidetic memory settings
    eidetic_memory_config_t eidetic;
} genius_profile_t;
```

### Region Configuration

```c
typedef struct {
    float size_multiplier;              // Neuron count scale (0.5 - 3.0)
    float feature_capacity_multiplier;  // Feature detection capacity
    float processing_speed_multiplier;  // Speed improvement
    float precision_multiplier;         // Accuracy/resolution
    float learning_rate_multiplier;     // Plasticity rate
    float custom_params[8];             // Region-specific
    uint32_t enable_flags;              // Feature bitmask
} genius_region_config_t;
```

### Cognitive Traits

```c
typedef struct {
    uint32_t working_memory_capacity;   // 7 default, up to 12
    float sustained_attention_duration;
    float pattern_sensitivity;
    float abstraction_level;
    float divergent_thinking;
    float mental_imagery_vividness;
    float flow_state_threshold;
    // ... many more
} genius_traits_t;
```

---

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| `0x1F00` | `GENIUS_ERROR_SUCCESS` | Operation succeeded |
| `0x1F01` | `GENIUS_ERROR_NULL_POINTER` | NULL argument passed |
| `0x1F02` | `GENIUS_ERROR_INVALID_TYPE` | Invalid genius type |
| `0x1F03` | `GENIUS_ERROR_INVALID_STATE` | Wrong activation state |
| `0x1F04` | `GENIUS_ERROR_ALREADY_ACTIVE` | Profile already active |
| `0x1F05` | `GENIUS_ERROR_NOT_ACTIVE` | No profile active |
| `0x1F06` | `GENIUS_ERROR_BLEND_FAILED` | Blend operation failed |
| `0x1F07` | `GENIUS_ERROR_CONNECTION_FAILED` | System connection failed |
| `0x1F08` | `GENIUS_ERROR_BIO_ASYNC_FAILED` | Bio-async registration failed |

---

## Activation States

```c
typedef enum {
    GENIUS_STATE_INACTIVE = 0,  // No profile active
    GENIUS_STATE_ACTIVE,        // Single profile active
    GENIUS_STATE_BLENDED,       // Multiple profiles blended
    GENIUS_STATE_FLOW,          // In flow state
    GENIUS_STATE_SUSPENDED      // Temporarily suspended
} genius_activation_state_t;
```

State transitions:
- `INACTIVE` -> `ACTIVE` (via `activate`)
- `ACTIVE` -> `BLENDED` (via `blend`)
- `ACTIVE`/`BLENDED` -> `FLOW` (via `enter_flow`)
- `FLOW` -> `ACTIVE`/`BLENDED` (via `exit_flow`)
- Any -> `INACTIVE` (via `deactivate`)

---

## Integration Examples

### With Brain Immune System

```c
genius_profiles_connect_immune(bridge, brain->immune_system);
genius_profiles_apply_immune_modulation(bridge, fatigue_level);
```

### With Training System

```c
genius_profiles_connect_training(bridge, training_ctx);
genius_profiles_training_step(bridge, input, target, learning_rate);
```

### With Mesh Network

```c
genius_profiles_connect_mesh(bridge, mesh_bootstrap);
genius_profiles_mesh_propose(bridge, proposal_data, size);
```

---

## Files

```
include/core/brain/genius/
├── nimcp_genius_profiles.h    # Main API
├── nimcp_genius_types.h       # Type definitions and error codes
├── nimcp_genius_traits.h      # Trait and eidetic structures
└── eidetic/
    └── nimcp_eidetic_memory.h # Eidetic integration API

src/core/brain/genius/
├── nimcp_genius_profiles.c    # Full implementation
├── CMakeLists.txt
└── eidetic/
    └── nimcp_eidetic_memory.c # Eidetic apply functions
```

---

## See Also

- [Hemispheric Brain](hemispheric-brain.md) - Lateralization system
- [Brain Regions Roadmap](brain-regions-roadmap.md) - Region adapters
- [LNN Module](lnn.md) - Liquid neural networks for genius profiles
