# Brain Function Mapping - Refactoring Reference

**Date:** 2025-12-08
**Purpose:** Document which functions moved to which modules during refactoring

---

## Lifecycle Functions (`nimcp_brain_lifecycle.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `allocate_brain()` | 1037-1086 | nimcp_brain_lifecycle.c | Allocate brain structure |
| `brain_destroy()` | 1654-2223 | nimcp_brain_lifecycle.c | Free all resources |
| `init_brain_config()` | 743-818 | nimcp_brain_lifecycle.c | Initialize configuration |
| `init_brain_stats()` | 834-846 | nimcp_brain_lifecycle.c | Initialize statistics |
| `init_output_labels()` | 1140-1159 | nimcp_brain_lifecycle.c | Initialize labels array |
| `init_attention_subsystem()` | 1219-1309 | nimcp_brain_lifecycle.c | Initialize attention |
| `init_brain_regions_subsystem()` | 1310-1430 | nimcp_brain_lifecycle.c | Initialize brain regions |
| `init_symbolic_logic_subsystem()` | 1431-1472 | nimcp_brain_lifecycle.c | Initialize symbolic logic |
| `init_symbolic_reasoning_subsystem()` | 1473-1520 | nimcp_brain_lifecycle.c | Initialize reasoning |
| `init_epistemic_subsystem()` | 1521-1551 | nimcp_brain_lifecycle.c | Initialize epistemic filter |
| `create_personality()` | 1566-1609 | nimcp_brain_lifecycle.c | Generate personality |
| `create_brain_network()` | 1103-1129 | nimcp_brain_lifecycle.c | Create adaptive network |
| `get_neuron_count()` | 569-585 | nimcp_brain_lifecycle.c | Size preset mapping |
| `get_default_sparsity()` | 598-612 | nimcp_brain_lifecycle.c | Sparsity defaults |
| `build_spike_params()` | 629-643 | nimcp_brain_lifecycle.c | Build spike config |
| `build_base_network_config()` | 658-694 | nimcp_brain_lifecycle.c | Build network config |
| `build_network_config()` | 710-725 | nimcp_brain_lifecycle.c | Build adaptive config |
| `validate_creation_params()` | 996-1025 | nimcp_brain_lifecycle.c | Validate parameters |

---

## Factory Functions (`factory/nimcp_brain_factory.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_create()` | ~1629 | factory/nimcp_brain_factory.c | Create brain with preset |
| `brain_create_custom()` | ~1642 | factory/nimcp_brain_factory.c | Create with custom config |

---

## Processing Functions (`nimcp_brain_processing.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_decide()` | 3028-3750 | nimcp_brain_processing.c | Main decision function |
| `allocate_decision()` | 2678-2697 | nimcp_brain_processing.c | Allocate decision struct |
| `copy_decision()` | 2715-2762 | nimcp_brain_processing.c | CoW decision copy |
| `copy_decision_deep()` | 2776-2819 | nimcp_brain_processing.c | Deep decision copy |
| `perform_forward_pass()` | 2832-2853 | nimcp_brain_processing.c | Forward propagation |
| `determine_output_label()` | 2860-2881 | nimcp_brain_processing.c | Find max output |
| `populate_interpretability()` | 2888-2913 | nimcp_brain_processing.c | Add explanations |
| `update_inference_stats()` | 2915-2946 | nimcp_brain_processing.c | Update statistics |
| `brain_decision_to_action()` | 2948-2987 | nimcp_brain_processing.c | Convert to action |
| `features_to_action()` | 2988-3011 | nimcp_brain_processing.c | Features to action |

---

## Multi-Modal Functions (`nimcp_brain_multimodal.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_process_multimodal()` | 6201-6786 | nimcp_brain_multimodal.c | Multi-sensory integration |
| `extract_sensory_features()` | 5320-5411 | nimcp_brain_multimodal.c | Extract features |
| `apply_attention_to_features()` | 5412-5479 | nimcp_brain_multimodal.c | Apply attention |
| `process_brain_regions()` | 5480-5590 | nimcp_brain_multimodal.c | Regional processing |
| `integrate_multimodal_features()` | 5591-5687 | nimcp_brain_multimodal.c | Cross-modal integration |
| `process_neural_network()` | 5688-5748 | nimcp_brain_multimodal.c | Network processing |
| `apply_cognitive_processing()` | 5749-5980 | nimcp_brain_multimodal.c | Cognitive assessments |
| `consolidation_strengthen()` | 5981-6065 | nimcp_brain_multimodal.c | Memory consolidation |
| `format_output()` | 6066-6200 | nimcp_brain_multimodal.c | Format output |

---

## Learning Functions (`learning/nimcp_brain_learning.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_learn_example()` | ~2611 | learning/nimcp_brain_learning.c | Learn from example |
| `brain_learn_batch()` | ~2626 | learning/nimcp_brain_learning.c | Batch learning |
| `brain_apply_reward_learning()` | ~2647 | learning/nimcp_brain_learning.c | RL learning |
| `brain_learn_from_llm()` | ~2665 | learning/nimcp_brain_learning.c | LLM distillation |
| `get_or_create_label_index()` | 2453-2475 | learning/nimcp_brain_learning.c | Label indexing |
| `label_to_output()` | 2490-2496 | learning/nimcp_brain_learning.c | Label encoding |
| `adapt_learning_rate_from_loss()` | 2509-2569 | learning/nimcp_brain_learning.c | Adaptive LR |
| `quantum_weight_energy()` | 2585-2593 | learning/nimcp_brain_learning.c | Quantum energy |

---

## State & Accessor Functions (`nimcp_brain_state.c`, `accessors/`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_get_network()` | 520-535 | accessors/nimcp_brain_accessors.c | Get network |
| `brain_get_neuromodulator_system()` | 544-552 | accessors/nimcp_brain_accessors.c | Get neuromodulators |
| `brain_get_working_memory()` | 2235-2241 | accessors/nimcp_brain_accessors.c | Get working memory |
| `brain_get_global_workspace()` | 2255-2261 | accessors/nimcp_brain_accessors.c | Get workspace |
| `brain_get_sleep_system()` | 2276-2284 | accessors/nimcp_brain_accessors.c | Get sleep system |
| `brain_get_theory_of_mind()` | 2299-2307 | accessors/nimcp_brain_accessors.c | Get ToM |
| `brain_get_explanation_generator()` | 2322-2330 | accessors/nimcp_brain_accessors.c | Get explainer |
| `brain_get_stats()` | ~5098 | nimcp_brain_state.c | Get statistics |
| `brain_get_num_inputs()` | ~5103 | nimcp_brain_state.c | Get input count |
| `brain_get_systems_consolidation()` | ~5118 | nimcp_brain_state.c | Get consolidation |
| `brain_get_cow_stats()` | ~5134 | nimcp_brain_state.c | Get COW stats |
| `brain_print_info()` | ~5146 | nimcp_brain_state.c | Print debug info |
| `brain_get_memory_usage()` | 5067-5080 | nimcp_brain_state.c | Memory footprint |

---

## Distributed Functions (`distributed/nimcp_brain_distributed.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_clone_cow()` | ~2422 | distributed/nimcp_brain_distributed.c | CoW cloning |
| `brain_mark_as_snapshot()` | ~2434 | distributed/nimcp_brain_distributed.c | Mark snapshot |
| `ensure_writable_network()` | 2349-2407 | distributed/nimcp_brain_distributed.c | Trigger COW |

---

## Persistence Functions (`persistence/nimcp_brain_persistence.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_save()` | ~4959 | persistence/nimcp_brain_persistence.c | Save to disk |
| `brain_load()` | ~5006 | persistence/nimcp_brain_persistence.c | Load from disk |
| `brain_save_snapshot()` | ~5048 | persistence/nimcp_brain_persistence.c | Save snapshot |
| `brain_restore_snapshot()` | ~5050 | persistence/nimcp_brain_persistence.c | Restore snapshot |
| `brain_list_snapshots()` | ~5052 | persistence/nimcp_brain_persistence.c | List snapshots |
| `brain_delete_snapshot()` | ~5054 | persistence/nimcp_brain_persistence.c | Delete snapshot |
| `save_working_memory_state()` | 4395-4450 | persistence/nimcp_brain_persistence.c | Save WM state |
| `save_metadata()` | 4451-4601 | persistence/nimcp_brain_persistence.c | Save metadata |
| `load_working_memory_item()` | 4602-4655 | persistence/nimcp_brain_persistence.c | Load WM item |
| `load_working_memory_state()` | 4656-4717 | persistence/nimcp_brain_persistence.c | Load WM state |
| `load_metadata()` | 4718-4869 | persistence/nimcp_brain_persistence.c | Load metadata |
| `ensure_snapshot_dir()` | 5018-5032 | persistence/nimcp_brain_persistence.c | Create snap dir |
| `get_snapshot_dir()` | 5040-5046 | persistence/nimcp_brain_persistence.c | Get snap dir |

---

## I/O Functions (`nimcp_brain_io.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_import_json()` | 6787-6792 | nimcp_brain_io.c | Import JSON |
| `brain_save_json()` | 6793-6832 | nimcp_brain_io.c | Save as JSON |
| `brain_load_json()` | 6894-6916 | nimcp_brain_io.c | Load JSON |

---

## Strategy Functions (`strategy/nimcp_brain_strategy.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `strategy_create()` | 439-487 | strategy/nimcp_brain_strategy.c | Create strategy |
| `strategy_destroy()` | 488-568 | strategy/nimcp_brain_strategy.c | Destroy strategy |
| `strategy_classification_lr()` | 285-289 | strategy/nimcp_brain_strategy.c | Classification LR |
| `strategy_classification_transform()` | 290-309 | strategy/nimcp_brain_strategy.c | Softmax transform |
| `strategy_classification_loss()` | 310-328 | strategy/nimcp_brain_strategy.c | Cross-entropy loss |
| `strategy_regression_lr()` | 329-333 | strategy/nimcp_brain_strategy.c | Regression LR |
| `strategy_regression_transform()` | 334-340 | strategy/nimcp_brain_strategy.c | Identity transform |
| `strategy_regression_loss()` | 341-358 | strategy/nimcp_brain_strategy.c | MSE loss |
| `strategy_pattern_lr()` | 359-363 | strategy/nimcp_brain_strategy.c | Pattern LR |
| `strategy_pattern_transform()` | 364-371 | strategy/nimcp_brain_strategy.c | Pattern transform |
| `strategy_pattern_loss()` | 372-389 | strategy/nimcp_brain_strategy.c | Pattern loss |
| `strategy_association_lr()` | 390-394 | strategy/nimcp_brain_strategy.c | Association LR |
| `strategy_association_transform()` | 395-410 | strategy/nimcp_brain_strategy.c | Association transform |
| `strategy_association_loss()` | 411-438 | strategy/nimcp_brain_strategy.c | Association loss |

---

## Bio-Async Functions (`nimcp_brain_bio_async.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_bio_init()` | 189-223 | nimcp_brain_bio_async.c | Init bio-async |
| `brain_publish_state_event()` | 224-254 | nimcp_brain_bio_async.c | Publish state |
| `brain_publish_processing_event()` | 255-284 | nimcp_brain_bio_async.c | Publish processing |

---

## Utility Functions (Remain in `nimcp_brain.c`)

| Function | Lines (orig) | Current Location | Responsibility |
|----------|-------------|------------------|----------------|
| `is_cached_input()` | 869-877 | nimcp_brain.c | Check cache |
| `cache_decision()` | 892-945 | nimcp_brain.c | Cache decision |
| `clear_cache()` | 946-995 | nimcp_brain.c | Clear cache |
| `brain_predict()` | 6917-6981 | nimcp_brain.c | Prediction wrapper |

---

## Analysis Functions (`analysis/nimcp_brain_topology.c`)

| Function | Lines (orig) | New Location | Responsibility |
|----------|-------------|--------------|----------------|
| `brain_get_top_neurons()` | ~5162 | analysis/nimcp_brain_topology.c | Top neurons |
| `brain_explain_decision()` | ~5179 | analysis/nimcp_brain_topology.c | Explain decision |
| `brain_prune()` | ~5198 | analysis/nimcp_brain_topology.c | Prune weak connections |

---

## Optimization Functions (Remain in `nimcp_brain.c` / Other)

| Function | Lines (orig) | Current Location | Responsibility |
|----------|-------------|------------------|----------------|
| `brain_resize_update_subsystems_internal()` | 6833-6893 | nimcp_brain_resize.c | Resize subsystems |

---

## Summary Statistics

- **Total Functions Analyzed:** ~80+
- **Functions Extracted:** ~70
- **Functions Remaining in main file:** ~10-15 (orchestration/utilities)
- **New Modules Created:** 20+
- **Lines Reduced:** 6982 → ~1500 (78% reduction)

---

## Function Call Graph (Simplified)

```
brain_decide()
├── is_cached_input()
├── ensure_writable_network()
├── allocate_decision()
├── perform_forward_pass()
│   ├── adaptive_network_forward()
│   └── adaptive_network_forward_readonly()
├── determine_output_label()
├── populate_interpretability()
│   └── adaptive_network_explain()
├── update_inference_stats()
└── cache_decision()

brain_create()
├── validate_creation_params()
├── strategy_create()
├── allocate_brain()
├── init_brain_config()
├── create_brain_network()
├── init_output_labels()
├── init_attention_subsystem()
├── init_brain_regions_subsystem()
├── init_symbolic_logic_subsystem()
├── init_epistemic_subsystem()
└── create_personality()

brain_destroy()
├── brain_save_snapshot() (if configured)
├── adaptive_network_destroy()
├── strategy_destroy()
├── [many subsystem destroy calls]
└── clear_cache()
```

---

**Last Updated:** 2025-12-08
**Maintained By:** NIMCP Development Team
