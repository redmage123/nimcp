# Brain Modules Quick Reference

Quick guide for finding brain-related functions after Phase 12.8 refactoring.

## Module Index

| Module | Purpose | Line Count | Key Operations |
|--------|---------|------------|----------------|
| **nimcp_brain_core** | Allocation & Init | ~2200 | Create, initialize, destroy brain |
| **nimcp_brain_processing** | Inference | ~1400 | Forward pass, decisions |
| **nimcp_brain_memory** | Memory I/O | ~300 | Save/load working memory |
| **nimcp_brain_state** | State Access | ~1200 | Get network, systems; COW |
| **nimcp_brain_io** | Persistence | ~900 | JSON, metadata, snapshots |
| **nimcp_brain_multimodal** | Multimodal | ~1500 | Visual, audio, speech processing |

## Function Finder

### Brain Lifecycle

| Function | Module | Description |
|----------|--------|-------------|
| `allocate_brain()` | core | Allocate brain structure |
| `create_brain_network()` | core | Create adaptive network |
| `brain_destroy()` | core | Cleanup and free brain |

### Initialization

| Function | Module | Description |
|----------|--------|-------------|
| `init_attention_subsystem()` | core | Initialize multihead attention |
| `init_brain_regions_subsystem()` | core | Initialize cortical regions |
| `init_symbolic_logic_subsystem()` | core | Initialize neural logic |
| `init_epistemic_subsystem()` | core | Initialize bias prevention |

### State Access

| Function | Module | Description |
|----------|--------|-------------|
| `brain_get_network()` | state | Get adaptive network |
| `brain_get_neuromodulator_system()` | state | Get neuromodulator system |
| `brain_get_sleep_system()` | state | Get sleep/wake system |
| `brain_get_theory_of_mind()` | state | Get theory of mind |
| `ensure_writable_network()` | state | Trigger COW if needed |

### Processing

| Function | Module | Description |
|----------|--------|-------------|
| `perform_forward_pass()` | processing | Neural network forward pass |
| `brain_decide()` | processing | Main inference function |
| `brain_decide_batch()` | processing | Batch inference |

### Multimodal

| Function | Module | Description |
|----------|--------|-------------|
| `brain_process_multimodal()` | multimodal | Complete multimodal pipeline |
| `apply_attention_to_features()` | multimodal | Attention-weighted features |
| `process_brain_regions()` | multimodal | Hierarchical region processing |

### Persistence

| Function | Module | Description |
|----------|--------|-------------|
| `save_metadata()` | io | Save brain config and labels |
| `load_metadata()` | io | Load brain config and labels |
| `brain_save_json()` | io | Save brain to JSON file |
| `brain_load_json()` | io | Load brain from JSON file |
| `brain_export_json()` | io | Export brain to JSON string |
| `brain_import_json()` | io | Import brain from JSON string |
| `brain_get_memory_usage()` | io | Calculate memory footprint |

### Memory

| Function | Module | Description |
|----------|--------|-------------|
| `save_working_memory_state()` | memory | Serialize working memory |
| `load_working_memory_state()` | memory | Deserialize working memory |

## Include Paths

### Using Specific Modules

```c
// For brain allocation and initialization
#include "core/brain/nimcp_brain_core.h"

// For inference and processing
#include "core/brain/nimcp_brain_processing.h"

// For working memory persistence
#include "core/brain/nimcp_brain_memory.h"

// For state accessors
#include "core/brain/nimcp_brain_state.h"

// For I/O and persistence
#include "core/brain/nimcp_brain_io.h"

// For multimodal processing
#include "core/brain/nimcp_brain_multimodal.h"
```

### Using Main Header (Includes All)

```c
// Traditional approach - includes everything
#include "core/brain/nimcp_brain.h"
```

## Typical Usage Patterns

### Creating a Brain

```c
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_core.h"

// Use factory function from factory module (preferred)
brain_t brain = brain_create("MyBrain", BRAIN_SIZE_MEDIUM,
                             TASK_CLASSIFICATION, 784, 10);

// Or manual allocation (advanced)
brain_t brain = allocate_brain();
brain->network = create_brain_network(784, 10, 5000, 0.1, ODE_EULER);
init_attention_subsystem(brain);
init_brain_regions_subsystem(brain);
```

### Running Inference

```c
#include "core/brain/nimcp_brain_processing.h"

// Single inference
brain_decision_t* decision = brain_decide(brain, features, num_features);
printf("Decision: %s (%.2f%% confidence)\n",
       decision->label, decision->confidence * 100);
brain_free_decision(decision);

// Batch inference
brain_decision_t* decisions = calloc(batch_size, sizeof(brain_decision_t));
brain_decide_batch(brain, inputs, batch_size, features_per_input, decisions);
```

### Saving and Loading

```c
#include "core/brain/nimcp_brain_io.h"

// Save brain to JSON
brain_save_json(brain, "my_brain.json", SERIALIZE_INCLUDE_METADATA);

// Load brain from JSON
brain_t loaded_brain = brain_load_json("my_brain.json");

// Get memory usage
size_t usage = brain_get_memory_usage(brain);
printf("Brain using %zu bytes\n", usage);
```

### Multimodal Processing

```c
#include "core/brain/nimcp_brain_multimodal.h"

brain_multimodal_input_t input = {
    .visual_data = image_pixels,
    .visual_width = 640,
    .visual_height = 480,
    .audio_data = audio_samples,
    .audio_samples = 16000,
    .direct_data = sensor_features,
    .direct_dim = 128
};

brain_multimodal_output_t output;
brain_process_multimodal(brain, &input, &output);
printf("Decision: %s\n", output.decision_label);
printf("Confidence: %.2f%%\n", output.confidence * 100);
```

### State Access

```c
#include "core/brain/nimcp_brain_state.h"

// Get subsystems
adaptive_network_t network = brain_get_network(brain);
neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
theory_of_mind_t tom = brain_get_theory_of_mind(brain);

// Ensure writable before modification
if (!ensure_writable_network(brain)) {
    fprintf(stderr, "Failed to make network writable\n");
    return;
}
```

## Migration Notes

**Current Status**: All functions are still implemented in `nimcp_brain.c`. The module files use `extern` declarations to link to the original implementations.

**Future**: Functions will be incrementally moved to their respective modules.

**Compatibility**: Code using `#include "core/brain/nimcp_brain.h"` will continue to work unchanged.

## Module Dependencies

```
nimcp_brain_core
  ├── Depends on: neuralnet, plasticity, glial, cognitive
  └── Used by: factory, brain creation

nimcp_brain_processing
  ├── Depends on: neuralnet, adaptive, core
  └── Used by: inference, decisions

nimcp_brain_memory
  ├── Depends on: working_memory, core
  └── Used by: persistence, snapshots

nimcp_brain_state
  ├── Depends on: neuromodulators, sleep, theory_of_mind, core
  └── Used by: accessors, COW operations

nimcp_brain_io
  ├── Depends on: cJSON, serialization, core
  └── Used by: save/load, snapshots

nimcp_brain_multimodal
  ├── Depends on: perception, attention, cognitive, core
  └── Used by: multimodal inference
```

## See Also

- [Full Refactoring Documentation](BRAIN_REFACTORING_PHASE_12_8.md)
- [Brain Factory Module](../include/core/brain/factory/nimcp_brain_factory.h)
- [Brain Internal Structures](../include/core/brain/nimcp_brain_internal.h)
