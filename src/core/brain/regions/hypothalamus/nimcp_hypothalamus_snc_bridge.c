/**
 * @file nimcp_hypothalamus_snc_bridge.c
 * @brief Implementation of Hypothalamus → SNc/VTA Bridge
 *
 * Converts hypothalamic reward signals into dopamine/RPE for learning.
 * Core component of Byrnes' steering subsystem → learning subsystem interface.
 *
 * @version Phase 4: SNc/VTA Bridge
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_snc_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_snc_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_SNC_BRIDGE"


/* Forward declarations for bio-async handler callbacks */
static nimcp_error_t snc_handle_reward_signal(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx);
static nimcp_error_t snc_handle_value_update(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx);

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

/** Module ID for bio-async registration */
#define HYPO_SNC_BRIDGE_MODULE_ID  0x11500000

/** Maximum phasic response duration in microseconds */
#define PHASIC_MAX_DURATION_US     500000   /* 500ms */

/** Minimum RPE magnitude to trigger phasic response */
#define MIN_PHASIC_RPE             0.05f

/*=============================================================================
 * INTERNAL HELPER DECLARATIONS
 *===========================================================================*/

static void snc_bridge_init_channels(hypo_snc_bridge_t* bridge);
static void snc_bridge_compute_rpe(hypo_snc_bridge_t* bridge,
                                   const hypo_reward_signal_t* reward,
                                   hypo_rpe_t* rpe);
static void snc_bridge_update_dopamine(hypo_snc_bridge_t* bridge,
                                       const hypo_rpe_t* rpe);
static void snc_bridge_decay_phasic(hypo_snc_bridge_t* bridge, float dt_sec);
hypo_snc_bridge_config_t hypo_snc_bridge_default_config(void) {
    hypo_snc_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    config.discount_gamma = HYPO_SNC_DEFAULT_GAMMA;
    config.rpe_threshold = MIN_PHASIC_RPE;
    config.burst_magnitude = 0.5f;
    config.dip_magnitude = 0.5f;
    config.decay_rate = 2.0f;

    for (int i = 0; i < HYPO_DA_CHANNEL_COUNT; i++) {
        config.channel_gains[i] = 1.0f;
    }

    config.alignment_sensitivity = 1.0f;
    config.use_external_snc = false;
    config.broadcast_enabled = true;

    return config;
}

hypo_snc_bridge_t* hypo_snc_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_snc_bridge_config_t* config) {

    if (!drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_snc_bridge_create: drives is NULL");
        return NULL;
    }

    hypo_snc_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_snc_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_snc_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_snc_bridge_default_config();
    }

    /* Store drive system reference */
    bridge->drives = drives;

    /* Initialize dopamine state */
    bridge->dopamine.global_tonic_level = HYPO_SNC_TONIC_BASELINE;
    bridge->dopamine.global_gain = 1.0f;
    bridge->dopamine.discount_gamma = bridge->config.discount_gamma;
    snc_bridge_init_channels(bridge);

    /* Create mutex for thread safety */
    mutex_attr_t attr = {
        .type = MUTEX_TYPE_RECURSIVE
    };
    bridge->base.mutex = nimcp_mutex_create(&attr);
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "hypo_snc_bridge_create: failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void hypo_snc_bridge_destroy(hypo_snc_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_snc");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

void hypo_snc_bridge_reset(hypo_snc_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset dopamine to tonic baseline */
    snc_bridge_init_channels(bridge);
    bridge->dopamine.global_gain = 1.0f;
    bridge->dopamine.value_estimate = 0.0f;
    bridge->dopamine.next_value_estimate = 0.0f;
    bridge->dopamine.burst_count = 0;
    bridge->dopamine.dip_count = 0;
    bridge->dopamine.avg_rpe = 0.0f;

    /* Reset statistics */
    bridge->rewards_processed = 0;
    bridge->broadcasts_sent = 0;
    bridge->cumulative_reward = 0.0f;
    bridge->cumulative_alignment_reward = 0.0f;

    /* Clear last RPE */
    memset(&bridge->last_rpe, 0, sizeof(hypo_rpe_t));

    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

hypo_rpe_t hypo_snc_bridge_process_reward(
    hypo_snc_bridge_t* bridge,
    const hypo_reward_signal_t* reward) {

    hypo_rpe_t rpe = {0};

    if (!bridge || !reward) {
        return rpe;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute RPE from reward */
    snc_bridge_compute_rpe(bridge, reward, &rpe);

    /* Update dopamine state based on RPE */
    snc_bridge_update_dopamine(bridge, &rpe);

    /* Store for later access */
    bridge->last_rpe = rpe;

    /* Update statistics */
    bridge->rewards_processed++;
    bridge->cumulative_reward += reward->reward_signal;
    bridge->cumulative_alignment_reward +=
        (reward->alignment_bonus - reward->alignment_penalty);

    /* Broadcast if enabled */
    if (bridge->config.broadcast_enabled) {
        /* Increment counter under lock before releasing for broadcast */
        bridge->broadcasts_sent += 2;
        nimcp_mutex_unlock(bridge->base.mutex);
        hypo_snc_bridge_broadcast_rpe(bridge, &rpe);
        hypo_snc_bridge_broadcast_dopamine(bridge);
    } else {
        nimcp_mutex_unlock(bridge->base.mutex);
    }
    return rpe;
}

void hypo_snc_bridge_update_value_prediction(
    hypo_snc_bridge_t* bridge,
    float next_value) {

    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dopamine.next_value_estimate = next_value;
    nimcp_mutex_unlock(bridge->base.mutex);
}

void hypo_snc_bridge_step(hypo_snc_bridge_t* bridge, uint64_t dt_us) {
    if (!bridge) return;

    float dt_sec = (float)dt_us / 1000000.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    snc_bridge_decay_phasic(bridge, dt_sec);
    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * DOPAMINE ACCESSORS
 *===========================================================================*/

float hypo_snc_bridge_get_dopamine(
    const hypo_snc_bridge_t* bridge,
    hypo_da_channel_t channel) {

    if (!bridge || channel >= HYPO_DA_CHANNEL_COUNT) {
        return HYPO_SNC_TONIC_BASELINE;
    }

    /* Cast away const to acquire mutex — reads must be synchronized */
    hypo_snc_bridge_t* mutable_bridge = (hypo_snc_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);
    float level = bridge->dopamine.channels[channel].level;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);
    return level;
}

hypo_da_signal_type_t hypo_snc_bridge_get_signal_type(
    const hypo_snc_bridge_t* bridge,
    hypo_da_channel_t channel) {

    if (!bridge || channel >= HYPO_DA_CHANNEL_COUNT) {
        return HYPO_DA_SIGNAL_TONIC;
    }

    /* Cast away const to acquire mutex — reads must be synchronized */
    hypo_snc_bridge_t* mutable_bridge = (hypo_snc_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);
    hypo_da_signal_type_t type = bridge->dopamine.channels[channel].type;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);
    return type;
}

float hypo_snc_bridge_get_global_dopamine(const hypo_snc_bridge_t* bridge) {
    if (!bridge) {
        return HYPO_SNC_TONIC_BASELINE;
    }

    /* Cast away const to acquire mutex — reads must be synchronized */
    hypo_snc_bridge_t* mutable_bridge = (hypo_snc_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);
    float sum = 0.0f;
    for (int i = 0; i < HYPO_DA_CHANNEL_COUNT; i++) {
        sum += bridge->dopamine.channels[i].level;
    }
    float result = sum / (float)HYPO_DA_CHANNEL_COUNT;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);
    return result;
}

const hypo_rpe_t* hypo_snc_bridge_get_last_rpe(const hypo_snc_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_snc_bridge_get_last_rpe: bridge is NULL");
        return NULL;
    }
    return &bridge->last_rpe;
}

/*=============================================================================
 * MODULATION FUNCTIONS
 *===========================================================================*/

void hypo_snc_bridge_modulate_tonic(
    hypo_snc_bridge_t* bridge,
    float arousal_level,
    float stress_level) {

    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Arousal increases tonic DA (alertness, readiness to act) */
    float arousal_effect = 1.0f + 0.3f * arousal_level;

    /* Chronic stress decreases tonic DA (anhedonia) */
    float stress_effect = 1.0f - 0.2f * stress_level;

    /* Combined effect on global gain */
    bridge->dopamine.global_gain = arousal_effect * stress_effect;
    bridge->dopamine.global_gain = nimcp_clampf(
        bridge->dopamine.global_gain, 0.5f, 2.0f);

    /* Update tonic baseline for all channels */
    float new_tonic = HYPO_SNC_TONIC_BASELINE * bridge->dopamine.global_gain;
    new_tonic = nimcp_clampf(new_tonic, 0.2f, 0.8f);

    for (int i = 0; i < HYPO_DA_CHANNEL_COUNT; i++) {
        bridge->dopamine.channels[i].tonic_baseline = new_tonic;
    }
    bridge->dopamine.global_tonic_level = new_tonic;

    nimcp_mutex_unlock(bridge->base.mutex);
}

void hypo_snc_bridge_set_channel_gain(
    hypo_snc_bridge_t* bridge,
    hypo_da_channel_t channel,
    float gain) {

    if (!bridge || channel >= HYPO_DA_CHANNEL_COUNT) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.channel_gains[channel] = nimcp_clampf(gain, 0.0f, 10.0f);
    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * EXTERNAL SNc INTEGRATION
 *===========================================================================*/

bool hypo_snc_bridge_connect_snc(
    hypo_snc_bridge_t* bridge,
    void* snc_module) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_snc_bridge_connect_snc: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->external_snc = snc_module;
    bridge->config.use_external_snc = (snc_module != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return true;
}

void hypo_snc_bridge_disconnect_snc(hypo_snc_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->external_snc = NULL;
    bridge->config.use_external_snc = false;
    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool hypo_snc_bridge_register_bio(
    hypo_snc_bridge_t* bridge,
    bool use_kg_wiring) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_snc_bridge_register_bio: bridge is NULL");
        return false;
    }

    (void)use_kg_wiring;  /* Reserved for future KG wiring */

    /* Register module with bio-router */
    bio_module_info_t info = {
        .module_id = HYPO_SNC_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_snc_bridge",
        .inbox_capacity = 0,  /* Use default */
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_snc_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handler for incoming reward signals from hypothalamus */
    nimcp_error_t err = bio_router_register_handler(
        bridge->bio_ctx,
        BIO_MSG_HYPO_REWARD_SIGNAL,
        snc_handle_reward_signal);
    if (err != NIMCP_SUCCESS) {
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_snc_bridge_register_bio: reward handler registration failed");
        return false;
    }

    /* Register handler for value updates from world model */
    err = bio_router_register_handler(
        bridge->bio_ctx,
        BIO_MSG_SNC_VALUE_UPDATE,
        snc_handle_value_update);
    if (err != NIMCP_SUCCESS) {
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_snc_bridge_register_bio: value handler registration failed");
        return false;
    }

    return true;
}

uint32_t hypo_snc_bridge_process_bio(
    hypo_snc_bridge_t* bridge,
    uint32_t max_messages) {

    if (!bridge) return 0;

    /* Process messages via bio-router (stub for now) */
    (void)max_messages;
    return 0;
}

nimcp_error_t hypo_snc_bridge_broadcast_dopamine(hypo_snc_bridge_t* bridge) {
    if (!bridge || !bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_snc_bridge_broadcast_dopamine: bridge or bio_ctx is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Build dopamine state message with header + payload */
    struct {
        bio_message_header_t header;
        float dopamine_levels[HYPO_DA_CHANNEL_COUNT];
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_SNC_DOPAMINE_STATE;
    msg.header.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    /* Pack dopamine levels into payload — lock to get consistent snapshot */
    nimcp_mutex_lock(bridge->base.mutex);
    for (int i = 0; i < HYPO_DA_CHANNEL_COUNT; i++) {
        msg.dopamine_levels[i] = bridge->dopamine.channels[i].level;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Send via bio-router (outside lock — broadcast may block) */
    return bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
}

nimcp_error_t hypo_snc_bridge_broadcast_rpe(
    hypo_snc_bridge_t* bridge,
    const hypo_rpe_t* rpe) {

    if (!bridge || !rpe || !bridge->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_snc_bridge_broadcast_rpe: bridge, rpe, or bio_ctx is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Build RPE message with header + payload */
    struct {
        bio_message_header_t header;
        hypo_rpe_t rpe_data;
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_SNC_RPE;
    msg.header.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.header.target_module = 0;  /* Broadcast */

    /* Determine priority based on RPE magnitude */
    if (fabsf(rpe->rpe) > 0.5f) {
        msg.header.flags = BIO_MSG_FLAG_URGENT | BIO_MSG_FLAG_BROADCAST;
    } else {
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    }

    /* Copy RPE data to payload */
    msg.rpe_data = *rpe;

    /* Send via bio-router */
    return bio_router_broadcast(bridge->bio_ctx, &msg, sizeof(msg));
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

void hypo_snc_bridge_get_stats(
    const hypo_snc_bridge_t* bridge,
    uint64_t* rewards_processed,
    float* avg_rpe,
    uint64_t* burst_count,
    uint64_t* dip_count) {

    if (!bridge) return;

    /* Cast away const to acquire mutex — reads must be synchronized */
    hypo_snc_bridge_t* mutable_bridge = (hypo_snc_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);

    if (rewards_processed) *rewards_processed = bridge->rewards_processed;
    if (avg_rpe) *avg_rpe = bridge->dopamine.avg_rpe;
    if (burst_count) *burst_count = bridge->dopamine.burst_count;
    if (dip_count) *dip_count = bridge->dopamine.dip_count;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);
}

void hypo_snc_bridge_get_reward_stats(
    const hypo_snc_bridge_t* bridge,
    float* total_reward,
    float* alignment_reward) {

    if (!bridge) return;

    /* Cast away const to acquire mutex — reads must be synchronized */
    hypo_snc_bridge_t* mutable_bridge = (hypo_snc_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);

    if (total_reward) *total_reward = bridge->cumulative_reward;
    if (alignment_reward) *alignment_reward = bridge->cumulative_alignment_reward;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);
}

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static void snc_bridge_init_channels(hypo_snc_bridge_t* bridge) {
    for (int i = 0; i < HYPO_DA_CHANNEL_COUNT; i++) {
        bridge->dopamine.channels[i].channel = (hypo_da_channel_t)i;
        bridge->dopamine.channels[i].level = HYPO_SNC_TONIC_BASELINE;
        bridge->dopamine.channels[i].tonic_baseline = HYPO_SNC_TONIC_BASELINE;
        bridge->dopamine.channels[i].type = HYPO_DA_SIGNAL_TONIC;
        bridge->dopamine.channels[i].decay_rate = bridge->config.decay_rate;
        bridge->dopamine.channels[i].last_update_us = 0;
    }
}

static void snc_bridge_compute_rpe(
    hypo_snc_bridge_t* bridge,
    const hypo_reward_signal_t* reward,
    hypo_rpe_t* rpe) {

    /* Extract reward components - use reward_signal field */
    rpe->actual_reward = reward->reward_signal;

    /* Use current value estimate as prediction */
    rpe->predicted_reward = bridge->dopamine.value_estimate;

    /* Basic RPE */
    rpe->rpe = rpe->actual_reward - rpe->predicted_reward;

    /* Full TD error: delta = r + gamma*V(s') - V(s) */
    float discounted_next = bridge->dopamine.discount_gamma *
                            bridge->dopamine.next_value_estimate;
    rpe->td_error = rpe->actual_reward + discounted_next - rpe->predicted_reward;

    /* Component breakdown */
    /* Drive component: main satisfaction/frustration from drives */
    rpe->drive_component = reward->dopamine_level * 0.5f;

    /* Alignment component: scaled by sensitivity (SAFETY CRITICAL) */
    rpe->alignment_component = (reward->alignment_bonus - reward->alignment_penalty) *
                               bridge->config.alignment_sensitivity;

    /* Curiosity component: epistemic value from HYPO_DRIVE_CURIOSITY */
    rpe->curiosity_component = 0.0f;  /* Would come from drive state */

    /* Track primary drive (if available) */
    rpe->primary_drive = HYPO_DRIVE_HUNGER;  /* Placeholder */

    rpe->timestamp_us = 0;  /* Would be filled with current time */

    /* Update value estimate for next iteration (simple TD update) */
    float learning_rate = NIMCP_LEARNING_RATE_COARSE;
    float new_value = bridge->dopamine.value_estimate + learning_rate * rpe->td_error;
    if (isfinite(new_value)) {
        bridge->dopamine.value_estimate = new_value;
    }

    /* Update running average RPE for statistics */
    float alpha = 0.01f;  /* Slow average */
    float new_avg = (1.0f - alpha) * bridge->dopamine.avg_rpe +
                    alpha * fabsf(rpe->rpe);
    if (isfinite(new_avg)) {
        bridge->dopamine.avg_rpe = new_avg;
    }
}

static void snc_bridge_update_dopamine(
    hypo_snc_bridge_t* bridge,
    const hypo_rpe_t* rpe) {

    float rpe_magnitude = fabsf(rpe->rpe);

    /* Only trigger phasic response if RPE exceeds threshold */
    if (rpe_magnitude < bridge->config.rpe_threshold) {
        /* Below threshold - no phasic response, return to tonic */
        for (int i = 0; i < HYPO_DA_CHANNEL_COUNT; i++) {
            bridge->dopamine.channels[i].type = HYPO_DA_SIGNAL_TONIC;
        }
        return;
    }

    /* Determine burst vs dip */
    bool is_burst = (rpe->rpe > 0);

    /* Calculate phasic magnitude */
    float phasic_change;
    if (is_burst) {
        phasic_change = bridge->config.burst_magnitude * rpe->rpe;
        bridge->dopamine.burst_count++;
    } else {
        phasic_change = bridge->config.dip_magnitude * rpe->rpe;  /* negative */
        bridge->dopamine.dip_count++;
    }

    /* Apply to all channels with channel-specific gains */
    for (int i = 0; i < HYPO_DA_CHANNEL_COUNT; i++) {
        float gain = bridge->config.channel_gains[i];
        float new_level = bridge->dopamine.channels[i].tonic_baseline +
                          phasic_change * gain;

        /* Clamp to valid range */
        new_level = nimcp_clampf(new_level, HYPO_SNC_DIP_MIN, HYPO_SNC_BURST_MAX);
        bridge->dopamine.channels[i].level = new_level;

        /* Set signal type */
        if (is_burst) {
            bridge->dopamine.channels[i].type = HYPO_DA_SIGNAL_BURST;
        } else {
            bridge->dopamine.channels[i].type = HYPO_DA_SIGNAL_DIP;
        }
    }
}

static void snc_bridge_decay_phasic(hypo_snc_bridge_t* bridge, float dt_sec) {
    for (int i = 0; i < HYPO_DA_CHANNEL_COUNT; i++) {
        hypo_da_channel_state_t* ch = &bridge->dopamine.channels[i];

        if (ch->type == HYPO_DA_SIGNAL_TONIC) {
            continue;  /* Already at tonic, no decay needed */
        }

        /* Exponential decay toward tonic baseline */
        float decay = ch->decay_rate * dt_sec;
        float diff = ch->level - ch->tonic_baseline;

        if (fabsf(diff) < 0.01f) {
            /* Close enough to tonic */
            ch->level = ch->tonic_baseline;
            ch->type = HYPO_DA_SIGNAL_TONIC;
        } else {
            /* Decay exponentially */
            ch->level = ch->tonic_baseline + diff * expf(-decay);
        }
    }
}

/*=============================================================================
 * BIO-ASYNC HANDLERS
 *===========================================================================*/

static nimcp_error_t snc_handle_reward_signal(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx) {

    (void)promise;  /* Not used for broadcast messages */

    hypo_snc_bridge_t* bridge = (hypo_snc_bridge_t*)ctx;
    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Message structure: header + reward payload */
    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    size_t payload_offset = sizeof(bio_message_header_t);

    if (msg_size < payload_offset + sizeof(hypo_reward_msg_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Extract reward signal from payload */
    const hypo_reward_msg_t* reward_msg =
        (const hypo_reward_msg_t*)((const uint8_t*)msg + payload_offset);

    /* Convert to reward signal format */
    hypo_reward_signal_t reward = {0};
    reward.reward_signal = reward_msg->reward_signal;
    reward.dopamine_level = reward_msg->dopamine_level;
    reward.alignment_bonus = reward_msg->alignment_bonus;
    reward.alignment_penalty = reward_msg->alignment_penalty;

    /* Process reward */
    hypo_snc_bridge_process_reward(bridge, &reward);

    (void)header;  /* Could check message type here */
    return NIMCP_SUCCESS;
}

static nimcp_error_t snc_handle_value_update(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx) {

    (void)promise;  /* Not used for broadcast messages */

    hypo_snc_bridge_t* bridge = (hypo_snc_bridge_t*)ctx;
    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Message structure: header + float payload */
    size_t payload_offset = sizeof(bio_message_header_t);

    if (msg_size < payload_offset + sizeof(float)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Extract value prediction from payload */
    float next_value = *(const float*)((const uint8_t*)msg + payload_offset);
    hypo_snc_bridge_update_value_prediction(bridge, next_value);

    return NIMCP_SUCCESS;
}
