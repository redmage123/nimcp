/**
 * @file nimcp_hypothalamus_hippocampus_bridge.c
 * @brief Implementation of Hypothalamus <-> Hippocampus Bridge
 *
 * @version Phase 12: Cognitive Layer Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_hippocampus_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_hippocampus_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_HIPPOCAMPUS_BRIDGE"

/**
 * @brief Memory category names
 */
static const char* s_memory_category_names[HYPO_MEM_CAT_COUNT] = {
    "FOOD",
    "WATER",
    "SHELTER",
    "REST",
    "SOCIAL",
    "THREAT",
    "EXPLORATION",
    "SKILL",
    "NEUTRAL"
};

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_hipp_bridge_config_t hypo_hipp_bridge_default_config(void) {
    hypo_hipp_bridge_config_t config = {0};

    /* Encoding priority computation */
    config.encoding_scale = HYPO_HIPP_ENCODING_SCALE;
    config.valence_scale = HYPO_HIPP_VALENCE_SCALE;
    config.baseline_encoding_priority = 0.2f;

    /* Memory-to-drive influence */
    config.memory_drive_scale = HYPO_HIPP_MEMORY_DRIVE_SCALE;
    config.enable_memory_drive_modulation = true;
    config.memory_drive_decay = 0.05f;

    /* Navigation integration */
    config.enable_navigation_goals = true;
    config.exploration_curiosity_weight = 0.3f;

    /* Consolidation control */
    config.enable_consolidation_signals = true;
    config.circadian_consolidation_phase = 2.0f; /* 2 AM optimal */

    /* Replay integration */
    config.enable_replay_rewards = true;
    config.replay_reward_scale = HYPO_HIPP_REPLAY_REWARD_SCALE;

    /* Bio-async */
    config.broadcast_enabled = true;

    return config;
}

hypo_hipp_bridge_t* hypo_hipp_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_hipp_bridge_config_t* config) {

    if (!drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_hipp_bridge_create: drives is NULL");
        return NULL;
    }

    hypo_hipp_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_hipp_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_hipp_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_hipp_bridge_default_config();
    }

    /* Store drives reference */
    bridge->drives = drives;
    bridge->hippocampus = NULL;

    /* Initialize state */
    memset(&bridge->current_priority, 0, sizeof(bridge->current_priority));
    memset(&bridge->current_context, 0, sizeof(bridge->current_context));
    memset(&bridge->current_goal, 0, sizeof(bridge->current_goal));

    /* Initialize associations */
    memset(bridge->associations, 0, sizeof(bridge->associations));
    bridge->num_associations = 0;

    /* Initialize memory influences */
    memset(bridge->memory_drive_influence, 0, sizeof(bridge->memory_drive_influence));

    /* Initialize timing */
    bridge->last_update_us = nimcp_time_now_us();
    bridge->last_consolidation_signal_us = 0;

    /* Initialize bio context */
    memset(&bridge->bio_ctx, 0, sizeof(bridge->bio_ctx));

    /* Initialize statistics */
    bridge->encoding_signals_sent = 0;
    bridge->memory_retrievals_processed = 0;
    bridge->replay_events_processed = 0;
    bridge->consolidation_signals_sent = 0;
    bridge->nav_goals_set = 0;

    /* Initialize bridge base (creates mutex) */
    bridge_base_init(&bridge->base, 0, "hypothalamus_hippocampus");

    NIMCP_LOG_INFO("Hypothalamus-Hippocampus bridge created");
    return bridge;
}

void hypo_hipp_bridge_destroy(hypo_hipp_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_hippocampus");

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOG_INFO("Hypothalamus-Hippocampus bridge destroyed");
}

void hypo_hipp_bridge_reset(hypo_hipp_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->current_priority, 0, sizeof(bridge->current_priority));
    memset(&bridge->current_context, 0, sizeof(bridge->current_context));
    memset(&bridge->current_goal, 0, sizeof(bridge->current_goal));

    /* Reset associations */
    memset(bridge->associations, 0, sizeof(bridge->associations));
    bridge->num_associations = 0;

    /* Reset memory influences */
    memset(bridge->memory_drive_influence, 0, sizeof(bridge->memory_drive_influence));

    /* Reset timing */
    bridge->last_update_us = nimcp_time_now_us();
    bridge->last_consolidation_signal_us = 0;

    /* Reset statistics */
    bridge->encoding_signals_sent = 0;
    bridge->memory_retrievals_processed = 0;
    bridge->replay_events_processed = 0;
    bridge->consolidation_signals_sent = 0;
    bridge->nav_goals_set = 0;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Hypothalamus-Hippocampus bridge reset");
}

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

int hypo_hipp_bridge_update(hypo_hipp_bridge_t* bridge, float dt_ms) {
    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_hipp_bridge_update: bridge or drives is NULL");
        return -1;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now_us = nimcp_time_now_us();

    /* Compute encoding priority from current drive states */
    bridge->current_priority = hypo_hipp_bridge_compute_encoding_priority(bridge);

    /* Decay memory influences on drives */
    if (bridge->config.enable_memory_drive_modulation) {
        float decay = expf(-bridge->config.memory_drive_decay * dt_ms / 1000.0f);
        for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
            bridge->memory_drive_influence[i] *= decay;
        }
    }

    /* Update timing */
    bridge->last_update_us = now_us;

    /* Broadcast encoding priority if enabled */
    if (bridge->config.broadcast_enabled) {
        hypo_hipp_bridge_broadcast_encoding_priority(bridge);
    }

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

hypo_hipp_encoding_priority_t hypo_hipp_bridge_compute_encoding_priority(
    hypo_hipp_bridge_t* bridge) {

    hypo_hipp_encoding_priority_t priority = {0};

    if (!bridge || !bridge->drives) return priority;

    /* Get drive urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        return priority;
    }

    /* Get dominant drive */
    priority.dominant_drive = hypo_drive_get_priority(bridge->drives);

    /* Initialize all priorities to baseline */
    for (int i = 0; i < HYPO_MEM_CAT_COUNT; i++) {
        priority.priorities[i] = bridge->config.baseline_encoding_priority;
    }

    /* Map drive urgencies to memory category priorities */
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        hypo_memory_category_t cat = hypo_drive_to_memory_cat[d];
        float contribution = urgencies[d] * bridge->config.encoding_scale;
        priority.priorities[cat] = nimcp_clampf(
            priority.priorities[cat] + contribution, 0.0f, 1.0f);
    }

    /* Get dominant drive urgency */
    if (priority.dominant_drive >= 0 && priority.dominant_drive < HYPO_DRIVE_COUNT) {
        priority.dominant_urgency = urgencies[priority.dominant_drive];
    }

    /* Compute emotional valence from drive satisfaction */
    /* High urgency = negative valence (unsatisfied), Low urgency = positive */
    float total_urgency = 0.0f;
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        total_urgency += urgencies[d];
    }
    float avg_urgency = total_urgency / HYPO_DRIVE_COUNT;
    priority.emotional_valence = nimcp_clampf(
        (0.5f - avg_urgency) * 2.0f * bridge->config.valence_scale,
        -1.0f, 1.0f);

    /* Get arousal level from drives */
    hypo_drive_system_t sys_state;
    if (hypo_drive_get_system_state(bridge->drives, &sys_state)) {
        priority.arousal_level = sys_state.arousal_level;
    } else {
        priority.arousal_level = avg_urgency; /* Fallback */
    }

    priority.timestamp_us = nimcp_time_now_us();

    return priority;
}

hypo_hipp_consolidation_signal_t hypo_hipp_bridge_compute_consolidation(
    hypo_hipp_bridge_t* bridge,
    float circadian_phase,
    float sleep_pressure) {

    hypo_hipp_consolidation_signal_t signal = {0};

    if (!bridge) return signal;

    signal.circadian_phase = circadian_phase;

    /* Consolidation urgency based on circadian phase */
    /* Higher when close to optimal consolidation phase (typically 2-4 AM) */
    float phase_diff = fabsf(circadian_phase - bridge->config.circadian_consolidation_phase);
    if (phase_diff > 12.0f) phase_diff = 24.0f - phase_diff; /* Wrap around */
    float phase_urgency = 1.0f - (phase_diff / 12.0f); /* 1.0 at optimal, 0.0 at 12h away */

    /* Combine with sleep pressure */
    signal.sleep_pressure_high = (sleep_pressure > 0.6f);
    signal.consolidation_urgency = nimcp_clampf(
        0.3f * phase_urgency + 0.7f * sleep_pressure, 0.0f, 1.0f);

    /* Determine priority categories based on drive urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    if (hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        /* Find top 3 drives */
        int top_drives[3] = {-1, -1, -1};
        float top_urgencies[3] = {-1.0f, -1.0f, -1.0f};

        for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
            for (int t = 0; t < 3; t++) {
                if (urgencies[d] > top_urgencies[t]) {
                    /* Shift down */
                    for (int s = 2; s > t; s--) {
                        top_drives[s] = top_drives[s-1];
                        top_urgencies[s] = top_urgencies[s-1];
                    }
                    top_drives[t] = d;
                    top_urgencies[t] = urgencies[d];
                    break;
                }
            }
        }

        /* Convert to memory categories */
        for (int t = 0; t < 3; t++) {
            if (top_drives[t] >= 0) {
                signal.priority_categories[t] = hypo_drive_to_memory_cat[top_drives[t]];
            } else {
                signal.priority_categories[t] = HYPO_MEM_CAT_NEUTRAL;
            }
        }
    }

    signal.timestamp_us = nimcp_time_now_us();

    return signal;
}

hypo_hipp_nav_goal_t hypo_hipp_bridge_compute_nav_goal(
    hypo_hipp_bridge_t* bridge) {

    hypo_hipp_nav_goal_t goal = {0};

    if (!bridge || !bridge->drives) return goal;

    /* Get dominant drive as goal motivation */
    goal.goal_drive = hypo_drive_get_priority(bridge->drives);

    /* Get urgency */
    float urgencies[HYPO_DRIVE_COUNT];
    if (hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        if (goal.goal_drive >= 0 && goal.goal_drive < HYPO_DRIVE_COUNT) {
            goal.goal_urgency = urgencies[goal.goal_drive];
        }

        /* Exploration bonus from curiosity drive */
        goal.exploration_bonus = urgencies[HYPO_DRIVE_CURIOSITY] *
                                bridge->config.exploration_curiosity_weight;
    }

    /* Target location from spatial context (if we have one) */
    goal.location_valid = false;
    if (bridge->current_context.resource_estimate[hypo_drive_to_memory_cat[goal.goal_drive]] > 0.5f) {
        /* We know of resources in current context */
        goal.target_location = bridge->current_context.current_location;
        goal.location_valid = true;
    }

    goal.timestamp_us = nimcp_time_now_us();

    return goal;
}

bool hypo_hipp_bridge_get_encoding_priority(
    const hypo_hipp_bridge_t* bridge,
    hypo_hipp_encoding_priority_t* priority) {

    if (!bridge || !priority) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_hipp_bridge_update: required parameter is NULL (bridge, priority)");
        return false;
    }

    *priority = bridge->current_priority;
    return true;
}

/*=============================================================================
 * MEMORY FEEDBACK PROCESSING
 *===========================================================================*/

float hypo_hipp_bridge_process_retrieval(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_memory_retrieval_t* retrieval) {

    if (!bridge || !retrieval) return 0.0f;

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    float influence = 0.0f;

    if (bridge->config.enable_memory_drive_modulation) {
        /* Map memory category back to drive */
        hypo_drive_type_t affected_drive = HYPO_DRIVE_COUNT;
        for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
            if (hypo_drive_to_memory_cat[d] == retrieval->category) {
                affected_drive = (hypo_drive_type_t)d;
                break;
            }
        }

        if (affected_drive < HYPO_DRIVE_COUNT) {
            /* Memory retrieval can increase drive (anticipation effect) */
            /* e.g., remembering food increases hunger slightly */
            influence = retrieval->strength * retrieval->relevance_score *
                       bridge->config.memory_drive_scale;

            /* Negative valence memories suppress drive (avoidance) */
            if (retrieval->emotional_valence < 0) {
                influence *= (1.0f + retrieval->emotional_valence); /* Reduce influence */
            }

            /* Accumulate influence */
            bridge->memory_drive_influence[affected_drive] += influence;
            bridge->memory_drive_influence[affected_drive] = nimcp_clampf(
                bridge->memory_drive_influence[affected_drive], 0.0f, 1.0f);

            /* Apply to drives through nucleus input */
            /* Strong positive memory -> increase drive via lateral hypothalamus */
            if (influence > 0.1f) {
                hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_LATERAL, influence);
            }
        }
    }

    bridge->memory_retrievals_processed++;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
    return influence;
}

void hypo_hipp_bridge_process_context(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_spatial_context_t* context) {

    if (!bridge || !context) return;

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_context = *context;

    /* Update safety drive based on location safety estimate */
    if (context->safety_estimate < 0.3f) {
        /* Unsafe location - boost safety drive */
        hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_PARAVENTRICULAR,
                                     (1.0f - context->safety_estimate) * 0.3f);
    }

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
}

float hypo_hipp_bridge_process_replay(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_replay_event_t* replay) {

    if (!bridge || !replay) return 0.0f;

    float reward_prediction = 0.0f;

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_replay_rewards) {
        /* Convert replay total reward to drive-relevant reward prediction */
        reward_prediction = replay->total_reward * bridge->config.replay_reward_scale;

        /* Forward replay = planning, amplify reward prediction */
        if (replay->forward_replay) {
            reward_prediction *= 1.2f;
        }

        /* If replay is relevant to current dominant drive, boost further */
        hypo_drive_type_t dominant = hypo_drive_get_priority(bridge->drives);
        if (replay->relevant_drive == dominant) {
            reward_prediction *= 1.5f;
        }

        reward_prediction = nimcp_clampf(reward_prediction, -1.0f, 1.0f);

        /* This reward prediction could be sent to SNc/VTA for DA modulation */
        /* For now, we just track it */
    }

    bridge->replay_events_processed++;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
    return reward_prediction;
}

bool hypo_hipp_bridge_get_memory_influences(
    const hypo_hipp_bridge_t* bridge,
    float* influences) {

    if (!bridge || !influences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, influences)");
        return false;
    }

    memcpy(influences, bridge->memory_drive_influence,
           sizeof(float) * HYPO_DRIVE_COUNT);
    return true;
}

/*=============================================================================
 * HIPPOCAMPUS CONNECTION
 *===========================================================================*/

bool hypo_hipp_bridge_connect(
    hypo_hipp_bridge_t* bridge,
    hippocampus_adapter_t* hippocampus) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    bridge->hippocampus = hippocampus;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_INFO("Hippocampus connected to hypothalamus bridge");
    return true;
}

int hypo_hipp_bridge_send_encoding_priority(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_encoding_priority_t* priority) {

    if (!bridge || !priority) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_hipp_bridge_send_encoding_priority: bridge or priority is NULL");
        return -1;
    }

    /* Direct hippocampus API call if connected */
    /* Note: hippocampus_adapter doesn't have a set_encoding_priority function yet,
       so this would need to be added or we rely on bio-async */

    bridge->encoding_signals_sent++;
    return 0;
}

int hypo_hipp_bridge_send_consolidation(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_consolidation_signal_t* signal) {

    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_hipp_bridge_send_consolidation: bridge or signal is NULL");
        return -1;
    }

    /* If directly connected, could trigger consolidation */
    if (bridge->hippocampus && signal->consolidation_urgency > 0.6f) {
        /* hippocampus_consolidate_memories() could be called here */
        bridge->consolidation_signals_sent++;
    }

    return 0;
}

int hypo_hipp_bridge_set_nav_goal(
    hypo_hipp_bridge_t* bridge,
    const hypo_hipp_nav_goal_t* goal) {

    if (!bridge || !goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_hipp_bridge_set_nav_goal: bridge or goal is NULL");
        return -1;
    }

    /* Set navigation goal if hippocampus connected and location valid */
    if (bridge->hippocampus && goal->location_valid) {
        if (hippocampus_set_navigation_goal(bridge->hippocampus, &goal->target_location)) {
            bridge->nav_goals_set++;
            return 0;
        }
    }

    /* Hippocampus not connected or location not valid — not an error */
    return -1;
}

/*=============================================================================
 * DRIVE-MEMORY ASSOCIATIONS
 *===========================================================================*/

bool hypo_hipp_bridge_create_association(
    hypo_hipp_bridge_t* bridge,
    hypo_drive_type_t drive,
    uint32_t memory_id,
    float strength) {

    if (!bridge || drive >= HYPO_DRIVE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: bridge is NULL");
        return false;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    bool success = false;

    /* Check if association already exists */
    for (uint32_t i = 0; i < bridge->num_associations; i++) {
        if (bridge->associations[i].drive == drive &&
            bridge->associations[i].memory_id == memory_id) {
            /* Update existing */
            bridge->associations[i].association_strength = strength;
            bridge->associations[i].last_activation_us = nimcp_time_now_us();
            success = true;
            break;
        }
    }

    if (!success && bridge->num_associations < HYPO_HIPP_MAX_ASSOCIATIONS) {
        /* Create new */
        hypo_hipp_association_t* assoc = &bridge->associations[bridge->num_associations];
        assoc->drive = drive;
        assoc->memory_id = memory_id;
        assoc->association_strength = strength;
        assoc->last_activation_us = nimcp_time_now_us();
        bridge->num_associations++;
        success = true;
    }

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
    return success;
}

float hypo_hipp_bridge_strengthen_association(
    hypo_hipp_bridge_t* bridge,
    hypo_drive_type_t drive,
    uint32_t memory_id,
    float delta) {

    if (!bridge || drive >= HYPO_DRIVE_COUNT) return -1.0f;

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    float new_strength = -1.0f;

    for (uint32_t i = 0; i < bridge->num_associations; i++) {
        if (bridge->associations[i].drive == drive &&
            bridge->associations[i].memory_id == memory_id) {
            bridge->associations[i].association_strength += delta;
            bridge->associations[i].association_strength = nimcp_clampf(
                bridge->associations[i].association_strength, 0.0f, 1.0f);
            bridge->associations[i].last_activation_us = nimcp_time_now_us();
            new_strength = bridge->associations[i].association_strength;
            break;
        }
    }

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
    return new_strength;
}

uint32_t hypo_hipp_bridge_get_associations(
    const hypo_hipp_bridge_t* bridge,
    hypo_drive_type_t drive,
    hypo_hipp_association_t* associations,
    uint32_t max_associations) {

    if (!bridge || !associations || drive >= HYPO_DRIVE_COUNT) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->num_associations && count < max_associations; i++) {
        if (bridge->associations[i].drive == drive) {
            associations[count++] = bridge->associations[i];
        }
    }

    return count;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/* Module ID for this bridge */
#define HYPO_HIPP_BRIDGE_MODULE_ID  0x1160

/**
 * @brief Handler for memory encoded message
 */
static nimcp_error_t hipp_handle_memory_encoded(const void* msg, size_t msg_size,
                                                 nimcp_bio_promise_t promise, void* ctx) {
    hypo_hipp_bridge_t* bridge = (hypo_hipp_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Extract memory ID from message payload */
    const struct {
        bio_message_header_t header;
        uint32_t memory_id;
    }* encoded_msg = msg;

    hypo_drive_type_t dominant = hypo_drive_get_priority(bridge->drives);
    hypo_hipp_bridge_create_association(bridge, dominant, encoded_msg->memory_id, 0.5f);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handler for context update message
 */
static nimcp_error_t hipp_handle_context_update(const void* msg, size_t msg_size,
                                                 nimcp_bio_promise_t promise, void* ctx) {
    hypo_hipp_bridge_t* bridge = (hypo_hipp_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const struct {
        bio_message_header_t header;
        hypo_hipp_spatial_context_t context;
    }* context_msg = msg;

    hypo_hipp_bridge_process_context(bridge, &context_msg->context);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handler for replay event message
 */
static nimcp_error_t hipp_handle_replay_event(const void* msg, size_t msg_size,
                                               nimcp_bio_promise_t promise, void* ctx) {
    hypo_hipp_bridge_t* bridge = (hypo_hipp_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const struct {
        bio_message_header_t header;
        hypo_hipp_replay_event_t replay;
    }* replay_msg = msg;

    hypo_hipp_bridge_process_replay(bridge, &replay_msg->replay);

    return NIMCP_SUCCESS;
}

bool hypo_hipp_bridge_register_bio(
    hypo_hipp_bridge_t* bridge,
    bool use_kg_wiring) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    (void)use_kg_wiring; /* For future KG-driven wiring */

    /* Create module info for registration */
    bio_module_info_t info = {
        .module_id = HYPO_HIPP_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_hippocampus_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    /* Register with router */
    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        NIMCP_LOG_ERROR("Failed to register hippocampus bridge with bio router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register message handlers */
    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_HIPPOCAMPUS_MEMORY_ENCODED,
                                     hipp_handle_memory_encoded) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_HIPPOCAMPUS_CONTEXT_UPDATE,
                                     hipp_handle_context_update) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
        return false;
    }

    if (bio_router_register_handler(bridge->bio_ctx, BIO_MSG_HIPPOCAMPUS_REPLAY_EVENT,
                                     hipp_handle_replay_event) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
        return false;
    }

    NIMCP_LOG_INFO("Hippocampus bridge registered with bio-async");
    return true;
}

uint32_t hypo_hipp_bridge_process_bio(
    hypo_hipp_bridge_t* bridge,
    uint32_t max_messages) {

    if (!bridge || !bridge->bio_ctx) return 0;

    return bio_router_process_inbox(bridge->bio_ctx, max_messages);
}

nimcp_error_t hypo_hipp_bridge_broadcast_encoding_priority(
    hypo_hipp_bridge_t* bridge) {

    if (!bridge || !bridge->bio_ctx || !bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    struct {
        bio_message_header_t header;
        hypo_hipp_encoding_priority_t priority;
    } msg;

    msg.header.type = BIO_MSG_HIPPOCAMPUS_ENCODING_PRIORITY;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_HIPP_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_hipp_encoding_priority_t);
    msg.priority = bridge->current_priority;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

nimcp_error_t hypo_hipp_bridge_broadcast_consolidation(
    hypo_hipp_bridge_t* bridge) {

    if (!bridge || !bridge->bio_ctx || !bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    struct {
        bio_message_header_t header;
        hypo_hipp_consolidation_signal_t signal;
    } msg;

    msg.header.type = BIO_MSG_HIPPOCAMPUS_CONSOLIDATE;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_HIPP_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_hipp_consolidation_signal_t);
    msg.signal = hypo_hipp_bridge_compute_consolidation(bridge, 0.0f, 0.5f);

    bridge->consolidation_signals_sent++;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* hypo_memory_category_string(hypo_memory_category_t category) {
    if (category >= HYPO_MEM_CAT_COUNT) return "UNKNOWN";
    return s_memory_category_names[category];
}

hypo_memory_category_t hypo_drive_to_memory_category(hypo_drive_type_t drive) {
    if (drive >= HYPO_DRIVE_COUNT) return HYPO_MEM_CAT_NEUTRAL;
    return hypo_drive_to_memory_cat[drive];
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

void hypo_hipp_bridge_get_stats(
    const hypo_hipp_bridge_t* bridge,
    uint64_t* encoding_signals,
    uint64_t* retrievals_processed,
    uint64_t* replay_events) {

    if (!bridge) return;

    if (encoding_signals) *encoding_signals = bridge->encoding_signals_sent;
    if (retrievals_processed) *retrievals_processed = bridge->memory_retrievals_processed;
    if (replay_events) *replay_events = bridge->replay_events_processed;
}
