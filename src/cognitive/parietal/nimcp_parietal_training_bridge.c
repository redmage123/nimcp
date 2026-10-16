/**
 * @file nimcp_parietal_training_bridge.c
 * @brief Implementation of Parietal-Training Integration Bridge
 * @version 1.0.0
 * @date 2026-01-20
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/parietal/nimcp_parietal_training_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_MODULE "parietal_training"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(parietal_training_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from parietal_training_bridge module (instance-level) */
static inline void parietal_training_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_parietal_training_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_parietal_training_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_parietal_training_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pending weight update
 */
typedef struct {
    parietal_learning_domain_t domain;
    float learning_rate;
    float gradient;
    uint64_t timestamp;
} pending_update_t;

/**
 * @brief Parietal-Training bridge structure
 */
struct parietal_training_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    parietal_train_state_t state;

    /* Configuration */
    parietal_training_config_t config;

    /* Connections */
    parietal_cortex_adapter_t* parietal;
    nimcp_brain_training_ctx_t* training;
    parietal_plasticity_bridge_t* plasticity;
    bio_router_t bio_router;

    /* Callbacks */
    parietal_learning_callback_t learning_callback;
    void* learning_callback_data;
    parietal_weight_update_callback_t update_callback;
    void* update_callback_data;

    /* Batch updates */
    pending_update_t* pending_updates;
    uint32_t pending_count;
    uint32_t pending_capacity;

    /* Statistics */
    parietal_training_stats_t stats;

    /* Synchronization */
    nimcp_mutex_t* mutex;

    /* Last update time per domain */
    uint64_t last_update_time[PARIETAL_DOMAIN_COUNT];

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(parietal_training_bridge)

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void training_event_handler(const void* event, void* user_data);
static int apply_weight_update(parietal_training_bridge_t* bridge,
                               parietal_learning_domain_t domain,
                               float learning_rate);
static float compute_domain_gradient(parietal_training_bridge_t* bridge,
                                     parietal_learning_domain_t domain,
                                     float loss_delta);
static int flush_batch_unlocked(parietal_training_bridge_t* bridge);

/* ============================================================================
 * Domain Names
 * ============================================================================ */

static const char* domain_names[] = {
    "coordinate_transform",
    "reaching_accuracy",
    "attention_allocation",
    "numerical_magnitude",
    "mental_rotation",
    "multisensory_binding",
    "body_schema",
    "pattern_detection"
};

static const char* response_names[] = {
    "none",
    "update_weights",
    "adjust_threshold",
    "modulate_gain",
    "consolidate"
};

static const char* state_names[] = {
    "uninitialized",
    "initialized",
    "connected",
    "learning",
    "paused",
    "error"
};

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int parietal_training_default_config(parietal_training_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_de", 0.0f);


    memset(config, 0, sizeof(*config));

    /* Global parameters */
    config->base_learning_rate = PARIETAL_TRAIN_DEFAULT_LR;
    config->learning_rate_decay = 0.99f;
    config->min_learning_rate = 1e-6f;

    /* Enable all domains with default settings */
    for (int i = 0; i < PARIETAL_DOMAIN_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PARIETAL_DOMAIN_COUNT > 256) {
            parietal_training_bridge_heartbeat("parietal_tra_loop",
                             (float)(i + 1) / (float)PARIETAL_DOMAIN_COUNT);
        }

        config->domains[i].enabled = true;
        config->domains[i].learning_rate = PARIETAL_TRAIN_DEFAULT_LR;
        config->domains[i].momentum = NIMCP_MOMENTUM_DEFAULT;
        config->domains[i].weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT;
        config->domains[i].min_confidence = NIMCP_CONFIDENCE_MEDIUM;
        config->domains[i].use_reward_modulation = true;
    }

    /* Training integration */
    config->register_with_training = true;
    config->receive_loss_signals = true;
    config->receive_gradient_signals = true;

    /* Plasticity integration */
    config->connect_to_plasticity = true;
    config->enable_stdp_learning = true;
    config->enable_bcm_learning = true;
    config->enable_homeostatic = true;

    /* Event handling */
    config->batch_weight_updates = true;
    config->update_batch_size = 32;
    config->update_interval_ms = 10;

    /* Bio-async */
    config->enable_bio_async = false;
    config->bio_router = NULL;

    /* Monitoring */
    config->verbose_logging = false;
    config->track_learning_progress = true;

    return 0;
}

parietal_training_bridge_t* parietal_training_create(
    const parietal_training_config_t* config,
    parietal_cortex_adapter_t* parietal,
    nimcp_brain_training_ctx_t* training
) {
    if (!parietal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_cr", 0.0f);


    parietal_training_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->magic = PARIETAL_TRAINING_BRIDGE_MAGIC;
    bridge->state = PARIETAL_TRAIN_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        parietal_training_default_config(&bridge->config);
    }

    /* Store connections */
    bridge->parietal = parietal;
    bridge->training = training;

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, 0, "parietal_training") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_training_create: validation failed");
        return NULL;
    }

    /* Allocate batch buffer */
    if (bridge->config.batch_weight_updates) {
        bridge->pending_capacity = bridge->config.update_batch_size * 2;
        bridge->pending_updates = nimcp_calloc(bridge->pending_capacity,
                                               sizeof(pending_update_t));
        if (!bridge->pending_updates) {
            nimcp_mutex_destroy(bridge->base.mutex);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_training_create: bridge->pending_updates is NULL");
            return NULL;
        }
    }

    /* Initialize statistics */
    bridge->stats.best_loss = INFINITY;
    bridge->stats.current_loss = INFINITY;

    bridge->state = PARIETAL_TRAIN_STATE_INITIALIZED;

    /* Connect to training if provided */
    if (training && bridge->config.register_with_training) {
        if (parietal_training_connect(bridge, training) == 0) {
            bridge->state = PARIETAL_TRAIN_STATE_CONNECTED;
        }
    }

    /* Connect bio-async if configured */
    if (bridge->config.enable_bio_async && bridge->config.bio_router) {
        parietal_training_connect_bio_async(bridge, bridge->config.bio_router);
    }

    return bridge;
}

void parietal_training_destroy(parietal_training_bridge_t* bridge) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "parietal_training");
    }

    /* Disconnect from training */
    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_de", 0.0f);


    if (bridge->training) {
        parietal_training_disconnect(bridge);
    }

    /* Clean up resources */
    if (bridge->pending_updates) {
        nimcp_free(bridge->pending_updates);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int parietal_training_connect(
    parietal_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC || !training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_training_connect: required parameter is NULL (bridge, training)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training = training;

    /* Register callback with training system */
    /* Note: This uses the training callback registration mechanism */
    /* The actual registration depends on the training API */

    bridge->stats.training_connected = true;

    if (bridge->state == PARIETAL_TRAIN_STATE_INITIALIZED) {
        bridge->state = PARIETAL_TRAIN_STATE_CONNECTED;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_training_disconnect(parietal_training_bridge_t* bridge) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_di", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Flush any pending updates */
    if (bridge->pending_count > 0) {
        flush_batch_unlocked(bridge);
    }

    bridge->training = NULL;
    bridge->stats.training_connected = false;

    if (bridge->state == PARIETAL_TRAIN_STATE_CONNECTED ||
        bridge->state == PARIETAL_TRAIN_STATE_LEARNING) {
        bridge->state = PARIETAL_TRAIN_STATE_INITIALIZED;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_training_connect_plasticity(
    parietal_training_bridge_t* bridge,
    parietal_plasticity_bridge_t* plasticity
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_connect_plasticity: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->plasticity = plasticity;
    bridge->stats.plasticity_connected = (plasticity != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_training_connect_bio_async(
    parietal_training_bridge_t* bridge,
    bio_router_t router
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_router = router;
    bridge->stats.bio_async_connected = (router != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Learning Implementation
 * ============================================================================ */

parietal_train_response_t parietal_training_process_signal(
    parietal_training_bridge_t* bridge,
    const parietal_learning_signal_t* signal
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC || !signal) {
        return PARIETAL_TRAIN_RESPONSE_NONE;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_pr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    parietal_train_response_t response = PARIETAL_TRAIN_RESPONSE_NONE;

    /* Check if domain is enabled */
    if (signal->domain >= PARIETAL_DOMAIN_COUNT ||
        !bridge->config.domains[signal->domain].enabled) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return response;
    }

    /* Update statistics */
    bridge->stats.total_events++;
    bridge->stats.domain_stats[signal->domain].training_events++;
    bridge->stats.current_loss = signal->loss_value;

    if (signal->loss_value < bridge->stats.best_loss) {
        bridge->stats.best_loss = signal->loss_value;
    }

    /* Determine response based on signal */
    if (signal->loss_delta < 0) {
        /* Loss decreased - reinforce current learning */
        response = PARIETAL_TRAIN_RESPONSE_UPDATE_WEIGHTS;
    } else if (signal->loss_delta > 0.1f) {
        /* Loss increased significantly - adjust */
        response = PARIETAL_TRAIN_RESPONSE_ADJUST_THRESHOLD;
    }

    /* Check reward signal for modulation */
    if (bridge->config.domains[signal->domain].use_reward_modulation &&
        signal->reward_signal > 0.5f) {
        response = PARIETAL_TRAIN_RESPONSE_UPDATE_WEIGHTS;
    }

    /* Apply response */
    if (response == PARIETAL_TRAIN_RESPONSE_UPDATE_WEIGHTS) {
        float lr = bridge->config.domains[signal->domain].learning_rate;
        lr *= signal->learning_rate / bridge->config.base_learning_rate;

        if (bridge->config.batch_weight_updates &&
            bridge->pending_count < bridge->pending_capacity) {
            /* Queue for batch update */
            pending_update_t* update = &bridge->pending_updates[bridge->pending_count++];
            update->domain = signal->domain;
            update->learning_rate = lr;
            update->gradient = compute_domain_gradient(bridge, signal->domain,
                                                       signal->loss_delta);
            update->timestamp = nimcp_time_get_us();

            /* Flush if batch is full */
            if (bridge->pending_count >= bridge->config.update_batch_size) {
                flush_batch_unlocked(bridge);
            }
        } else {
            /* Apply immediately */
            apply_weight_update(bridge, signal->domain, lr);
        }
    }

    /* Invoke callback */
    if (bridge->learning_callback) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->learning_callback(signal, bridge->learning_callback_data);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    bridge->state = PARIETAL_TRAIN_STATE_LEARNING;

    nimcp_mutex_unlock(bridge->base.mutex);

    return response;
}

int parietal_training_update_weights(
    parietal_training_bridge_t* bridge,
    parietal_learning_domain_t domain,
    float learning_rate
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_update_weights: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_up", 0.0f);


    if (domain >= PARIETAL_DOMAIN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "parietal_training_update_weights: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    int result = apply_weight_update(bridge, domain, learning_rate);
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/**
 * @brief Internal unlocked version of flush_batch - must be called with mutex held
 */
static int flush_batch_unlocked(parietal_training_bridge_t* bridge) {
    int updates_applied = 0;

    for (uint32_t i = 0; i < bridge->pending_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->pending_count > 256) {
            parietal_training_bridge_heartbeat("parietal_tra_loop",
                             (float)(i + 1) / (float)bridge->pending_count);
        }

        pending_update_t* update = &bridge->pending_updates[i];
        if (apply_weight_update(bridge, update->domain, update->learning_rate) == 0) {
            updates_applied++;
        }
    }

    bridge->pending_count = 0;
    bridge->stats.batches_processed++;

    return updates_applied;
}

int parietal_training_flush_batch(parietal_training_bridge_t* bridge) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_flush_batch: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_fl", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    int updates_applied = flush_batch_unlocked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return updates_applied;
}

int parietal_training_set_domain_lr(
    parietal_training_bridge_t* bridge,
    parietal_learning_domain_t domain,
    float learning_rate
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_set_domain_lr: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_se", 0.0f);


    if (domain >= PARIETAL_DOMAIN_COUNT || learning_rate < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "parietal_training_set_domain_lr: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.domains[domain].learning_rate = learning_rate;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_training_set_domain_enabled(
    parietal_training_bridge_t* bridge,
    parietal_learning_domain_t domain,
    bool enabled
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_set_domain_enabled: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_se", 0.0f);


    if (domain >= PARIETAL_DOMAIN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "parietal_training_set_domain_enabled: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.domains[domain].enabled = enabled;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int parietal_training_set_learning_callback(
    parietal_training_bridge_t* bridge,
    parietal_learning_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_set_learning_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_se", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learning_callback = callback;
    bridge->learning_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_training_set_update_callback(
    parietal_training_bridge_t* bridge,
    parietal_weight_update_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_training_set_update_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_se", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->update_callback = callback;
    bridge->update_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

parietal_train_state_t parietal_training_get_state(
    const parietal_training_bridge_t* bridge
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        return PARIETAL_TRAIN_STATE_ERROR;
    }
    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_ge", 0.0f);


    return bridge->state;
}

int parietal_training_get_stats(
    const parietal_training_bridge_t* bridge,
    parietal_training_stats_t* stats
) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_training_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_ge", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

void parietal_training_reset_stats(parietal_training_bridge_t* bridge) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Preserve connection status */
    bool training_connected = bridge->stats.training_connected;
    bool plasticity_connected = bridge->stats.plasticity_connected;
    bool bio_async_connected = bridge->stats.bio_async_connected;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->stats.training_connected = training_connected;
    bridge->stats.plasticity_connected = plasticity_connected;
    bridge->stats.bio_async_connected = bio_async_connected;
    bridge->stats.best_loss = INFINITY;
    bridge->stats.current_loss = INFINITY;

    nimcp_mutex_unlock(bridge->base.mutex);
}

bool parietal_training_is_connected(const parietal_training_bridge_t* bridge) {
    if (!bridge || bridge->magic != PARIETAL_TRAINING_BRIDGE_MAGIC) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    parietal_training_bridge_heartbeat("parietal_tra_parietal_training_is", 0.0f);


    return bridge->training != NULL;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* parietal_training_domain_name(parietal_learning_domain_t domain) {
    if (domain >= PARIETAL_DOMAIN_COUNT) {
        return "unknown";
    }
    return domain_names[domain];
}

const char* parietal_training_response_name(parietal_train_response_t response) {
    if (response > PARIETAL_TRAIN_RESPONSE_CONSOLIDATE) {
        return "unknown";
    }
    return response_names[response];
}

const char* parietal_training_state_name(parietal_train_state_t state) {
    if (state > PARIETAL_TRAIN_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* parietal_training_bridge_version(void) {
    return PARIETAL_TRAINING_BRIDGE_VERSION;
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static int apply_weight_update(parietal_training_bridge_t* bridge,
                               parietal_learning_domain_t domain,
                               float learning_rate) {
    if (!bridge || domain >= PARIETAL_DOMAIN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "parietal_training_bridge_version: bridge is NULL");
        return -1;
    }

    /* Check update interval */
    uint64_t now = nimcp_time_get_us();
    uint64_t interval_us = bridge->config.update_interval_ms * 1000;

    if (now - bridge->last_update_time[domain] < interval_us) {
        return 0;  /* Skip - too soon */
    }

    bridge->last_update_time[domain] = now;

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->stats.domain_stats[domain].weight_updates++;
    bridge->stats.domain_stats[domain].last_update_time_ms = now / 1000;

    /* Invoke update callback */
    if (bridge->update_callback) {
        bridge->update_callback(domain, learning_rate, bridge->update_callback_data);
    }

    /* Route to plasticity bridge if connected */
    if (bridge->plasticity && bridge->config.connect_to_plasticity) {
        /* The actual plasticity update would be triggered here */
        /* This depends on the parietal_plasticity_bridge API */
    }

    return 0;
}

static float compute_domain_gradient(parietal_training_bridge_t* bridge,
                                     parietal_learning_domain_t domain,
                                     float loss_delta) {
    (void)bridge;
    (void)domain;

    /* Simple gradient approximation based on loss change */
    /* In practice, this would use actual gradient information */
    return -loss_delta;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void parietal_training_bridge_set_instance_health_agent(
    parietal_training_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int parietal_training_bridge_training_begin(parietal_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_training_bridge_training_begin: NULL argument");
        return -1;
    }
    parietal_training_bridge_heartbeat_instance(bridge->health_agent, "parietal_training_bridge_training_begin", 0.0f);
    return 0;
}

int parietal_training_bridge_training_end(parietal_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_training_bridge_training_end: NULL argument");
        return -1;
    }
    parietal_training_bridge_heartbeat_instance(bridge->health_agent, "parietal_training_bridge_training_end", 1.0f);
    return 0;
}

int parietal_training_bridge_training_step(parietal_training_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_training_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "parietal_training_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "parietal_training_bridge_training_step");
    parietal_training_bridge_heartbeat_instance(bridge->health_agent, "parietal_training_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
