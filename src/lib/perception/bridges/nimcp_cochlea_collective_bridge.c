/**
 * @file nimcp_cochlea_collective_bridge.c
 * @brief Cochlea-Collective Cognition integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_collective_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_collective_bridge)

#define LOG_MODULE "COCHLEA_COLLECTIVE_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

struct cochlea_collective_bridge {
    bridge_base_t base;                         /* MUST be first */
    cochlea_collective_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    collective_cognition_t* collective;

    /* Session */
    bool in_session;
    uint32_t session_id;

    /* Sync state */
    cochlea_audio_sync_t sync_state;

    /* Shared goals */
    cochlea_shared_goal_t shared_goal;

    /* Distributed coverage */
    cochlea_distributed_coverage_t coverage;

    /* Phi computation */
    float phi_contribution;

    /* Bidirectional timestamps */
    uint64_t last_outbound_ts;
    uint64_t last_inbound_ts;
};

//=============================================================================
// Helpers
//=============================================================================

static uint64_t collective_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static inline float collective_clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_collective_config_t cochlea_collective_config_default(void) {
    cochlea_collective_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.instance_id = 0;
    cfg.enable_hyperscanning = true;
    cfg.sync_window_ms = 50.0f;
    cfg.enable_joint_attention = true;
    cfg.enable_distributed_coverage = false;
    cfg.compute_phi = true;
    return cfg;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_collective_bridge_t* cochlea_collective_bridge_create(
    cochlea_t* cochlea,
    collective_cognition_t* collective,
    const cochlea_collective_config_t* config
) {
    cochlea_collective_bridge_heartbeat("create", 0.0f);

    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_bridge_create: cochlea NULL");
        return NULL;
    }
    if (!collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_bridge_create: collective NULL");
        return NULL;
    }

    cochlea_collective_bridge_t* bridge = (cochlea_collective_bridge_t*)
        nimcp_calloc(1, sizeof(cochlea_collective_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_collective_bridge_create: bridge is NULL");
        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_collective") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_collective_bridge_create: validation failed");
        return NULL;
    }

    /* Store references */
    bridge->cochlea = cochlea;
    bridge->collective = collective;

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_collective_config_default();
    }

    /* Initialize sync state */
    for (int i = 0; i < COCHLEA_COLLECTIVE_SYNC_BANDS; i++) {
        bridge->sync_state.phase_coherence[i] = 0.0f;
    }

    bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    bridge_base_connect_b_unlocked(&bridge->base, collective);

    cochlea_collective_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_collective_bridge_destroy(cochlea_collective_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_collective");
    cochlea_collective_bridge_heartbeat("destroy", 0.0f);

    if (bridge->in_session) {
        cochlea_collective_leave(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_collective_bridge_update(
    cochlea_collective_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_bridge_update: bridge NULL");
        return -1;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_bridge_update: cochlea_output NULL");
        return -1;
    }

    cochlea_collective_bridge_heartbeat("update", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    (void)dt_ms;

    if (bridge->in_session) {
        /* Decay sync coherence toward zero when not actively syncing */
        for (int i = 0; i < COCHLEA_COLLECTIVE_SYNC_BANDS; i++) {
            bridge->sync_state.phase_coherence[i] *= 0.99f;
        }
        bridge->sync_state.gamma_sync *= 0.99f;
        bridge->sync_state.theta_sync *= 0.99f;
        bridge->sync_state.alpha_sync *= 0.99f;

        /* Compute phi if enabled */
        if (bridge->config.compute_phi) {
            /* Simple phi estimate based on sync coherence */
            float total_coherence = 0.0f;
            for (int i = 0; i < COCHLEA_COLLECTIVE_SYNC_BANDS; i++) {
                total_coherence += bridge->sync_state.phase_coherence[i];
            }
            bridge->phi_contribution = total_coherence / (float)COCHLEA_COLLECTIVE_SYNC_BANDS;
        }
    }

    bridge->last_outbound_ts = collective_get_time_ms();

    bridge_base_record_update(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_collective_bridge_heartbeat("update", 1.0f);
    return 0;
}

nimcp_error_t cochlea_collective_bridge_reset(cochlea_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_bridge_reset: bridge NULL");
        return -1;
    }
    cochlea_collective_bridge_heartbeat("reset", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->in_session = false;
    bridge->session_id = 0;

    memset(&bridge->sync_state, 0, sizeof(bridge->sync_state));
    memset(&bridge->shared_goal, 0, sizeof(bridge->shared_goal));
    memset(&bridge->coverage, 0, sizeof(bridge->coverage));
    bridge->phi_contribution = 0.0f;
    bridge->last_outbound_ts = 0;
    bridge->last_inbound_ts = 0;

    bridge_base_reset_unlocked(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_collective_bridge_heartbeat("reset", 1.0f);
    return 0;
}

//=============================================================================
// Session Management
//=============================================================================

nimcp_error_t cochlea_collective_join(
    cochlea_collective_bridge_t* bridge,
    uint32_t session_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_join: bridge NULL");
        return -1;
    }

    cochlea_collective_bridge_heartbeat("join", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->in_session = true;
    bridge->session_id = session_id;

    /* Initialize sync bands to low coherence */
    for (int i = 0; i < COCHLEA_COLLECTIVE_SYNC_BANDS; i++) {
        bridge->sync_state.phase_coherence[i] = 0.1f;
    }
    bridge->sync_state.gamma_sync = 0.1f;
    bridge->sync_state.theta_sync = 0.1f;
    bridge->sync_state.alpha_sync = 0.1f;

    bridge->last_inbound_ts = collective_get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_collective_bridge_heartbeat("join", 1.0f);
    return 0;
}

nimcp_error_t cochlea_collective_leave(cochlea_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_leave: bridge NULL");
        return -1;
    }

    cochlea_collective_bridge_heartbeat("leave", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->in_session = false;
    bridge->session_id = 0;

    /* Clear sync state */
    memset(&bridge->sync_state, 0, sizeof(bridge->sync_state));
    memset(&bridge->shared_goal, 0, sizeof(bridge->shared_goal));

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_collective_bridge_heartbeat("leave", 1.0f);
    return 0;
}

bool cochlea_collective_is_joined(const cochlea_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_is_joined: bridge NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool r = bridge->in_session;
    nimcp_mutex_unlock(bridge->base.mutex);
    return r;
}

//=============================================================================
// Audio Synchronization (Outbound)
//=============================================================================

nimcp_error_t cochlea_collective_sync_audio(
    cochlea_collective_bridge_t* bridge,
    const cochlea_output_t* output
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_sync_audio: bridge NULL");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_sync_audio: output NULL");
        return -1;
    }

    cochlea_collective_bridge_heartbeat("sync_audio", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->in_session) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "cochlea_collective_sync_audio: not in session");
        return -1;
    }

    /* Simulate increasing coherence when syncing */
    for (int i = 0; i < COCHLEA_COLLECTIVE_SYNC_BANDS; i++) {
        bridge->sync_state.phase_coherence[i] = collective_clampf(
            bridge->sync_state.phase_coherence[i] + 0.05f, 0.0f, 1.0f);
    }
    bridge->sync_state.gamma_sync = collective_clampf(
        bridge->sync_state.gamma_sync + 0.03f, 0.0f, 1.0f);
    bridge->sync_state.theta_sync = collective_clampf(
        bridge->sync_state.theta_sync + 0.04f, 0.0f, 1.0f);
    bridge->sync_state.alpha_sync = collective_clampf(
        bridge->sync_state.alpha_sync + 0.02f, 0.0f, 1.0f);

    bridge->last_outbound_ts = collective_get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_collective_bridge_heartbeat("sync_audio", 1.0f);
    return 0;
}

nimcp_error_t cochlea_collective_get_sync(
    const cochlea_collective_bridge_t* bridge,
    cochlea_audio_sync_t* sync
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_get_sync: bridge NULL");
        return -1;
    }
    if (!sync) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_get_sync: sync NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *sync = bridge->sync_state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Shared Goals (Inbound)
//=============================================================================

nimcp_error_t cochlea_collective_receive_goal(
    cochlea_collective_bridge_t* bridge,
    cochlea_shared_goal_t* goal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_receive_goal: bridge NULL");
        return -1;
    }
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_receive_goal: goal NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->in_session) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "cochlea_collective_receive_goal: not in session");
        return -1;
    }

    *goal = bridge->shared_goal;
    bridge->last_inbound_ts = collective_get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

nimcp_error_t cochlea_collective_set_coverage(
    cochlea_collective_bridge_t* bridge,
    const cochlea_distributed_coverage_t* coverage
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_set_coverage: bridge NULL");
        return -1;
    }
    if (!coverage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_set_coverage: coverage NULL");
        return -1;
    }

    cochlea_collective_bridge_heartbeat("set_coverage", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->coverage = *coverage;
    bridge->last_inbound_ts = collective_get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_collective_bridge_heartbeat("set_coverage", 1.0f);
    return 0;
}

//=============================================================================
// Phi Computation
//=============================================================================

float cochlea_collective_compute_phi(const cochlea_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_compute_phi: bridge NULL");
        return 0.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float phi = 0.0f;
    if (bridge->in_session && bridge->config.compute_phi) {
        /* Phi based on integration of sync coherence across bands */
        float total_coherence = 0.0f;
        for (int i = 0; i < COCHLEA_COLLECTIVE_SYNC_BANDS; i++) {
            total_coherence += bridge->sync_state.phase_coherence[i];
        }
        /* Phi scales with coherence and gamma sync */
        phi = (total_coherence / (float)COCHLEA_COLLECTIVE_SYNC_BANDS)
              * (0.5f + 0.5f * bridge->sync_state.gamma_sync);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return phi;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_collective_verify_bidirectional(const cochlea_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_verify_bidirectional: bridge NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool ok = (bridge->last_outbound_ts > 0) && (bridge->last_inbound_ts > 0);
    nimcp_mutex_unlock(bridge->base.mutex);
    return ok;
}

uint64_t cochlea_collective_get_last_outbound(const cochlea_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_get_last_outbound: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}

uint64_t cochlea_collective_get_last_inbound(const cochlea_collective_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_collective_get_last_inbound: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}
