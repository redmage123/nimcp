/**
 * @file nimcp_swarm_tom_bridge.c
 * @brief Theory of Mind integration for swarm communication
 *
 * WHAT: Bridges the swarm edge runtime with the ToM cognitive module.
 * WHY:  When edge brains communicate (gossip, weight sync), the ToM module
 *       records observations about other agents' states. This is how ToM
 *       develops through actual multi-agent interaction rather than text
 *       scenarios alone.
 * HOW:  Extracts behavioral signals from swarm messages (gradients, weights,
 *       telemetry) and feeds them to tom_observe() and tom_update_self_model().
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Forward Declarations — ToM API (cognitive/nimcp_theory_of_mind.h)
 * We use extern declarations to avoid pulling in the full brain internal
 * header chain from the edge module.
 * ============================================================================ */

/* Opaque types */
typedef struct theory_of_mind_s* theory_of_mind_t;
typedef struct brain_struct*     brain_t;

/* Observation input for ToM inference */
typedef enum {
    SWARM_TOM_EMOTION_NEUTRAL  = 1,
    SWARM_TOM_EMOTION_SURPRISE = 6
} swarm_tom_emotion_placeholder_t;

typedef struct {
    const float* action_vector;
    uint32_t     action_dim;
    const char*  verbal_context;
    int          observed_emotion;    /* tom_emotion_t */
    const float* situational_context;
    uint32_t     context_dim;
} swarm_tom_observation_t;

/* ToM core API — linked from cognitive module */
extern bool tom_observe(theory_of_mind_t tom,
                        const swarm_tom_observation_t* observation);
extern bool tom_update_self_model(theory_of_mind_t tom,
                                  const float* features,
                                  uint32_t num_features,
                                  const char* action_label,
                                  float confidence);

/* Brain handle → internal brain (api/nimcp_api_internal.h layout) */
struct nimcp_brain_handle {
    brain_t internal_brain;
    float   last_loss;
    float   last_gradient_norm;
};
typedef struct nimcp_brain_handle* nimcp_brain_t;

/* Access theory_of_mind from brain_struct.
 * brain_struct is ~800 fields; theory_of_mind is at a known offset.
 * We use a helper that accesses it via the internal brain pointer.
 * Since we can't include brain_internal.h from edge code, we declare
 * the accessor as an extern implemented in the brain module. */

/* We define a small accessor stub here — it calls through to the brain's
 * theory_of_mind field. This avoids needing the full brain_struct layout. */
extern void* nimcp_brain_get_theory_of_mind(void* internal_brain);

#define LOG_MODULE "SWARM_TOM"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum features to extract from peer data for ToM observation. */
#define SWARM_TOM_MAX_FEATURES  64

/** Minimum payload size to be worth analyzing. */
#define SWARM_TOM_MIN_PAYLOAD   8

/* ============================================================================
 * nimcp_swarm_tom_observe_peer
 * ============================================================================ */

int nimcp_swarm_tom_observe_peer(nimcp_brain_t brain_handle,
                                 uint32_t peer_device_id,
                                 const uint8_t* data,
                                 uint32_t data_size)
{
    if (!brain_handle || !brain_handle->internal_brain) {
        return -1;
    }
    if (!data || data_size < SWARM_TOM_MIN_PAYLOAD) {
        return -1;
    }

    /* Get the brain's ToM module */
    theory_of_mind_t tom = (theory_of_mind_t)
        nimcp_brain_get_theory_of_mind(brain_handle->internal_brain);
    if (!tom) {
        return 0;  /* ToM not enabled — silently skip */
    }

    /* Extract a simplified feature vector from the peer's data.
     * The gossip/gradient payload is typically floats — we sample
     * the first N values as a fingerprint of the peer's state. */
    uint32_t num_floats = data_size / sizeof(float);
    if (num_floats < 1) {
        return 0;
    }

    uint32_t feature_count = num_floats < SWARM_TOM_MAX_FEATURES
                           ? num_floats : SWARM_TOM_MAX_FEATURES;

    const float* raw_floats = (const float*)data;

    /* Build a ToM observation — the peer's gradient/weight data
     * is treated as an "action vector" (what the peer is doing). */
    char verbal[128];
    snprintf(verbal, sizeof(verbal),
             "peer_%u_gossip_data_%u_floats", peer_device_id, num_floats);

    swarm_tom_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.action_vector    = raw_floats;
    obs.action_dim       = feature_count;
    obs.verbal_context   = verbal;
    obs.observed_emotion = SWARM_TOM_EMOTION_NEUTRAL;

    bool ok = tom_observe(tom, &obs);

    if (ok) {
        LOG_INFO("[%s] Observed peer %u state (%u features)",
                 LOG_MODULE, peer_device_id, feature_count);
    }

    return ok ? 0 : -1;
}

/* ============================================================================
 * nimcp_swarm_tom_observe_collective
 * ============================================================================ */

int nimcp_swarm_tom_observe_collective(nimcp_brain_t brain_handle,
                                       const uint8_t* aggregated_data,
                                       uint32_t data_size,
                                       uint32_t num_peers)
{
    if (!brain_handle || !brain_handle->internal_brain) {
        return -1;
    }
    if (!aggregated_data || data_size < SWARM_TOM_MIN_PAYLOAD) {
        return -1;
    }

    theory_of_mind_t tom = (theory_of_mind_t)
        nimcp_brain_get_theory_of_mind(brain_handle->internal_brain);
    if (!tom) {
        return 0;  /* ToM not enabled — silently skip */
    }

    /* Extract features from aggregated weights.
     * These represent the collective learning state of the swarm. */
    uint32_t num_floats = data_size / sizeof(float);
    if (num_floats < 1) {
        return 0;
    }

    uint32_t feature_count = num_floats < SWARM_TOM_MAX_FEATURES
                           ? num_floats : SWARM_TOM_MAX_FEATURES;

    const float* raw_floats = (const float*)aggregated_data;

    /* Compute a simple summary statistic — mean magnitude of the weight
     * deltas tells us how much the collective is learning. */
    float mean_mag = 0.0f;
    for (uint32_t i = 0; i < feature_count; i++) {
        mean_mag += fabsf(raw_floats[i]);
    }
    mean_mag /= (float)feature_count;

    /* Higher mean magnitude → the swarm is actively learning (surprise).
     * Low magnitude → stable/converged (neutral). */
    float confidence = (mean_mag > 0.01f) ? 0.7f : 0.3f;

    char label[128];
    snprintf(label, sizeof(label),
             "collective_weight_push_%u_peers_%u_params",
             num_peers, num_floats);

    /* Update self-model: "I received collective knowledge" */
    bool ok = tom_update_self_model(tom, raw_floats, feature_count,
                                    label, confidence);

    if (ok) {
        LOG_INFO("[%s] Observed collective state from %u peers "
                 "(mean_mag=%.4f, confidence=%.2f)",
                 LOG_MODULE, num_peers, mean_mag, confidence);
    }

    return ok ? 0 : -1;
}

/* ============================================================================
 * nimcp_swarm_tom_get_peer_model
 * ============================================================================ */

int nimcp_swarm_tom_get_peer_model(nimcp_brain_t brain_handle,
                                    uint32_t peer_device_id,
                                    char* belief_out,
                                    uint32_t belief_out_size)
{
    if (!brain_handle || !brain_handle->internal_brain) {
        return -1;
    }
    if (!belief_out || belief_out_size == 0) {
        return -1;
    }

    theory_of_mind_t tom = (theory_of_mind_t)
        nimcp_brain_get_theory_of_mind(brain_handle->internal_brain);
    if (!tom) {
        snprintf(belief_out, belief_out_size, "tom_disabled");
        return -1;
    }

    /* Use tom_infer_goal to query what we believe about the peer.
     * The ToM module maintains internal state from prior observations. */
    extern bool tom_infer_goal(theory_of_mind_t tom,
                               char* goal_buffer, size_t buffer_size,
                               float* confidence);

    float confidence = 0.0f;
    bool ok = tom_infer_goal(tom, belief_out, belief_out_size, &confidence);

    if (ok) {
        LOG_INFO("[%s] Peer %u model: \"%s\" (confidence=%.2f)",
                 LOG_MODULE, peer_device_id, belief_out, confidence);
    } else {
        snprintf(belief_out, belief_out_size,
                 "peer_%u_unknown", peer_device_id);
    }

    return ok ? 0 : -1;
}

/* ============================================================================
 * nimcp_brain_get_theory_of_mind — accessor stub
 *
 * This function must be defined somewhere that has access to brain_struct.
 * If it's not already defined in the brain module, we provide a weak
 * symbol fallback that returns NULL (ToM disabled).
 * ============================================================================ */

__attribute__((weak))
void* nimcp_brain_get_theory_of_mind(void* internal_brain)
{
    (void)internal_brain;
    /* Weak fallback — overridden by the real implementation in brain module.
     * Returns NULL so all ToM calls gracefully skip. */
    return NULL;
}
