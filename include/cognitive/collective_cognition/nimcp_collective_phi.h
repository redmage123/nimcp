/**
 * @file nimcp_collective_phi.h
 * @brief Integrated Information Theory (IIT) metrics for collective consciousness
 *
 * WHAT: Measure integrated information (phi) across multiple brain instances
 * WHY: Quantify collective consciousness based on IIT 3.0
 * HOW: Compute phi from information integration and network topology
 *
 * THEORETICAL BASIS:
 * - Integrated Information Theory (Tononi, 2004, 2008, 2014)
 * - Phi = intrinsic cause-effect power of a system
 * - Consciousness is integrated information
 * - Network topology affects integration capacity
 *
 * KEY CONCEPTS:
 * - Information: Reduction of uncertainty (Shannon)
 * - Integration: Information above and beyond the parts
 * - Exclusion: Definite causal boundaries
 * - Composition: Specific conscious experience
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_COLLECTIVE_PHI_H
#define NIMCP_COLLECTIVE_PHI_H

#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Maximum phi computation depth (affects precision vs performance) */
#define PHI_MAX_COMPUTATION_DEPTH       4

/** Number of qualia dimensions tracked */
#define PHI_QUALIA_DIMENSIONS           8

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Phi aggregation methods
 */
typedef enum {
    PHI_AGG_SUM = 0,        /**< Simple sum of local phis */
    PHI_AGG_AVG,            /**< Average of local phis */
    PHI_AGG_GEOMETRIC,      /**< Geometric mean */
    PHI_AGG_SYNERGISTIC     /**< Sum with synergy bonus */
} phi_aggregation_method_t;

/**
 * @brief Per-instance phi contribution
 */
typedef struct {
    uint32_t instance_id;
    float local_phi;            /**< Individual phi */
    float network_contribution; /**< Contribution to network phi */
    float information_flow_in;  /**< Incoming information rate */
    float information_flow_out; /**< Outgoing information rate */
    float integration_factor;   /**< How well integrated [0-1] */
} instance_phi_contribution_t;

/**
 * @brief Information flow between instances
 */
typedef struct {
    uint32_t from_instance;
    uint32_t to_instance;
    float flow_rate;            /**< Information bits per second */
    float mutual_information;   /**< Shared information content */
    float transfer_entropy;     /**< Directional information transfer */
} information_flow_t;

/**
 * @brief Qualia report (subjective experience dimensions)
 */
typedef struct {
    float valence;              /**< Positive/negative [-1, 1] */
    float arousal;              /**< Low/high activation [0, 1] */
    float complexity;           /**< Simple/complex [0, 1] */
    float coherence;            /**< Fragmented/unified [0, 1] */
    float temporal_depth;       /**< Present-focused/extended [0, 1] */
    float spatial_extent;       /**< Local/global [0, 1] */
    float agency;               /**< Passive/active [0, 1] */
    float metacognition;        /**< Aware of awareness [0, 1] */
} qualia_report_t;

/**
 * @brief Emergence event (significant phi changes)
 */
typedef struct {
    uint64_t timestamp_us;
    float phi_before;
    float phi_after;
    float delta;
    uint32_t instances_involved;
    bool is_emergence;          /**< true = phi increased, false = decreased */
    char description[64];
} emergence_event_t;

/**
 * @brief Collective phi statistics
 */
typedef struct {
    uint64_t computations;
    float avg_phi;
    float max_phi;
    float min_phi;
    float phi_variance;
    uint64_t emergence_events;
    uint64_t fragmentation_events;
    float avg_information_flow;
} collective_phi_stats_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create collective phi system
 *
 * @param config Configuration (NULL for defaults)
 * @return Collective phi handle or NULL on failure
 */
collective_phi_system_t* collective_phi_create(const collective_phi_config_t* config);

/**
 * @brief Destroy collective phi system
 *
 * @param cps Collective phi system to destroy
 */
void collective_phi_destroy(collective_phi_system_t* cps);

/**
 * @brief Reset collective phi system
 *
 * @param cps Collective phi system
 * @return 0 on success, -1 on error
 */
int collective_phi_reset(collective_phi_system_t* cps);

/*=============================================================================
 * Instance Management API
 *===========================================================================*/

/**
 * @brief Register an instance for phi computation
 *
 * @param cps Collective phi system
 * @param instance_id Instance identifier
 * @param initial_phi Initial local phi value
 * @return 0 on success, -1 on error
 */
int collective_phi_register_instance(
    collective_phi_system_t* cps,
    uint32_t instance_id,
    float initial_phi
);

/**
 * @brief Unregister an instance
 *
 * @param cps Collective phi system
 * @param instance_id Instance to unregister
 * @return 0 on success, -1 on error
 */
int collective_phi_unregister_instance(
    collective_phi_system_t* cps,
    uint32_t instance_id
);

/**
 * @brief Update local phi for an instance
 *
 * @param cps Collective phi system
 * @param instance_id Instance ID
 * @param local_phi New local phi value
 * @return 0 on success, -1 on error
 */
int collective_phi_update_local(
    collective_phi_system_t* cps,
    uint32_t instance_id,
    float local_phi
);

/*=============================================================================
 * Information Flow API
 *===========================================================================*/

/**
 * @brief Update information flow between instances
 *
 * @param cps Collective phi system
 * @param from_instance Source instance
 * @param to_instance Target instance
 * @param flow Information flow data
 * @return 0 on success, -1 on error
 */
int collective_phi_update_flow(
    collective_phi_system_t* cps,
    uint32_t from_instance,
    uint32_t to_instance,
    const information_flow_t* flow
);

/**
 * @brief Get information flow between instances
 *
 * @param cps Collective phi system
 * @param from_instance Source instance
 * @param to_instance Target instance
 * @param flow Output flow data
 * @return 0 on success, -1 on error
 */
int collective_phi_get_flow(
    const collective_phi_system_t* cps,
    uint32_t from_instance,
    uint32_t to_instance,
    information_flow_t* flow
);

/*=============================================================================
 * Computation API
 *===========================================================================*/

/**
 * @brief Update/recompute collective phi
 *
 * @param cps Collective phi system
 * @return 0 on success, -1 on error
 */
int collective_phi_update(collective_phi_system_t* cps);

/**
 * @brief Get current collective phi metrics
 *
 * @param cps Collective phi system
 * @param phi Output phi structure
 * @return 0 on success, -1 on error
 */
int collective_phi_get(
    const collective_phi_system_t* cps,
    collective_phi_t* phi
);

/**
 * @brief Get consciousness level classification
 *
 * @param cps Collective phi system
 * @return Consciousness level
 */
collective_consciousness_level_t collective_phi_get_level(
    const collective_phi_system_t* cps
);

/**
 * @brief Get per-instance phi contribution
 *
 * @param cps Collective phi system
 * @param instance_id Instance ID
 * @param contribution Output contribution
 * @return 0 on success, -1 on error
 */
int collective_phi_get_contribution(
    const collective_phi_system_t* cps,
    uint32_t instance_id,
    instance_phi_contribution_t* contribution
);

/*=============================================================================
 * Qualia API
 *===========================================================================*/

/**
 * @brief Get current qualia report
 *
 * Estimates the "quality" of collective consciousness.
 *
 * @param cps Collective phi system
 * @param report Output qualia report
 * @return 0 on success, -1 on error
 */
int collective_phi_get_qualia(
    const collective_phi_system_t* cps,
    qualia_report_t* report
);

/**
 * @brief Update qualia from external input
 *
 * @param cps Collective phi system
 * @param report New qualia values
 * @return 0 on success, -1 on error
 */
int collective_phi_update_qualia(
    collective_phi_system_t* cps,
    const qualia_report_t* report
);

/*=============================================================================
 * Network Analysis API
 *===========================================================================*/

/**
 * @brief Get integration matrix
 *
 * Returns NxN matrix of integration between all instance pairs.
 *
 * @param cps Collective phi system
 * @param matrix Output matrix (row-major, caller allocates)
 * @param size Input: matrix dimension, Output: actual dimension
 * @return 0 on success, -1 on error
 */
int collective_phi_get_integration_matrix(
    const collective_phi_system_t* cps,
    float* matrix,
    uint32_t* size
);

/**
 * @brief Compute minimum information partition (MIP)
 *
 * The MIP is the partition that least reduces integrated information.
 *
 * @param cps Collective phi system
 * @param partition Output partition (array of group IDs per instance)
 * @param num_groups Output number of groups
 * @return Phi of MIP
 */
float collective_phi_compute_mip(
    collective_phi_system_t* cps,
    uint32_t* partition,
    uint32_t* num_groups
);

/*=============================================================================
 * Event API
 *===========================================================================*/

/**
 * @brief Get recent emergence events
 *
 * @param cps Collective phi system
 * @param events Output event array
 * @param max_events Maximum events to return
 * @return Number of events returned
 */
uint32_t collective_phi_get_events(
    const collective_phi_system_t* cps,
    emergence_event_t* events,
    uint32_t max_events
);

/**
 * @brief Clear event history
 *
 * @param cps Collective phi system
 */
void collective_phi_clear_events(collective_phi_system_t* cps);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get collective phi statistics
 *
 * @param cps Collective phi system
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int collective_phi_get_stats(
    const collective_phi_system_t* cps,
    collective_phi_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param cps Collective phi system
 */
void collective_phi_reset_stats(collective_phi_system_t* cps);

/*=============================================================================
 * Debug API
 *===========================================================================*/

/**
 * @brief Dump collective phi state for debugging
 *
 * @param cps Collective phi system
 */
void collective_phi_dump(const collective_phi_system_t* cps);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_PHI_H */
