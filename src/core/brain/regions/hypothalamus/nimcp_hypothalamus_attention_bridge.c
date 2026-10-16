/**
 * @file nimcp_hypothalamus_attention_bridge.c
 * @brief Implementation of Hypothalamus -> Attention Bridge
 *
 * WHAT: Bridge between hypothalamus drives and attention/salience systems
 * WHY:  Drive states must bias attention (hungry = food salient, threatened = threats salient)
 * HOW:  Maps drive urgencies to salience modulation weights, modulates attention gate
 *
 * @version Phase 7: Attention Bridge
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_attention_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_attention_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_ATTENTION_BRIDGE"


/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define HYPO_ATTN_BRIDGE_MODULE_ID  0x1170

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Compute category boost from all drives
 */
static float compute_category_boost(
    hypo_attn_bridge_t* bridge,
    hypo_salience_category_t category) {

    if (!bridge || !bridge->drives) {
        return 0.0f;
    }

    float total_boost = 0.0f;

    /* Sum contributions from all drives */
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        float mapping = bridge->config.drive_category_map[d][category];
        if (mapping > 0.0f) {
            /* Get drive urgency */
            hypo_drive_state_t state;
            if (hypo_drive_get_state(bridge->drives, (hypo_drive_type_t)d, &state)) {
                float urgency = state.urgency;

                /* Only apply boost if urgency exceeds threshold */
                if (urgency > bridge->config.urgency_threshold) {
                    float boost = (urgency - bridge->config.urgency_threshold) *
                                  mapping * bridge->config.boost_scale;
                    total_boost += boost;
                }
            }
        }
    }

    /* Clamp to max boost */
    if (total_boost > bridge->config.max_boost - 1.0f) {
        total_boost = bridge->config.max_boost - 1.0f;
    }

    return total_boost;
}

/**
 * @brief Find dominant drive
 */
static void find_dominant_drive(
    hypo_attn_bridge_t* bridge,
    hypo_drive_type_t* drive_out,
    float* urgency_out) {

    hypo_drive_type_t dominant = HYPO_DRIVE_SAFETY;  /* Default to safety */
    float max_urgency = 0.0f;

    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        hypo_drive_state_t state;
        if (hypo_drive_get_state(bridge->drives, (hypo_drive_type_t)d, &state)) {
            if (state.urgency > max_urgency) {
                max_urgency = state.urgency;
                dominant = (hypo_drive_type_t)d;
            }
        }
    }

    if (drive_out) *drive_out = dominant;
    if (urgency_out) *urgency_out = max_urgency;
}

/**
 * @brief Apply modulation to a salience value
 */
static float apply_modulation(
    hypo_attn_bridge_t* bridge,
    float base_salience,
    float boost) {

    float result;

    switch (bridge->config.mode) {
        case HYPO_ATTN_MODE_MULTIPLICATIVE:
            result = base_salience * (1.0f + boost);
            break;

        case HYPO_ATTN_MODE_ADDITIVE:
            result = base_salience + boost * 0.5f;  /* Scale additive */
            break;

        case HYPO_ATTN_MODE_GATING:
            /* Binary: pass if boost > 0.5, otherwise suppress */
            result = (boost > 0.5f) ? base_salience : base_salience * 0.1f;
            break;

        case HYPO_ATTN_MODE_MIXED:
        default:
            /* Combination: multiplicative with soft gating */
            result = base_salience * (1.0f + boost * 0.5f) + boost * 0.25f;
            break;
    }

    /* Clamp to [0, max_boost] */
    if (result < 0.0f) result = 0.0f;
    if (result > bridge->config.max_boost) result = bridge->config.max_boost;

    return result;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *===========================================================================*/

/**
 * @brief Handle drive state message
 */
static nimcp_error_t attn_handle_drive_state(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* ctx) {
    (void)msg;
    (void)msg_size;
    (void)promise;
    hypo_attn_bridge_t* bridge = (hypo_attn_bridge_t*)ctx;
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge context is NULL");

    /* Drive state changed, update modulation */
    hypo_attn_bridge_update_modulation(bridge);

    /* Push to attention gate if connected */
    if (bridge->attention_gate) {
        hypo_attn_bridge_push_to_gate(bridge);
    }

    /* Broadcast if enabled */
    if (bridge->config.broadcast_enabled) {
        hypo_attn_bridge_broadcast_modulation(bridge);
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_attn_bridge_config_t hypo_attn_bridge_default_config(void) {
    hypo_attn_bridge_config_t config = {0};

    /* Modulation parameters */
    config.mode = HYPO_ATTN_MODE_MULTIPLICATIVE;
    config.urgency_threshold = HYPO_ATTN_URGENCY_THRESHOLD;
    config.max_boost = HYPO_ATTN_MAX_BOOST;
    config.boost_scale = HYPO_ATTN_DEFAULT_BOOST;

    /* Category base weights (default equal) */
    for (int c = 0; c < HYPO_SAL_CAT_COUNT; c++) {
        config.category_base_weights[c] = 1.0f;
    }

    /* Drive-to-category mapping defaults */
    /* Clear all first */
    memset(config.drive_category_map, 0, sizeof(config.drive_category_map));

    /* HUNGER -> FOOD (strong) */
    config.drive_category_map[HYPO_DRIVE_HUNGER][HYPO_SAL_CAT_FOOD] = 2.0f;

    /* THIRST -> WATER (strong) */
    config.drive_category_map[HYPO_DRIVE_THIRST][HYPO_SAL_CAT_WATER] = 2.0f;

    /* TEMPERATURE -> TEMPERATURE (strong) */
    config.drive_category_map[HYPO_DRIVE_TEMPERATURE][HYPO_SAL_CAT_TEMPERATURE] = 2.0f;

    /* FATIGUE -> REST (strong) */
    config.drive_category_map[HYPO_DRIVE_FATIGUE][HYPO_SAL_CAT_REST] = 2.0f;

    /* SAFETY -> THREAT and ESCAPE (very strong - safety critical) */
    config.drive_category_map[HYPO_DRIVE_SAFETY][HYPO_SAL_CAT_THREAT] = 3.0f;
    config.drive_category_map[HYPO_DRIVE_SAFETY][HYPO_SAL_CAT_ESCAPE] = 2.5f;

    /* SOCIAL -> SOCIAL (strong) */
    config.drive_category_map[HYPO_DRIVE_SOCIAL][HYPO_SAL_CAT_SOCIAL] = 2.0f;

    /* CURIOSITY -> NOVEL and INFORMATION (strong) */
    config.drive_category_map[HYPO_DRIVE_CURIOSITY][HYPO_SAL_CAT_NOVEL] = 2.0f;
    config.drive_category_map[HYPO_DRIVE_CURIOSITY][HYPO_SAL_CAT_INFORMATION] = 1.5f;

    /* AUTONOMY -> AUTONOMY (moderate) */
    config.drive_category_map[HYPO_DRIVE_AUTONOMY][HYPO_SAL_CAT_AUTONOMY] = 1.5f;

    /* COMPETENCE -> COMPETENCE (moderate) */
    config.drive_category_map[HYPO_DRIVE_COMPETENCE][HYPO_SAL_CAT_COMPETENCE] = 1.5f;

    /* Safety/alignment parameters */
    config.alignment_boost = 1.5f;
    config.threat_priority = 2.0f;
    config.safety_override = true;  /* Always prioritize threat detection */

    /* Integration options */
    config.connect_attention_gate = true;
    config.connect_salience_module = true;
    config.broadcast_enabled = true;

    return config;
}

hypo_attn_bridge_t* hypo_attn_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_attn_bridge_config_t* config) {

    if (!drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_attn_bridge_create: drives is NULL");
        return NULL;
    }

    hypo_attn_bridge_t* bridge = (hypo_attn_bridge_t*)nimcp_calloc(1, sizeof(hypo_attn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_attn_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_attn_bridge_default_config();
    }

    /* Store drive system reference */
    bridge->drives = drives;

    /* Initialize salience state */
    for (int c = 0; c < HYPO_SAL_CAT_COUNT; c++) {
        bridge->salience.categories[c].category = (hypo_salience_category_t)c;
        bridge->salience.categories[c].base_weight = bridge->config.category_base_weights[c];
        bridge->salience.categories[c].drive_boost = 0.0f;
        bridge->salience.categories[c].effective_weight = bridge->config.category_base_weights[c];

        /* Find primary drive for this category */
        float max_map = 0.0f;
        hypo_drive_type_t primary = HYPO_DRIVE_SAFETY;
        for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
            if (bridge->config.drive_category_map[d][c] > max_map) {
                max_map = bridge->config.drive_category_map[d][c];
                primary = (hypo_drive_type_t)d;
            }
        }
        bridge->salience.categories[c].primary_drive = primary;
        bridge->salience.categories[c].drive_sensitivity = max_map;
    }

    bridge->salience.global_gain = 1.0f;
    bridge->salience.novelty_boost = 0.0f;
    bridge->salience.threat_boost = 0.0f;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_attention");

    return bridge;
}

void hypo_attn_bridge_destroy(hypo_attn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_attention");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

void hypo_attn_bridge_reset(hypo_attn_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset category boosts */
    for (int c = 0; c < HYPO_SAL_CAT_COUNT; c++) {
        bridge->salience.categories[c].drive_boost = 0.0f;
        bridge->salience.categories[c].effective_weight =
            bridge->salience.categories[c].base_weight;
    }

    bridge->salience.global_gain = 1.0f;
    bridge->salience.novelty_boost = 0.0f;
    bridge->salience.threat_boost = 0.0f;

    /* Clear targets */
    bridge->active_targets = 0;

    /* Reset statistics */
    bridge->modulations_computed = 0;
    bridge->targets_boosted = 0;
    bridge->targets_suppressed = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

hypo_salience_state_t hypo_attn_bridge_update_modulation(
    hypo_attn_bridge_t* bridge) {

    hypo_salience_state_t result = {0};

    if (!bridge || !bridge->drives) {
        return result;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find dominant drive */
    find_dominant_drive(bridge,
                        &bridge->salience.dominant_drive,
                        &bridge->salience.dominant_urgency);

    /* Update each category's boost */
    for (int c = 0; c < HYPO_SAL_CAT_COUNT; c++) {
        float boost = compute_category_boost(bridge, (hypo_salience_category_t)c);
        bridge->salience.categories[c].drive_boost = boost;
        bridge->salience.categories[c].effective_weight =
            bridge->salience.categories[c].base_weight * (1.0f + boost);
    }

    /* Special handling for threat category if safety override enabled */
    if (bridge->config.safety_override) {
        hypo_drive_state_t safety_state;
        if (hypo_drive_get_state(bridge->drives, HYPO_DRIVE_SAFETY, &safety_state)) {
            if (safety_state.urgency > bridge->config.urgency_threshold) {
                /* Ensure threat category always gets boost */
                float threat_boost = safety_state.urgency * bridge->config.threat_priority;
                if (threat_boost > bridge->salience.categories[HYPO_SAL_CAT_THREAT].drive_boost) {
                    bridge->salience.categories[HYPO_SAL_CAT_THREAT].drive_boost = threat_boost;
                    bridge->salience.categories[HYPO_SAL_CAT_THREAT].effective_weight =
                        bridge->salience.categories[HYPO_SAL_CAT_THREAT].base_weight *
                        (1.0f + threat_boost);
                }
                bridge->salience.threat_boost = threat_boost;
            }
        }
    }

    /* Update timestamp */
    bridge->salience.timestamp_us = nimcp_time_get_us();
    bridge->salience.modulations_applied++;
    bridge->modulations_computed++;

    result = bridge->salience;

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float hypo_attn_bridge_modulate_target(
    hypo_attn_bridge_t* bridge,
    uint32_t target_id,
    hypo_salience_category_t category,
    float base_salience) {

    if (!bridge || category >= HYPO_SAL_CAT_COUNT) {
        return base_salience;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float boost = bridge->salience.categories[category].drive_boost;
    float modulated = apply_modulation(bridge, base_salience, boost);

    /* Track in targets list if not full */
    if (bridge->active_targets < HYPO_ATTN_MAX_TARGETS) {
        hypo_attn_target_mod_t* target = &bridge->targets[bridge->active_targets++];
        target->target_id = target_id;
        target->category = category;
        target->original_salience = base_salience;
        target->modulated_salience = modulated;
        target->boost_applied = boost;
        target->is_suppressed = (modulated < base_salience);

        if (modulated > base_salience) {
            bridge->targets_boosted++;
        } else if (modulated < base_salience) {
            bridge->targets_suppressed++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return modulated;
}

uint32_t hypo_attn_bridge_modulate_batch(
    hypo_attn_bridge_t* bridge,
    const uint32_t* target_ids,
    const hypo_salience_category_t* categories,
    const float* base_saliences,
    float* modulated_out,
    uint32_t count) {

    if (!bridge || !target_ids || !categories || !base_saliences || !modulated_out) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (categories[i] < HYPO_SAL_CAT_COUNT) {
            float boost = bridge->salience.categories[categories[i]].drive_boost;
            modulated_out[i] = apply_modulation(bridge, base_saliences[i], boost);
            processed++;
        } else {
            modulated_out[i] = base_saliences[i];  /* Pass through unchanged */
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return processed;
}

float hypo_attn_bridge_get_category_boost(
    const hypo_attn_bridge_t* bridge,
    hypo_salience_category_t category) {

    if (!bridge || category >= HYPO_SAL_CAT_COUNT) {
        return 0.0f;
    }

    return bridge->salience.categories[category].drive_boost;
}

hypo_drive_type_t hypo_attn_bridge_get_dominant_drive(
    const hypo_attn_bridge_t* bridge,
    float* urgency_out) {

    if (!bridge) {
        if (urgency_out) *urgency_out = 0.0f;
        return HYPO_DRIVE_SAFETY;
    }

    if (urgency_out) {
        *urgency_out = bridge->salience.dominant_urgency;
    }

    return bridge->salience.dominant_drive;
}

/*=============================================================================
 * ATTENTION GATE INTEGRATION
 *===========================================================================*/

bool hypo_attn_bridge_connect_gate(
    hypo_attn_bridge_t* bridge,
    void* gate) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_attn_bridge_reset: bridge is NULL");
        return false;
    }

    bridge->attention_gate = gate;
    return true;
}

uint32_t hypo_attn_bridge_push_to_gate(hypo_attn_bridge_t* bridge) {
    if (!bridge || !bridge->attention_gate) {
        return 0;
    }

    /* TODO: Implement when attention gate interface is fully defined */
    /* For now, just return 0 */
    return 0;
}

bool hypo_attn_bridge_connect_salience(
    hypo_attn_bridge_t* bridge,
    void* evaluator) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_attn_bridge_push_to_gate: bridge is NULL");
        return false;
    }

    bridge->salience_evaluator = evaluator;
    return true;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool hypo_attn_bridge_register_bio(
    hypo_attn_bridge_t* bridge,
    bool use_kg_wiring) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_attn_bridge_push_to_gate: bridge is NULL");
        return false;
    }

    /* Register module */
    bio_module_info_t info = {
        .module_id = HYPO_ATTN_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_attn_bridge",
        .inbox_capacity = 0,  /* No inbox needed for now */
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_attn_bridge_push_to_gate: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handler for drive state messages */
    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_HYPO_DRIVE_STATE,
                                     attn_handle_drive_state) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_attn_bridge_push_to_gate: bridge->bio_ctx is NULL");
        return false;
    }

    return true;
}

uint32_t hypo_attn_bridge_process_bio(
    hypo_attn_bridge_t* bridge,
    uint32_t max_messages) {

    if (!bridge || !bridge->bio_ctx) {
        return 0;
    }

    return bio_router_process_inbox(bridge->bio_ctx, max_messages);
}

nimcp_error_t hypo_attn_bridge_broadcast_modulation(hypo_attn_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge && bridge->bio_ctx, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL or bio_ctx is NULL");

    if (!bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Construct message */
    struct {
        bio_message_header_t header;
        hypo_salience_state_t salience;
    } msg;

    msg.header.type = BIO_MSG_ATTENTION_MODULATION;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_ATTN_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast to all */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_salience_state_t);
    msg.salience = bridge->salience;

    bridge->broadcasts_sent++;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

void hypo_attn_bridge_get_stats(
    const hypo_attn_bridge_t* bridge,
    uint64_t* modulations_computed,
    uint64_t* targets_boosted,
    uint64_t* targets_suppressed) {

    if (!bridge) {
        return;
    }

    if (modulations_computed) *modulations_computed = bridge->modulations_computed;
    if (targets_boosted) *targets_boosted = bridge->targets_boosted;
    if (targets_suppressed) *targets_suppressed = bridge->targets_suppressed;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* hypo_salience_category_string(hypo_salience_category_t category) {
    switch (category) {
        case HYPO_SAL_CAT_FOOD:        return "FOOD";
        case HYPO_SAL_CAT_WATER:       return "WATER";
        case HYPO_SAL_CAT_THREAT:      return "THREAT";
        case HYPO_SAL_CAT_ESCAPE:      return "ESCAPE";
        case HYPO_SAL_CAT_SOCIAL:      return "SOCIAL";
        case HYPO_SAL_CAT_NOVEL:       return "NOVEL";
        case HYPO_SAL_CAT_INFORMATION: return "INFORMATION";
        case HYPO_SAL_CAT_COMPETENCE:  return "COMPETENCE";
        case HYPO_SAL_CAT_AUTONOMY:    return "AUTONOMY";
        case HYPO_SAL_CAT_REST:        return "REST";
        case HYPO_SAL_CAT_TEMPERATURE: return "TEMPERATURE";
        case HYPO_SAL_CAT_ALIGNMENT:   return "ALIGNMENT";
        default:                       return "UNKNOWN";
    }
}

hypo_salience_category_t hypo_drive_to_salience_category(hypo_drive_type_t drive) {
    switch (drive) {
        case HYPO_DRIVE_HUNGER:      return HYPO_SAL_CAT_FOOD;
        case HYPO_DRIVE_THIRST:      return HYPO_SAL_CAT_WATER;
        case HYPO_DRIVE_TEMPERATURE: return HYPO_SAL_CAT_TEMPERATURE;
        case HYPO_DRIVE_FATIGUE:     return HYPO_SAL_CAT_REST;
        case HYPO_DRIVE_SOCIAL:      return HYPO_SAL_CAT_SOCIAL;
        case HYPO_DRIVE_CURIOSITY:   return HYPO_SAL_CAT_NOVEL;
        case HYPO_DRIVE_SAFETY:      return HYPO_SAL_CAT_THREAT;
        case HYPO_DRIVE_AUTONOMY:    return HYPO_SAL_CAT_AUTONOMY;
        case HYPO_DRIVE_COMPETENCE:  return HYPO_SAL_CAT_COMPETENCE;
        default:                     return HYPO_SAL_CAT_NOVEL;
    }
}
