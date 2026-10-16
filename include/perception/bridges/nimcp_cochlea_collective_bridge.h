/**
 * @file nimcp_cochlea_collective_bridge.h
 * @brief Cochlea-Collective Cognition integration bridge
 *
 * WHAT: Synchronize cochlear processing across distributed brain instances
 * WHY:  Enable collective listening, shared auditory attention, audio swarm
 * HOW:  Hyperscanning for audio sync, shared intentionality for joint attention
 *
 * THEORETICAL BASIS:
 * - Joint attention: Multiple agents attending to same sound source
 * - Collective hearing: Distributed frequency coverage (bat colony)
 * - Auditory hyperscanning: Phase synchronization across instances
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Local cochlea -> Collective: Audio features, detections, phi
 * - INBOUND:  Collective -> Local cochlea: Shared attention, distributed goals
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_COLLECTIVE_BRIDGE_H
#define NIMCP_COCHLEA_COLLECTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct collective_cognition collective_cognition_t;
typedef struct hyperscanning hyperscanning_t;
typedef struct shared_intentionality shared_intentionality_t;

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_COLLECTIVE_MAX_INSTANCES    64
#define COCHLEA_COLLECTIVE_SYNC_BANDS       8

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Audio synchronization state
 */
typedef struct {
    float phase_coherence[COCHLEA_COLLECTIVE_SYNC_BANDS];
    float gamma_sync;                 /**< High-freq binding (30-100 Hz) */
    float theta_sync;                 /**< Memory/attention (4-8 Hz) */
    float alpha_sync;                 /**< Inhibition (8-12 Hz) */
} cochlea_audio_sync_t;

/**
 * @brief Shared listening goal
 */
typedef struct {
    float shared_attention_freq_hz;   /**< Collective focus frequency */
    float shared_attention_azimuth;   /**< Collective focus direction */
    bool joint_localization_active;   /**< Joint sound localization */
    uint32_t participating_instances; /**< Number of participating instances */
} cochlea_shared_goal_t;

/**
 * @brief Distributed coverage (bat colony mode)
 */
typedef struct {
    float my_freq_range_min;          /**< My frequency range start */
    float my_freq_range_max;          /**< My frequency range end */
    bool distributed_echolocation;    /**< Distributed echolocation mode */
    uint32_t colony_size;             /**< Size of distributed colony */
} cochlea_distributed_coverage_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Instance identity */
    uint32_t instance_id;             /**< This instance's ID */

    /* Synchronization */
    bool enable_hyperscanning;        /**< Enable phase sync */
    float sync_window_ms;             /**< Sync window duration */

    /* Shared intentionality */
    bool enable_joint_attention;      /**< Enable joint attention */
    bool enable_distributed_coverage; /**< Enable distributed frequency coverage */

    /* Phi contribution */
    bool compute_phi;                 /**< Compute IIT phi contribution */
} cochlea_collective_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_collective_bridge cochlea_collective_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_collective_config_t cochlea_collective_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_collective_bridge_t* cochlea_collective_bridge_create(
    cochlea_t* cochlea,
    collective_cognition_t* collective,
    const cochlea_collective_config_t* config
);

void cochlea_collective_bridge_destroy(cochlea_collective_bridge_t* bridge);

nimcp_error_t cochlea_collective_bridge_update(
    cochlea_collective_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_collective_bridge_reset(cochlea_collective_bridge_t* bridge);

//=============================================================================
// Session Management
//=============================================================================

/**
 * @brief Join collective listening session
 */
nimcp_error_t cochlea_collective_join(
    cochlea_collective_bridge_t* bridge,
    uint32_t session_id
);

/**
 * @brief Leave collective session
 */
nimcp_error_t cochlea_collective_leave(cochlea_collective_bridge_t* bridge);

/**
 * @brief Check if in collective session
 */
bool cochlea_collective_is_joined(const cochlea_collective_bridge_t* bridge);

//=============================================================================
// Audio Synchronization (Outbound)
//=============================================================================

/**
 * @brief Sync audio features to collective
 */
nimcp_error_t cochlea_collective_sync_audio(
    cochlea_collective_bridge_t* bridge,
    const cochlea_output_t* output
);

/**
 * @brief Get current sync state
 */
nimcp_error_t cochlea_collective_get_sync(
    const cochlea_collective_bridge_t* bridge,
    cochlea_audio_sync_t* sync
);

//=============================================================================
// Shared Goals (Inbound)
//=============================================================================

/**
 * @brief Receive shared attention goal
 */
nimcp_error_t cochlea_collective_receive_goal(
    cochlea_collective_bridge_t* bridge,
    cochlea_shared_goal_t* goal
);

/**
 * @brief Set distributed coverage assignment
 */
nimcp_error_t cochlea_collective_set_coverage(
    cochlea_collective_bridge_t* bridge,
    const cochlea_distributed_coverage_t* coverage
);

//=============================================================================
// Phi Computation
//=============================================================================

/**
 * @brief Compute phi contribution for collective
 */
float cochlea_collective_compute_phi(const cochlea_collective_bridge_t* bridge);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_collective_verify_bidirectional(const cochlea_collective_bridge_t* bridge);
uint64_t cochlea_collective_get_last_outbound(const cochlea_collective_bridge_t* bridge);
uint64_t cochlea_collective_get_last_inbound(const cochlea_collective_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_COLLECTIVE_BRIDGE_H */
