/**
 * @file nimcp_pr_memory_adapter.h
 * @brief Prime Resonance Memory Adapter for Memory Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts Prime Resonance memory system for Memory layer integration
 * WHY:  PR memory provides quaternion-based semantic memory encoding
 * HOW:  Implements nimcp_module_interface_t wrapping PR memory core
 *
 * PRIME RESONANCE MEMORY:
 * - Quaternion state encoding: (consolidation, emotion, salience, accessibility)
 * - Z-Ladder tiered memory organization
 * - Resonance-based retrieval
 * - Entanglement between related memories
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_MEMORY_ADAPTER_H
#define NIMCP_PR_MEMORY_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nimcp_pr_memory_adapter_struct* nimcp_pr_memory_adapter_t;

/**
 * @brief Quaternion memory state (matches PR memory core)
 */
typedef struct {
    float w;    /**< Consolidation strength [0,1] */
    float x;    /**< Emotional valence [-1,+1] */
    float y;    /**< Salience/attention [0,1] */
    float z;    /**< Accessibility [0,1] */
} pr_quaternion_state_t;

typedef struct {
    uint32_t max_memories;              /**< Maximum memory capacity */
    uint32_t z_ladder_tiers;            /**< Number of Z-ladder tiers */
    float resonance_threshold;          /**< Min resonance for retrieval */
    float consolidation_rate;           /**< Rate of consolidation */
    float decay_rate;                   /**< Memory decay rate */
    bool enable_entanglement;           /**< Enable memory entanglement */
    bool enable_reconsolidation;        /**< Enable memory reconsolidation */
    bool enable_logging;                /**< Enable adapter logging */
} nimcp_pr_memory_config_t;

typedef struct {
    uint32_t total_memories;            /**< Current memory count */
    uint32_t consolidated_memories;     /**< Fully consolidated count */
    float mean_consolidation;           /**< Average consolidation strength */
    float mean_accessibility;           /**< Average accessibility */
    float resonance_activity;           /**< Current resonance level */
    uint32_t active_entanglements;      /**< Active entanglement pairs */
    bool is_active;
} nimcp_pr_memory_state_t;

typedef struct {
    uint64_t updates_processed;
    uint64_t messages_handled;
    uint64_t memories_encoded;
    uint64_t memories_retrieved;
    uint64_t consolidation_events;
    uint64_t reconsolidation_events;
} nimcp_pr_memory_stats_t;

NIMCP_EXPORT nimcp_pr_memory_config_t nimcp_pr_memory_adapter_default_config(void);
NIMCP_EXPORT nimcp_pr_memory_adapter_t nimcp_pr_memory_adapter_create(const nimcp_pr_memory_config_t* config);
NIMCP_EXPORT void nimcp_pr_memory_adapter_destroy(nimcp_pr_memory_adapter_t adapter);
NIMCP_EXPORT nimcp_module_interface_t* nimcp_pr_memory_adapter_get_interface(nimcp_pr_memory_adapter_t adapter);

/**
 * @brief Encode a new memory with quaternion state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pr_memory_adapter_encode(
    nimcp_pr_memory_adapter_t adapter,
    const float* content,
    uint32_t content_size,
    const pr_quaternion_state_t* initial_state,
    uint32_t* memory_id_out
);

/**
 * @brief Retrieve memory by resonance matching
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pr_memory_adapter_retrieve(
    nimcp_pr_memory_adapter_t adapter,
    const float* cue,
    uint32_t cue_size,
    float* content_out,
    uint32_t max_content,
    pr_quaternion_state_t* state_out
);

/**
 * @brief Get memory quaternion state by ID
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pr_memory_adapter_get_memory_state(
    nimcp_pr_memory_adapter_t adapter,
    uint32_t memory_id,
    pr_quaternion_state_t* state_out
);

/**
 * @brief Trigger consolidation cycle
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pr_memory_adapter_consolidate(
    nimcp_pr_memory_adapter_t adapter,
    float duration_ms
);

NIMCP_EXPORT nimcp_layer_error_t nimcp_pr_memory_adapter_get_state(nimcp_pr_memory_adapter_t adapter, nimcp_pr_memory_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_pr_memory_adapter_get_stats(nimcp_pr_memory_adapter_t adapter, nimcp_pr_memory_stats_t* stats_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_pr_memory_adapter_reset_stats(nimcp_pr_memory_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_MEMORY_ADAPTER_H */
