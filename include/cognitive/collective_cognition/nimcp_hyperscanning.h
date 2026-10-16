/**
 * @file nimcp_hyperscanning.h
 * @brief Inter-brain neural synchronization system
 *
 * WHAT: Real-time neural synchronization between NIMCP brain instances
 * WHY: Enable distributed consciousness through coordinated neural activity
 * HOW: Phase-locking value (PLV) computation across EEG-like frequency bands
 *
 * THEORETICAL BASIS:
 * - Hyperscanning: Simultaneous recording of neural activity from multiple brains
 * - Phase-Locking Value (PLV): Measure of inter-brain synchronization
 * - Frequency Bands: Delta/Theta/Alpha/Beta/Gamma for different cognitive functions
 * - Leader-Follower Dynamics: Identifying influence direction in social interaction
 *
 * FREQUENCY BANDS:
 * - Delta (0.5-4 Hz): Deep coordination, empathy, turn-taking
 * - Theta (4-8 Hz): Memory encoding, emotional bonding
 * - Alpha (8-13 Hz): Relaxed attention, inhibition, mutual monitoring
 * - Beta (13-30 Hz): Active thinking, motor planning, action coordination
 * - Gamma (30-100 Hz): Binding, consciousness, shared attention
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_HYPERSCANNING_H
#define NIMCP_HYPERSCANNING_H

#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Maximum tracked instance pairs for detailed sync metrics */
#define HYPERSCAN_MAX_PAIRS             ((COLLECTIVE_MAX_INSTANCES * (COLLECTIVE_MAX_INSTANCES - 1)) / 2)

/** Minimum PLV to consider synchronized */
#define HYPERSCAN_PLV_THRESHOLD         0.5f

/** Entrainment requires sustained high sync */
#define HYPERSCAN_ENTRAINMENT_WINDOW    10

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Neural state for a single instance
 */
typedef struct {
    uint32_t instance_id;
    uint64_t timestamp_us;

    /* Power spectral density per band [0-1] */
    float band_power[SYNC_BAND_COUNT];

    /* Phase angle per band [0, 2*PI] */
    float band_phase[SYNC_BAND_COUNT];

    /* Global workspace integration */
    float gw_broadcast_strength;
    uint32_t gw_dominant_module;

    /* Metabolic state for load balancing */
    float atp_level;
    float fatigue_level;
} hyperscanning_neural_state_t;

/**
 * @brief Phase-locking metrics between two instances
 */
typedef struct {
    uint32_t instance_a;
    uint32_t instance_b;

    /* Phase-locking value per band [0-1] */
    float plv[SYNC_BAND_COUNT];

    /* Spectral coherence per band [0-1] */
    float coherence[SYNC_BAND_COUNT];

    /* Temporal dynamics */
    float lag_ms;           /**< A leads if positive, B leads if negative */
    float granger_causality; /**< Directional influence A->B [0-1] */
    bool is_synchronized;    /**< PLV above threshold */
} hyperscan_pair_t;

/**
 * @brief Entrainment request for active synchronization
 */
typedef struct {
    uint32_t requester_id;
    uint32_t target_id;
    sync_band_t target_band;
    float target_frequency_hz;
    float target_phase;
    uint32_t duration_ms;
} entrainment_request_t;

/**
 * @brief Entrainment status
 */
typedef enum {
    ENTRAINMENT_NONE = 0,
    ENTRAINMENT_REQUESTED,
    ENTRAINMENT_IN_PROGRESS,
    ENTRAINMENT_ACHIEVED,
    ENTRAINMENT_FAILED,
    ENTRAINMENT_RELEASED
} entrainment_status_t;

/**
 * @brief Callback for neural state updates
 */
typedef void (*hyperscanning_state_callback_fn)(
    const hyperscanning_neural_state_t* state,
    void* user_data
);

/**
 * @brief Callback for entrainment events
 */
typedef void (*hyperscanning_entrainment_callback_fn)(
    uint32_t requester_id,
    uint32_t target_id,
    entrainment_status_t status,
    void* user_data
);

/**
 * @brief Statistics for hyperscanning
 */
typedef struct {
    uint64_t states_received;
    uint64_t states_processed;
    uint64_t sync_computations;
    uint64_t entrainment_requests;
    uint64_t entrainment_successes;
    uint64_t entrainment_failures;
    float avg_global_sync;
    float max_global_sync;
    float avg_plv_gamma;
    float avg_plv_theta;
} hyperscanning_stats_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create hyperscanning system
 *
 * @param config Configuration (NULL for defaults)
 * @return Hyperscanning handle or NULL on failure
 */
hyperscanning_t* hyperscanning_create(const hyperscanning_config_t* config);

/**
 * @brief Destroy hyperscanning system
 *
 * @param hs Hyperscanning system to destroy
 */
void hyperscanning_destroy(hyperscanning_t* hs);

/**
 * @brief Reset hyperscanning system
 *
 * @param hs Hyperscanning system
 * @return 0 on success, -1 on error
 */
int hyperscanning_reset(hyperscanning_t* hs);

/*=============================================================================
 * Instance Management API
 *===========================================================================*/

/**
 * @brief Register an instance for hyperscanning
 *
 * @param hs Hyperscanning system
 * @param instance_id Instance identifier
 * @param state_callback Callback for state updates (optional)
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int hyperscanning_register_instance(
    hyperscanning_t* hs,
    uint32_t instance_id,
    hyperscanning_state_callback_fn state_callback,
    void* user_data
);

/**
 * @brief Unregister an instance
 *
 * @param hs Hyperscanning system
 * @param instance_id Instance to unregister
 * @return 0 on success, -1 on error
 */
int hyperscanning_unregister_instance(
    hyperscanning_t* hs,
    uint32_t instance_id
);

/**
 * @brief Update neural state for an instance
 *
 * @param hs Hyperscanning system
 * @param state New neural state
 * @return 0 on success, -1 on error
 */
int hyperscanning_update_state(
    hyperscanning_t* hs,
    const hyperscanning_neural_state_t* state
);

/*=============================================================================
 * Synchronization API
 *===========================================================================*/

/**
 * @brief Compute synchronization metrics
 *
 * Call this periodically to update all pair metrics and global state.
 *
 * @param hs Hyperscanning system
 * @return 0 on success, -1 on error
 */
int hyperscanning_update(hyperscanning_t* hs);

/**
 * @brief Get synchronization metrics for an instance pair
 *
 * @param hs Hyperscanning system
 * @param instance_a First instance
 * @param instance_b Second instance
 * @param pair Output pair metrics
 * @return 0 on success, -1 on error
 */
int hyperscanning_get_pair_sync(
    const hyperscanning_t* hs,
    uint32_t instance_a,
    uint32_t instance_b,
    hyperscan_pair_t* pair
);

/**
 * @brief Get global hyperscanning state
 *
 * @param hs Hyperscanning system
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hyperscanning_get_state(
    const hyperscanning_t* hs,
    hyperscan_state_t* state
);

/**
 * @brief Get PLV for a specific band between two instances
 *
 * @param hs Hyperscanning system
 * @param instance_a First instance
 * @param instance_b Second instance
 * @param band Frequency band
 * @return PLV value [0-1], or -1 on error
 */
float hyperscanning_get_plv(
    const hyperscanning_t* hs,
    uint32_t instance_a,
    uint32_t instance_b,
    sync_band_t band
);

/*=============================================================================
 * Entrainment API
 *===========================================================================*/

/**
 * @brief Request entrainment to a target instance
 *
 * The requester will attempt to synchronize their neural oscillations
 * to match the target's oscillations in the specified band.
 *
 * @param hs Hyperscanning system
 * @param request Entrainment request
 * @return 0 on success, -1 on error
 */
int hyperscanning_entrain_to(
    hyperscanning_t* hs,
    const entrainment_request_t* request
);

/**
 * @brief Release active entrainment
 *
 * @param hs Hyperscanning system
 * @param instance_id Instance to release entrainment for
 * @return 0 on success, -1 on error
 */
int hyperscanning_release_entrainment(
    hyperscanning_t* hs,
    uint32_t instance_id
);

/**
 * @brief Get entrainment status for an instance
 *
 * @param hs Hyperscanning system
 * @param instance_id Instance to check
 * @return Entrainment status
 */
entrainment_status_t hyperscanning_get_entrainment_status(
    const hyperscanning_t* hs,
    uint32_t instance_id
);

/**
 * @brief Set entrainment callback
 *
 * @param hs Hyperscanning system
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int hyperscanning_set_entrainment_callback(
    hyperscanning_t* hs,
    hyperscanning_entrainment_callback_fn callback,
    void* user_data
);

/*=============================================================================
 * Leader-Follower API
 *===========================================================================*/

/**
 * @brief Get the current leader instance
 *
 * The leader is the instance with highest influence on others
 * (typically highest gamma power and others synchronizing to them).
 *
 * @param hs Hyperscanning system
 * @return Leader instance ID, or 0 if no leader
 */
uint32_t hyperscanning_get_leader(const hyperscanning_t* hs);

/**
 * @brief Get leader's influence strength
 *
 * @param hs Hyperscanning system
 * @return Influence strength [0-1]
 */
float hyperscanning_get_leader_influence(const hyperscanning_t* hs);

/**
 * @brief Get influence direction between two instances
 *
 * @param hs Hyperscanning system
 * @param from_instance Potential influencer
 * @param to_instance Potential follower
 * @return Influence strength [0-1], negative if to_instance influences from_instance
 */
float hyperscanning_get_influence(
    const hyperscanning_t* hs,
    uint32_t from_instance,
    uint32_t to_instance
);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get hyperscanning statistics
 *
 * @param hs Hyperscanning system
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hyperscanning_get_stats(
    const hyperscanning_t* hs,
    hyperscanning_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param hs Hyperscanning system
 */
void hyperscanning_reset_stats(hyperscanning_t* hs);

/*=============================================================================
 * Debug API
 *===========================================================================*/

/**
 * @brief Dump hyperscanning state for debugging
 *
 * @param hs Hyperscanning system
 */
void hyperscanning_dump(const hyperscanning_t* hs);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPERSCANNING_H */
