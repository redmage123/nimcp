/**
 * @file nimcp_mesh_kg_routing_bridge.h
 * @brief Bridge between KG Module Wiring and Mesh Pattern Router
 *
 * WHAT: Integrates structural topology (KG) with pattern-based routing (Mesh)
 * WHY:  Combines crisp declarative routing with fuzzy learned routing
 * HOW:  KG provides priors, filtering, and validation for pattern matching
 *
 * DUAL-PROCESS ROUTING:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                         HYBRID ROUTING                                   │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                          │
 * │   INPUT                                                                  │
 * │     │                                                                    │
 * │     ▼                                                                    │
 * │   ┌─────────────────┐                                                   │
 * │   │ Has known type? │──YES──► KG Handler Lookup (FAST PATH)             │
 * │   └────────┬────────┘              │                                    │
 * │            │ NO                    │                                    │
 * │            ▼                       │                                    │
 * │   ┌─────────────────┐              │                                    │
 * │   │ KG Topology     │              │                                    │
 * │   │ Filter          │              │                                    │
 * │   └────────┬────────┘              │                                    │
 * │            │ Candidates            │                                    │
 * │            ▼                       │                                    │
 * │   ┌─────────────────┐              │                                    │
 * │   │ Pattern Match   │              │                                    │
 * │   │ (Similarity)    │              │                                    │
 * │   └────────┬────────┘              │                                    │
 * │            │                       │                                    │
 * │            ▼                       ▼                                    │
 * │   ┌─────────────────┐     ┌─────────────────┐                          │
 * │   │ KG Validation   │     │ Direct Route    │                          │
 * │   └────────┬────────┘     └────────┬────────┘                          │
 * │            │                       │                                    │
 * │            └───────────┬───────────┘                                    │
 * │                        ▼                                                │
 * │                  ENDORSER SET                                           │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * INTEGRATION BENEFITS:
 * 1. Structural priors: KG topology initializes pattern router biases
 * 2. Topological filtering: Only compute similarity for connected modules
 * 3. Hybrid routing: Crisp for known types, fuzzy for novel patterns
 * 4. Validation: Reject pattern matches that violate structural constraints
 * 5. Cross-modal discovery: Find convergence points for multimodal inputs
 * 6. Explainability: Both structural and similarity-based explanations
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_KG_ROUTING_BRIDGE_H
#define NIMCP_MESH_KG_ROUTING_BRIDGE_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Magic number for bridge validation */
#define MESH_KG_BRIDGE_MAGIC            0x4B475242  /* "KGRB" */

/** @brief Maximum modules in topology cache */
#define MESH_KG_MAX_TOPOLOGY_CACHE      256

/** @brief Maximum convergence points for cross-modal */
#define MESH_KG_MAX_CONVERGENCE_POINTS  16

/** @brief Default structural weight for priors */
#define MESH_KG_DEFAULT_STRUCTURAL_WEIGHT 0.3f

/* ============================================================================
 * Routing Mode
 * ============================================================================ */

/**
 * @brief Routing mode selection
 *
 * WHAT: How to combine KG and pattern routing
 * WHY:  Different scenarios need different balance
 */
typedef enum mesh_kg_routing_mode {
    MESH_KG_ROUTE_HYBRID,           /**< KG for known, pattern for novel */
    MESH_KG_ROUTE_KG_ONLY,          /**< Only use KG handlers (fast) */
    MESH_KG_ROUTE_PATTERN_ONLY,     /**< Only use pattern matching */
    MESH_KG_ROUTE_KG_FILTER_PATTERN,/**< KG filters, then pattern ranks */
    MESH_KG_ROUTE_PATTERN_VALIDATE_KG /**< Pattern finds, KG validates */
} mesh_kg_routing_mode_t;

/* ============================================================================
 * Routing Explanation
 * ============================================================================ */

/**
 * @brief Explanation for why a module was selected as endorser
 *
 * WHAT: Dual explanation (structural + similarity)
 * WHY:  Transparency and debugging
 */
typedef struct mesh_kg_routing_explanation {
    mesh_participant_id_t module_id;

    /* Pattern-based explanation */
    float pattern_similarity;       /**< Cosine similarity [0, 1] */
    float activation_level;         /**< After neuromodulation */
    bool selected_by_pattern;       /**< Was pattern match above threshold */

    /* Structural explanation */
    bool has_kg_connection;         /**< Is there a KG edge */
    bool handles_message_type;      /**< Does KG say it handles this type */
    uint32_t kg_handler_priority;   /**< Priority in KG */
    char connection_path[128];      /**< e.g., "sensory -> pfc -> motor" */

    /* Combined */
    float combined_score;           /**< Weighted combination */
    bool validated;                 /**< Passed KG validation */
} mesh_kg_routing_explanation_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief KG-Mesh bridge configuration
 */
typedef struct mesh_kg_bridge_config {
    mesh_kg_routing_mode_t mode;    /**< Routing mode */

    /* Weighting */
    float structural_weight;        /**< Weight for KG topology [0, 1] */
    float pattern_weight;           /**< Weight for pattern similarity [0, 1] */

    /* Filtering */
    bool enable_topological_filter; /**< Only match connected modules */
    uint32_t max_hops;              /**< Max hops for topological search */

    /* Validation */
    bool enable_structural_validation; /**< Reject invalid connections */
    bool allow_novel_connections;   /**< Allow patterns without KG edges */

    /* Learning */
    bool learn_from_routing;        /**< Update pattern router from success */
    float learning_rate;            /**< How fast to incorporate KG priors */

    /* Performance */
    bool enable_topology_cache;     /**< Cache KG lookups */
    bool enable_logging;            /**< Detailed logging */
} mesh_kg_bridge_config_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief KG-Mesh routing bridge (opaque)
 */
typedef struct mesh_kg_routing_bridge mesh_kg_routing_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
void mesh_kg_bridge_default_config(mesh_kg_bridge_config_t* config);

/**
 * @brief Create KG-Mesh routing bridge
 *
 * @param router Pattern router to bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
mesh_kg_routing_bridge_t* mesh_kg_bridge_create(
    mesh_pattern_router_t* router,
    const mesh_kg_bridge_config_t* config
);

/**
 * @brief Destroy bridge
 */
void mesh_kg_bridge_destroy(mesh_kg_routing_bridge_t* bridge);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Register module with both KG wiring and pattern field
 *
 * WHAT: Single registration for hybrid routing
 * WHY:  Keep KG and pattern router in sync
 * HOW:  Registers in both systems, creates cross-references
 *
 * @param bridge Bridge instance
 * @param module_id Participant ID
 * @param wiring KG wiring descriptor
 * @param field Pattern receptive field
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_register_module(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t module_id,
    const kg_module_wiring_t* wiring,
    const mesh_receptive_field_t* field
);

/**
 * @brief Initialize pattern router from KG topology
 *
 * WHAT: Use KG connections as priors for pattern learning
 * WHY:  Bootstrap learning with known structure
 * HOW:  For each KG edge, bias pattern router toward source patterns
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_init_from_topology(
    mesh_kg_routing_bridge_t* bridge
);

/* ============================================================================
 * Routing API
 * ============================================================================ */

/**
 * @brief Route transaction using hybrid KG + pattern approach
 *
 * WHAT: Main routing function combining both systems
 * WHY:  Best of crisp and fuzzy routing
 * HOW:  Mode determines routing strategy
 *
 * @param bridge Bridge instance
 * @param tx Transaction to route
 * @param endorsers Output: selected endorsers
 * @param max_endorsers Max output size
 * @param count_out Output: number of endorsers
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_route(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_transaction_t* tx,
    mesh_activation_t* endorsers,
    size_t max_endorsers,
    size_t* count_out
);

/**
 * @brief Route with explanation
 *
 * WHAT: Route and explain why each module was selected
 * WHY:  Transparency, debugging, introspection
 *
 * @param bridge Bridge instance
 * @param tx Transaction to route
 * @param endorsers Output: selected endorsers
 * @param explanations Output: explanations for each
 * @param max_endorsers Max output size
 * @param count_out Output: number of endorsers
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_route_with_explanation(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_transaction_t* tx,
    mesh_activation_t* endorsers,
    mesh_kg_routing_explanation_t* explanations,
    size_t max_endorsers,
    size_t* count_out
);

/* ============================================================================
 * Topological Filtering API
 * ============================================================================ */

/**
 * @brief Get modules reachable from source within N hops
 *
 * WHAT: Topological neighborhood lookup
 * WHY:  Filter candidates before pattern matching
 *
 * @param bridge Bridge instance
 * @param source_id Source module
 * @param max_hops Maximum hops (1 = direct connections only)
 * @param neighbors Output: reachable modules
 * @param max_neighbors Max output size
 * @param count_out Output: number found
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_get_topological_neighbors(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t source_id,
    uint32_t max_hops,
    mesh_participant_id_t* neighbors,
    size_t max_neighbors,
    size_t* count_out
);

/**
 * @brief Check if structural connection exists
 *
 * @param bridge Bridge instance
 * @param from_id Source module
 * @param to_id Target module
 * @return true if KG edge exists
 */
bool mesh_kg_bridge_has_connection(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t from_id,
    mesh_participant_id_t to_id
);

/* ============================================================================
 * Cross-Modal Discovery API
 * ============================================================================ */

/**
 * @brief Find convergence points for multiple input sources
 *
 * WHAT: Modules that receive from all specified sources
 * WHY:  Discover integration points for multimodal inputs
 *
 * Example: Visual + Auditory → finds STS (Superior Temporal Sulcus)
 *
 * @param bridge Bridge instance
 * @param sources Array of source module IDs
 * @param source_count Number of sources
 * @param convergence_points Output: modules receiving from all sources
 * @param max_points Max output size
 * @param count_out Output: number found
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_find_convergence_points(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_participant_id_t* sources,
    size_t source_count,
    mesh_participant_id_t* convergence_points,
    size_t max_points,
    size_t* count_out
);

/**
 * @brief Suggest endorsers for novel multimodal pattern
 *
 * WHAT: Use KG to suggest endorsers for patterns router hasn't learned
 * WHY:  Handle novel cross-modal inputs gracefully
 *
 * @param bridge Bridge instance
 * @param patterns Array of patterns (one per modality)
 * @param pattern_sources Which module each pattern came from
 * @param pattern_count Number of patterns
 * @param suggested Output: suggested endorsers
 * @param max_suggested Max output size
 * @param count_out Output: number suggested
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_suggest_multimodal_endorsers(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_t* patterns,
    const mesh_participant_id_t* pattern_sources,
    size_t pattern_count,
    mesh_activation_t* suggested,
    size_t max_suggested,
    size_t* count_out
);

/* ============================================================================
 * Validation API
 * ============================================================================ */

/**
 * @brief Validate pattern match against KG structure
 *
 * WHAT: Check if pattern-based activation is structurally valid
 * WHY:  Catch spurious pattern matches
 *
 * @param bridge Bridge instance
 * @param source_id Where transaction came from
 * @param target_id Pattern-matched target
 * @param reason_out Output: reason if invalid (optional)
 * @param reason_size Size of reason buffer
 * @return true if valid connection
 */
bool mesh_kg_bridge_validate_activation(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t source_id,
    mesh_participant_id_t target_id,
    char* reason_out,
    size_t reason_size
);

/**
 * @brief Filter activations by structural validity
 *
 * WHAT: Remove pattern matches that violate KG constraints
 * WHY:  Clean up endorser set
 *
 * @param bridge Bridge instance
 * @param source_id Transaction source
 * @param activations Activations to filter (modified in place)
 * @param count In: number of activations, Out: after filtering
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_filter_by_structure(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t source_id,
    mesh_activation_t* activations,
    size_t* count
);

/* ============================================================================
 * Learning API
 * ============================================================================ */

/**
 * @brief Learn from routing outcome
 *
 * WHAT: Update pattern router based on routing success/failure
 * WHY:  Continuous improvement of pattern-based routing
 *
 * @param bridge Bridge instance
 * @param tx Original transaction
 * @param endorsers Modules that endorsed
 * @param endorser_count Number of endorsers
 * @param success Whether transaction succeeded
 * @param reward Reward signal [0, 1]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_learn_outcome(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_transaction_t* tx,
    const mesh_participant_id_t* endorsers,
    size_t endorser_count,
    bool success,
    float reward
);

/**
 * @brief Strengthen pattern associations along KG edges
 *
 * WHAT: Use KG topology to guide pattern learning
 * WHY:  Faster convergence by respecting structure
 *
 * @param bridge Bridge instance
 * @param from_id Source module
 * @param to_id Target module
 * @param strength How much to strengthen [0, 1]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_strengthen_connection(
    mesh_kg_routing_bridge_t* bridge,
    mesh_participant_id_t from_id,
    mesh_participant_id_t to_id,
    float strength
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct mesh_kg_bridge_stats {
    uint64_t total_routings;
    uint64_t kg_fast_path;          /**< Routed via KG handlers only */
    uint64_t pattern_only;          /**< Routed via pattern only */
    uint64_t hybrid_routings;       /**< Used both systems */
    uint64_t validations_passed;
    uint64_t validations_failed;
    uint64_t topology_cache_hits;
    uint64_t topology_cache_misses;
    float avg_endorsers_per_route;
    float avg_pattern_similarity;
} mesh_kg_bridge_stats_t;

/**
 * @brief Get bridge statistics
 */
nimcp_error_t mesh_kg_bridge_get_stats(
    mesh_kg_routing_bridge_t* bridge,
    mesh_kg_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 */
nimcp_error_t mesh_kg_bridge_reset_stats(mesh_kg_routing_bridge_t* bridge);

/* ============================================================================
 * Introspection API
 * ============================================================================ */

/**
 * @brief Get explanation for a specific routing decision
 *
 * @param bridge Bridge instance
 * @param tx Transaction
 * @param module_id Module to explain
 * @param explanation Output: detailed explanation
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_kg_bridge_explain_routing(
    mesh_kg_routing_bridge_t* bridge,
    const mesh_pattern_transaction_t* tx,
    mesh_participant_id_t module_id,
    mesh_kg_routing_explanation_t* explanation
);

/**
 * @brief Format explanation as human-readable string
 *
 * @param explanation Explanation to format
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
int mesh_kg_bridge_format_explanation(
    const mesh_kg_routing_explanation_t* explanation,
    char* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_KG_ROUTING_BRIDGE_H */
