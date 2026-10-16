/**
 * @file nimcp_genius_plasticity_bridge.c
 * @brief Mathematical Genius - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-24
 */

#include "cognitive/parietal/nimcp_genius_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/nimcp_kg_hierarchy.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(genius_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from genius_plasticity_bridge module (instance-level) */
static inline void genius_plasticity_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_genius_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_genius_plasticity_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_genius_plasticity_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "GENIUS_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct genius_plasticity_bridge {
    bridge_base_t base;
    genius_plasticity_config_t config;
    struct mathematical_genius* genius;

    /* State */
    genius_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Heartbeat tracking (Phase 8) */
    uint64_t last_heartbeat_us;

    /* Synapse management */
    genius_plasticity_synapse_t* synapses;
    uint32_t synapse_count;
    uint32_t synapse_capacity;

    /* Learning state */
    genius_learning_state_t learning_state;

    /* Mode-specific skill tracking */
    float mode_skills[GENIUS_MODE_COUNT];

    /* Callbacks */
    genius_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    genius_plasticity_skill_callback_t skill_callback;
    void* skill_callback_data;
    genius_plasticity_breakthrough_callback_t breakthrough_callback;
    void* breakthrough_callback_data;

    /* Statistics */
    genius_plasticity_stats_t stats;

    /* KG Wiring */
    struct kg_module_wiring* kg_wiring;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(genius_plasticity_bridge)

//=============================================================================
// Helper Functions
//=============================================================================

static genius_plasticity_synapse_t* find_synapse(genius_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static float compute_stdp_weight_change(const genius_plasticity_config_t* config,
                                        float pre_time, float post_time) {
    float dt = post_time - pre_time;
    float dw = 0.0f;

    if (dt > 0.0f) {
        /* LTP: post after pre */
        dw = config->stdp_a_plus * expf(-dt / config->stdp_tau_plus_ms);
    } else {
        /* LTD: pre after post */
        dw = -config->stdp_a_minus * expf(dt / config->stdp_tau_minus_ms);
    }

    return dw;
}

static void update_mode_skill(genius_plasticity_bridge_t* bridge, genius_mode_t mode, float delta) {
    if (mode >= GENIUS_MODE_COUNT) return;

    float old_skill = bridge->mode_skills[mode];
    bridge->mode_skills[mode] = nimcp_myelin_clamp(old_skill + delta, 0.0f, 1.0f);

    if (bridge->skill_callback && fabsf(delta) > 0.01f) {
        bridge->skill_callback(bridge, mode, old_skill, bridge->mode_skills[mode],
                              bridge->skill_callback_data);
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

genius_plasticity_config_t genius_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_co", 0.0f);


    genius_plasticity_config_t config = {
        .base_learning_rate = GENIUS_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = 0.0105f,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 0.3f,

        .homeostatic_tau_ms = 10000.0f,
        .target_insight_rate = 0.1f,

        .proof_success_boost = 0.1f,
        .pattern_found_boost = 0.05f,
        .insight_modulation = 0.08f,
        .breakthrough_boost = 0.2f,
        .elegance_bonus = 0.03f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_pattern_recognition = true,
        .protect_intuition = true,
        .protection_strength = 0.9f,

        .gauss_learning_rate = 0.005f,
        .newton_learning_rate = 0.005f,
        .erdos_learning_rate = 0.005f,

        .max_synapses = GENIUS_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

genius_plasticity_bridge_t* genius_plasticity_create(const genius_plasticity_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_cr", 0.0f);


    genius_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(genius_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge allocation failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = genius_plasticity_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "genius_plasticity") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "bridge_base_init failed for genius_plasticity");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate synapse array */
    bridge->synapse_capacity = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->synapse_capacity, sizeof(genius_plasticity_synapse_t));
    if (!bridge->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "synapse array allocation failed for genius_plasticity_bridge");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize learning state */
    bridge->learning_state.pattern_sensitivity = 0.5f;
    bridge->learning_state.proof_skill = 0.3f;
    bridge->learning_state.conjecture_quality = 0.3f;
    bridge->learning_state.analogy_strength = 0.3f;
    bridge->learning_state.insight_frequency = 0.1f;
    bridge->learning_state.elegance_perception = 0.5f;
    bridge->learning_state.learning_rate_mod = 1.0f;
    bridge->learning_state.strongest_mode = GENIUS_MODE_ADAPTIVE;

    /* Initialize mode skills */
    for (int i = 0; i < GENIUS_MODE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GENIUS_MODE_COUNT > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)GENIUS_MODE_COUNT);
        }

        bridge->mode_skills[i] = 0.3f;
    }

    /* Initialize state */
    bridge->state = GENIUS_PLASTICITY_STATE_IDLE;
    bridge->synapse_count = 0;
    bridge->current_time_us = 0;

    memset(&bridge->stats, 0, sizeof(genius_plasticity_stats_t));

    /* Initialize KG wiring */
    bridge->kg_wiring = genius_plasticity_create_kg_wiring();

    NIMCP_LOGGING_INFO("Created %s bridge", "genius_plasticity");
    return bridge;
}

void genius_plasticity_destroy(genius_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "genius_plasticity");

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_de", 0.0f);


    if (bridge->synapses) nimcp_free(bridge->synapses);

    /* Destroy KG wiring */
    if (bridge->kg_wiring) {
        kg_module_wiring_destroy(bridge->kg_wiring);
        bridge->kg_wiring = NULL;
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int genius_plasticity_reset(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        bridge->synapses[i].weight = bridge->synapses[i].initial_weight;
        bridge->synapses[i].eligibility_trace = 0.0f;
        bridge->synapses[i].bcm_threshold = bridge->config.bcm_target_rate;
    }

    /* Reset learning state */
    bridge->learning_state.pattern_sensitivity = 0.5f;
    bridge->learning_state.proof_skill = 0.3f;
    bridge->learning_state.learning_rate_mod = 1.0f;

    bridge->state = GENIUS_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_link_genius(genius_plasticity_bridge_t* bridge, struct mathematical_genius* genius) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_link_genius: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_li", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->genius = genius;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int genius_plasticity_register_synapse(genius_plasticity_bridge_t* bridge,
                                       uint32_t synapse_id,
                                       genius_synapse_type_t type,
                                       float initial_weight,
                                       genius_mode_t mode) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_link_genius: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->synapse_count >= bridge->synapse_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "genius_plasticity_link_genius: capacity exceeded");
        return -1;
    }

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id) != NULL) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "genius_plasticity_link_genius: validation failed");
        return -1;
    }

    /* Add synapse */
    genius_plasticity_synapse_t* syn = &bridge->synapses[bridge->synapse_count++];
    syn->synapse_id = synapse_id;
    syn->type = type;
    syn->weight = nimcp_myelin_clamp(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    syn->initial_weight = syn->weight;
    syn->eligibility_trace = 0.0f;
    syn->bcm_threshold = bridge->config.bcm_target_rate;
    syn->avg_activity = 0.0f;
    syn->last_update_us = 0;
    syn->update_count = 0;
    syn->associated_mode = mode;

    /* Apply protection based on type */
    syn->is_protected = false;
    if (type == GENIUS_SYNAPSE_PATTERN_RECOGNITION && bridge->config.protect_pattern_recognition) {
        syn->is_protected = true;
    }
    if (type == GENIUS_SYNAPSE_INTUITION && bridge->config.protect_intuition) {
        syn->is_protected = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_unregister_synapse(genius_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_un", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            /* Swap with last and decrement count */
            bridge->synapses[i] = bridge->synapses[--bridge->synapse_count];
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* Synapse not found — normal condition */
}

int genius_plasticity_get_synapse(genius_plasticity_bridge_t* bridge,
                                  uint32_t synapse_id,
                                  genius_plasticity_synapse_t* synapse) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_unregister_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ge", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, synapse, sizeof(*synapse));

    nimcp_mutex_lock(bridge->base.mutex);

    genius_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (syn) {
        *synapse = *syn;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* Synapse not found — normal condition */
}

int genius_plasticity_protect_synapse(genius_plasticity_bridge_t* bridge,
                                      uint32_t synapse_id,
                                      bool protect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_pr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    genius_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (syn) {
        syn->is_protected = protect;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* Synapse not found — normal condition */
}

//=============================================================================
// Learning Functions
//=============================================================================

int genius_plasticity_learn(genius_plasticity_bridge_t* bridge,
                            genius_learn_event_t event,
                            float magnitude,
                            uint32_t synapse_id,
                            float context) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_le", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_PLASTICITY_STATE_LEARNING;

    genius_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    magnitude = nimcp_myelin_clamp(magnitude, 0.0f, 1.0f);
    float lr = bridge->config.base_learning_rate * bridge->learning_state.learning_rate_mod;
    float dw = 0.0f;

    /* Check protection */
    if (syn->is_protected) {
        lr *= (1.0f - bridge->config.protection_strength);
        bridge->stats.protected_updates_blocked++;
    }

    /* Apply event-specific learning */
    switch (event) {
        case GENIUS_LEARN_PROOF_SUCCESS:
            dw = lr * magnitude * bridge->config.proof_success_boost;
            bridge->learning_state.proof_skill += 0.01f * magnitude;
            bridge->stats.proof_success_events++;
            update_mode_skill(bridge, syn->associated_mode, 0.02f * magnitude);
            break;

        case GENIUS_LEARN_PROOF_FAILURE:
            dw = -lr * magnitude * 0.5f * bridge->config.proof_success_boost;
            bridge->stats.proof_failure_events++;
            break;

        case GENIUS_LEARN_PATTERN_FOUND:
            dw = lr * magnitude * bridge->config.pattern_found_boost;
            bridge->learning_state.pattern_sensitivity += 0.01f * magnitude;
            bridge->stats.pattern_found_events++;
            break;

        case GENIUS_LEARN_PATTERN_MISSED:
            dw = -lr * magnitude * 0.3f * bridge->config.pattern_found_boost;
            break;

        case GENIUS_LEARN_CONJECTURE_VERIFIED:
            dw = lr * magnitude * bridge->config.insight_modulation;
            bridge->learning_state.conjecture_quality += 0.01f * magnitude;
            break;

        case GENIUS_LEARN_CONJECTURE_REFUTED:
            dw = -lr * magnitude * 0.5f * bridge->config.insight_modulation;
            break;

        case GENIUS_LEARN_INSIGHT_EMERGED:
            dw = lr * magnitude * bridge->config.insight_modulation;
            bridge->learning_state.insight_frequency += 0.01f * magnitude;
            bridge->stats.insight_events++;
            break;

        case GENIUS_LEARN_ANALOGY_FOUND:
            dw = lr * magnitude * bridge->config.insight_modulation;
            bridge->learning_state.analogy_strength += 0.01f * magnitude;
            break;

        case GENIUS_LEARN_MODE_EFFECTIVE:
            update_mode_skill(bridge, syn->associated_mode, 0.03f * magnitude);
            break;

        case GENIUS_LEARN_MODE_INEFFECTIVE:
            update_mode_skill(bridge, syn->associated_mode, -0.01f * magnitude);
            break;

        case GENIUS_LEARN_ELEGANCE_ACHIEVED:
            dw = lr * magnitude * bridge->config.elegance_bonus;
            bridge->learning_state.elegance_perception += 0.01f * magnitude;
            break;

        case GENIUS_LEARN_BREAKTHROUGH:
            dw = lr * magnitude * bridge->config.breakthrough_boost;
            bridge->learning_state.breakthroughs_achieved++;
            bridge->stats.breakthrough_events++;
            update_mode_skill(bridge, syn->associated_mode, 0.05f * magnitude);

            if (bridge->breakthrough_callback) {
                bridge->breakthrough_callback(bridge, "Mathematical breakthrough achieved",
                                             bridge->breakthrough_callback_data);
            }
            break;
    }

    /* Apply weight change with eligibility trace */
    dw += syn->eligibility_trace * context;
    syn->weight = nimcp_myelin_clamp(syn->weight + dw, bridge->config.weight_min, bridge->config.weight_max);
    syn->update_count++;
    syn->last_update_us = bridge->current_time_us;

    /* Update statistics */
    bridge->stats.total_learning_events++;
    bridge->stats.weight_updates++;
    bridge->stats.mean_weight_change = 0.99f * bridge->stats.mean_weight_change + 0.01f * fabsf(dw);

    if (dw > 0) {
        bridge->stats.total_potentiation += dw;
    } else {
        bridge->stats.total_depression += fabsf(dw);
    }

    /* Invoke callback outside mutex to prevent deadlock */
    genius_plasticity_learn_callback_t cb = bridge->learn_callback;
    void* cb_data = bridge->learn_callback_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        cb(bridge, event, magnitude, cb_data);
    }

    return 0;
}

float genius_plasticity_apply_stdp(genius_plasticity_bridge_t* bridge,
                                   uint32_t synapse_id,
                                   float pre_time,
                                   float post_time) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ap", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    genius_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NAN;
    }

    float dw = compute_stdp_weight_change(&bridge->config, pre_time, post_time);

    /* Apply protection */
    if (syn->is_protected) {
        dw *= (1.0f - bridge->config.protection_strength);
    }

    /* Apply mode-specific learning rate */
    float mode_lr = bridge->config.base_learning_rate;
    switch (syn->associated_mode) {
        case GENIUS_MODE_GAUSS:
            mode_lr = bridge->config.gauss_learning_rate;
            break;
        case GENIUS_MODE_NEWTON:
            mode_lr = bridge->config.newton_learning_rate;
            break;
        case GENIUS_MODE_ERDOS:
            mode_lr = bridge->config.erdos_learning_rate;
            break;
        default:
            break;
    }

    dw *= mode_lr;
    syn->weight = nimcp_myelin_clamp(syn->weight + dw, bridge->config.weight_min, bridge->config.weight_max);
    syn->update_count++;
    syn->last_update_us = bridge->current_time_us;

    bridge->stats.weight_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return dw;
}

int genius_plasticity_apply_proof_reward(genius_plasticity_bridge_t* bridge,
                                         float reward,
                                         float elegance) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ap", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_myelin_clamp(reward, -1.0f, 1.0f);
    elegance = nimcp_myelin_clamp(elegance, 0.0f, 1.0f);

    float modulation = reward * bridge->config.proof_success_boost +
                       elegance * bridge->config.elegance_bonus;

    /* Apply to all synapses with positive eligibility traces */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        genius_plasticity_synapse_t* syn = &bridge->synapses[i];
        if (syn->eligibility_trace > 0.01f) {
            float dw = modulation * syn->eligibility_trace;

            if (syn->is_protected) {
                dw *= (1.0f - bridge->config.protection_strength);
            }

            syn->weight = nimcp_myelin_clamp(syn->weight + dw, bridge->config.weight_min, bridge->config.weight_max);
            syn->update_count++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_apply_insight_reward(genius_plasticity_bridge_t* bridge,
                                           float insight_strength,
                                           genius_mode_t mode) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ap", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    insight_strength = nimcp_myelin_clamp(insight_strength, 0.0f, 1.0f);
    float modulation = insight_strength * bridge->config.insight_modulation;

    /* Apply to synapses associated with the mode */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        genius_plasticity_synapse_t* syn = &bridge->synapses[i];
        if (syn->associated_mode == mode && syn->eligibility_trace > 0.01f) {
            float dw = modulation * syn->eligibility_trace;

            if (syn->is_protected) {
                dw *= (1.0f - bridge->config.protection_strength);
            }

            syn->weight = nimcp_myelin_clamp(syn->weight + dw, bridge->config.weight_min, bridge->config.weight_max);
            syn->update_count++;
        }
    }

    update_mode_skill(bridge, mode, 0.02f * insight_strength);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_update_bcm(genius_plasticity_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_update_bcm: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float tau = bridge->config.bcm_tau_ms;
    float target = bridge->config.bcm_target_rate;

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        genius_plasticity_synapse_t* syn = &bridge->synapses[i];
        syn->bcm_threshold += dt_ms * (syn->avg_activity * syn->avg_activity - syn->bcm_threshold) / tau;
        syn->bcm_threshold = nimcp_myelin_clamp(syn->bcm_threshold, 0.01f, 1.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_homeostatic_update(genius_plasticity_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ho", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "genius_plasticity_homeostatic_update");
    BRIDGE_LGSS_GATE(bridge, "genius_plasticity_homeostatic_update");

    nimcp_mutex_lock(bridge->base.mutex);

    float tau = bridge->config.homeostatic_tau_ms;
    float target = bridge->config.target_insight_rate;

    /* Compute mean weight */
    float mean_weight = 0.0f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        mean_weight += bridge->synapses[i].weight;
    }
    if (bridge->synapse_count > 0) {
        mean_weight /= (float)bridge->synapse_count;
    }

    /* Scale weights towards target */
    float scale_factor = 1.0f + dt_ms * (0.5f - mean_weight) / tau;

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        genius_plasticity_synapse_t* syn = &bridge->synapses[i];
        if (!syn->is_protected) {
            syn->weight *= scale_factor;
            syn->weight = nimcp_myelin_clamp(syn->weight, bridge->config.weight_min, bridge->config.weight_max);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int genius_plasticity_update_traces(genius_plasticity_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / 100.0f); /* 100ms trace decay */

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        bridge->synapses[i].eligibility_trace *= decay;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_consolidate(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_PLASTICITY_STATE_CONSOLIDATING;

    /* Consolidate strong weights, weaken weak weights */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        genius_plasticity_synapse_t* syn = &bridge->synapses[i];
        if (!syn->is_protected) {
            if (syn->weight > 0.7f) {
                syn->weight = fminf(syn->weight * 1.05f, bridge->config.weight_max);
            } else if (syn->weight < 0.3f) {
                syn->weight *= 0.95f;
            }
        }
        /* Reset eligibility traces */
        syn->eligibility_trace = 0.0f;
    }

    bridge->state = GENIUS_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int genius_plasticity_get_learning_state(genius_plasticity_bridge_t* bridge,
                                         genius_learning_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_consolidate: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ge", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);

    *state = bridge->learning_state;

    /* Clamp values */
    state->pattern_sensitivity = nimcp_myelin_clamp(state->pattern_sensitivity, 0.0f, 1.0f);
    state->proof_skill = nimcp_myelin_clamp(state->proof_skill, 0.0f, 1.0f);
    state->conjecture_quality = nimcp_myelin_clamp(state->conjecture_quality, 0.0f, 1.0f);
    state->analogy_strength = nimcp_myelin_clamp(state->analogy_strength, 0.0f, 1.0f);
    state->insight_frequency = nimcp_myelin_clamp(state->insight_frequency, 0.0f, 1.0f);
    state->elegance_perception = nimcp_myelin_clamp(state->elegance_perception, 0.0f, 1.0f);

    /* Find strongest mode */
    float max_skill = bridge->mode_skills[0];
    state->strongest_mode = GENIUS_MODE_GAUSS;
    for (int i = 1; i < GENIUS_MODE_COUNT; i++) {
        if (bridge->mode_skills[i] > max_skill) {
            max_skill = bridge->mode_skills[i];
            state->strongest_mode = (genius_mode_t)i;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float genius_plasticity_get_mode_skill(genius_plasticity_bridge_t* bridge, genius_mode_t mode) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_plasticity_get_mode_skill: bridge is NULL");
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ge", 0.0f);


    if (mode >= GENIUS_MODE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_plasticity_get_mode_skill: invalid mode %d (max: %d)",
            (int)mode, GENIUS_MODE_COUNT - 1);
        return -1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float skill = bridge->mode_skills[mode];
    nimcp_mutex_unlock(bridge->base.mutex);
    return skill;
}

int genius_plasticity_get_state(genius_plasticity_bridge_t* bridge,
                                genius_plasticity_bridge_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_get_mode_skill: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ge", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->synapse_count;

    /* Compute mean weight and variance */
    float sum = 0.0f, sum_sq = 0.0f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        sum += bridge->synapses[i].weight;
        sum_sq += bridge->synapses[i].weight * bridge->synapses[i].weight;
    }

    if (bridge->synapse_count > 0) {
        state->mean_weight = sum / (float)bridge->synapse_count;
        state->weight_variance = sum_sq / (float)bridge->synapse_count -
                                 state->mean_weight * state->mean_weight;
    } else {
        state->mean_weight = 0.0f;
        state->weight_variance = 0.0f;
    }

    state->learning_rate_effective = bridge->config.base_learning_rate *
                                     bridge->learning_state.learning_rate_mod;
    state->recent_insight_rate = bridge->learning_state.insight_frequency;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_get_stats(genius_plasticity_bridge_t* bridge,
                                genius_plasticity_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_get_mode_skill: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ge", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;

    /* Copy mode skills */
    for (int i = 0; i < GENIUS_MODE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GENIUS_MODE_COUNT > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)GENIUS_MODE_COUNT);
        }

        stats->mode_skill[i] = bridge->mode_skills[i];
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_reset_stats(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(genius_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int genius_plasticity_register_learn_callback(genius_plasticity_bridge_t* bridge,
                                              genius_plasticity_learn_callback_t callback,
                                              void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_re", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_register_skill_callback(genius_plasticity_bridge_t* bridge,
                                              genius_plasticity_skill_callback_t callback,
                                              void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_re", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->skill_callback = callback;
    bridge->skill_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_register_breakthrough_callback(genius_plasticity_bridge_t* bridge,
                                                     genius_plasticity_breakthrough_callback_t callback,
                                                     void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_re", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->breakthrough_callback = callback;
    bridge->breakthrough_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int genius_plasticity_bio_async_connect(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_bio_async_disconnect(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool genius_plasticity_is_bio_async_connected(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_is", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);
    return connected;
}

//=============================================================================
// Heartbeat and State Serialization (Phase 8)
//=============================================================================

int genius_plasticity_send_heartbeat(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_plasticity_send_heartbeat: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->last_heartbeat_us = nimcp_time_get_us();
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint64_t genius_plasticity_get_last_heartbeat(const genius_plasticity_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Cast away const for mutex lock; the read is logically const */
    genius_plasticity_bridge_t* nc = (genius_plasticity_bridge_t*)bridge;
    nimcp_mutex_lock(nc->base.mutex);
    uint64_t last_hb = nc->last_heartbeat_us;
    nimcp_mutex_unlock(nc->base.mutex);
    return last_hb;
}

bool genius_plasticity_is_heartbeat_stale(const genius_plasticity_bridge_t* bridge,
                                           uint32_t timeout_ms) {
    if (!bridge) return true;

    uint64_t last_hb = genius_plasticity_get_last_heartbeat(bridge);
    if (last_hb == 0) return true;

    uint64_t now_us = nimcp_time_get_us();
    uint64_t elapsed_us = now_us - last_hb;
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

    return elapsed_us > timeout_us;
}

int genius_plasticity_serialize_state(genius_plasticity_bridge_t* bridge,
                                       genius_plasticity_serialized_t* serialized) {
    if (!bridge || !serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_plasticity_serialize_state: bridge or serialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_se", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, serialized, sizeof(*serialized));

    nimcp_mutex_lock(bridge->base.mutex);

    memset(serialized, 0, sizeof(*serialized));
    serialized->version = 1;
    serialized->num_synapses = bridge->synapse_count;
    serialized->timestamp_us = nimcp_time_get_us();

    /* Capture bridge state */
    serialized->state.state = bridge->state;
    serialized->state.active_synapses = bridge->synapse_count;

    /* Compute mean weight */
    float sum_weight = 0.0f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        sum_weight += bridge->synapses[i].weight;
    }
    serialized->state.mean_weight = (bridge->synapse_count > 0)
        ? (sum_weight / (float)bridge->synapse_count) : 0.0f;

    /* Copy learning state */
    memcpy(&serialized->learning, &bridge->learning_state, sizeof(genius_learning_state_t));

    /* Copy statistics */
    memcpy(&serialized->stats, &bridge->stats, sizeof(genius_plasticity_stats_t));

    /* Compute checksum */
    serialized->checksum = genius_plasticity_compute_checksum(serialized);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_plasticity_deserialize_state(genius_plasticity_bridge_t* bridge,
                                         const genius_plasticity_serialized_t* serialized) {
    if (!bridge || !serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_plasticity_deserialize_state: bridge or serialized is NULL");
        return -1;
    }

    /* Verify checksum */
    if (!genius_plasticity_verify_checksum(serialized)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_plasticity_deserialize_state: checksum verification failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_de", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, serialized, sizeof(*serialized));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Restore state */
    bridge->state = serialized->state.state;

    /* Restore learning state */
    memcpy(&bridge->learning_state, &serialized->learning, sizeof(genius_learning_state_t));

    /* Restore statistics */
    memcpy(&bridge->stats, &serialized->stats, sizeof(genius_plasticity_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

uint32_t genius_plasticity_compute_checksum(const genius_plasticity_serialized_t* serialized) {
    if (!serialized) return 0;

    /* Simple FNV-1a hash over relevant fields */
    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_co", 0.0f);


    uint32_t hash = 2166136261u;
    const uint8_t* data = (const uint8_t*)serialized;
    size_t len = offsetof(genius_plasticity_serialized_t, checksum);

    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            genius_plasticity_bridge_heartbeat("genius_plast_loop",
                             (float)(i + 1) / (float)len);
        }

        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}

bool genius_plasticity_verify_checksum(const genius_plasticity_serialized_t* serialized) {
    if (!serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_verify_checksum: serialized is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ve", 0.0f);


    uint32_t computed = genius_plasticity_compute_checksum(serialized);
    return computed == serialized->checksum;
}

//=============================================================================
// KG Wiring Integration
//=============================================================================

kg_module_wiring_t* genius_plasticity_create_kg_wiring(void) {
    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_cr", 0.0f);


    kg_module_wiring_t* wiring = kg_module_wiring_create(
        KG_GENIUS_PLASTICITY_MODULE_NAME,
        KG_GENIUS_PLASTICITY_MODULE_TYPE
    );
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wiring is NULL");
        return NULL;
    }

    /* Set hierarchical placement - parietal cortex layer V */
    wiring->target_layer = KG_LAYER_V;
    wiring->hemisphere_affinity = KG_HEMISPHERE_BILATERAL;

    /* Set metadata */
    kg_module_wiring_set_metadata(wiring,
        "NIMCP Team",
        "MATHEMATICAL_LEARNING",
        "Mathematical genius plasticity and learning bridge"
    );
    kg_module_wiring_set_version(wiring, 1, 0, 0);

    /* Register inputs from proof system and reward systems */
    kg_module_wiring_add_input(wiring, "proof_system", KG_MSG_PLASTICITY_PROOF_RESULT, true);
    kg_module_wiring_add_input(wiring, "genius_snn_bridge", KG_MSG_PLASTICITY_INSIGHT_EVENT, false);
    kg_module_wiring_add_input(wiring, "reward_system", KG_MSG_PLASTICITY_REWARD_SIGNAL, false);
    kg_module_wiring_add_input(wiring, "mode_controller", KG_MSG_PLASTICITY_MODE_FEEDBACK, false);

    /* Register outputs for weight updates and skill changes */
    kg_module_wiring_add_output(wiring, KG_MSG_PLASTICITY_WEIGHT_UPDATE, "Synaptic weight update for mathematical concepts");
    kg_module_wiring_add_output(wiring, KG_MSG_PLASTICITY_SKILL_CHANGE, "Mathematical skill level change notification");
    kg_module_wiring_add_output(wiring, KG_MSG_PLASTICITY_CONSOLIDATION, "Mathematical knowledge consolidation event");
    kg_module_wiring_add_output(wiring, KG_MSG_PLASTICITY_HOMEOSTASIS, "Homeostatic plasticity adjustment event");

    /* Register message handlers */
    kg_module_wiring_add_handler(wiring, KG_MSG_PLASTICITY_LEARN_EVENT, 100);
    kg_module_wiring_add_handler(wiring, KG_MSG_PLASTICITY_STDP_UPDATE, 150);
    kg_module_wiring_add_handler(wiring, KG_MSG_PLASTICITY_BCM_UPDATE, 150);

    /* Set network type to SNN (STDP-based plasticity) */
    wiring->network_type = KG_WEIGHT_SNN;

    /* Add custom metadata */
    kg_module_wiring_add_metadata_entry(wiring, "brain_region", "parietal_angular_gyrus");
    kg_module_wiring_add_metadata_entry(wiring, "plasticity_types", "stdp,bcm,reward_modulated");
    kg_module_wiring_add_metadata_entry(wiring, "protected_synapses", "pattern_recognition,intuition");

    return wiring;
}

kg_module_wiring_t* genius_plasticity_get_kg_wiring(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_plasticity_get_kg_wiring: bridge is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    genius_plasticity_bridge_heartbeat("genius_plast_genius_plasticity_ge", 0.0f);


    return bridge->kg_wiring;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void genius_plasticity_bridge_set_instance_health_agent(
    genius_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int genius_plasticity_bridge_training_begin(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    genius_plasticity_bridge_heartbeat_instance(bridge->health_agent, "genius_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int genius_plasticity_bridge_training_end(genius_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    genius_plasticity_bridge_heartbeat_instance(bridge->health_agent, "genius_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int genius_plasticity_bridge_training_step(genius_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_plasticity_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "genius_plasticity_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "genius_plasticity_bridge_training_step");
    genius_plasticity_bridge_heartbeat_instance(bridge->health_agent, "genius_plasticity_bridge_training_step", progress);
    return 0;
}
