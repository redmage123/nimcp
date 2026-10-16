/**
 * @file nimcp_training_module_fep_bridge.c
 * @brief Free Energy Principle bridge for Training Module implementation
 */

#include "middleware/training/nimcp_training_module_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_module_fep_bridge)

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct training_module_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    training_module_fep_config_t config;
    fep_system_t* fep_system;
    nimcp_training_context_t* training_module;

    /* Effects */
    training_module_fep_effects_t fep_effects;
    fep_training_effects_t training_effects;

    /* State */
    training_module_fep_state_t state;
    training_module_fep_stats_t stats;

    /* Flags */
    bool owns_fep_system;
};

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int training_module_fep_default_config(training_module_fep_config_t* config) {
    if (!config) {
        return -1;
    }

    /* FEP parameters */
    config->belief_learning_rate = NIMCP_LEARNING_RATE_COARSE;
    config->precision_learning_rate = 0.05f;
    config->initial_precision = 1.0f;
    config->learn_precision = true;

    /* Prediction error thresholds */
    config->surprise_threshold = 5.0f;
    config->convergence_threshold = 0.01f;

    /* Modulation settings */
    config->modulate_learning_rate = true;
    config->lr_modulation_strength = 0.3f;
    config->enable_early_stopping = true;

    /* Bio-async */
    config->enable_bio_async = false;

    return 0;
}

training_module_fep_bridge_t* training_module_fep_create(
    const training_module_fep_config_t* config,
    nimcp_training_context_t* training_module,
    fep_system_t* fep_system
) {
    if (!training_module) {
        NIMCP_LOGGING_ERROR("training_module_fep_create: NULL training module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_module is NULL");

        return NULL;
    }

    /* Allocate bridge */
    training_module_fep_bridge_t* bridge = (training_module_fep_bridge_t*)
        nimcp_malloc(sizeof(training_module_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("training_module_fep_create: Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_module_fep_create: bridge allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(training_module_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        training_module_fep_default_config(&bridge->config);
    }

    /* Set training module */
    bridge->training_module = training_module;

    /* Create or use FEP system */
    if (fep_system) {
        bridge->fep_system = fep_system;
        bridge->owns_fep_system = false;
    } else {
        /* Create FEP system with appropriate configuration */
        fep_config_t fep_config;
        fep_default_config(&fep_config);

        fep_config.num_levels = 1;  /* Single level for training */
        uint32_t level_dim = 2;      /* loss and gradient */
        fep_config.level_dims = &level_dim;
        fep_config.belief_learning_rate = bridge->config.belief_learning_rate;
        fep_config.precision_learning_rate = bridge->config.precision_learning_rate;
        fep_config.initial_precision = bridge->config.initial_precision;
        fep_config.learn_precision = bridge->config.learn_precision;

        bridge->fep_system = fep_create(&fep_config, 2, 1);
        if (!bridge->fep_system) {
            NIMCP_LOGGING_ERROR("training_module_fep_create: Failed to create FEP system");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_module_fep_create: FEP system creation failed");
            goto cleanup;
        }
        bridge->owns_fep_system = true;
    }

    /* Initialize mutex */
    if (bridge_base_init(&bridge->base, 0, "training_module_fep") != 0) { goto cleanup; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_WARN("training_module_fep_create: Failed to create mutex");
    }

    /* Initialize effects */
    bridge->fep_effects.learning_rate_modulation = 1.0f;
    bridge->fep_effects.regularization_strength = 0.0f;
    bridge->fep_effects.exploration_bonus = 0.0f;
    bridge->fep_effects.should_stop_early = false;

    /* Initialize statistics */
    bridge->stats.min_free_energy = INFINITY;
    bridge->stats.max_free_energy = -INFINITY;

    NIMCP_LOGGING_INFO("Created training module FEP bridge");

    return bridge;

cleanup:
    if (bridge->owns_fep_system && bridge->fep_system) {
        fep_destroy(bridge->fep_system);
    }
    if (bridge->base.mutex) { bridge_base_cleanup(&bridge->base); }
    nimcp_free(bridge);
    return NULL;
}

void training_module_fep_destroy(training_module_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        training_module_fep_disconnect_bio_async(bridge);
    }

    /* Destroy FEP system if owned */
    if (bridge->owns_fep_system && bridge->fep_system) {
        fep_destroy(bridge->fep_system);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed training module FEP bridge");
}

/* ============================================================================
 * Integration
 * ============================================================================ */

int training_module_fep_update(
    training_module_fep_bridge_t* bridge,
    float loss,
    float gradient_norm
) {
    if (!bridge || !bridge->fep_system) {
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Update training effects */
    bridge->training_effects.training_loss = loss;
    bridge->training_effects.gradient_norm = gradient_norm;
    bridge->training_effects.training_steps++;

    /* Create observation from training data */
    float observation[2];
    observation[0] = loss;
    observation[1] = gradient_norm;

    /* Process observation through FEP */
    int result = fep_process_observation(bridge->fep_system, observation, 2);
    if (result != 0) {
        NIMCP_LOGGING_ERROR("training_module_fep_update: FEP observation processing failed");
        if (bridge->base.mutex) {
            nimcp_platform_mutex_unlock(bridge->base.mutex);
        }
        return result;
    }

    /* Update beliefs */
    result = fep_update_beliefs(bridge->fep_system);
    if (result != 0) {
        NIMCP_LOGGING_WARN("training_module_fep_update: Belief update failed");
    }

    /* Update state */
    float free_energy = fep_get_free_energy(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    bridge->state.update_count++;
    bridge->state.avg_free_energy =
        (bridge->state.avg_free_energy * (bridge->state.update_count - 1) + free_energy) /
        bridge->state.update_count;
    bridge->state.avg_prediction_error =
        (bridge->state.avg_prediction_error * (bridge->state.update_count - 1) + pred_error) /
        bridge->state.update_count;

    if (pred_error > bridge->state.max_surprise) {
        bridge->state.max_surprise = pred_error;
    }

    /* Check for convergence or surprise */
    if (pred_error < bridge->config.convergence_threshold) {
        bridge->state.convergence_count++;
    } else {
        bridge->state.convergence_count = 0;
    }

    if (pred_error > bridge->config.surprise_threshold) {
        bridge->state.surprise_count++;
        bridge->stats.early_stop_triggers++;
    }

    /* Update statistics */
    bridge->stats.total_updates++;
    if (free_energy < bridge->stats.min_free_energy) {
        bridge->stats.min_free_energy = free_energy;
    }
    if (free_energy > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = free_energy;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int training_module_fep_compute_effects(training_module_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Get current FEP state */
    float free_energy = fep_get_free_energy(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);
    float surprise = fep_compute_surprise(bridge->fep_system);

    /* Compute learning rate modulation */
    if (bridge->config.modulate_learning_rate) {
        /* Higher prediction error → lower learning rate (more conservative) */
        /* Lower prediction error → higher learning rate (more aggressive) */
        float error_factor = expf(-pred_error * bridge->config.lr_modulation_strength);
        float surprise_factor = expf(-surprise * bridge->config.lr_modulation_strength);
        bridge->fep_effects.learning_rate_modulation =
            0.5f * (error_factor + surprise_factor);

        /* Clamp to [0.1, 2.0] */
        if (bridge->fep_effects.learning_rate_modulation < 0.1f) {
            bridge->fep_effects.learning_rate_modulation = 0.1f;
        } else if (bridge->fep_effects.learning_rate_modulation > 2.0f) {
            bridge->fep_effects.learning_rate_modulation = 2.0f;
        }
    } else {
        bridge->fep_effects.learning_rate_modulation = 1.0f;
    }

    /* Compute regularization strength from complexity */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);
    bridge->fep_effects.regularization_strength =
        fminf(fe.complexity / (fe.total + 1e-8f), 1.0f);

    /* Compute exploration bonus from uncertainty */
    fep_belief_t beliefs;
    fep_get_beliefs(bridge->fep_system, 0, &beliefs);
    float uncertainty = 0.0f;
    for (uint32_t i = 0; i < beliefs.dim; i++) {
        uncertainty += beliefs.variance[i];
    }
    uncertainty /= beliefs.dim;
    bridge->fep_effects.exploration_bonus = fminf(uncertainty, 1.0f);

    /* Check early stopping */
    if (bridge->config.enable_early_stopping) {
        bridge->fep_effects.should_stop_early =
            (surprise > bridge->config.surprise_threshold) ||
            (bridge->state.convergence_count > 10);
    } else {
        bridge->fep_effects.should_stop_early = false;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int training_module_fep_apply_effects(training_module_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    /* Note: In a full implementation, this would modify training module parameters */
    /* For now, we just track that modulation occurred */
    bridge->stats.total_modulations++;

    return 0;
}

int training_module_fep_step(
    training_module_fep_bridge_t* bridge,
    float loss,
    float gradient_norm
) {
    int result;

    result = training_module_fep_update(bridge, loss, gradient_norm);
    if (result != 0) {
        return result;
    }

    result = training_module_fep_compute_effects(bridge);
    if (result != 0) {
        return result;
    }

    result = training_module_fep_apply_effects(bridge);
    if (result != 0) {
        return result;
    }

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int training_module_fep_get_effects(
    const training_module_fep_bridge_t* bridge,
    training_module_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

float training_module_fep_get_free_energy(const training_module_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return 0.0f;
    }
    return fep_get_free_energy(bridge->fep_system);
}

float training_module_fep_get_prediction_error(const training_module_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        return 0.0f;
    }
    return fep_get_prediction_error(bridge->fep_system, 0);
}

bool training_module_fep_should_stop(const training_module_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->fep_effects.should_stop_early;
}

int training_module_fep_get_stats(
    const training_module_fep_bridge_t* bridge,
    training_module_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int training_module_fep_get_state(
    const training_module_fep_bridge_t* bridge,
    training_module_fep_state_t* state
) {
    if (!bridge || !state) {
        return -1;
    }

    *state = bridge->state;
    return 0;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int training_module_fep_connect_bio_async(training_module_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    /* Note: BIO_MODULE_FEP_TRAINING_MODULE would need to be added to nimcp_bio_messages.h */
    /* For now, we mark it as enabled without actual registration */
    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("Training module FEP bridge connected to bio-async");

    return 0;
}

int training_module_fep_disconnect_bio_async(training_module_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Not connected */
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Training module FEP bridge disconnected from bio-async");

    return 0;
}

bool training_module_fep_is_bio_async_connected(const training_module_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
