/**
 * @file nimcp_mirror_plasticity_bridge.c
 * @brief Mirror Neuron - Plasticity Module Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/rng/nimcp_rand.h"

#define LOG_MODULE "mirror_plasticity_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structures
//=============================================================================

struct mirror_plasticity_bridge {
    bridge_base_t base;                  /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    mirror_plasticity_config_t config;

    /* Plasticity orchestrator */
    plasticity_orchestrator_t* orchestrator;
    bool owns_orchestrator;

    /* Synapse storage */
    mirror_plasticity_synapse_t* synapses;
    uint32_t max_synapses;
    uint32_t num_synapses;
    uint32_t next_synapse_id;

    /* Action mapping */
    uint32_t action_synapse_counts[MIRROR_PLASTICITY_MAX_ACTIONS];

    /* State */
    mirror_plasticity_state_t state;
    float current_lr_modulation;
    bool learning_blocked;

    /* External systems */
    void* immune_system;
    void* sleep_system;
    bool bio_async_connected;

    /* Callbacks */
    mirror_plasticity_weight_callback_t weight_callback;
    void* weight_callback_data;
    mirror_plasticity_consolidation_callback_t consolidation_callback;
    void* consolidation_callback_data;
    mirror_plasticity_homeostatic_callback_t homeostatic_callback;
    void* homeostatic_callback_data;
    mirror_plasticity_energy_callback_t energy_callback;
    void* energy_callback_data;

    /* Statistics */
    mirror_plasticity_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
    uint64_t last_consolidation_us;

    /* Health agent (instance-level) */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_plasticity_bridge)

static uint64_t get_time_us(void) {
    return nimcp_time_get_us();
}

static void init_synapse(mirror_plasticity_synapse_t* s, uint32_t id,
                         uint32_t action, mirror_synapse_type_t type, float weight) {
    memset(s, 0, sizeof(*s));
    s->synapse_id = id;
    s->action_id = action;
    s->type = type;
    s->weight = weight;
    s->initial_weight = weight;
    s->bcm_threshold = 0.5f;
}

static mirror_plasticity_synapse_t* find_synapse(
    mirror_plasticity_bridge_t* bridge, uint32_t id) {
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;
}

static float compute_stdp(mirror_plasticity_bridge_t* bridge,
                          float delta_t_ms, float weight) {
    const mirror_plasticity_config_t* cfg = &bridge->config;

    if (delta_t_ms > 0) {  /* LTP: pre before post */
        if (delta_t_ms > cfg->stdp_ltp_window_ms) return 0.0f;
        float dw = cfg->stdp_a_plus * expf(-delta_t_ms / cfg->stdp_tau_plus);
        return dw * (cfg->weight_max - weight);  /* Soft bound */
    } else {  /* LTD: post before pre */
        delta_t_ms = -delta_t_ms;
        if (delta_t_ms > cfg->stdp_ltd_window_ms) return 0.0f;
        float dw = -cfg->stdp_a_minus * expf(-delta_t_ms / cfg->stdp_tau_minus);
        return dw * (weight - cfg->weight_min);  /* Soft bound */
    }
}

//=============================================================================
// Orchestrator Event Callbacks
//=============================================================================

static void on_ltp_event(const plasticity_event_t* event, void* user_data) {
    mirror_plasticity_bridge_t* bridge = (mirror_plasticity_bridge_t*)user_data;
    if (!bridge || !event) return;

    bridge->stats.total_ltp_events++;
    bridge->stats.avg_ltp_magnitude =
        0.99f * bridge->stats.avg_ltp_magnitude + 0.01f * event->delta;

    /* Find and update synapse */
    mirror_plasticity_synapse_t* s = find_synapse(bridge, event->synapse_id);
    if (s) {
        float old_weight = s->weight;
        s->weight = nimcp_clampf(s->weight + event->delta,
                           bridge->config.weight_min, bridge->config.weight_max);
        s->ltp_count++;
        s->total_weight_change += event->delta;

        /* User callback */
        if (bridge->weight_callback) {
            bridge->weight_callback(s->synapse_id, s->action_id,
                old_weight, s->weight, MIRROR_LEARN_LTP, bridge->weight_callback_data);
        }
    }
}

static void on_ltd_event(const plasticity_event_t* event, void* user_data) {
    mirror_plasticity_bridge_t* bridge = (mirror_plasticity_bridge_t*)user_data;
    if (!bridge || !event) return;

    bridge->stats.total_ltd_events++;
    bridge->stats.avg_ltd_magnitude =
        0.99f * bridge->stats.avg_ltd_magnitude + 0.01f * fabsf(event->delta);

    mirror_plasticity_synapse_t* s = find_synapse(bridge, event->synapse_id);
    if (s) {
        float old_weight = s->weight;
        s->weight = nimcp_clampf(s->weight + event->delta,
                           bridge->config.weight_min, bridge->config.weight_max);
        s->ltd_count++;
        s->total_weight_change += event->delta;

        if (bridge->weight_callback) {
            bridge->weight_callback(s->synapse_id, s->action_id,
                old_weight, s->weight, MIRROR_LEARN_LTD, bridge->weight_callback_data);
        }
    }
}

static void on_consolidation_event(const plasticity_event_t* event, void* user_data) {
    mirror_plasticity_bridge_t* bridge = (mirror_plasticity_bridge_t*)user_data;
    if (!bridge || !event) return;

    bridge->stats.consolidation_events++;
    bridge->stats.consolidated_synapses++;

    if (bridge->consolidation_callback) {
        /* Find action for this synapse */
        mirror_plasticity_synapse_t* s = find_synapse(bridge, event->synapse_id);
        uint32_t action_id = s ? s->action_id : 0;
        bridge->consolidation_callback(action_id, 1, s ? s->weight : 0.0f,
                                       bridge->consolidation_callback_data);
    }
}

static void on_homeostatic_event(const plasticity_event_t* event, void* user_data) {
    mirror_plasticity_bridge_t* bridge = (mirror_plasticity_bridge_t*)user_data;
    if (!bridge || !event) return;

    bridge->stats.homeostatic_events++;
    bridge->stats.total_scaling_factor += event->delta;

    if (bridge->homeostatic_callback) {
        bridge->homeostatic_callback(event->old_value, bridge->config.target_rate_hz,
                                     event->delta, bridge->homeostatic_callback_data);
    }

    /* Apply scaling to all synapses */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        float old_w = bridge->synapses[i].weight;
        bridge->synapses[i].weight *= event->delta;
        bridge->synapses[i].weight = nimcp_clampf(bridge->synapses[i].weight,
            bridge->config.weight_min, bridge->config.weight_max);

        if (bridge->weight_callback) {
            bridge->weight_callback(bridge->synapses[i].synapse_id,
                bridge->synapses[i].action_id, old_w, bridge->synapses[i].weight,
                MIRROR_LEARN_HOMEOSTATIC, bridge->weight_callback_data);
        }
    }
}

static void on_energy_depleted(const plasticity_event_t* event, void* user_data) {
    mirror_plasticity_bridge_t* bridge = (mirror_plasticity_bridge_t*)user_data;
    if (!bridge) return;

    bridge->learning_blocked = true;
    if (bridge->energy_callback) {
        bridge->energy_callback(event->new_value, true, bridge->energy_callback_data);
    }
}

static void on_energy_restored(const plasticity_event_t* event, void* user_data) {
    mirror_plasticity_bridge_t* bridge = (mirror_plasticity_bridge_t*)user_data;
    if (!bridge) return;

    bridge->learning_blocked = false;
    if (bridge->energy_callback) {
        bridge->energy_callback(event->new_value, false, bridge->energy_callback_data);
    }
}

//=============================================================================
// Lifecycle API
//=============================================================================

mirror_plasticity_config_t mirror_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_co", 0.0f);


    mirror_plasticity_config_t cfg = {
        .stdp_ltp_window_ms = MIRROR_PLASTICITY_STDP_WINDOW,
        .stdp_ltd_window_ms = 30.0f,
        .stdp_a_plus = 0.005f,
        .stdp_a_minus = 0.00525f,
        .stdp_tau_plus = NIMCP_STDP_TAU_PLUS_MS,
        .stdp_tau_minus = NIMCP_STDP_TAU_MINUS_MS,

        .enable_bcm = true,
        .bcm_threshold_tau = 1000.0f,
        .bcm_activity_tau = 100.0f,

        .enable_homeostatic = true,
        .target_rate_hz = 5.0f,
        .homeostatic_tau_ms = 3600000.0f,  /* 1 hour */

        .enable_eligibility = true,
        .eligibility_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .reward_modulation_gain = 1.0f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,
        .initial_weight = NIMCP_SYNAPSE_WEIGHT_INIT,

        .enable_bio_async = true,
        .enable_immune_integration = true,
        .enable_sleep_integration = true,

        .update_interval_ms = 1.0f,
        .consolidation_interval_ms = 60000.0f
    };
    return cfg;
}

mirror_plasticity_bridge_t* mirror_plasticity_create(
    const mirror_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_cr", 0.0f);


    mirror_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->config = config ? *config : mirror_plasticity_config_default();

    /* Allocate synapse storage */
    bridge->max_synapses = MIRROR_PLASTICITY_MAX_SYNAPSES;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(mirror_plasticity_synapse_t));
    if (!bridge->synapses) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Create orchestrator */
    plasticity_orchestrator_config_t orch_cfg;
    plasticity_orchestrator_default_config(&orch_cfg);
    orch_cfg.global_learning_rate = MIRROR_PLASTICITY_DEFAULT_LR;
    orch_cfg.connect_bio_async = bridge->config.enable_bio_async;

    bridge->orchestrator = plasticity_orchestrator_create(&orch_cfg);
    if (!bridge->orchestrator) {
        nimcp_free(bridge->synapses);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_plasticity_create: bridge->orchestrator is NULL");
        return NULL;
    }
    bridge->owns_orchestrator = true;

    /* Register event callbacks */
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_LTP, on_ltp_event, bridge);
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_LTD, on_ltd_event, bridge);
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_CONSOLIDATION, on_consolidation_event, bridge);
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_HOMEOSTATIC_SCALE, on_homeostatic_event, bridge);
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_ENERGY_DEPLETED, on_energy_depleted, bridge);
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_ENERGY_RESTORED, on_energy_restored, bridge);

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "mirror_plasticity") != 0) {
        plasticity_orchestrator_destroy(bridge->orchestrator);
        nimcp_free(bridge->synapses);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_plasticity_create: validation failed");
        return NULL;
    }

    bridge->state = MIRROR_PLASTICITY_STATE_IDLE;
    bridge->current_lr_modulation = 1.0f;
    bridge->last_update_us = get_time_us();
    bridge->last_consolidation_us = get_time_us();

    NIMCP_LOG_INFO(LOG_MODULE, "Created mirror-plasticity bridge (max_synapses=%u)",
                   bridge->max_synapses);
    return bridge;
}

mirror_plasticity_bridge_t* mirror_plasticity_create_with_orchestrator(
    const mirror_plasticity_config_t* config,
    plasticity_orchestrator_t* orchestrator
) {
    if (!orchestrator) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "orchestrator is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_cr", 0.0f);


    mirror_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->config = config ? *config : mirror_plasticity_config_default();

    bridge->max_synapses = MIRROR_PLASTICITY_MAX_SYNAPSES;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(mirror_plasticity_synapse_t));
    if (!bridge->synapses) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_plasticity_create_with_orchestrator: bridge->synapses is NULL");
        return NULL;
    }

    bridge->orchestrator = orchestrator;
    bridge->owns_orchestrator = false;

    /* Register callbacks */
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_LTP, on_ltp_event, bridge);
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_LTD, on_ltd_event, bridge);
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_CONSOLIDATION, on_consolidation_event, bridge);
    plasticity_orchestrator_register_event_callback(
        bridge->orchestrator, PLASTICITY_EVENT_HOMEOSTATIC_SCALE, on_homeostatic_event, bridge);

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "mirror_plasticity") != 0) {
        nimcp_free(bridge->synapses);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_plasticity_create_with_orchestrator: validation failed");
        return NULL;
    }

    bridge->state = MIRROR_PLASTICITY_STATE_IDLE;
    bridge->current_lr_modulation = 1.0f;
    bridge->last_update_us = get_time_us();

    return bridge;
}

void mirror_plasticity_destroy(mirror_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mirror_plasticity");

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_de", 0.0f);


    mirror_plasticity_disconnect_bio_async(bridge);

    if (bridge->owns_orchestrator && bridge->orchestrator) {
        plasticity_orchestrator_destroy(bridge->orchestrator);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);
    if (bridge->synapses) nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOG_INFO(LOG_MODULE, "Destroyed mirror-plasticity bridge");
}

//=============================================================================
// Synapse Management
//=============================================================================

uint32_t mirror_plasticity_register_synapse(
    mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    mirror_synapse_type_t type,
    float initial_weight
) {
    if (!bridge || bridge->num_synapses >= bridge->max_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_register_synapse: bridge is NULL or synapses full");
        return UINT32_MAX;
    }
    if (action_id >= MIRROR_PLASTICITY_MAX_ACTIONS) return UINT32_MAX;

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t id = bridge->next_synapse_id++;
    mirror_plasticity_synapse_t* s = &bridge->synapses[bridge->num_synapses++];
    init_synapse(s, id, action_id, type, initial_weight);

    bridge->action_synapse_counts[action_id]++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return id;
}

int mirror_plasticity_unregister_synapse(
    mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_un", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            uint32_t action = bridge->synapses[i].action_id;
            if (action < MIRROR_PLASTICITY_MAX_ACTIONS) {
                bridge->action_synapse_counts[action]--;
            }
            /* Shift remaining synapses */
            memmove(&bridge->synapses[i], &bridge->synapses[i + 1],
                    (bridge->num_synapses - i - 1) * sizeof(mirror_plasticity_synapse_t));
            bridge->num_synapses--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;
}

int mirror_plasticity_get_synapse(
    const mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    mirror_plasticity_synapse_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_synapse: bridge or state is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    nimcp_mutex_lock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    mirror_plasticity_synapse_t* s = find_synapse((mirror_plasticity_bridge_t*)bridge, synapse_id);
    if (s) {
        *state = *s;
        nimcp_mutex_unlock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
        return 0;
    }
    nimcp_mutex_unlock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    return -1;
}

float mirror_plasticity_get_weight(
    const mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_weight: bridge is NULL");
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    nimcp_mutex_lock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    mirror_plasticity_synapse_t* s = find_synapse((mirror_plasticity_bridge_t*)bridge, synapse_id);
    float w = s ? s->weight : NAN;
    nimcp_mutex_unlock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    return w;
}

int mirror_plasticity_get_action_weights(
    const mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float* weights,
    uint32_t max_weights
) {
    if (!bridge || !weights) {
    BRIDGE_BBB_VALIDATE(bridge, weights, sizeof(*weights));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_action_weights: bridge or weights is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    nimcp_mutex_lock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->num_synapses && count < max_weights; i++) {
        if (bridge->synapses[i].action_id == action_id) {
            weights[count++] = bridge->synapses[i].weight;
        }
    }

    nimcp_mutex_unlock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    return (int)count;
}

//=============================================================================
// Mirror --> Plasticity Pathway
//=============================================================================

/* Internal unlocked version - must hold mutex before calling */
static float pre_spike_unlocked(
    mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
) {
    if (bridge->learning_blocked) return 0.0f;

    bridge->state = MIRROR_PLASTICITY_STATE_LEARNING;

    mirror_plasticity_synapse_t* s = find_synapse(bridge, synapse_id);
    if (!s) {
        return 0.0f;
    }

    float dw = 0.0f;

    /* STDP: check for recent postsynaptic spike (LTD case) */
    if (s->last_post_spike_us > 0) {
        float delta_t_ms = (float)(timestamp_us - s->last_post_spike_us) / 1000.0f;
        /* Negative delta_t for LTD (post before pre) */
        dw = compute_stdp(bridge, -delta_t_ms, s->weight);
        if (fabsf(dw) > 1e-8f) {
            float old_w = s->weight;
            s->weight = nimcp_clampf(s->weight + dw, bridge->config.weight_min,
                               bridge->config.weight_max);
            s->ltd_count++;
            s->total_weight_change += dw;
            bridge->stats.total_ltd_events++;

            if (bridge->weight_callback) {
                bridge->weight_callback(synapse_id, s->action_id, old_w, s->weight,
                                       MIRROR_LEARN_LTD, bridge->weight_callback_data);
            }
        }
    }

    /* Update trace and timestamp */
    s->pre_trace += 1.0f;
    s->last_pre_spike_us = timestamp_us;
    s->last_update_us = timestamp_us;

    /* Update eligibility trace */
    if (bridge->config.enable_eligibility) {
        s->eligibility_trace = bridge->config.eligibility_decay * s->eligibility_trace + 1.0f;
    }

    bridge->stats.total_pre_spikes++;

    /* Notify orchestrator */
    plasticity_orchestrator_pre_spike(bridge->orchestrator, synapse_id, timestamp_us / 1000);

    bridge->state = MIRROR_PLASTICITY_STATE_IDLE;
    return dw;
}

/* Internal unlocked version - must hold mutex before calling */
static float post_spike_unlocked(
    mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
) {
    if (bridge->learning_blocked) return 0.0f;

    bridge->state = MIRROR_PLASTICITY_STATE_LEARNING;

    mirror_plasticity_synapse_t* s = find_synapse(bridge, synapse_id);
    if (!s) {
        return 0.0f;
    }

    float dw = 0.0f;

    /* STDP: check for recent presynaptic spike (LTP case) */
    if (s->last_pre_spike_us > 0) {
        float delta_t_ms = (float)(timestamp_us - s->last_pre_spike_us) / 1000.0f;
        /* Positive delta_t for LTP (pre before post) */
        dw = compute_stdp(bridge, delta_t_ms, s->weight);
        if (fabsf(dw) > 1e-8f) {
            float old_w = s->weight;
            s->weight = nimcp_clampf(s->weight + dw, bridge->config.weight_min,
                               bridge->config.weight_max);
            s->ltp_count++;
            s->total_weight_change += dw;
            bridge->stats.total_ltp_events++;

            if (bridge->weight_callback) {
                bridge->weight_callback(synapse_id, s->action_id, old_w, s->weight,
                                       MIRROR_LEARN_LTP, bridge->weight_callback_data);
            }
        }
    }

    /* Update trace and timestamp */
    s->post_trace += 1.0f;
    s->last_post_spike_us = timestamp_us;
    s->last_update_us = timestamp_us;

    /* BCM threshold update */
    if (bridge->config.enable_bcm) {
        float activity = s->post_trace;
        s->avg_activity = 0.99f * s->avg_activity + 0.01f * activity;
        s->bcm_threshold = 0.999f * s->bcm_threshold + 0.001f * s->avg_activity * s->avg_activity;
    }

    bridge->stats.total_post_spikes++;

    /* Notify orchestrator */
    plasticity_orchestrator_post_spike(bridge->orchestrator, synapse_id, timestamp_us / 1000);

    bridge->state = MIRROR_PLASTICITY_STATE_IDLE;
    return dw;
}

float mirror_plasticity_pre_spike(
    mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_pre_spike: bridge is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_pr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float dw = pre_spike_unlocked(bridge, synapse_id, timestamp_us);
    nimcp_mutex_unlock(bridge->base.mutex);
    return dw;
}

float mirror_plasticity_post_spike(
    mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_post_spike: bridge is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_po", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float dw = post_spike_unlocked(bridge, synapse_id, timestamp_us);
    nimcp_mutex_unlock(bridge->base.mutex);
    return dw;
}

int mirror_plasticity_observation(
    mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float strength,
    uint64_t timestamp_us
) {
    if (!bridge || action_id >= MIRROR_PLASTICITY_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_observation: bridge is NULL or action_id invalid");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ob", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Trigger pre-spike for all synapses of this action */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].action_id == action_id &&
            bridge->synapses[i].type == MIRROR_SYNAPSE_OBS_TO_HIDDEN) {
            /* Probabilistic spike based on strength */
            if (strength > 0.5f || nimcp_rand_uniform() < strength) {
                pre_spike_unlocked(bridge, bridge->synapses[i].synapse_id, timestamp_us);
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_plasticity_execution(
    mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float strength,
    uint64_t timestamp_us
) {
    if (!bridge || action_id >= MIRROR_PLASTICITY_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_execution: bridge is NULL or action_id invalid");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ex", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Trigger post-spike for all synapses of this action */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].action_id == action_id) {
            if (strength > 0.5f || nimcp_rand_uniform() < strength) {
                post_spike_unlocked(bridge, bridge->synapses[i].synapse_id, timestamp_us);
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Reward and Learning
//=============================================================================

int mirror_plasticity_reward(
    mirror_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
) {
    if (!bridge || bridge->learning_blocked) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_reward: bridge is NULL or learning blocked");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MIRROR_PLASTICITY_STATE_LEARNING;

    int updated = 0;
    float scaled_reward = reward * bridge->config.reward_modulation_gain;

    /* Apply reward to all eligible synapses */
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        mirror_plasticity_synapse_t* s = &bridge->synapses[i];
        if (s->eligibility_trace > 0.01f) {
            float dw = scaled_reward * s->eligibility_trace * bridge->current_lr_modulation;
            float old_w = s->weight;
            s->weight = nimcp_clampf(s->weight + dw, bridge->config.weight_min,
                               bridge->config.weight_max);
            s->total_weight_change += dw;
            updated++;

            if (bridge->weight_callback && fabsf(dw) > 1e-8f) {
                mirror_learn_event_t evt = dw > 0 ? MIRROR_LEARN_LTP : MIRROR_LEARN_LTD;
                bridge->weight_callback(s->synapse_id, s->action_id, old_w, s->weight,
                                       evt, bridge->weight_callback_data);
            }
        }
    }

    bridge->stats.total_rewards++;
    bridge->stats.avg_reward_magnitude =
        0.99f * bridge->stats.avg_reward_magnitude + 0.01f * fabsf(reward);

    /* Notify orchestrator */
    plasticity_orchestrator_reward(bridge->orchestrator, reward, timestamp_us / 1000);

    bridge->state = MIRROR_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return updated;
}

int mirror_plasticity_reward_action(
    mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float reward
) {
    if (!bridge || action_id >= MIRROR_PLASTICITY_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_reward_action: bridge is NULL or action_id invalid");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int updated = 0;
    float scaled_reward = reward * bridge->config.reward_modulation_gain;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].action_id == action_id) {
            mirror_plasticity_synapse_t* s = &bridge->synapses[i];
            if (s->eligibility_trace > 0.01f) {
                float dw = scaled_reward * s->eligibility_trace;
                s->weight = nimcp_clampf(s->weight + dw, bridge->config.weight_min,
                                   bridge->config.weight_max);
                updated++;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return updated;
}

int mirror_plasticity_consolidate(mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_consolidate: bridge is NULL");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MIRROR_PLASTICITY_STATE_CONSOLIDATING;

    int consolidated = 0;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        mirror_plasticity_synapse_t* s = &bridge->synapses[i];
        /* Consolidate synapses with significant weight changes */
        if (fabsf(s->weight - s->initial_weight) > 0.1f) {
            s->initial_weight = s->weight;  /* Lock in change */
            consolidated++;
        }
    }

    /* Fire consolidation event through orchestrator (if callback registered) */
    /* Note: orchestrator handles consolidation through its internal state machine */

    bridge->stats.consolidation_events++;
    bridge->stats.consolidated_synapses += consolidated;
    bridge->last_consolidation_us = get_time_us();

    bridge->state = MIRROR_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return consolidated;
}

//=============================================================================
// Plasticity --> Mirror Pathway
//=============================================================================

int mirror_plasticity_get_action_modulation(
    const mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float* modulation
) {
    if (!bridge || !modulation || action_id >= MIRROR_PLASTICITY_MAX_ACTIONS) {
    BRIDGE_BBB_VALIDATE(bridge, modulation, sizeof(*modulation));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_action_modulation: bridge or modulation is NULL or action_id invalid");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    nimcp_mutex_lock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);

    /* Compute average weight for action as modulation */
    float sum = 0.0f;
    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].action_id == action_id) {
            sum += bridge->synapses[i].weight;
            count++;
        }
    }

    *modulation = count > 0 ? (sum / count) : 0.5f;

    nimcp_mutex_unlock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    return 0;
}

float mirror_plasticity_get_lr_modulation(const mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_lr_modulation: bridge is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    return bridge->current_lr_modulation;
}

bool mirror_plasticity_is_learning_blocked(const mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_is_learning_blocked: bridge is NULL");
        return true;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_is", 0.0f);


    return bridge->learning_blocked;
}

//=============================================================================
// External Integration
//=============================================================================

int mirror_plasticity_connect_immune(
    mirror_plasticity_bridge_t* bridge,
    void* immune_system
) {
    if (!bridge || !immune_system) {
    BRIDGE_BBB_VALIDATE(bridge, immune_system, sizeof(*immune_system));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_connect_immune: bridge or immune_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune_system = immune_system;
    plasticity_orchestrator_connect_immune(bridge->orchestrator, immune_system);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_plasticity_connect_sleep(
    mirror_plasticity_bridge_t* bridge,
    void* sleep_system
) {
    if (!bridge || !sleep_system) {
    BRIDGE_BBB_VALIDATE(bridge, sleep_system, sizeof(*sleep_system));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_connect_sleep: bridge or sleep_system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->sleep_system = sleep_system;
    plasticity_orchestrator_connect_sleep(bridge->orchestrator, sleep_system);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_plasticity_connect_bio_async(mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_connect_bio_async: bridge is NULL");
        return 0;
    }
    if (bridge->bio_async_connected) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    int ret = plasticity_orchestrator_connect_bio_async(bridge->orchestrator);
    if (ret == 0) bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ret;
}

int mirror_plasticity_disconnect_bio_async(mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_disconnect_bio_async: bridge is NULL");
        return 0;
    }
    if (!bridge->bio_async_connected) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_di", 0.0f);


    bridge->bio_async_connected = false;
    return 0;
}

bool mirror_plasticity_is_bio_async_connected(const mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_is", 0.0f);


    return bridge->bio_async_connected;
}

//=============================================================================
// Callback Registration
//=============================================================================

int mirror_plasticity_register_weight_callback(
    mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_weight_callback_t callback,
    void* user_data
) {
    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_register_weight_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->weight_callback = callback;
    bridge->weight_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_plasticity_register_consolidation_callback(
    mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_consolidation_callback_t callback,
    void* user_data
) {
    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_register_consolidation_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->consolidation_callback = callback;
    bridge->consolidation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_plasticity_register_homeostatic_callback(
    mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_homeostatic_callback_t callback,
    void* user_data
) {
    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_register_homeostatic_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->homeostatic_callback = callback;
    bridge->homeostatic_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_plasticity_register_energy_callback(
    mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_energy_callback_t callback,
    void* user_data
) {
    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_register_energy_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->energy_callback = callback;
    bridge->energy_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query
//=============================================================================

int mirror_plasticity_get_state(
    const mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_state: bridge or state is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    nimcp_mutex_lock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);

    state->state = bridge->state;
    state->total_synapses = bridge->num_synapses;

    /* Count active synapses and compute weight stats */
    state->active_synapses = 0;
    state->mean_weight = 0.0f;
    state->min_weight = bridge->config.weight_max;
    state->max_weight = bridge->config.weight_min;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        const mirror_plasticity_synapse_t* s = &bridge->synapses[i];
        state->mean_weight += s->weight;
        if (s->weight < state->min_weight) state->min_weight = s->weight;
        if (s->weight > state->max_weight) state->max_weight = s->weight;
        if (s->last_update_us > bridge->last_update_us - 1000000) {
            state->active_synapses++;
        }
    }

    if (bridge->num_synapses > 0) {
        state->mean_weight /= bridge->num_synapses;

        /* Compute variance */
        state->weight_variance = 0.0f;
        for (uint32_t i = 0; i < bridge->num_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
                mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                                 (float)(i + 1) / (float)bridge->num_synapses);
            }

            float diff = bridge->synapses[i].weight - state->mean_weight;
            state->weight_variance += diff * diff;
        }
        state->weight_variance /= bridge->num_synapses;
    }

    state->current_learning_rate = bridge->current_lr_modulation;
    state->immune_modulation = 1.0f;  /* Would query from orchestrator */
    state->sleep_modulation = 1.0f;

    nimcp_mutex_unlock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    return 0;
}

int mirror_plasticity_get_stats(
    const mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_stats: bridge or stats is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    nimcp_mutex_lock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((mirror_plasticity_bridge_t*)bridge)->base.mutex);
    return 0;
}

void mirror_plasticity_reset_stats(mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_reset_stats: bridge is NULL");
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

float mirror_plasticity_get_atp_level(const mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_atp_level: bridge is NULL");
        return 0.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    return plasticity_orchestrator_get_atp_level(bridge->orchestrator);
}

//=============================================================================
// Update Loop
//=============================================================================

int mirror_plasticity_update(mirror_plasticity_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_time_us();
    float elapsed = (float)(now - bridge->last_update_us) / 1000.0f;

    /* Rate limiting */
    if (elapsed < bridge->config.update_interval_ms) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Decay traces */
    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus);
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        bridge->synapses[i].pre_trace *= decay;
        bridge->synapses[i].post_trace *= decay;
        if (bridge->config.enable_eligibility) {
            bridge->synapses[i].eligibility_trace *= bridge->config.eligibility_decay;
        }
    }

    /* Update orchestrator */
    plasticity_orchestrator_update(bridge->orchestrator, (uint64_t)dt_ms);

    /* Check for consolidation */
    float since_consolidation = (float)(now - bridge->last_consolidation_us) / 1000.0f;
    if (since_consolidation >= bridge->config.consolidation_interval_ms) {
        mirror_plasticity_consolidate(bridge);
    }

    /* Update learning rate modulation from ATP level */
    float atp = plasticity_orchestrator_get_atp_level(bridge->orchestrator);
    bridge->current_lr_modulation = atp > 0.3f ? 1.0f : (atp / 0.3f);
    bridge->learning_blocked = atp < 0.1f;

    bridge->last_update_us = now;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int mirror_plasticity_reset(mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            mirror_plasticity_bridge_heartbeat("mirror_plast_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        mirror_plasticity_synapse_t* s = &bridge->synapses[i];
        s->weight = s->initial_weight;
        s->pre_trace = 0.0f;
        s->post_trace = 0.0f;
        s->eligibility_trace = 0.0f;
        s->ltp_count = 0;
        s->ltd_count = 0;
        s->total_weight_change = 0.0f;
    }

    bridge->state = MIRROR_PLASTICITY_STATE_IDLE;
    bridge->current_lr_modulation = 1.0f;
    bridge->learning_blocked = false;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Direct Access
//=============================================================================

plasticity_orchestrator_t* mirror_plasticity_get_orchestrator(
    mirror_plasticity_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_orchestrator: bridge is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    return bridge->orchestrator;
}

int mirror_plasticity_get_orchestrator_stats(
    const mirror_plasticity_bridge_t* bridge,
    plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_plasticity_get_orchestrator_stats: bridge or stats is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_plasticity_bridge_heartbeat("mirror_plast_mirror_plasticity_ge", 0.0f);


    return plasticity_orchestrator_get_stats(bridge->orchestrator, stats);
}

//=============================================================================
// Instance Health Agent Setter (B22 Upgrade)
//=============================================================================

void mirror_plasticity_bridge_set_instance_health_agent(
    mirror_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B22 Upgrade)
//=============================================================================

int mirror_plasticity_bridge_training_begin(mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_plasticity_bridge_heartbeat_instance(bridge->health_agent, "mirror_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_plasticity_bridge_training_end(mirror_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_plasticity_bridge_heartbeat_instance(bridge->health_agent, "mirror_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int mirror_plasticity_bridge_training_step(mirror_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_plasticity_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "mirror_plasticity_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "mirror_plasticity_bridge_training_step");
    mirror_plasticity_bridge_heartbeat_instance(bridge->health_agent, "mirror_plasticity_bridge_training_step", progress);
    return 0;
}
