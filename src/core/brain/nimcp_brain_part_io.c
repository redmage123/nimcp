// nimcp_brain_part_io.c - io functions
// Part of nimcp_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain.c


//=============================================================================
// Analysis & Monitoring API
//=============================================================================

/**
 * @brief Get brain statistics
 *
 * WHY: Provides performance and training metrics
 * Essential for monitoring and debugging
 *
 * COMPLEXITY: O(1) - mostly copying cached stats
 *
 * @param brain Brain handle
 * @param stats Output statistics
 * @return true on success
 */
// brain_get_stats() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

/**
 * @brief Get number of input features for this brain
 */
// brain_get_num_inputs() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

/**
 * @brief Get systems consolidation subsystem
 *
 * WHAT: Access the brain's systems consolidation component
 * WHY:  Allow other modules (e.g., mental health) to interact with memory consolidation
 * HOW:  Return pointer to systems consolidation subsystem
 *
 * THREAD SAFETY: Thread-safe (read-only access to pointer)
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @return Pointer to systems consolidation, or NULL if brain is NULL or consolidation not initialized
 */
// brain_get_systems_consolidation() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

/**
 * @brief Get COW statistics for brain
 *
 * WHAT: Report copy-on-write memory sharing status
 * WHY:  Allow monitoring of memory efficiency gains
 * HOW:  Check is_cow_clone flag and calculate shared/private memory
 *
 * THREAD SAFETY: Thread-safe (read-only access)
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @param cow_stats Output COW statistics
 * @return true on success
 */
// brain_get_cow_stats() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

/**
 * @brief Print brain info to stdout
 *
 * WHY: Convenient debugging and monitoring
 * Human-readable status display
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 */
// brain_print_info() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

/**
 * @brief Get most important neurons
 *
 * WHY: Identifies which neurons contribute most to decisions
 * Useful for pruning and interpretability
 *
 * COMPLEXITY: O(n*log(k)) where n = total_neurons, k = top_n
 *
 * @param brain Brain handle
 * @param top_n Number of neurons to return
 * @param neuron_ids Output array of neuron IDs
 * @param importances Output array of importance scores
 * @return Number of neurons returned
 */
// brain_get_top_neurons() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

/**
 * @brief Explain why brain made a decision
 *
 * WHY: Provides human-readable explanation of decision
 * Critical for trust and debugging
 *
 * COMPLEXITY: O(k) where k = num_active_neurons
 *
 * @param brain Brain handle
 * @param features Input that led to decision
 * @param num_features Feature count
 * @param explanation Output buffer
 * @param max_length Max explanation length
 * @return true on success
 */
// brain_explain_decision() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

//=============================================================================
// Optimization API
//=============================================================================

/**
 * @brief Prune weak connections
 *
 * WHY: Removes low-weight synapses to improve efficiency
 * Reduces memory and speeds up inference
 *
 * COMPLEXITY: O(n*c) where c = connections per neuron
 * BENEFIT: 2-10x inference speedup possible
 *
 * @param brain Brain handle
 * @param threshold Prune synapses with weight < threshold
 * @return Number of synapses pruned
 */
// brain_prune() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

/**
 * @brief Optimize brain for inference
 *
 * WHY: Prepares brain for production deployment
 * Performs aggressive optimization for speed
 *
 * COMPLEXITY: O(n*c)
 * BENEFIT: Can achieve 5-10x speedup
 *
 * @param brain Brain handle
 * @return true on success
 */
// brain_optimize_for_inference() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

/**
 * @brief Get recommended pruning threshold
 *
 * WHY: Provides heuristic for safe pruning
 * Balances sparsity vs accuracy
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @param target_sparsity Desired sparsity (0-1)
 * @return Recommended threshold
 */
// brain_recommend_pruning_threshold() - MOVED TO: src/core/brain/strategy/nimcp_brain_strategy.c

//=============================================================================
// Phase 3: Distributed Brain API Implementation
//=============================================================================

/**
 * WHAT: Create distributed brain with P2P coordination
 * WHY:  Enable multi-brain collaborative learning and shared chemical signals
 * HOW:  Create standard brain, then attach distributed cognition coordinator
 *
 * THREAD SAFETY: Thread-safe creation
 * PERFORMANCE: O(n) where n = num_neurons + network initialization
 */
// brain_create_distributed() - MOVED TO: src/core/brain/distributed/nimcp_brain_distributed.c

/**
 * WHAT: Enable distributed coordination on existing brain
 * WHY:  Allow conversion of standalone brain to distributed mode
 * HOW:  Creates distrib_cognition coordinator and starts sync threads
 *
 * THREAD SAFETY: Thread-safe if brain not actively processing
 */
// brain_enable_distributed() - MOVED TO: src/core/brain/distributed/nimcp_brain_distributed.c

/**
 * WHAT: Synchronize neuromodulators with peer brains
 * WHY:  Allow explicit control of sync timing for performance tuning
 * HOW:  Calls distrib_cognition_broadcast_neuromod for all neuromod types
 *
 * THREAD SAFETY: Thread-safe
 * PERFORMANCE: O(P × N) where P=peers, N=neuromod types
 */
// brain_sync_neuromodulators() - MOVED TO: src/core/brain/distributed/nimcp_brain_distributed.c

/**
 * WHAT: Get distributed cognition statistics
 * WHY:  Monitor distributed brain performance and health
 * HOW:  Forwards query to underlying distrib_cognition coordinator
 */
// brain_get_distributed_stats() - MOVED TO: src/core/brain/distributed/nimcp_brain_distributed.c

/**
 * WHAT: Check if brain is distributed
 * WHY:  Allow callers to query brain mode before calling distributed APIs
 * HOW:  Return true if distributed coordinator exists
 */
// brain_is_distributed() - MOVED TO: src/core/brain/distributed/nimcp_brain_distributed.c

//=============================================================================
// Comprehensive Module Access API - Stub Implementations
//=============================================================================

// ============================================================================
// Phase 7: Advanced Subsystem Accessor Functions - EXTRACTED
// ============================================================================
// EXTRACTED TO: src/core/brain/accessors/nimcp_brain_accessors.c
// Functions: brain_get_glial, brain_get_oscillations, brain_get_introspection,
//            brain_get_ethics, brain_get_salience, brain_get_consolidation,
//            brain_get_curiosity, brain_get_knowledge, brain_get_logic,
//            brain_get_symbolic_logic, brain_get_pink_noise,
//            brain_get_mirror_activations, brain_compute_empathy,
//            brain_enable_astrocytes, brain_get_astrocyte_stats


//=============================================================================
// Phase 8: Unified Multi-Modal Processing Implementation
//=============================================================================
// EXTRACTED TO: src/core/brain/nimcp_brain_multimodal.c
// DATE: 2025-12-08
//
// All multimodal processing functions have been moved to a dedicated module:
// - extract_sensory_features() - Visual/audio/speech feature extraction
// - apply_attention_to_features() - Multihead attention for selective processing
// - process_brain_regions() - Hierarchical brain regions processing
// - integrate_multimodal_features() - Cross-modal integration
// - process_neural_network() - Network processing with glial/oscillations
// - apply_cognitive_processing() - Introspection/ethics/salience/curiosity
// - consolidation_strengthen() - Memory consolidation
// - format_output() - Decision formatting and explanation generation
// - brain_process_multimodal() - Complete multimodal pipeline
//
// See: include/core/brain/nimcp_brain_multimodal.h
//=============================================================================


//=============================================================================
// Phase 9.0: Pre-Trained Models Implementation
//=============================================================================
// NOTE: Extracted to src/core/brain/pretrained/nimcp_brain_pretrained.c
// Includes: brain_model_exists, brain_download_model, brain_get_model_info,
//           brain_create_pretrained, brain_load_pretrained, brain_finetune
// See include/core/brain/pretrained/nimcp_brain_pretrained.h for API


//=============================================================================
// Shannon Information Theory API (Phase C4)
//=============================================================================
// EXTRACTED TO: src/core/brain/information/nimcp_brain_shannon.c
// DATE: 2025-11-19
//
// All Shannon information theory functions have been moved to a dedicated module:
// - brain_enable_shannon_monitoring()
// - brain_get_shannon_metrics()
// - brain_set_shannon_config()
// - brain_enable_quantum_shannon_diffusion()
// - brain_set_quantum_shannon_mixing()
// - brain_set_quantum_shannon_steps()
// - brain_get_quantum_shannon_metrics()
// - brain_evolve_quantum_shannon()
// - brain_enable_cross_modal_monitoring()
// - brain_get_cross_modal_graph()
// - brain_get_cross_modal_metrics()
// - brain_set_cross_modal_threshold()
//
// See: include/core/brain/information/nimcp_brain_shannon.h
//=============================================================================

//=============================================================================
// Community Detection & Network Topology Analysis
//=============================================================================

//=============================================================================
// Network Topology & Community Detection - EXTRACTED
//=============================================================================
// EXTRACTED TO: src/core/brain/analysis/nimcp_brain_topology.c
// Functions: brain_build_topology_graph (static), brain_detect_communities,
//            brain_get_neuron_community, brain_detect_hubs, brain_is_hub_neuron,
//            brain_compute_topology_metrics, brain_validate_topology,
//            brain_get_network_analyzer


char* brain_export_json(brain_t brain, uint32_t flags)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "root is NULL");

        return NULL;
    }

    cJSON_AddStringToObject(root, "schema_version", "1.0");
    cJSON_AddStringToObject(root, "status", "stub_implementation");

    char* json = (flags & (1 << 7)) ? cJSON_PrintUnformatted(root) : cJSON_Print(root);
    cJSON_Delete(root);

    return json;
}


brain_t brain_import_json(const char* json_str)
{
    (void)json_str;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_import_json: operation failed");
    return NULL;
}


bool brain_save_json(brain_t brain, const char* filepath, uint32_t flags)
{
    if (!brain || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save_json: required parameter is NULL (brain, filepath)");
        return false;
    }

    char* json = brain_export_json(brain, flags);
    if (!json) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save_json: json is NULL");
        return false;
    }

    FILE* f = fopen(filepath, "w");
    if (!f) {
        nimcp_free(json);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save_json: f is NULL");
        return false;
    }
    
    size_t written = fwrite(json, 1, strlen(json), f);
    fclose(f);
    nimcp_free(json);
    
    return (written > 0);
}


brain_t brain_load_json(const char* filepath)
{
    (void)filepath;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_load_json: operation failed");
    return NULL;
}
