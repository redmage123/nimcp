/**
 * @file nimcp_training_bio_async_bridge.c
 * @brief Bio-Async Integration Bridge Implementation for Training Module
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "training/integration/nimcp_training_bio_async_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/math/nimcp_math_helpers.h"

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_bio_async_bridge)

#define LOG_MODULE "TRAINING_BIO_ASYNC_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Message handler registration
 */
typedef struct {
    training_bio_message_type_t type;
    training_bio_message_handler_t handler;
    void* user_data;
    bool active;
} training_message_handler_entry_t;

/**
 * @brief Registered worker information
 */
typedef struct {
    uint32_t worker_id;
    bool is_coordinator;
    bool is_ready;
    uint64_t last_seen_us;
    float throughput;
} training_worker_info_t;

/**
 * @brief Gradient sync state
 */
typedef struct {
    float* buffer;
    size_t count;
    uint32_t received_count;
    bool sync_in_progress;
    uint64_t sync_start_us;
} gradient_sync_state_t;

/**
 * @brief Loss aggregation state
 */
typedef struct {
    float accumulated_loss;
    uint32_t received_count;
    bool aggregation_in_progress;
} loss_aggregation_state_t;

/**
 * @brief Bio-async bridge internal structure for training
 */
struct training_bio_async_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    training_bio_bridge_config_t config;

    /* Connections */
    struct nimcp_bio_async* bio_async;
    struct training_integration_hub_struct* hub;
    struct nimcp_gradient_manager_ctx* grad_mgr;
    struct dist_ctx_s* dist_ctx;
    bool connected;

    /* Effects state */
    training_to_bio_async_effects_t outgoing_effects;
    bio_async_to_training_effects_t incoming_effects;

    /* Message handlers */
    training_message_handler_entry_t handlers[TRAIN_BIO_MAX_HANDLERS];
    size_t num_handlers;

    /* Gradient sync callbacks */
    training_gradient_sync_callback_t gradient_callback;
    void* gradient_callback_user_data;

    /* Loss broadcast callbacks */
    training_loss_callback_t loss_callback;
    void* loss_callback_user_data;

    /* Worker tracking */
    training_worker_info_t workers[TRAIN_BIO_MAX_WORKERS];
    uint32_t num_workers;
    uint32_t this_worker_id;
    bool is_coordinator;

    /* Gradient sync state */
    gradient_sync_state_t gradient_sync;

    /* Loss aggregation state */
    loss_aggregation_state_t loss_agg;

    /* Training state */
    uint32_t current_epoch;
    uint32_t current_batch;
    float current_loss;
    float best_loss;
    float current_lr;

    /* Statistics */
    training_bio_bridge_stats_t stats;
};


/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Find worker by ID (internal, must hold mutex)
 */
static training_worker_info_t* find_worker_unlocked(
    training_bio_async_bridge_t* bridge,
    uint32_t worker_id
) {
    for (uint32_t i = 0; i < bridge->num_workers; i++) {
        if (bridge->workers[i].worker_id == worker_id) {
            return &bridge->workers[i];
        }
    }
    return NULL;
}

/**
 * @brief Dispatch message to handlers (internal, must hold mutex)
 */
static void dispatch_message_unlocked(
    training_bio_async_bridge_t* bridge,
    training_bio_message_type_t type,
    const void* payload,
    size_t payload_size
) {
    for (size_t i = 0; i < bridge->num_handlers; i++) {
        if (bridge->handlers[i].active && bridge->handlers[i].type == type) {
            bridge->handlers[i].handler(
                type,
                payload,
                payload_size,
                bridge->handlers[i].user_data
            );
        }
    }
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

training_bio_bridge_config_t training_bio_bridge_default_config(void) {
    training_bio_bridge_config_t config = {0};

    /* Neuromodulator sensitivity */
    config.dopamine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.norepinephrine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.acetylcholine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.serotonin_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Gradient synchronization */
    config.sync_coherence_threshold = TRAIN_BIO_DEFAULT_SYNC_COHERENCE;
    config.sync_timeout_ms = NIMCP_LONG_TIMEOUT_MS; /* 30 seconds */
    config.enable_gradient_compression = false;
    config.compression_threshold = 0.001f;

    /* Phase coupling */
    config.coupling_strength = NIMCP_MODULATION_DEFAULT;
    config.default_oscillation_band = 3; /* BETA band for working memory */

    /* Message routing */
    config.enable_message_logging = false;
    config.message_queue_size = TRAIN_BIO_DEFAULT_QUEUE_SIZE;

    /* Distributed training */
    config.max_workers = TRAIN_BIO_MAX_WORKERS;
    config.straggler_timeout_ms = 60000; /* 60 seconds */
    config.enable_async_gradient_sync = true;

    /* Checkpoint coordination */
    config.enable_checkpoint_waves = true;
    config.checkpoint_wave_threshold = NIMCP_CONFIDENCE_HIGH;

    /* Glial wave parameters */
    config.glial_wave_decay = 0.1f;

    return config;
}

training_bio_async_bridge_t* training_bio_bridge_create(
    const training_bio_bridge_config_t* config
) {
    training_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(training_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = training_bio_bridge_default_config();
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, 0, "training_bio_async") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_bio_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize effects with defaults */
    memset(&bridge->outgoing_effects, 0, sizeof(training_to_bio_async_effects_t));
    memset(&bridge->incoming_effects, 0, sizeof(bio_async_to_training_effects_t));

    bridge->incoming_effects.available_compute = 1.0f;
    bridge->incoming_effects.available_memory = 1.0f;
    bridge->incoming_effects.circadian_factor = 1.0f;

    /* Initialize training state */
    bridge->best_loss = INFINITY;
    bridge->current_lr = 0.001f; /* Default LR */

    /* Initialize statistics */
    bridge->stats.best_loss = INFINITY;

    return bridge;
}

training_bio_async_bridge_t* training_bio_bridge_create_default(void) {
    return training_bio_bridge_create(NULL);
}

void training_bio_bridge_destroy(training_bio_async_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "training_bio_async");
    }

    /* Disconnect first */
    if (bridge->connected) {
        training_bio_bridge_disconnect(bridge);
    }

    /* Free gradient sync buffer */
    if (bridge->gradient_sync.buffer) {
        nimcp_free(bridge->gradient_sync.buffer);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

int training_bio_bridge_connect(
    training_bio_async_bridge_t* bridge,
    struct nimcp_bio_async* bio_async
) {
    if (!bridge || !bio_async) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async = bio_async;

    /* Update connection status */
    bridge->connected = (bridge->bio_async != NULL);

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_connect_hub(
    training_bio_async_bridge_t* bridge,
    struct training_integration_hub_struct* hub
) {
    if (!bridge || !hub) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hub = hub;
    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_connect_gradient_manager(
    training_bio_async_bridge_t* bridge,
    struct nimcp_gradient_manager_ctx* grad_mgr
) {
    if (!bridge || !grad_mgr) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->grad_mgr = grad_mgr;
    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_connect_distributed(
    training_bio_async_bridge_t* bridge,
    struct dist_ctx_s* dist_ctx
) {
    if (!bridge || !dist_ctx) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dist_ctx = dist_ctx;
    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_disconnect(training_bio_async_bridge_t* bridge) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->bio_async = NULL;
    bridge->hub = NULL;
    bridge->grad_mgr = NULL;
    bridge->dist_ctx = NULL;
    bridge->connected = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

bool training_bio_bridge_is_connected(const training_bio_async_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->connected;
}

/*=============================================================================
 * UPDATE
 *===========================================================================*/

int training_bio_bridge_update(
    training_bio_async_bridge_t* bridge,
    float delta_time_ms
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    uint64_t start_time = nimcp_platform_time_monotonic_us();

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_NOT_CONNECTED;
    }

    /* Decay neuromodulator levels over time */
    float decay_factor = expf(-delta_time_ms / 1000.0f * 0.1f);

    if (bridge->outgoing_effects.dopamine_release > 0) {
        bridge->outgoing_effects.dopamine_release *= decay_factor;
        if (bridge->outgoing_effects.dopamine_release < 0.01f) {
            bridge->outgoing_effects.dopamine_release = 0;
        }
    }

    if (bridge->outgoing_effects.norepinephrine_level > 0) {
        bridge->outgoing_effects.norepinephrine_level *= decay_factor;
        if (bridge->outgoing_effects.norepinephrine_level < 0.01f) {
            bridge->outgoing_effects.norepinephrine_level = 0;
            bridge->outgoing_effects.gradient_explosion_alert = false;
            bridge->outgoing_effects.gradient_vanishing_alert = false;
        }
    }

    if (bridge->outgoing_effects.acetylcholine_level > 0) {
        bridge->outgoing_effects.acetylcholine_level *= decay_factor;
        if (bridge->outgoing_effects.acetylcholine_level < 0.01f) {
            bridge->outgoing_effects.acetylcholine_level = 0;
        }
    }

    /* Check for straggler workers */
    if (bridge->num_workers > 1) {
        uint64_t now = nimcp_platform_time_monotonic_us();
        uint64_t timeout_us = bridge->config.straggler_timeout_ms * 1000ULL;

        for (uint32_t i = 0; i < bridge->num_workers; i++) {
            if (bridge->workers[i].is_ready &&
                (now - bridge->workers[i].last_seen_us) > timeout_us) {
                bridge->incoming_effects.straggler_count++;
                bridge->stats.straggler_events++;
            }
        }
    }

    /* Update statistics */
    uint64_t elapsed = nimcp_platform_time_monotonic_us() - start_time;
    bridge->stats.total_update_time_us += elapsed;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

/*=============================================================================
 * MESSAGING
 *===========================================================================*/

int training_bio_bridge_send_message(
    training_bio_async_bridge_t* bridge,
    training_bio_message_type_t message_type,
    const void* payload,
    size_t payload_size
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_NOT_CONNECTED;
    }

    /* Dispatch to local handlers */
    dispatch_message_unlocked(bridge, message_type, payload, payload_size);

    /* In a full implementation, this would send through bio-async */
    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_register_handler(
    training_bio_async_bridge_t* bridge,
    training_bio_message_type_t message_type,
    training_bio_message_handler_t handler,
    void* user_data
) {
    if (!bridge || !handler) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if we can add more handlers */
    if (bridge->num_handlers >= TRAIN_BIO_MAX_HANDLERS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_HANDLERS_FULL;
    }

    /* Check if handler for this type already exists */
    for (size_t i = 0; i < bridge->num_handlers; i++) {
        if (bridge->handlers[i].type == message_type) {
            /* Update existing handler */
            bridge->handlers[i].handler = handler;
            bridge->handlers[i].user_data = user_data;
            bridge->handlers[i].active = true;
            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_OK;
        }
    }

    /* Add new handler */
    training_message_handler_entry_t* entry = &bridge->handlers[bridge->num_handlers++];
    entry->type = message_type;
    entry->handler = handler;
    entry->user_data = user_data;
    entry->active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_unregister_handler(
    training_bio_async_bridge_t* bridge,
    training_bio_message_type_t message_type
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (size_t i = 0; i < bridge->num_handlers; i++) {
        if (bridge->handlers[i].type == message_type) {
            bridge->handlers[i].active = false;
            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_OK;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return TRAIN_BIO_OK; /* Not found is not an error */
}

/*=============================================================================
 * TRAINING COORDINATION
 *===========================================================================*/

int training_bio_bridge_signal_batch_complete(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    uint32_t batch,
    float loss
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_epoch = epoch;
    bridge->current_batch = batch;
    bridge->current_loss = loss;
    bridge->outgoing_effects.completed_batches++;

    /* Update running average loss */
    bridge->stats.avg_loss = bridge->stats.avg_loss * NIMCP_EMA_DECAY_DEFAULT + loss * NIMCP_LEARNING_RATE_DEFAULT;
    NIMCP_EMA_GUARD(bridge->stats.avg_loss, loss);

    /* Check for loss improvement */
    if (loss < bridge->best_loss) {
        float old_best = bridge->best_loss;
        bridge->best_loss = loss;
        bridge->stats.best_loss = loss;
        /* Guard: first batch has old_best=INFINITY → use 1.0 as max improvement */
        bridge->outgoing_effects.loss_improvement =
            (isfinite(old_best) && old_best > 0.0f) ? (old_best - loss) / old_best : 1.0f;

        /* Release dopamine for improvement */
        bridge->outgoing_effects.dopamine_release =
            fminf(0.3f, bridge->outgoing_effects.loss_improvement * bridge->config.dopamine_sensitivity);
    }

    /* Send batch complete message */
    training_loss_payload_t payload = {
        .worker_id = bridge->this_worker_id,
        .batch_id = batch,
        .epoch = epoch,
        .loss_value = loss,
        .primary_loss = loss,
        .auxiliary_loss = 0.0f,
        .regularization_loss = 0.0f
    };

    dispatch_message_unlocked(bridge, TRAIN_MSG_BATCH_COMPLETE, &payload, sizeof(payload));
    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_signal_epoch_complete(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    float avg_loss,
    float val_loss
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_epoch = epoch;
    bridge->outgoing_effects.completed_epochs++;

    /* Calculate training progress */
    bridge->outgoing_effects.training_progress = (float)(epoch + 1) / 100.0f; /* Assume 100 epochs max */
    if (bridge->outgoing_effects.training_progress > 1.0f) {
        bridge->outgoing_effects.training_progress = 1.0f;
    }

    /* Release dopamine for epoch completion */
    bridge->outgoing_effects.dopamine_release =
        TRAIN_BIO_DEFAULT_EPOCH_DOPAMINE * bridge->config.dopamine_sensitivity;

    /* Update serotonin based on convergence */
    if (val_loss >= 0 && val_loss < bridge->best_loss * 1.1f) {
        bridge->outgoing_effects.serotonin_level = 0.7f;
        bridge->outgoing_effects.convergence_detected = (val_loss <= avg_loss * 1.05f);
    }

    /* Prepare message payload */
    typedef struct {
        uint32_t epoch;
        float avg_loss;
        float val_loss;
    } epoch_complete_payload_t;

    epoch_complete_payload_t payload = {
        .epoch = epoch,
        .avg_loss = avg_loss,
        .val_loss = val_loss
    };

    dispatch_message_unlocked(bridge, TRAIN_MSG_EPOCH_DONE, &payload, sizeof(payload));
    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_signal_lr_adjusted(
    training_bio_async_bridge_t* bridge,
    float old_lr,
    float new_lr
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_lr = new_lr;

    /* Trigger LR adjustment wave if significant change */
    float lr_change_ratio = fabsf(new_lr - old_lr) / old_lr;
    if (lr_change_ratio > 0.1f && bridge->config.enable_checkpoint_waves) {
        bridge->outgoing_effects.trigger_lr_adjustment_wave = true;
    }

    /* Prepare message payload */
    typedef struct {
        float old_lr;
        float new_lr;
    } lr_payload_t;

    lr_payload_t payload = {
        .old_lr = old_lr,
        .new_lr = new_lr
    };

    dispatch_message_unlocked(bridge, TRAIN_MSG_LR_ADJUSTED, &payload, sizeof(payload));
    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_signal_early_stopping(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    float best_loss
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.early_stopping_triggered = true;
    bridge->outgoing_effects.convergence_detected = true;

    /* High serotonin for successful completion */
    bridge->outgoing_effects.serotonin_level = 0.9f;

    /* Prepare message payload */
    typedef struct {
        uint32_t epoch;
        float best_loss;
    } early_stop_payload_t;

    early_stop_payload_t payload = {
        .epoch = epoch,
        .best_loss = best_loss
    };

    dispatch_message_unlocked(bridge, TRAIN_MSG_TRAINING_STOP, &payload, sizeof(payload));
    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

/*=============================================================================
 * GRADIENT SYNCHRONIZATION
 *===========================================================================*/

int training_bio_bridge_sync_gradients(
    training_bio_async_bridge_t* bridge,
    uint32_t worker_id,
    const float* gradients,
    size_t gradient_count
) {
    if (!bridge || !gradients) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    if (gradient_count == 0) {
        return TRAIN_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_NOT_CONNECTED;
    }

    /* Allocate or reallocate buffer if needed */
    if (bridge->gradient_sync.buffer == NULL ||
        bridge->gradient_sync.count != gradient_count) {
        if (bridge->gradient_sync.buffer) {
            nimcp_free(bridge->gradient_sync.buffer);
        }
        bridge->gradient_sync.buffer = nimcp_calloc(gradient_count, sizeof(float));
        if (!bridge->gradient_sync.buffer) {
            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_ERROR_ALLOC_FAILED;
        }
        bridge->gradient_sync.count = gradient_count;
    }

    /* Calculate gradient norm */
    float norm = 0.0f;
    for (size_t i = 0; i < gradient_count; i++) {
        norm += gradients[i] * gradients[i];
    }
    norm = sqrtf(norm);

    /* Check for gradient explosion */
    if (norm > 1000.0f) {
        bridge->outgoing_effects.gradient_explosion_alert = true;
        bridge->outgoing_effects.norepinephrine_level =
            TRAIN_BIO_DEFAULT_GRAD_EXPLODE_NE * bridge->config.norepinephrine_sensitivity;

        dispatch_message_unlocked(bridge, TRAIN_MSG_GRADIENT_EXPLOSION, &norm, sizeof(norm));
        bridge->stats.messages_sent++;
    }

    /* Check for vanishing gradients */
    if (norm < 1e-7f) {
        bridge->outgoing_effects.gradient_vanishing_alert = true;
        bridge->outgoing_effects.norepinephrine_level = 0.5f * bridge->config.norepinephrine_sensitivity;

        dispatch_message_unlocked(bridge, TRAIN_MSG_GRADIENT_VANISHING, &norm, sizeof(norm));
        bridge->stats.messages_sent++;
    }

    /* Accumulate gradients */
    for (size_t i = 0; i < gradient_count; i++) {
        bridge->gradient_sync.buffer[i] += gradients[i];
    }
    bridge->gradient_sync.received_count++;
    bridge->gradient_sync.sync_in_progress = true;
    bridge->gradient_sync.sync_start_us = nimcp_platform_time_monotonic_us();

    /* Request phase synchronization */
    bridge->outgoing_effects.request_gradient_sync = true;
    bridge->outgoing_effects.workers_to_sync = bridge->num_workers;

    /* Update statistics */
    bridge->stats.gradient_syncs++;
    bridge->stats.avg_gradient_norm =
        bridge->stats.avg_gradient_norm * 0.9f + norm * 0.1f;

    /* Prepare and dispatch sync message */
    training_gradient_sync_payload_t payload = {
        .worker_id = worker_id,
        .batch_id = bridge->current_batch,
        .epoch = bridge->current_epoch,
        .gradient_norm = norm,
        .gradients = NULL, /* Don't send full gradients in message */
        .gradient_count = gradient_count,
        .compressed = false,
        .compression_ratio = 1.0f
    };

    dispatch_message_unlocked(bridge, TRAIN_MSG_GRADIENT_SYNC_START, &payload, sizeof(payload));
    bridge->stats.messages_sent++;

    /* Notify gradient callback if registered */
    if (bridge->gradient_callback) {
        payload.gradients = (float*)gradients; /* Temporarily set for callback */
        bridge->gradient_callback(&payload, bridge->gradient_callback_user_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_await_gradient_sync(
    training_bio_async_bridge_t* bridge,
    uint32_t timeout_ms,
    float* out_gradients
) {
    if (!bridge || !out_gradients) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    uint64_t start_time = nimcp_platform_time_monotonic_us();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000ULL;

    while (1) {
        nimcp_mutex_lock(bridge->base.mutex);

        /* Check if sync is complete (all workers contributed) */
        if (bridge->gradient_sync.received_count >= bridge->num_workers ||
            bridge->num_workers <= 1) {

            /* Average the gradients */
            if (bridge->gradient_sync.buffer && bridge->gradient_sync.count > 0) {
                float divisor = (float)bridge->gradient_sync.received_count;
                if (divisor < 1.0f) divisor = 1.0f;

                for (size_t i = 0; i < bridge->gradient_sync.count; i++) {
                    out_gradients[i] = bridge->gradient_sync.buffer[i] / divisor;
                }
            }

            /* Calculate sync time */
            uint64_t sync_time_us = nimcp_platform_time_monotonic_us() -
                                    bridge->gradient_sync.sync_start_us;
            float sync_time_ms = (float)sync_time_us / 1000.0f;
            bridge->stats.avg_sync_time_ms =
                bridge->stats.avg_sync_time_ms * 0.9f + sync_time_ms * 0.1f;

            /* Reset sync state */
            bridge->gradient_sync.sync_in_progress = false;
            bridge->gradient_sync.received_count = 0;
            memset(bridge->gradient_sync.buffer, 0,
                   bridge->gradient_sync.count * sizeof(float));

            /* Update effects */
            bridge->incoming_effects.gradient_sync_achieved = true;
            bridge->outgoing_effects.request_gradient_sync = false;

            dispatch_message_unlocked(bridge, TRAIN_MSG_GRADIENT_SYNC_DONE, NULL, 0);
            bridge->stats.messages_sent++;

            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_OK;
        }

        /* Check timeout */
        uint64_t elapsed = nimcp_platform_time_monotonic_us() - start_time;
        if (elapsed >= timeout_us) {
            bridge->stats.gradient_sync_timeouts++;
            bridge->outgoing_effects.sync_timeout_alert = true;
            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_ERROR_SYNC_TIMEOUT;
        }

        nimcp_mutex_unlock(bridge->base.mutex);

        /* Sleep briefly before checking again */
        nimcp_platform_sleep_ms(1);
    }
}

int training_bio_bridge_create_gradient_phase_sync(
    training_bio_async_bridge_t* bridge,
    const uint32_t* worker_ids,
    size_t worker_count,
    struct nimcp_phase_sync** sync
) {
    if (!bridge || !worker_ids || !sync) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_NOT_CONNECTED;
    }

    /* In a full implementation, this would create phase synchronization */
    (void)worker_count;
    *sync = NULL; /* Placeholder */

    bridge->stats.phase_syncs_created++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_wait_gradient_coherent(
    training_bio_async_bridge_t* bridge,
    struct nimcp_phase_sync* sync,
    float coherence_threshold,
    uint32_t timeout_ms
) {
    if (!bridge || !sync) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    /* In a full implementation, this would wait for phase coherence */
    (void)coherence_threshold;
    (void)timeout_ms;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.phase_syncs_achieved++;
    bridge->stats.avg_coherence = coherence_threshold; /* Simplified */
    bridge->incoming_effects.current_sync_coherence = coherence_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_on_gradient_sync(
    training_bio_async_bridge_t* bridge,
    training_gradient_sync_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->gradient_callback = callback;
    bridge->gradient_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

/*=============================================================================
 * LOSS BROADCASTING
 *===========================================================================*/

int training_bio_bridge_broadcast_loss(
    training_bio_async_bridge_t* bridge,
    const training_loss_payload_t* payload
) {
    if (!bridge || !payload) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_NOT_CONNECTED;
    }

    /* Update current loss */
    bridge->current_loss = payload->loss_value;

    /* Start aggregation */
    bridge->loss_agg.accumulated_loss += payload->loss_value;
    bridge->loss_agg.received_count++;
    bridge->loss_agg.aggregation_in_progress = true;

    /* Dispatch message */
    dispatch_message_unlocked(bridge, TRAIN_MSG_LOSS_BROADCAST, payload, sizeof(*payload));
    bridge->stats.messages_sent++;
    bridge->stats.loss_broadcasts++;

    /* Notify callback */
    if (bridge->loss_callback) {
        bridge->loss_callback(payload, bridge->loss_callback_user_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_aggregate_loss(
    training_bio_async_bridge_t* bridge,
    uint32_t timeout_ms,
    float* out_avg_loss
) {
    if (!bridge || !out_avg_loss) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    uint64_t start_time = nimcp_platform_time_monotonic_us();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000ULL;

    while (1) {
        nimcp_mutex_lock(bridge->base.mutex);

        /* Check if aggregation is complete */
        if (bridge->loss_agg.received_count >= bridge->num_workers ||
            bridge->num_workers <= 1) {

            float divisor = (float)bridge->loss_agg.received_count;
            if (divisor < 1.0f) divisor = 1.0f;

            *out_avg_loss = bridge->loss_agg.accumulated_loss / divisor;

            /* Reset aggregation state */
            bridge->loss_agg.accumulated_loss = 0.0f;
            bridge->loss_agg.received_count = 0;
            bridge->loss_agg.aggregation_in_progress = false;

            /* Dispatch aggregate message */
            dispatch_message_unlocked(bridge, TRAIN_MSG_LOSS_AGGREGATE, out_avg_loss, sizeof(float));
            bridge->stats.messages_sent++;

            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_OK;
        }

        /* Check timeout */
        uint64_t elapsed = nimcp_platform_time_monotonic_us() - start_time;
        if (elapsed >= timeout_us) {
            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_ERROR_SYNC_TIMEOUT;
        }

        nimcp_mutex_unlock(bridge->base.mutex);
        nimcp_platform_sleep_ms(1);
    }
}

int training_bio_bridge_on_loss_broadcast(
    training_bio_async_bridge_t* bridge,
    training_loss_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->loss_callback = callback;
    bridge->loss_callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

/*=============================================================================
 * CHECKPOINT COORDINATION
 *===========================================================================*/

int training_bio_bridge_signal_checkpoint_start(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    uint32_t global_step,
    bool is_best
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    training_checkpoint_payload_t payload = {
        .epoch = epoch,
        .global_step = global_step,
        .best_metric = bridge->best_loss,
        .is_best = is_best,
        .checkpoint_path = NULL
    };

    /* Trigger checkpoint wave if enabled */
    if (bridge->config.enable_checkpoint_waves) {
        bridge->outgoing_effects.trigger_checkpoint_wave = true;
    }

    dispatch_message_unlocked(bridge, TRAIN_MSG_CHECKPOINT_START, &payload, sizeof(payload));
    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_signal_checkpoint_complete(
    training_bio_async_bridge_t* bridge,
    const training_checkpoint_payload_t* payload
) {
    if (!bridge || !payload) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.checkpoints_saved++;
    bridge->outgoing_effects.trigger_checkpoint_wave = false;

    /* Release dopamine for checkpoint completion */
    if (payload->is_best) {
        bridge->outgoing_effects.dopamine_release = 0.5f * bridge->config.dopamine_sensitivity;
    }

    dispatch_message_unlocked(bridge, TRAIN_MSG_CHECKPOINT_COMPLETE, payload, sizeof(*payload));
    bridge->stats.messages_sent++;

    if (payload->is_best) {
        dispatch_message_unlocked(bridge, TRAIN_MSG_BEST_MODEL_UPDATE, payload, sizeof(*payload));
        bridge->stats.messages_sent++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_initiate_checkpoint_wave(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    struct nimcp_glial_wave** wave
) {
    if (!bridge || !wave) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_NOT_CONNECTED;
    }

    /* Record the checkpoint request */
    bridge->outgoing_effects.trigger_checkpoint_wave = true;

    /* In a full implementation, this would create a glial wave */
    (void)epoch;
    *wave = NULL; /* Placeholder */

    bridge->stats.glial_waves_initiated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_await_checkpoint_wave(
    training_bio_async_bridge_t* bridge,
    struct nimcp_glial_wave* wave,
    uint32_t timeout_ms
) {
    if (!bridge || !wave) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    /* In a full implementation, this would wait for wave propagation */
    (void)timeout_ms;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.avg_wave_intensity = bridge->config.checkpoint_wave_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

/*=============================================================================
 * DISTRIBUTED TRAINING COORDINATION
 *===========================================================================*/

int training_bio_bridge_register_worker(
    training_bio_async_bridge_t* bridge,
    uint32_t worker_id,
    bool is_coordinator
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_workers >= TRAIN_BIO_MAX_WORKERS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_HANDLERS_FULL;
    }

    /* Check if already registered */
    if (find_worker_unlocked(bridge, worker_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_ALREADY_CONNECTED;
    }

    /* Add worker */
    training_worker_info_t* worker = &bridge->workers[bridge->num_workers++];
    worker->worker_id = worker_id;
    worker->is_coordinator = is_coordinator;
    worker->is_ready = false;
    worker->last_seen_us = nimcp_platform_time_monotonic_us();
    worker->throughput = 0.0f;

    /* Update our identity if this is us */
    if (is_coordinator || bridge->num_workers == 1) {
        bridge->this_worker_id = worker_id;
        bridge->is_coordinator = is_coordinator;
    }

    bridge->incoming_effects.total_workers = bridge->num_workers;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_signal_worker_ready(
    training_bio_async_bridge_t* bridge,
    uint32_t worker_id
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    training_worker_info_t* worker = find_worker_unlocked(bridge, worker_id);
    if (!worker) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_INVALID_WORKER_ID;
    }

    worker->is_ready = true;
    worker->last_seen_us = nimcp_platform_time_monotonic_us();

    /* Count active workers */
    uint32_t active = 0;
    for (uint32_t i = 0; i < bridge->num_workers; i++) {
        if (bridge->workers[i].is_ready) {
            active++;
        }
    }
    bridge->incoming_effects.active_workers = active;

    dispatch_message_unlocked(bridge, TRAIN_MSG_WORKER_READY, &worker_id, sizeof(worker_id));
    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_barrier(
    training_bio_async_bridge_t* bridge,
    uint32_t timeout_ms
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    uint64_t start_time = nimcp_platform_time_monotonic_us();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000ULL;

    while (1) {
        nimcp_mutex_lock(bridge->base.mutex);

        /* Check if all workers are ready */
        uint32_t ready_count = 0;
        for (uint32_t i = 0; i < bridge->num_workers; i++) {
            if (bridge->workers[i].is_ready) {
                ready_count++;
            }
        }

        if (ready_count >= bridge->num_workers) {
            /* All workers ready, reset ready state */
            for (uint32_t i = 0; i < bridge->num_workers; i++) {
                bridge->workers[i].is_ready = false;
            }

            bridge->stats.worker_syncs++;
            bridge->incoming_effects.synchronized_workers = ready_count;

            dispatch_message_unlocked(bridge, TRAIN_MSG_SYNC_BARRIER, NULL, 0);
            bridge->stats.messages_sent++;

            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_OK;
        }

        /* Check timeout */
        uint64_t elapsed = nimcp_platform_time_monotonic_us() - start_time;
        if (elapsed >= timeout_us) {
            nimcp_mutex_unlock(bridge->base.mutex);
            return TRAIN_BIO_ERROR_SYNC_TIMEOUT;
        }

        nimcp_mutex_unlock(bridge->base.mutex);
        nimcp_platform_sleep_ms(1);
    }
}

int training_bio_bridge_broadcast_params(
    training_bio_async_bridge_t* bridge,
    uint32_t source_worker_id,
    const float* params,
    size_t param_count
) {
    if (!bridge || !params) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->connected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return TRAIN_BIO_ERROR_NOT_CONNECTED;
    }

    /* In a full implementation, this would broadcast parameters */
    typedef struct {
        uint32_t source_id;
        size_t count;
    } param_broadcast_info_t;

    param_broadcast_info_t info = {
        .source_id = source_worker_id,
        .count = param_count
    };

    dispatch_message_unlocked(bridge, TRAIN_MSG_PARAM_BROADCAST, &info, sizeof(info));
    bridge->stats.messages_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

uint32_t training_bio_bridge_get_active_workers(
    const training_bio_async_bridge_t* bridge
) {
    if (!bridge) {
        return 0;
    }

    return bridge->incoming_effects.active_workers;
}

uint32_t training_bio_bridge_check_stragglers(
    training_bio_async_bridge_t* bridge,
    uint32_t* straggler_ids,
    uint32_t max_stragglers
) {
    if (!bridge || !straggler_ids || max_stragglers == 0) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t straggler_count = 0;
    uint64_t now = nimcp_platform_time_monotonic_us();
    uint64_t timeout_us = bridge->config.straggler_timeout_ms * 1000ULL;

    for (uint32_t i = 0; i < bridge->num_workers && straggler_count < max_stragglers; i++) {
        if ((now - bridge->workers[i].last_seen_us) > timeout_us) {
            straggler_ids[straggler_count++] = bridge->workers[i].worker_id;

            /* Dispatch straggler warning */
            dispatch_message_unlocked(bridge, TRAIN_MSG_WORKER_STRAGGLER,
                                      &bridge->workers[i].worker_id, sizeof(uint32_t));
            bridge->stats.messages_sent++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return straggler_count;
}

/*=============================================================================
 * NEUROMODULATOR CONTROL
 *===========================================================================*/

int training_bio_bridge_release_dopamine(
    training_bio_async_bridge_t* bridge,
    float amount,
    training_bio_message_type_t trigger
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    if (amount < 0.0f || amount > 1.0f) {
        return TRAIN_BIO_ERROR_INVALID_CONFIG;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.dopamine_release =
        amount * bridge->config.dopamine_sensitivity;

    (void)trigger;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_signal_priority(
    training_bio_async_bridge_t* bridge,
    float priority,
    training_bio_message_type_t alert_type
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.norepinephrine_level =
        priority * bridge->config.norepinephrine_sensitivity;

    (void)alert_type;

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_modulate_attention(
    training_bio_async_bridge_t* bridge,
    float attention,
    int32_t layer_idx
) {
    if (!bridge) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->outgoing_effects.acetylcholine_level =
        attention * bridge->config.acetylcholine_sensitivity;

    if (layer_idx >= 0) {
        bridge->outgoing_effects.focused_layer = (uint32_t)layer_idx;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return TRAIN_BIO_OK;
}

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

int training_bio_bridge_get_outgoing_effects(
    const training_bio_async_bridge_t* bridge,
    training_to_bio_async_effects_t* effects
) {
    if (!bridge || !effects) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((training_bio_async_bridge_t*)bridge)->base.mutex);
    *effects = bridge->outgoing_effects;
    nimcp_mutex_unlock(((training_bio_async_bridge_t*)bridge)->base.mutex);

    return TRAIN_BIO_OK;
}

int training_bio_bridge_get_incoming_effects(
    const training_bio_async_bridge_t* bridge,
    bio_async_to_training_effects_t* effects
) {
    if (!bridge || !effects) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((training_bio_async_bridge_t*)bridge)->base.mutex);
    *effects = bridge->incoming_effects;
    nimcp_mutex_unlock(((training_bio_async_bridge_t*)bridge)->base.mutex);

    return TRAIN_BIO_OK;
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int training_bio_bridge_get_stats(
    const training_bio_async_bridge_t* bridge,
    training_bio_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        return TRAIN_BIO_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((training_bio_async_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((training_bio_async_bridge_t*)bridge)->base.mutex);

    return TRAIN_BIO_OK;
}

void training_bio_bridge_reset_stats(training_bio_async_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(training_bio_bridge_stats_t));
    bridge->stats.best_loss = INFINITY;
    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* training_bio_message_type_name(training_bio_message_type_t type) {
    switch (type) {
        /* Batch/Epoch messages */
        case TRAIN_MSG_BATCH_START:         return "BATCH_START";
        case TRAIN_MSG_BATCH_COMPLETE:      return "BATCH_COMPLETE";
        case TRAIN_MSG_EPOCH_START:         return "EPOCH_START";
        case TRAIN_MSG_EPOCH_DONE:          return "EPOCH_DONE";
        case TRAIN_MSG_TRAINING_START:      return "TRAINING_START";
        case TRAIN_MSG_TRAINING_STOP:       return "TRAINING_STOP";

        /* Gradient messages */
        case TRAIN_MSG_GRADIENT_COMPUTED:   return "GRADIENT_COMPUTED";
        case TRAIN_MSG_GRADIENT_UPDATE:     return "GRADIENT_UPDATE";
        case TRAIN_MSG_GRADIENT_SYNC_START: return "GRADIENT_SYNC_START";
        case TRAIN_MSG_GRADIENT_SYNC_DONE:  return "GRADIENT_SYNC_DONE";
        case TRAIN_MSG_GRADIENT_EXPLOSION:  return "GRADIENT_EXPLOSION";
        case TRAIN_MSG_GRADIENT_VANISHING:  return "GRADIENT_VANISHING";

        /* Checkpoint messages */
        case TRAIN_MSG_CHECKPOINT_START:    return "CHECKPOINT_START";
        case TRAIN_MSG_CHECKPOINT_COMPLETE: return "CHECKPOINT_COMPLETE";
        case TRAIN_MSG_CHECKPOINT_LOAD:     return "CHECKPOINT_LOAD";
        case TRAIN_MSG_CHECKPOINT_LOADED:   return "CHECKPOINT_LOADED";
        case TRAIN_MSG_BEST_MODEL_UPDATE:   return "BEST_MODEL_UPDATE";

        /* Loss messages */
        case TRAIN_MSG_LOSS_COMPUTED:       return "LOSS_COMPUTED";
        case TRAIN_MSG_LOSS_BROADCAST:      return "LOSS_BROADCAST";
        case TRAIN_MSG_LOSS_AGGREGATE:      return "LOSS_AGGREGATE";
        case TRAIN_MSG_VALIDATION_LOSS:     return "VALIDATION_LOSS";
        case TRAIN_MSG_LOSS_IMPROVED:       return "LOSS_IMPROVED";
        case TRAIN_MSG_LOSS_PLATEAU:        return "LOSS_PLATEAU";

        /* Distributed messages */
        case TRAIN_MSG_WORKER_READY:        return "WORKER_READY";
        case TRAIN_MSG_WORKER_DONE:         return "WORKER_DONE";
        case TRAIN_MSG_SYNC_BARRIER:        return "SYNC_BARRIER";
        case TRAIN_MSG_PARAM_BROADCAST:     return "PARAM_BROADCAST";
        case TRAIN_MSG_ALL_REDUCE_START:    return "ALL_REDUCE_START";
        case TRAIN_MSG_ALL_REDUCE_DONE:     return "ALL_REDUCE_DONE";
        case TRAIN_MSG_WORKER_STRAGGLER:    return "WORKER_STRAGGLER";

        /* Learning rate messages */
        case TRAIN_MSG_LR_ADJUSTED:         return "LR_ADJUSTED";
        case TRAIN_MSG_LR_WARMUP_DONE:      return "LR_WARMUP_DONE";
        case TRAIN_MSG_LR_DECAY_STEP:       return "LR_DECAY_STEP";
        case TRAIN_MSG_LR_SCHEDULE_CHANGE:  return "LR_SCHEDULE_CHANGE";

        default:                            return "UNKNOWN";
    }
}

const char* training_bio_error_name(training_bio_error_t error) {
    switch (error) {
        case TRAIN_BIO_OK:                      return "OK";
        case TRAIN_BIO_ERROR_NULL_POINTER:      return "NULL_POINTER";
        case TRAIN_BIO_ERROR_NOT_CONNECTED:     return "NOT_CONNECTED";
        case TRAIN_BIO_ERROR_INVALID_CONFIG:    return "INVALID_CONFIG";
        case TRAIN_BIO_ERROR_HANDLERS_FULL:     return "HANDLERS_FULL";
        case TRAIN_BIO_ERROR_SYNC_TIMEOUT:      return "SYNC_TIMEOUT";
        case TRAIN_BIO_ERROR_GRADIENT_OVERFLOW: return "GRADIENT_OVERFLOW";
        case TRAIN_BIO_ERROR_CHECKPOINT_FAILED: return "CHECKPOINT_FAILED";
        case TRAIN_BIO_ERROR_WORKER_DISCONNECT: return "WORKER_DISCONNECT";
        case TRAIN_BIO_ERROR_QUEUE_FULL:        return "QUEUE_FULL";
        case TRAIN_BIO_ERROR_ALLOC_FAILED:      return "ALLOC_FAILED";
        case TRAIN_BIO_ERROR_ALREADY_CONNECTED: return "ALREADY_CONNECTED";
        case TRAIN_BIO_ERROR_INVALID_WORKER_ID: return "INVALID_WORKER_ID";
        case TRAIN_BIO_ERROR_PHASE_INCOHERENT:  return "PHASE_INCOHERENT";
        case TRAIN_BIO_ERROR_WAVE_TIMEOUT:      return "WAVE_TIMEOUT";
        default:                                return "UNKNOWN";
    }
}
