/**
 * @file nimcp_omni_training_bridge.c
 * @brief Implementation of Omnidirectional Inference to Training Module Bridge
 */

#include "middleware/training/nimcp_omni_training_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/memory/nimcp_temporal_replay.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_training_bridge)

#define LOG_MODULE "OMNI_TRAINING_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static omni_difficulty_t compute_difficulty(float avg_pe,
                                            const omni_training_config_t* config) {
    if (avg_pe < config->easy_threshold) {
        return OMNI_DIFFICULTY_EASY;
    } else if (avg_pe < config->medium_threshold) {
        return OMNI_DIFFICULTY_MEDIUM;
    } else {
        return OMNI_DIFFICULTY_HARD;
    }
}

static float get_lr_scale_for_difficulty(omni_difficulty_t difficulty,
                                         const omni_training_config_t* config) {
    switch (difficulty) {
        case OMNI_DIFFICULTY_EASY: return 1.0f;
        case OMNI_DIFFICULTY_MEDIUM: return 0.8f;
        case OMNI_DIFFICULTY_HARD: return config->hard_lr_scale;
        case OMNI_DIFFICULTY_ADAPTIVE: return 0.9f;
        default: return 1.0f;
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_training_default_config(omni_training_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_default_config: config is NULL");

    memset(config, 0, sizeof(omni_training_config_t));

    config->mode = OMNI_TRAIN_MODE_MIXED;
    config->enable_curriculum = true;

    config->replay_batch_size = OMNI_TRAINING_DEFAULT_BATCH_SIZE;
    config->replay_ratio = OMNI_TRAINING_DEFAULT_REPLAY_RATIO;
    config->use_priority_replay = true;
    config->priority_exponent = 0.6f;

    config->contrastive_margin = 0.5f;
    config->contrastive_temperature = 0.07f;
    config->num_negatives = 4;

    config->grad_source = OMNI_GRAD_BIDIRECTIONAL;
    config->forward_grad_weight = 1.0f;
    config->backward_grad_weight = 0.8f;
    config->enable_gradient_clipping = true;
    config->gradient_clip_norm = 1.0f;

    config->easy_threshold = OMNI_TRAINING_CURRICULUM_EASY_THRESHOLD;
    config->medium_threshold = OMNI_TRAINING_CURRICULUM_MEDIUM_THRESHOLD;
    config->hard_lr_scale = 0.5f;

    config->enable_bio_async = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_training_bridge_t* omni_training_bridge_create(
    const omni_training_config_t* config) {

    omni_training_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_training_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(omni_training_config_t));
    } else {
        omni_training_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "omni_training") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_training_default_config: bridge->base is NULL");
        return NULL;
    }

    memset(&bridge->stats, 0, sizeof(omni_training_stats_t));

    return bridge;
}

void omni_training_bridge_destroy(omni_training_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_training");

    if (bridge->gradient_buffer) {
        nimcp_free(bridge->gradient_buffer);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_training_connect_jepa(omni_training_bridge_t* bridge,
                                jepa_bidirectional_t* jepa) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_connect: bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->jepa = jepa;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_connect_hopfield(omni_training_bridge_t* bridge,
                                    hopfield_memory_t* hopfield) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_connect_hopfield: bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hopfield = hopfield;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_connect_pred_hier(omni_training_bridge_t* bridge,
                                     predictive_hierarchy_t* pred_hier) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_connect_pred_hier: bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pred_hier = pred_hier;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_connect_replay(omni_training_bridge_t* bridge,
                                  temporal_replay_t* replay) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_connect_replay: bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->replay = replay;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_connect_gradient_manager(omni_training_bridge_t* bridge,
                                            gradient_manager_t* grad_mgr) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_connect_gradient_manager: bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->grad_mgr = grad_mgr;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_connect_training_ctx(omni_training_bridge_t* bridge,
                                        nimcp_brain_training_ctx_t* train_ctx) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_connect_training_ctx: bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->train_ctx = train_ctx;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_training_update(omni_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_update: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute omni → training effects */
    float avg_pe = 0.0f;
    uint32_t num_levels = 0;

    if (bridge->pred_hier) {
        num_levels = pred_hier_num_levels(bridge->pred_hier);
        float fe = pred_hier_compute_free_energy(bridge->pred_hier);
        avg_pe = isnan(fe) ? 0.0f : fe / num_levels;
    }

    bridge->omni_effects.num_levels = num_levels;
    bridge->omni_effects.total_loss = avg_pe;
    bridge->omni_effects.difficulty = compute_difficulty(avg_pe, &bridge->config);
    bridge->omni_effects.learning_rate_scale =
        get_lr_scale_for_difficulty(bridge->omni_effects.difficulty, &bridge->config);

    /* Compute training → omni effects */
    bridge->training_effects.weight_update_magnitude = 0.0f;
    bridge->training_effects.precision_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    bridge->training_effects.enable_backward_training =
        (bridge->config.grad_source == OMNI_GRAD_BIDIRECTIONAL ||
         bridge->config.grad_source == OMNI_GRAD_BACKWARD);
    bridge->training_effects.enable_lateral_training = false;
    bridge->training_effects.replay_batch_size = bridge->config.replay_batch_size;
    bridge->training_effects.replay_sequence_length = 32;

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->stats.avg_pred_error =
        (bridge->stats.avg_pred_error * (bridge->stats.total_updates - 1) + avg_pe) /
        bridge->stats.total_updates;

    switch (bridge->omni_effects.difficulty) {
        case OMNI_DIFFICULTY_EASY: bridge->stats.easy_samples++; break;
        case OMNI_DIFFICULTY_MEDIUM: bridge->stats.medium_samples++; break;
        case OMNI_DIFFICULTY_HARD: bridge->stats.hard_samples++; break;
        default: break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_step(omni_training_bridge_t* bridge, float* loss) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_step: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    float step_loss = 0.0f;

    if (bridge->pred_hier && bridge->config.mode != OMNI_TRAIN_MODE_INFERENCE) {
        pred_hier_set_training(bridge->pred_hier, true);
        pred_hier_learn_step(bridge->pred_hier, NULL, &step_loss);
    }

    bridge->stats.training_steps++;
    bridge->stats.online_steps++;

    if (loss) *loss = step_loss;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_replay_step(omni_training_bridge_t* bridge, float* loss) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_replay_step: bridge is NULL");
    NIMCP_CHECK_THROW(bridge->replay, NIMCP_ERROR_NOT_INITIALIZED,
                      "omni_training_replay_step: replay not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    float replay_loss = 0.0f;

    /* Sample from replay buffer */
    if (temporal_replay_count(bridge->replay) > bridge->config.replay_batch_size) {
        replay_batch_t* batch = replay_batch_create(
            bridge->config.replay_batch_size,
            64,  /* Default state dim - would be configured */
            0);

        if (batch) {
            replay_mode_t mode = bridge->config.use_priority_replay ?
                                 REPLAY_MODE_PRIORITY : REPLAY_MODE_RANDOM;

            if (temporal_replay_sample(bridge->replay, mode,
                                       bridge->config.replay_batch_size, batch) == NIMCP_SUCCESS) {
                /* Training would happen here with the batch */
                replay_loss = 0.01f;  /* Placeholder */
            }

            replay_batch_destroy(batch);
        }
    }

    bridge->stats.training_steps++;
    bridge->stats.replay_steps++;
    bridge->stats.avg_temporal_loss =
        (bridge->stats.avg_temporal_loss * (bridge->stats.replay_steps - 1) + replay_loss) /
        bridge->stats.replay_steps;

    if (loss) *loss = replay_loss;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_contrastive_step(omni_training_bridge_t* bridge, float* loss) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_contrastive_step: bridge is NULL");
    NIMCP_CHECK_THROW(bridge->hopfield, NIMCP_ERROR_NOT_INITIALIZED,
                      "omni_training_contrastive_step: hopfield not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    float contrastive_loss = 0.0f;

    /* Contrastive learning with Hopfield patterns would be implemented here */
    if (hopfield_memory_pattern_count(bridge->hopfield) > 0) {
        contrastive_loss = 0.01f;  /* Placeholder */
    }

    bridge->stats.training_steps++;
    bridge->stats.contrastive_steps++;
    bridge->stats.avg_contrastive_loss =
        (bridge->stats.avg_contrastive_loss * (bridge->stats.contrastive_steps - 1) +
         contrastive_loss) / bridge->stats.contrastive_steps;

    if (loss) *loss = contrastive_loss;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Gradient API
 * ============================================================================ */

int omni_training_get_pe_gradients(const omni_training_bridge_t* bridge,
                                    uint32_t level,
                                    float* gradients) {
    NIMCP_CHECK_THROW(bridge && gradients, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_get_pe_gradients: NULL argument");

    nimcp_mutex_lock(((omni_training_bridge_t*)bridge)->base.mutex);

    if (bridge->pred_hier) {
        pred_hier_get_error(bridge->pred_hier, level, gradients);
    }

    nimcp_mutex_unlock(((omni_training_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_accumulate_gradients(omni_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_accumulate_gradients: bridge is NULL");
    /* Gradient accumulation would be implemented here */
    return NIMCP_SUCCESS;
}

int omni_training_apply_gradients(omni_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_apply_gradients: bridge is NULL");
    /* Gradient application would be implemented here */
    return NIMCP_SUCCESS;
}

int omni_training_zero_gradients(omni_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_zero_gradients: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->gradient_buffer && bridge->gradient_buffer_size > 0) {
        memset(bridge->gradient_buffer, 0, bridge->gradient_buffer_size * sizeof(float));
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Curriculum API
 * ============================================================================ */

omni_difficulty_t omni_training_get_difficulty(
    const omni_training_bridge_t* bridge) {
    if (!bridge) return OMNI_DIFFICULTY_EASY;
    return bridge->omni_effects.difficulty;
}

int omni_training_set_difficulty(omni_training_bridge_t* bridge,
                                  omni_difficulty_t difficulty) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_set_difficulty: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->omni_effects.difficulty = difficulty;
    bridge->omni_effects.learning_rate_scale =
        get_lr_scale_for_difficulty(difficulty, &bridge->config);
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float omni_training_get_lr_scale(const omni_training_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->omni_effects.learning_rate_scale;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_training_get_omni_effects(const omni_training_bridge_t* bridge,
                                    omni_to_training_effects_t* effects) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_get_omni_effects: NULL argument");
    nimcp_mutex_lock(((omni_training_bridge_t*)bridge)->base.mutex);
    memcpy(effects, &bridge->omni_effects, sizeof(omni_to_training_effects_t));
    nimcp_mutex_unlock(((omni_training_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_get_training_effects(const omni_training_bridge_t* bridge,
                                        training_to_omni_effects_t* effects) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_get_training_effects: NULL argument");
    nimcp_mutex_lock(((omni_training_bridge_t*)bridge)->base.mutex);
    memcpy(effects, &bridge->training_effects, sizeof(training_to_omni_effects_t));
    nimcp_mutex_unlock(((omni_training_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_get_stats(const omni_training_bridge_t* bridge,
                             omni_training_stats_t* stats) {
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_get_stats: NULL argument");
    nimcp_mutex_lock(((omni_training_bridge_t*)bridge)->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(omni_training_stats_t));
    nimcp_mutex_unlock(((omni_training_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_training_reset_stats(omni_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_reset_stats: bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_training_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_training_gradient_ready(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_training_bridge_t* bridge = (omni_training_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM,
                      "handle_training_gradient_ready: NULL argument");

    /* Apply gradients when ready */
    omni_training_apply_gradients(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_training_replay_trigger(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_training_bridge_t* bridge = (omni_training_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM,
                      "handle_training_replay_trigger: NULL argument");

    /* Trigger replay step */
    float loss = 0.0f;
    omni_training_replay_step(bridge, &loss);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_training_connect_bio_async(omni_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_connect_bio_async: bridge is NULL");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_TRAINING_BRIDGE,
        .module_name = "omni_training_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_OPERATION_FAILED,
                      "omni_training_connect_bio_async: failed to register module");

    bridge->bio_context = ctx;

    bio_router_register_handler(ctx, BIO_MSG_TRAINING_STEP_REQUEST,
                                 handle_training_gradient_ready);
    bio_router_register_handler(ctx, BIO_MSG_TRAINING_STEP_COMPLETE,
                                 handle_training_replay_trigger);

    bridge->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_training_disconnect_bio_async(omni_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM,
                      "omni_training_disconnect_bio_async: bridge is NULL");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_training_is_bio_async_connected(const omni_training_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_training_mode_to_string(omni_training_mode_t mode) {
    switch (mode) {
        case OMNI_TRAIN_MODE_INFERENCE: return "INFERENCE";
        case OMNI_TRAIN_MODE_ONLINE: return "ONLINE";
        case OMNI_TRAIN_MODE_REPLAY: return "REPLAY";
        case OMNI_TRAIN_MODE_CONTRASTIVE: return "CONTRASTIVE";
        case OMNI_TRAIN_MODE_MIXED: return "MIXED";
        default: return "UNKNOWN";
    }
}

const char* omni_training_difficulty_to_string(omni_difficulty_t difficulty) {
    switch (difficulty) {
        case OMNI_DIFFICULTY_EASY: return "EASY";
        case OMNI_DIFFICULTY_MEDIUM: return "MEDIUM";
        case OMNI_DIFFICULTY_HARD: return "HARD";
        case OMNI_DIFFICULTY_ADAPTIVE: return "ADAPTIVE";
        default: return "UNKNOWN";
    }
}

const char* omni_training_grad_source_to_string(omni_gradient_source_t source) {
    switch (source) {
        case OMNI_GRAD_FORWARD: return "FORWARD";
        case OMNI_GRAD_BACKWARD: return "BACKWARD";
        case OMNI_GRAD_BIDIRECTIONAL: return "BIDIRECTIONAL";
        case OMNI_GRAD_CONTRASTIVE: return "CONTRASTIVE";
        case OMNI_GRAD_TEMPORAL: return "TEMPORAL";
        default: return "UNKNOWN";
    }
}
