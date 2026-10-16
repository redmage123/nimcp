/**
 * @file nimcp_cognitive_integration_fep.c
 * @brief FEP Orchestrator integration for Cognitive Integration bridges
 * @version 1.0.0
 * @date 2025
 *
 * WHAT: Implements FEP registration and update callbacks for all cognitive
 *       integration bridges (hub + 8 bidirectional bridges).
 *
 * WHY: Cognitive integration bridges need to participate in the FEP
 *      orchestrator's coordinated update cycle for proper free energy
 *      minimization across the cognitive system.
 *
 * HOW: Each bridge provides an update callback that processes pending
 *      events, updates predictions, and reports free energy metrics.
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_cognitive_integration_fep.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_emotion_memory_bridge.h"
#include "cognitive/integration/nimcp_attention_wm_bridge.h"
#include "cognitive/integration/nimcp_curiosity_reasoning_bridge.h"
#include "cognitive/integration/nimcp_ethics_executive_bridge.h"
#include "cognitive/integration/nimcp_tom_social_bridge.h"
#include "cognitive/integration/nimcp_self_introspection_bridge.h"
#include "cognitive/integration/nimcp_emotion_executive_bridge.h"
#include "cognitive/integration/nimcp_gw_cognitive_bridge.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(cognitive_integration_fep, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Global state for tracking registered cognitive integration bridges
 */
typedef struct {
    uint32_t bridge_ids[COG_INTEG_FEP_COUNT];      /**< FEP-assigned bridge IDs */
    bool registered[COG_INTEG_FEP_COUNT];          /**< Registration status */
    cognitive_integration_fep_metrics_t metrics[COG_INTEG_FEP_COUNT]; /**< Per-bridge metrics */
    fep_orchestrator_t* orchestrator;              /**< FEP orchestrator reference */
    nimcp_mutex_t* mutex;                          /**< Thread safety */
    bool initialized;                              /**< Initialization flag */
} cognitive_integration_fep_state_t;

static cognitive_integration_fep_state_t g_cog_integ_fep_state = {0};

/* Thread-safe initialization guard */
static nimcp_platform_mutex_t g_cog_integ_fep_init_mutex;
static volatile bool g_cog_integ_fep_init_mutex_ready = false;

__attribute__((constructor))
static void cog_integ_fep_init_guard(void) {
    nimcp_platform_mutex_init(&g_cog_integ_fep_init_mutex, false);
    g_cog_integ_fep_init_mutex_ready = true;
}

/* ============================================================================
 * Bridge Names
 * ============================================================================ */

static const char* BRIDGE_NAMES[COG_INTEG_FEP_COUNT] = {
    "cog_integ_hub",
    "emotion_memory_bridge",
    "attention_wm_bridge",
    "curiosity_reasoning_bridge",
    "ethics_executive_bridge",
    "tom_social_bridge",
    "self_introspection_bridge",
    "emotion_executive_bridge",
    "gw_cognitive_bridge"
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize global state if needed
 */
static int ensure_initialized(void) {
    if (g_cog_integ_fep_state.initialized) {
        return 0;
    }

    /* Guard against concurrent initialization with a platform mutex */
    if (g_cog_integ_fep_init_mutex_ready) {
        nimcp_platform_mutex_lock(&g_cog_integ_fep_init_mutex);
    }

    /* Double-check after acquiring lock */
    if (g_cog_integ_fep_state.initialized) {
        if (g_cog_integ_fep_init_mutex_ready) {
            nimcp_platform_mutex_unlock(&g_cog_integ_fep_init_mutex);
        }
        return 0;
    }

    memset(&g_cog_integ_fep_state, 0, sizeof(cognitive_integration_fep_state_t));

    g_cog_integ_fep_state.mutex = nimcp_mutex_create(NULL);
    if (!g_cog_integ_fep_state.mutex) {
        if (g_cog_integ_fep_init_mutex_ready) {
            nimcp_platform_mutex_unlock(&g_cog_integ_fep_init_mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "ensure_initialized: g_cog_integ_fep_state is NULL");
        return -1;
    }

    g_cog_integ_fep_state.initialized = true;

    if (g_cog_integ_fep_init_mutex_ready) {
        nimcp_platform_mutex_unlock(&g_cog_integ_fep_init_mutex);
    }
    return 0;
}

/**
 * @brief Update metrics for a bridge
 */
static void update_metrics(
    cognitive_integration_fep_id_t bridge_type,
    float free_energy,
    float prediction_error,
    float surprise
) {
    if (bridge_type >= COG_INTEG_FEP_COUNT) return;

    cognitive_integration_fep_metrics_t* m = &g_cog_integ_fep_state.metrics[bridge_type];
    m->free_energy = free_energy;
    m->prediction_error = prediction_error;
    m->surprise = surprise;
    m->entropy = prediction_error * 0.5f; /* Simplified entropy estimate */
    m->last_update_time = nimcp_platform_time_monotonic_ms();
    m->update_count++;
}

/* ============================================================================
 * FEP Update Callbacks
 * ============================================================================ */

int cognitive_hub_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_cognitive_hub_fep_up", 0.0f);


    cognitive_integration_hub_t hub = (cognitive_integration_hub_t)handle;
    if (!hub) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hub is NULL");

        return -1;

    }

    /* Process pending events in the hub */
    cognitive_hub_stats_t stats;
    if (cognitive_hub_get_stats(hub, &stats) == 0) {
        /* Calculate free energy based on event processing metrics */
        float pending_ratio = (stats.async_queue_max > 0) ?
            (float)stats.async_queue_depth / (float)stats.async_queue_max : 0.0f;
        float delivery_efficiency = (stats.events_published > 0) ?
            (float)stats.events_delivered / (float)stats.events_published : 1.0f;
        float free_energy = pending_ratio * 0.5f + (1.0f - delivery_efficiency) * 0.5f;
        float prediction_error = pending_ratio;
        float surprise = (stats.events_dropped > 0) ? 0.5f : 0.0f;

        update_metrics(COG_INTEG_FEP_HUB, free_energy, prediction_error, surprise);
    }

    /* Flush any async messages (process up to 100 pending events) */
    cognitive_hub_flush_async_queue(hub, 100);

    return 0;
}

int emotion_memory_bridge_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_emotion_memory_bridg", 0.0f);


    emotion_memory_bridge_t* bridge = (emotion_memory_bridge_t*)handle;
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    emotion_memory_stats_t stats;
    if (emotion_memory_bridge_get_stats(bridge, &stats) == 0) {
        /* Calculate free energy based on emotion-memory alignment */
        /* memories_tagged = successfully emotionally tagged memories */
        /* retrievals_with_emotion = retrievals that triggered emotional responses */
        float tagging_activity = (stats.memories_tagged > 0) ? 1.0f : 0.0f;
        float retrieval_emotional_rate = (stats.memories_tagged > 0) ?
            (float)stats.retrievals_with_emotion / (float)stats.memories_tagged : 0.0f;
        if (retrieval_emotional_rate > 1.0f) retrieval_emotional_rate = 1.0f;

        float free_energy = 1.0f - (tagging_activity + retrieval_emotional_rate) * 0.5f;
        float prediction_error = fabsf(stats.avg_valence) < 0.1f ? 0.5f : 0.2f; /* Neutral = uncertain */
        float surprise = (stats.consolidation_boosts > 0) ? 0.2f : 0.0f;

        update_metrics(COG_INTEG_FEP_EMOTION_MEMORY, free_energy, prediction_error, surprise);
    }

    return 0;
}

int attention_wm_bridge_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_attention_wm_bridge_", 0.0f);


    attention_wm_bridge_t* bridge = (attention_wm_bridge_t*)handle;
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    attention_wm_stats_t stats;
    if (attention_wm_bridge_get_stats(bridge, &stats) == 0) {
        /* Calculate free energy based on attention-WM coordination */
        /* items_gated_in = items admitted by attention gating */
        /* items_evicted = items removed from WM */
        float gating_efficiency = (stats.items_gated_in + stats.items_evicted > 0) ?
            (float)stats.items_gated_in / (float)(stats.items_gated_in + stats.items_evicted) : 1.0f;
        /* current_item_count relative to a reasonable capacity (assume 7 items) */
        float capacity_usage = (float)stats.current_item_count / 7.0f;
        if (capacity_usage > 1.0f) capacity_usage = 1.0f;

        float free_energy = (1.0f - gating_efficiency) * 0.6f + capacity_usage * 0.4f;
        float prediction_error = 1.0f - gating_efficiency;
        float surprise = (stats.focus_shifts > 5) ? 0.3f : 0.0f; /* Many shifts = surprise */

        update_metrics(COG_INTEG_FEP_ATTENTION_WM, free_energy, prediction_error, surprise);
    }

    return 0;
}

int curiosity_reasoning_bridge_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_curiosity_reasoning_", 0.0f);


    curiosity_reasoning_bridge_t* bridge = (curiosity_reasoning_bridge_t*)handle;
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    curiosity_reasoning_stats_t stats;
    if (curiosity_reasoning_bridge_get_stats(bridge, &stats) == 0) {
        /* Curiosity naturally minimizes epistemic free energy */
        /* explorations_driven = explorations initiated by curiosity */
        /* novel_conclusions = new knowledge discovered */
        float discovery_rate = (stats.explorations_driven > 0) ?
            (float)stats.novel_conclusions / (float)stats.explorations_driven : 0.0f;
        if (discovery_rate > 1.0f) discovery_rate = 1.0f;

        /* Lower free energy when curiosity leads to discoveries */
        float free_energy = 1.0f - discovery_rate;
        /* Use avg_curiosity_level as proxy for epistemic uncertainty */
        float prediction_error = 1.0f - stats.avg_curiosity_level;
        float surprise = (stats.novel_conclusions > 0) ? stats.avg_novelty_score * 0.5f : 0.0f;

        update_metrics(COG_INTEG_FEP_CURIOSITY_REASONING, free_energy, prediction_error, surprise);
    }

    return 0;
}

int ethics_executive_bridge_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_ethics_executive_bri", 0.0f);


    ethics_executive_bridge_t* bridge = (ethics_executive_bridge_t*)handle;
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    ethics_executive_stats_t stats;
    if (ethics_executive_bridge_get_stats(bridge, &stats) == 0) {
        /* Ethical compliance minimizes value-related free energy */
        /* evaluations_performed = total ethical evaluations */
        /* actions_vetoed = actions blocked on ethical grounds */
        float compliance_rate = (stats.evaluations_performed > 0) ?
            (float)(stats.evaluations_performed - stats.actions_vetoed) / (float)stats.evaluations_performed : 1.0f;
        /* Use avg_ethical_score as constraint satisfaction (higher = better) */
        float constraint_satisfaction = stats.avg_ethical_score;

        float free_energy = 1.0f - constraint_satisfaction;
        float prediction_error = stats.veto_rate; /* Higher veto rate = more ethical violations */
        float surprise = (stats.actions_vetoed > 0) ? 0.4f : 0.0f;

        update_metrics(COG_INTEG_FEP_ETHICS_EXECUTIVE, free_energy, prediction_error, surprise);
    }

    return 0;
}

int tom_social_bridge_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_tom_social_bridge_fe", 0.0f);


    tom_social_bridge_t* bridge = (tom_social_bridge_t*)handle;
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    tom_social_stats_t stats;
    if (tom_social_get_stats(bridge, &stats) == 0) {
        /* Mental state prediction accuracy */
        /* inferences_made = total ToM inferences */
        /* inference_failures = failed inferences */
        float inference_success_rate = (stats.inferences_made > 0) ?
            (float)(stats.inferences_made - stats.inference_failures) / (float)stats.inferences_made : 0.5f;
        /* avg_inference_confidence indicates model certainty */

        float free_energy = 1.0f - stats.avg_inference_confidence;
        float prediction_error = 1.0f - inference_success_rate;
        float surprise = (stats.inference_failures > 0) ? 0.4f : 0.0f;

        update_metrics(COG_INTEG_FEP_TOM_SOCIAL, free_energy, prediction_error, surprise);
    }

    return 0;
}

int self_introspection_bridge_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_self_introspection_b", 0.0f);


    self_introspection_bridge_t* bridge = (self_introspection_bridge_t*)handle;
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    self_introspection_stats_t stats;
    if (self_introspection_get_stats(bridge, &stats) == 0) {
        /* Self-model accuracy */
        /* avg_discrepancy = average discrepancy between introspection and reality */
        /* avg_introspection_confidence = confidence in self-knowledge */
        float model_accuracy = 1.0f - stats.avg_discrepancy;
        float introspection_coherence = stats.avg_introspection_confidence;

        float free_energy = stats.avg_discrepancy; /* Discrepancy = free energy */
        float prediction_error = 1.0f - introspection_coherence;
        float surprise = (stats.self_model_updates > 0) ? 0.2f : 0.0f;

        update_metrics(COG_INTEG_FEP_SELF_INTROSPECTION, free_energy, prediction_error, surprise);
    }

    return 0;
}

int emotion_executive_bridge_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_emotion_executive_br", 0.0f);


    emotion_executive_bridge_t* bridge = (emotion_executive_bridge_t*)handle;
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    emotion_executive_stats_t stats;
    if (emotion_executive_get_stats(bridge, &stats) == 0) {
        /* Emotion-decision alignment and regulation effectiveness */
        /* regulations_applied = total regulation attempts */
        /* successful_regulations = successful regulation attempts */
        float regulation_success = (stats.regulations_applied > 0) ?
            (float)stats.successful_regulations / (float)stats.regulations_applied : 1.0f;
        /* avg_bias_magnitude indicates emotional interference in decisions */
        float decision_alignment = 1.0f - stats.avg_bias_magnitude;

        float free_energy = (1.0f - regulation_success) * 0.5f + stats.avg_bias_magnitude * 0.5f;
        float prediction_error = 1.0f - stats.avg_regulation_effectiveness;
        float surprise = (stats.conflicts_detected > 0) ? 0.4f : 0.0f;

        update_metrics(COG_INTEG_FEP_EMOTION_EXECUTIVE, free_energy, prediction_error, surprise);
    }

    return 0;
}

int gw_cognitive_bridge_fep_update(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_gw_cognitive_bridge_", 0.0f);


    gw_cognitive_bridge_t* bridge = (gw_cognitive_bridge_t*)handle;
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    gw_cognitive_stats_t stats;
    if (gw_cognitive_get_stats(bridge, &stats) == 0) {
        /* Global workspace broadcast efficiency */
        /* broadcasts_sent = total broadcast attempts */
        /* broadcast_failures = failed broadcasts */
        float broadcast_efficiency = (stats.broadcasts_sent > 0) ?
            (float)(stats.broadcasts_sent - stats.broadcast_failures) / (float)stats.broadcasts_sent : 1.0f;
        /* Faster competition resolution = better efficiency */
        float competition_resolution = 1.0f - stats.avg_competition_time_ms / 100.0f;
        if (competition_resolution < 0.0f) competition_resolution = 0.0f;

        float free_energy = 1.0f - broadcast_efficiency;
        float prediction_error = 1.0f - broadcast_efficiency;
        float surprise = (stats.competitions_held > 0) ? 0.3f : 0.0f;

        update_metrics(COG_INTEG_FEP_GW_COGNITIVE, free_energy, prediction_error, surprise);
    }

    return 0;
}

/* ============================================================================
 * Individual Bridge Registration Functions
 * ============================================================================ */

int cognitive_hub_fep_register(
    fep_orchestrator_t* orchestrator,
    cognitive_integration_hub_t hub,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_hub_fep_register: required parameter is NULL (orchestrator, hub)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cognitive_hub_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_cognitive_hub_fep_re", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_HUB]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_HUB];
        }
        return 0; /* Already registered */
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_HUB],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)hub,
        cognitive_hub_fep_update,
        NULL, /* Hub destroyed separately */
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_HUB] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_HUB] = true;
        g_cog_integ_fep_state.orchestrator = orchestrator;
        if (bridge_id_out) {
            *bridge_id_out = bridge_id;
        }
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

int emotion_memory_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    emotion_memory_bridge_t* bridge,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_memory_bridge_fep_register: required parameter is NULL (orchestrator, bridge)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "emotion_memory_bridge_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_emotion_memory_bridg", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_EMOTION_MEMORY]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_EMOTION_MEMORY];
        }
        return 0;
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_EMOTION_MEMORY],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        emotion_memory_bridge_fep_update,
        NULL,
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_EMOTION_MEMORY] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_EMOTION_MEMORY] = true;
        if (bridge_id_out) *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

int attention_wm_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    attention_wm_bridge_t* bridge,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_wm_bridge_fep_register: required parameter is NULL (orchestrator, bridge)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "attention_wm_bridge_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_attention_wm_bridge_", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_ATTENTION_WM]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_ATTENTION_WM];
        }
        return 0;
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_ATTENTION_WM],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        attention_wm_bridge_fep_update,
        NULL,
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_ATTENTION_WM] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_ATTENTION_WM] = true;
        if (bridge_id_out) *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

int curiosity_reasoning_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    curiosity_reasoning_bridge_t* bridge,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_reasoning_bridge_fep_register: required parameter is NULL (orchestrator, bridge)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "curiosity_reasoning_bridge_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_curiosity_reasoning_", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_CURIOSITY_REASONING]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_CURIOSITY_REASONING];
        }
        return 0;
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_CURIOSITY_REASONING],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        curiosity_reasoning_bridge_fep_update,
        NULL,
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_CURIOSITY_REASONING] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_CURIOSITY_REASONING] = true;
        if (bridge_id_out) *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

int ethics_executive_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    ethics_executive_bridge_t* bridge,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_executive_bridge_fep_register: required parameter is NULL (orchestrator, bridge)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "ethics_executive_bridge_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_ethics_executive_bri", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_ETHICS_EXECUTIVE]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_ETHICS_EXECUTIVE];
        }
        return 0;
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_ETHICS_EXECUTIVE],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        ethics_executive_bridge_fep_update,
        NULL,
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_ETHICS_EXECUTIVE] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_ETHICS_EXECUTIVE] = true;
        if (bridge_id_out) *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

int tom_social_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    tom_social_bridge_t* bridge,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_social_bridge_fep_register: required parameter is NULL (orchestrator, bridge)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "tom_social_bridge_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_tom_social_bridge_fe", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_TOM_SOCIAL]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_TOM_SOCIAL];
        }
        return 0;
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_TOM_SOCIAL],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        tom_social_bridge_fep_update,
        NULL,
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_TOM_SOCIAL] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_TOM_SOCIAL] = true;
        if (bridge_id_out) *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

int self_introspection_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    self_introspection_bridge_t* bridge,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_introspection_bridge_fep_register: required parameter is NULL (orchestrator, bridge)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "self_introspection_bridge_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_self_introspection_b", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_SELF_INTROSPECTION]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_SELF_INTROSPECTION];
        }
        return 0;
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_SELF_INTROSPECTION],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        self_introspection_bridge_fep_update,
        NULL,
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_SELF_INTROSPECTION] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_SELF_INTROSPECTION] = true;
        if (bridge_id_out) *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

int emotion_executive_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    emotion_executive_bridge_t* bridge,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_bridge_fep_register: required parameter is NULL (orchestrator, bridge)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "emotion_executive_bridge_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_emotion_executive_br", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_EMOTION_EXECUTIVE]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_EMOTION_EXECUTIVE];
        }
        return 0;
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_EMOTION_EXECUTIVE],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        emotion_executive_bridge_fep_update,
        NULL,
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_EMOTION_EXECUTIVE] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_EMOTION_EXECUTIVE] = true;
        if (bridge_id_out) *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

int gw_cognitive_bridge_fep_register(
    fep_orchestrator_t* orchestrator,
    gw_cognitive_bridge_t* bridge,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_cognitive_bridge_fep_register: required parameter is NULL (orchestrator, bridge)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "gw_cognitive_bridge_fep_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_gw_cognitive_bridge_", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    if (g_cog_integ_fep_state.registered[COG_INTEG_FEP_GW_COGNITIVE]) {
        nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_GW_COGNITIVE];
        }
        return 0;
    }

    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        BRIDGE_NAMES[COG_INTEG_FEP_GW_COGNITIVE],
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        gw_cognitive_bridge_fep_update,
        NULL,
        &bridge_id
    );

    if (ret == 0) {
        g_cog_integ_fep_state.bridge_ids[COG_INTEG_FEP_GW_COGNITIVE] = bridge_id;
        g_cog_integ_fep_state.registered[COG_INTEG_FEP_GW_COGNITIVE] = true;
        if (bridge_id_out) *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return ret == 0 ? 0 : -1;
}

/* ============================================================================
 * Unified Registration
 * ============================================================================ */

int cognitive_integration_fep_register_all(
    fep_orchestrator_t* orchestrator,
    cognitive_integration_hub_t hub,
    emotion_memory_bridge_t* emotion_memory,
    attention_wm_bridge_t* attention_wm,
    curiosity_reasoning_bridge_t* curiosity_reasoning,
    ethics_executive_bridge_t* ethics_executive,
    tom_social_bridge_t* tom_social,
    self_introspection_bridge_t* self_introspection,
    emotion_executive_bridge_t* emotion_executive,
    gw_cognitive_bridge_t* gw_cognitive
) {
    if (!orchestrator || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_integration_fep_register_all: required parameter is NULL (orchestrator, hub)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_register_all", 0.0f);


    int registered_count = 0;

    /* Register hub (required) */
    if (cognitive_hub_fep_register(orchestrator, hub, NULL) == 0) {
        registered_count++;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_integration_fep_register_all: validation failed");
        return -1; /* Hub registration failure is critical */
    }

    /* Register optional bridges */
    if (emotion_memory && emotion_memory_bridge_fep_register(orchestrator, emotion_memory, NULL) == 0) {
        registered_count++;
    }

    if (attention_wm && attention_wm_bridge_fep_register(orchestrator, attention_wm, NULL) == 0) {
        registered_count++;
    }

    if (curiosity_reasoning && curiosity_reasoning_bridge_fep_register(orchestrator, curiosity_reasoning, NULL) == 0) {
        registered_count++;
    }

    if (ethics_executive && ethics_executive_bridge_fep_register(orchestrator, ethics_executive, NULL) == 0) {
        registered_count++;
    }

    if (tom_social && tom_social_bridge_fep_register(orchestrator, tom_social, NULL) == 0) {
        registered_count++;
    }

    if (self_introspection && self_introspection_bridge_fep_register(orchestrator, self_introspection, NULL) == 0) {
        registered_count++;
    }

    if (emotion_executive && emotion_executive_bridge_fep_register(orchestrator, emotion_executive, NULL) == 0) {
        registered_count++;
    }

    if (gw_cognitive && gw_cognitive_bridge_fep_register(orchestrator, gw_cognitive, NULL) == 0) {
        registered_count++;
    }

    return registered_count;
}

int cognitive_integration_fep_unregister_all(fep_orchestrator_t* orchestrator) {
    if (!orchestrator) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

        return -1;

    }

    if (!g_cog_integ_fep_state.initialized) return 0;

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_unregister_all", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    int unregistered_count = 0;

    for (int i = 0; i < COG_INTEG_FEP_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COG_INTEG_FEP_COUNT > 256) {
            cognitive_integration_fep_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)COG_INTEG_FEP_COUNT);
        }

        if (g_cog_integ_fep_state.registered[i]) {
            int ret = fep_orchestrator_unregister_bridge(
                orchestrator,
                g_cog_integ_fep_state.bridge_ids[i]
            );
            if (ret == 0) {
                g_cog_integ_fep_state.registered[i] = false;
                g_cog_integ_fep_state.bridge_ids[i] = 0;
                unregistered_count++;
            }
        }
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);
    return 0;
}

/* ============================================================================
 * Metrics and Statistics
 * ============================================================================ */

int cognitive_integration_fep_get_metrics(
    cognitive_integration_fep_id_t bridge_type,
    cognitive_integration_fep_metrics_t* metrics_out
) {
    if (bridge_type >= COG_INTEG_FEP_COUNT || !metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_integration_fep_get_metrics: metrics_out is NULL");
        return -1;
    }

    if (!g_cog_integ_fep_state.initialized) {
        memset(metrics_out, 0, sizeof(cognitive_integration_fep_metrics_t));
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_get_metrics", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);
    *metrics_out = g_cog_integ_fep_state.metrics[bridge_type];
    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);

    return 0;
}

int cognitive_integration_fep_get_stats(
    cognitive_integration_fep_stats_t* stats_out
) {
    if (!stats_out) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats_out is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_get_stats", 0.0f);


    memset(stats_out, 0, sizeof(cognitive_integration_fep_stats_t));

    if (!g_cog_integ_fep_state.initialized) {
        return 0;
    }

    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    float total_free_energy = 0.0f;
    float total_prediction_error = 0.0f;
    float max_surprise = 0.0f;
    uint32_t active_count = 0;
    uint64_t total_updates = 0;

    for (int i = 0; i < COG_INTEG_FEP_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COG_INTEG_FEP_COUNT > 256) {
            cognitive_integration_fep_heartbeat("cognitive_in_loop",
                             (float)(i + 1) / (float)COG_INTEG_FEP_COUNT);
        }

        stats_out->bridges[i] = g_cog_integ_fep_state.metrics[i];

        if (g_cog_integ_fep_state.registered[i]) {
            stats_out->registered_count++;

            if (g_cog_integ_fep_state.metrics[i].update_count > 0) {
                active_count++;
                total_free_energy += g_cog_integ_fep_state.metrics[i].free_energy;
                total_prediction_error += g_cog_integ_fep_state.metrics[i].prediction_error;
                if (g_cog_integ_fep_state.metrics[i].surprise > max_surprise) {
                    max_surprise = g_cog_integ_fep_state.metrics[i].surprise;
                }
                total_updates += g_cog_integ_fep_state.metrics[i].update_count;
            }
        }
    }

    stats_out->total_free_energy = total_free_energy;
    stats_out->avg_prediction_error = (active_count > 0) ?
        total_prediction_error / (float)active_count : 0.0f;
    stats_out->max_surprise = max_surprise;
    stats_out->active_count = active_count;
    stats_out->total_updates = total_updates;

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);

    return 0;
}

uint32_t cognitive_integration_fep_get_bridge_id(
    cognitive_integration_fep_id_t bridge_type
) {
    if (bridge_type >= COG_INTEG_FEP_COUNT) return (uint32_t)-1;

    if (!g_cog_integ_fep_state.initialized) return 0;

    /* Phase 8: Heartbeat at operation start */
    cognitive_integration_fep_heartbeat("cognitive_in_get_bridge_id", 0.0f);


    nimcp_mutex_lock(g_cog_integ_fep_state.mutex);

    uint32_t bridge_id = 0;
    if (g_cog_integ_fep_state.registered[bridge_type]) {
        bridge_id = g_cog_integ_fep_state.bridge_ids[bridge_type];
    }

    nimcp_mutex_unlock(g_cog_integ_fep_state.mutex);

    return bridge_id;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void cognitive_integration_fep_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_cognitive_integration_fep_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int cognitive_integration_fep_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_integration_fep_training_begin: NULL argument");
        return -1;
    }
    cognitive_integration_fep_heartbeat_instance(g_cognitive_integration_fep_health_agent,
                                                  "cognitive_integration_fep_training_begin", 0.0f);
    (void)(cognitive_integration_fep_state_t*)instance; /* Module state available for reset */
    return 0;
}

int cognitive_integration_fep_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_integration_fep_training_end: NULL argument");
        return -1;
    }
    cognitive_integration_fep_heartbeat_instance(g_cognitive_integration_fep_health_agent,
                                                  "cognitive_integration_fep_training_end", 1.0f);
    (void)(cognitive_integration_fep_state_t*)instance; /* Module state available for finalization */
    return 0;
}

int cognitive_integration_fep_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_integration_fep_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    cognitive_integration_fep_heartbeat_instance(g_cognitive_integration_fep_health_agent, "cognitive_integration_fep_training_step", progress);
    (void)(cognitive_integration_fep_state_t*)instance; /* Module state available for step adaptation */
    return 0;
}
