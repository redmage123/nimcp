//=============================================================================
// nimcp_inferior_colliculus_bridges.c - IC Bridge Integrations
//=============================================================================
/**
 * @file nimcp_inferior_colliculus_bridges.c
 * @brief Bridge stubs for inferior colliculus integration with other subsystems
 *
 * BRIDGES:
 * - Thalamic MGN relay: IC -> MGN -> auditory cortex
 * - Superior colliculus: IC -> SC for sound-driven orienting
 * - Training: reward/error signals for IC tuning
 * - Substrate GPU: offload spectral analysis
 * - Bio-async: asynchronous IC processing
 * - Immune: IC health monitoring
 */

#include "core/brain/subcortical/nimcp_inferior_colliculus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(ic_bridges, MESH_ADAPTER_CATEGORY_SUBCORTICAL)

/* ============================================================================
 * THALAMIC MGN RELAY BRIDGE
 *
 * Relays IC tonotopic output to the medial geniculate nucleus (MGN)
 * of the thalamus for forwarding to auditory cortex (A1).
 * ============================================================================ */

/**
 * @brief Relay IC output to thalamic MGN
 *
 * @param ic IC handle
 * @param mgn_input Output buffer for MGN (num_channels floats)
 * @param mgn_size Size of MGN buffer
 * @return 0 on success, -1 on error
 */
int ic_bridge_relay_to_mgn(inferior_colliculus_t* ic,
                            float* mgn_input,
                            uint32_t mgn_size) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_relay_to_mgn: ic is NULL");
        return -1;
    }
    if (!mgn_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_relay_to_mgn: mgn_input is NULL");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("IC -> MGN relay: %u channels", mgn_size);

    /* Copy ICC tonotopic response to MGN input buffer */
    int result = ic_get_icc_response(ic, mgn_input, mgn_size);
    if (result != 0) {
        NIMCP_LOGGING_WARN("IC -> MGN relay failed to get ICC response");
        return -1;
    }

    return 0;
}

/**
 * @brief Receive top-down modulation from auditory cortex via MGN
 *
 * @param ic IC handle
 * @param cortical_feedback Feedback signal from A1
 * @param size Size of feedback buffer
 * @return 0 on success, -1 on error
 */
int ic_bridge_receive_cortical_feedback(inferior_colliculus_t* ic,
                                         const float* cortical_feedback,
                                         uint32_t size) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_receive_cortical_feedback: ic is NULL");
        return -1;
    }
    if (!cortical_feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_receive_cortical_feedback: cortical_feedback is NULL");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("IC <- cortical feedback: %u channels", size);

    /* STUB: Top-down modulation of ICD (dorsal cortex).
     * Future: modulate frequency selectivity and spatial tuning. */
    (void)size;
    return 0;
}

/* ============================================================================
 * SUPERIOR COLLICULUS ORIENTING BRIDGE
 *
 * Sends sound localization data to SC for auditory-driven orienting.
 * When a salient sound is detected, SC can trigger a gaze shift.
 * ============================================================================ */

/**
 * @brief Send sound localization to superior colliculus for orienting
 *
 * @param ic IC handle
 * @param azimuth_out Output: azimuth estimate (degrees)
 * @param elevation_out Output: elevation estimate (degrees)
 * @param salience_out Output: sound salience [0-1]
 * @return 0 on success, -1 on error
 */
int ic_bridge_sc_orienting(inferior_colliculus_t* ic,
                            float* azimuth_out,
                            float* elevation_out,
                            float* salience_out) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_sc_orienting: ic is NULL");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("IC -> SC orienting bridge");

    float az = ic_get_azimuth(ic);
    float el = ic_get_elevation(ic);

    if (azimuth_out) *azimuth_out = az;
    if (elevation_out) *elevation_out = el;

    /* Compute salience from mean activation */
    if (salience_out) {
        ic_stats_t stats;
        memset(&stats, 0, sizeof(stats));
        if (ic_get_stats(ic, &stats) == 0) {
            *salience_out = stats.mean_activation;
        } else {
            *salience_out = 0.0f;
        }
    }

    return 0;
}

/* ============================================================================
 * TRAINING BRIDGE
 *
 * Receives reward/error signals for IC parameter tuning.
 * ============================================================================ */

/**
 * @brief Apply reward signal to IC for tuning localization accuracy
 *
 * @param ic IC handle
 * @param reward Reward signal [-1, 1]
 * @param target_azimuth Correct azimuth (degrees) for error computation
 * @return 0 on success, -1 on error
 */
int ic_bridge_training_reward(inferior_colliculus_t* ic,
                               float reward,
                               float target_azimuth) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_training_reward: ic is NULL");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("IC training: reward=%.3f, target_az=%.1f",
                        (double)reward, (double)target_azimuth);

    /* STUB: Adjust ITD/ILD weights based on localization error.
     * Future: compute error = target_azimuth - ic->azimuth_estimate,
     * update itd_weight/ild_weight via gradient. */
    (void)reward;
    (void)target_azimuth;
    return 0;
}

/* ============================================================================
 * SUBSTRATE GPU BRIDGE
 *
 * Offloads spectral analysis to GPU when available.
 * ============================================================================ */

/**
 * @brief Offload IC spectral analysis to GPU
 *
 * @param ic IC handle
 * @param gpu_ctx GPU context (opaque)
 * @return 0 on success, -1 on error (falls back to CPU)
 */
int ic_bridge_gpu_spectral(inferior_colliculus_t* ic, void* gpu_ctx) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_gpu_spectral: ic is NULL");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("IC GPU spectral bridge (ctx=%p)", gpu_ctx);

    /* STUB: GPU-accelerated filterbank / FFT.
     * Future: upload samples, run parallel bandpass, download activations. */
    if (!gpu_ctx) {
        NIMCP_LOGGING_DEBUG("IC GPU bridge: no GPU context, using CPU fallback");
        return -1;
    }

    return 0;
}

/* ============================================================================
 * BIO-ASYNC BRIDGE
 *
 * Enables asynchronous IC processing via bio-async promises.
 * ============================================================================ */

/**
 * @brief Submit asynchronous audio processing
 *
 * @param ic IC handle
 * @param left Left ear samples
 * @param right Right ear samples
 * @param num_samples Number of samples
 * @param promise_out Output: promise handle (opaque)
 * @return 0 on success, -1 on error
 */
int ic_bridge_async_process(inferior_colliculus_t* ic,
                             const float* left,
                             const float* right,
                             uint32_t num_samples,
                             void** promise_out) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_async_process: ic is NULL");
        return -1;
    }
    if (!left || !right) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_async_process: audio buffers are NULL");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("IC async process: %u samples", num_samples);

    /* STUB: Submit to bio-async executor.
     * Future: create promise, enqueue ic_process_audio task. */
    if (promise_out) *promise_out = NULL;

    /* Synchronous fallback */
    return ic_process_audio(ic, left, right, num_samples);
}

/* ============================================================================
 * IMMUNE BRIDGE
 *
 * Reports IC health to the brain immune system.
 * ============================================================================ */

/**
 * @brief Report IC health metrics to immune system
 *
 * @param ic IC handle
 * @param health_out Output: health score [0-1]
 * @return 0 on success, -1 on error
 */
int ic_bridge_immune_health(inferior_colliculus_t* ic, float* health_out) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_bridge_immune_health: ic is NULL");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("IC immune health check");

    /* Compute health from stats */
    ic_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    float health = 1.0f;

    if (ic_get_stats(ic, &stats) == 0) {
        /* Health degrades if no updates for a long time */
        /* or if mean activation is stuck at extremes */
        if (stats.mean_activation > 0.99f || stats.mean_activation < 0.001f) {
            health = 0.5f;  /* Suspicious: saturated or silent */
        }
    } else {
        health = 0.3f;  /* Cannot read stats */
    }

    if (health_out) *health_out = health;
    return 0;
}

/**
 * @brief Notify IC of immune system alert
 *
 * @param ic IC handle
 * @param alert_level Alert level [0-1]
 * @param alert_type Alert type string
 */
void ic_bridge_immune_alert(inferior_colliculus_t* ic,
                             float alert_level,
                             const char* alert_type) {
    if (!ic) return;

    NIMCP_LOGGING_INFO("IC immune alert: level=%.2f type=%s",
                       (double)alert_level,
                       alert_type ? alert_type : "unknown");

    /* STUB: Adjust IC sensitivity under immune stress.
     * Future: reduce gain during inflammation, increase during recovery. */
    (void)alert_level;
    (void)alert_type;
}
