/**
 * @file nimcp_reasoning_mesh_bridge.h
 * @brief Reasoning-Mesh Bridge — distributed evidence gathering via mesh network
 *
 * WHAT: Bridges the reasoning chain engine with the mesh network for distributed
 *       consensus-based evidence gathering across brain modules
 * WHY:  Reasoning currently queries brain modules sequentially; the mesh enables
 *       concurrent, consensus-validated evidence from memory, KG, and planning
 * HOW:  Registers reasoning as a mesh participant, fires MESH_TX_REASONING
 *       transactions, collects endorsements as evidence, publishes beliefs
 *
 * INTEGRATION ORDER (in reasoning_engine_reason):
 *   1. Portia budget (hardware constraints)
 *   2. Hypothalamus modulation (cognitive state)
 *   3. Mesh evidence gathering (distributed consensus) <- THIS
 *
 * @version 1.0.0
 * @date 2026-02-25
 */

#ifndef NIMCP_REASONING_MESH_BRIDGE_H
#define NIMCP_REASONING_MESH_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations to avoid header cycles */
struct brain_struct;
typedef struct brain_struct* brain_t;

/* Need reasoning config for apply */
#include "cognitive/reasoning/nimcp_reasoning_chain.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum endorsements to collect per reasoning query */
#define REASONING_MESH_MAX_ENDORSEMENTS   16

/** Default timeout for mesh evidence gathering (ms) */
#define REASONING_MESH_DEFAULT_TIMEOUT_MS 100

/** Local ID for reasoning participant in mesh */
#define REASONING_MESH_LOCAL_ID           0x0001

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Evidence source type from mesh endorsement
 */
typedef enum {
    REASONING_EVIDENCE_NONE = 0,
    REASONING_EVIDENCE_MEMORY,         /**< From hippocampus/memory modules */
    REASONING_EVIDENCE_KNOWLEDGE,      /**< From knowledge graph modules */
    REASONING_EVIDENCE_PLANNING,       /**< From planning modules */
    REASONING_EVIDENCE_EMOTIONAL,      /**< From amygdala/emotional modules */
    REASONING_EVIDENCE_SENSORY,        /**< From sensory processing modules */
    REASONING_EVIDENCE_EXECUTIVE       /**< From executive control modules */
} reasoning_evidence_source_t;

/**
 * @brief Single piece of mesh evidence (from an endorsement)
 */
typedef struct {
    reasoning_evidence_source_t source; /**< Where this evidence came from */
    float confidence;                   /**< Source's confidence in evidence [0,1] */
    float relevance;                    /**< Relevance to the query [0,1] */
    char description[256];              /**< Human-readable evidence description */
    uint64_t source_id;                 /**< Mesh participant ID of source */
    bool endorsed;                      /**< Whether source endorsed the query */
} reasoning_mesh_evidence_t;

/**
 * @brief Mesh evidence gathering result
 *
 * WHAT: Collected evidence from mesh network participants
 * WHY:  Multi-module consensus on reasoning evidence
 * HOW:  Populated by mesh_gather_evidence(), consumed by apply
 */
typedef struct {
    /* Whether mesh is available */
    bool mesh_available;

    /* Evidence from endorsing modules */
    reasoning_mesh_evidence_t evidence[REASONING_MESH_MAX_ENDORSEMENTS];
    uint32_t evidence_count;

    /* Aggregate metrics */
    float consensus_confidence;         /**< Weighted average confidence */
    float coherence;                    /**< Channel coherence [0,1] */
    uint32_t endorsements_received;     /**< Total endorsements */
    uint32_t endorsements_approved;     /**< Approved endorsements */

    /* Timing */
    float gather_time_ms;               /**< Time spent gathering evidence */

    /* Mesh status */
    uint64_t channel_participant_count; /**< Participants in reasoning channel */
} reasoning_mesh_result_t;

/*=============================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Return an empty (no mesh) result
 *
 * WHAT: Default result when mesh is unavailable
 * WHY:  Graceful degradation
 *
 * @return Empty result with mesh_available=false
 */
reasoning_mesh_result_t reasoning_mesh_empty_result(void);

/**
 * @brief Gather evidence from mesh network for a reasoning query
 *
 * WHAT: Fire MESH_TX_REASONING transaction, collect endorsements
 * WHY:  Distributed evidence gathering from memory, KG, planning modules
 * HOW:  Creates transaction, submits to LEFT_HEMISPHERE channel, waits for
 *       endorsements within timeout, aggregates results
 *
 * @param brain Brain instance (may be NULL -> empty result)
 * @param query The reasoning query string
 * @param timeout_ms Maximum time to wait for endorsements (0 = default)
 * @return Gathered evidence (empty result if mesh unavailable)
 */
reasoning_mesh_result_t reasoning_mesh_gather_evidence(
    brain_t brain,
    const char* query,
    uint32_t timeout_ms
);

/**
 * @brief Apply mesh evidence to reasoning chain
 *
 * WHAT: Add mesh evidence as additional reasoning steps
 * WHY:  Incorporate distributed evidence into the reasoning trace
 * HOW:  For each endorsed evidence, add a reasoning step with source info
 *
 * @param chain Reasoning chain to add evidence to (non-NULL)
 * @param result Mesh evidence result (non-NULL)
 * @return Number of steps added, or -1 on error
 */
int reasoning_mesh_apply_evidence(
    reasoning_chain_t* chain,
    const reasoning_mesh_result_t* result
);

/**
 * @brief Apply mesh consensus to reasoning config
 *
 * WHAT: Adjust confidence threshold based on consensus strength
 * WHY:  Strong mesh consensus -> higher confidence in reasoning
 * HOW:  If consensus_confidence > 0.8, boost confidence_threshold slightly
 *
 * @param config Engine config to modify (non-NULL)
 * @param result Mesh evidence result (non-NULL)
 * @return 0 on success, -1 on error
 */
int reasoning_mesh_apply_consensus(
    reasoning_engine_config_t* config,
    const reasoning_mesh_result_t* result
);

/**
 * @brief Format mesh evidence result as human-readable summary
 *
 * @param result Result to summarize (non-NULL)
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Characters written, or -1 on error
 */
int reasoning_mesh_result_summary(
    const reasoning_mesh_result_t* result,
    char* buffer,
    uint32_t buffer_size
);

/**
 * @brief Check if mesh is available for reasoning
 *
 * WHAT: Quick check if mesh network is initialized and reasoning channel exists
 * WHY:  Avoid expensive operations when mesh is not available
 *
 * @return true if mesh is available for reasoning
 */
bool reasoning_mesh_is_available(void);

/**
 * @brief Set mesh bootstrap for reasoning bridge
 *
 * WHAT: Store reference to the global mesh bootstrap
 * WHY:  Mesh bootstrap is process-global; reasoning needs access to query channels
 * HOW:  Called during system init (medulla/mesh bootstrap) or explicitly
 *
 * @param bootstrap Mesh bootstrap handle (NULL to disable)
 */
struct mesh_bootstrap;
void reasoning_mesh_set_bootstrap(struct mesh_bootstrap* bootstrap);

/**
 * @brief Get mesh channel statistics relevant to reasoning
 *
 * @param participants_out Output: participant count in reasoning channel
 * @param coherence_out Output: channel coherence [0,1]
 * @return 0 on success, -1 if mesh unavailable
 */
int reasoning_mesh_get_channel_stats(
    uint32_t* participants_out,
    float* coherence_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_MESH_BRIDGE_H */
