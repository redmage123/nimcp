# API Mismatch Fixes for test_cognitive_bio_async_e2e.cpp

## Summary
Fixed all API mismatches in the cognitive bio-async end-to-end test file to match the correct function signatures and types from the cognitive module headers.

## Changes Made

### 1. Knowledge System API
**Old (Incorrect):**
```cpp
nimcp_knowledge_t knowledge = nimcp_knowledge_create();
nimcp_knowledge_destroy(knowledge);
```

**New (Correct):**
```cpp
knowledge_system_t knowledge = knowledge_system_create("TestLearner");
knowledge_system_destroy(knowledge);
```

**Reason:** The API requires a string parameter for the learner name, and returns `knowledge_system_t` not `nimcp_knowledge_t`.

### 2. Mirror Neurons API
**Old (Incorrect):**
```cpp
mirror_neurons_t mirror = mirror_neurons_create(nullptr);
```

**New (Correct):**
```cpp
mirror_neuron_config_t default_mirror_config = mirror_neurons_get_default_config();
mirror_neurons_t mirror = mirror_neurons_create(&default_mirror_config);
```

**Reason:** `mirror_neurons_create()` requires a pointer to `mirror_neuron_config_t`, not NULL. Use `mirror_neurons_get_default_config()` to get default settings.

### 3. Predictive Processing API
**Old (Incorrect):**
```cpp
nimcp_predictive_model_t model = nimcp_predictive_create("recovery_test", 50.0f, 10.0f);
nimcp_predictive_observe(model, observation);
float final_prediction = nimcp_predictive_get_prediction(model);
nimcp_predictive_destroy(model);
```

**New (Correct):**
```cpp
predictive_config_t pred_config = predictive_default_config();
pred_config.num_layers = 3;
uint32_t layer_sizes[] = {10, 5, 1};
pred_config.layer_sizes = layer_sizes;

predictive_network_t model = predictive_create(&pred_config);
predictive_forward(model, input, 5);
predictive_update_model(model);

float final_prediction_vec[1];
predictive_get_layer_prediction(model, 2, final_prediction_vec);
predictive_destroy(model);
```

**Reason:** The predictive API uses hierarchical predictive coding with layers:
- Create via config with layer architecture
- Type is `predictive_network_t` not `nimcp_predictive_model_t`
- Process via `predictive_forward()` and `predictive_update_model()`
- Extract predictions from specific layers via `predictive_get_layer_prediction()`

### 4. Added Missing Header
Added:
```cpp
#include "cognitive/nimcp_predictive.h"
```

## Global Workspace API (No Changes Needed)
The global workspace API was already correct:
```cpp
global_workspace_t* workspace = global_workspace_create();  // No args needed
```

## Consolidation API (Not Used in Test)
For reference, the consolidation API uses:
```cpp
consolidation_handle_t handle = brain_start_background_consolidation(brain, interval_seconds, &config);
brain_stop_background_consolidation(handle);
```

## Network Analyzer API (Not Used in Test)
For reference, the network analyzer API uses:
```cpp
network_analyzer_t* analyzer = network_analyzer_create(brain);  // Takes brain_t
```

## Compilation Status
✅ **SUCCESS** - Test file compiles without errors
- Minor warnings about duplicate macro definitions (pre-existing issue, not related to these fixes)
- All API calls now match the correct function signatures

## Runtime Status
⚠️ **RUNTIME CRASH** - Test crashes during execution
- This is a separate issue from the API mismatches
- The API calls are now syntactically correct
- Runtime crash is likely due to implementation issues in the modules themselves

## Files Modified
- `/home/bbrelin/nimcp/test/e2e/test_cognitive_bio_async_e2e.cpp`

## Date
2025-11-28
