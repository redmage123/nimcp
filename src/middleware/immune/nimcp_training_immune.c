/**
 * @file nimcp_training_immune.c
 * @brief Training-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * Implementation of bidirectional training-immune coordination.
 * See header for architecture and biological basis.
 */

#include "middleware/immune/nimcp_training_immune.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <pthread.h>
#endif

/* ============================================================================
 * Platform-Specific Implementations
 * ============================================================================ */

/* Use standard NIMCP mutex API for consistency */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_immune)

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/**
 * @brief Safe mutex lock with error logging
 * @param mutex Pointer to mutex
 * @param context Context string for error message
 */
#define SAFE_MUTEX_LOCK(mutex, context) \
    do { \
        if ((mutex) && nimcp_mutex_lock(mutex) != 0) { \
            NIMCP_LOGGING_WARN("Mutex lock failed in %s", context); \
        } \
    } while(0)

/**
 * @brief Safe mutex unlock with error logging
 * @param mutex Pointer to mutex
 * @param context Context string for error message
 */
#define SAFE_MUTEX_UNLOCK(mutex, context) \
    do { \
        if ((mutex) && nimcp_mutex_unlock(mutex) != 0) { \
            NIMCP_LOGGING_WARN("Mutex unlock failed in %s", context); \
        } \
    } while(0)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/**
 * @brief Compute learning rate factor for inflammation level
 *
 * WHAT: Map inflammation to LR reduction
 * WHY:  Model fever-induced learning suppression
 * HOW:  Use configured factors per inflammation level
 */
static float compute_lr_factor(
    const training_immune_system_t* system,
    brain_inflammation_level_t inflammation
) {
    if (!system) return 1.0f;

    switch (inflammation) {
        case INFLAMMATION_NONE:
            return TRAINING_IMMUNE_LR_FACTOR_NONE;
        case INFLAMMATION_LOCAL:
            return system->config.lr_factor_local;
        case INFLAMMATION_REGIONAL:
            return system->config.lr_factor_regional;
        case INFLAMMATION_SYSTEMIC:
            return system->config.lr_factor_systemic;
        case INFLAMMATION_STORM:
            return system->config.lr_factor_storm;
        default:
            return 1.0f;
    }
}

/**
 * @brief Check if value is NaN
 */
static bool is_nan_value(float value) {
    return value != value;
}

/**
 * @brief Check if value is infinite
 */
static bool is_inf_value(float value) {
    return !is_nan_value(value) && (value > FLT_MAX || value < -FLT_MAX);
}

/**
 * @brief Add metrics to history buffer
 *
 * WHAT: Store metrics in circular buffer
 * WHY:  Track training history for divergence detection
 * HOW:  Circular buffer with fixed capacity
 */
static void add_metrics_to_history(
    training_immune_system_t* system,
    const training_immune_metrics_t* metrics
) {
    if (!system || !metrics || !system->history) return;

    /* Circular buffer */
    system->history[system->history_index] = *metrics;
    system->history_index = (system->history_index + 1) % system->history_capacity;

    if (system->history_count < system->history_capacity) {
        system->history_count++;
    }
}

/**
 * @brief Create instability epitope signature
 *
 * WHAT: Convert instability to threat signature
 * WHY:  Present training failure as immune antigen
 * HOW:  Encode type and severity as byte pattern
 */
static void create_instability_epitope(
    training_instability_type_t type,
    uint32_t severity,
    uint8_t* epitope,
    size_t* epitope_len
) {
    if (!epitope || !epitope_len) return;

    /* Simple encoding: type ID + severity */
    epitope[0] = (uint8_t)type;
    epitope[1] = (uint8_t)(severity & 0xFF);
    epitope[2] = (uint8_t)((severity >> 8) & 0xFF);
    epitope[3] = 0xAA; /* Magic marker for training instability */

    *epitope_len = 4;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int training_immune_default_config(training_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(training_immune_config_t));

    /* Immune modulation settings */
    config->enable_lr_modulation = true;
    config->enable_grad_scaling = true;
    config->min_lr_factor = 0.01f; /* Never reduce below 1% */

    /* Inflammation LR factors */
    config->lr_factor_local = TRAINING_IMMUNE_LR_FACTOR_LOCAL;
    config->lr_factor_regional = TRAINING_IMMUNE_LR_FACTOR_REGIONAL;
    config->lr_factor_systemic = TRAINING_IMMUNE_LR_FACTOR_SYSTEMIC;
    config->lr_factor_storm = TRAINING_IMMUNE_LR_FACTOR_STORM;

    /* Divergence detection thresholds */
    config->loss_explosion_ratio = TRAINING_IMMUNE_LOSS_EXPLOSION_RATIO;
    config->grad_explosion_threshold = TRAINING_IMMUNE_GRAD_EXPLOSION_THRESHOLD;
    config->grad_vanishing_threshold = 1e-7f;
    config->plateau_steps = TRAINING_IMMUNE_LOSS_PLATEAU_STEPS;
    config->plateau_delta = 1e-4f;

    /* Immune response settings */
    config->enable_auto_immune_response = true;
    config->min_response_duration_ms = TRAINING_IMMUNE_MIN_RESPONSE_DURATION_MS;
    config->inflammation_ema_alpha = TRAINING_IMMUNE_INFLAMMATION_EMA_ALPHA;

    /* Monitoring */
    config->history_size = TRAINING_IMMUNE_MAX_HISTORY;
    config->enable_logging = false;

    /* Perception/Cortical thresholds */
    config->perception_collapse_threshold = 0.1f;
    config->cortical_fe_explosion_threshold = 100.0f;
    config->cortical_burst_collapse_threshold = 0.1f;

    return 0;
}

training_immune_system_t* training_immune_create(
    const training_immune_config_t* config
) {
    training_immune_config_t default_config;

    /* Use defaults if no config provided */
    if (!config) {
        training_immune_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate system */
    training_immune_system_t* system =
        (training_immune_system_t*)nimcp_malloc(sizeof(training_immune_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    memset(system, 0, sizeof(training_immune_system_t));

    /* Copy config */
    system->config = *config;
    system->phase = TRAINING_IMMUNE_PHASE_HEALTHY;

    /* Allocate history buffer */
    system->history_capacity = config->history_size;
    system->history = (training_immune_metrics_t*)nimcp_malloc(
        system->history_capacity * sizeof(training_immune_metrics_t)
    );
    if (!system->history) {
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_immune_create: system->history is NULL");
        return NULL;
    }

    /* Allocate event buffer */
    system->event_capacity = 64;
    system->events = (training_instability_event_t*)nimcp_malloc(
        system->event_capacity * sizeof(training_instability_event_t)
    );
    if (!system->events) {
        nimcp_free(system->history);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_create: system->events is NULL");
        return NULL;
    }

    /* Initialize state */
    system->inflammation = INFLAMMATION_NONE;
    system->inflammation_ema = 0.0f;
    system->current_lr_factor = 1.0f;
    system->best_loss = FLT_MAX;
    system->next_event_id = 1;

    /* Create and initialize mutex */
    system->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(system->events);
        nimcp_free(system->history);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_immune_create: system->mutex is NULL");
        return NULL;
    }

    if (nimcp_mutex_init(system->mutex, NULL) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        nimcp_free(system->events);
        nimcp_free(system->history);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "training_immune_create: validation failed");
        return NULL;
    }

    system->start_time_ms = get_timestamp_ms();

    if (config->enable_logging) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Training immune system created");
    }

    return system;
}

void training_immune_destroy(training_immune_system_t* system) {
    if (!system) return;

    /* Stop if running */
    if (system->running) {
        training_immune_stop(system);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }

    /* Free buffers */
    nimcp_free(system->events);
    nimcp_free(system->history);
    nimcp_free(system);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Training immune system destroyed");
    }
}

int training_immune_start(training_immune_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_start: system is NULL");
        return -1;
    }
    if (system->running) return 0;

    SAFE_MUTEX_LOCK(system->mutex, "training_immune_start");

    system->running = true;
    system->phase = TRAINING_IMMUNE_PHASE_MONITORING;
    system->start_time_ms = get_timestamp_ms();

    SAFE_MUTEX_UNLOCK(system->mutex, "training_immune_start");

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Training immune monitoring started");
    }

    return 0;
}

int training_immune_stop(training_immune_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_stop: system is NULL");
        return -1;
    }
    if (!system->running) return 0;

    SAFE_MUTEX_LOCK(system->mutex, "training_immune_stop");

    system->running = false;
    system->phase = TRAINING_IMMUNE_PHASE_RESOLVED;

    SAFE_MUTEX_UNLOCK(system->mutex, "training_immune_stop");

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Training immune monitoring stopped");
    }

    return 0;
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int training_immune_connect_brain_immune(
    training_immune_system_t* system,
    brain_immune_system_t* brain_immune
) {
    if (!system || !brain_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_connect_brain_immune: required parameter is NULL (system, brain_immune)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    system->brain_immune = brain_immune;
    system->config.has_brain_immune = true;

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Connected to brain immune system");
    }

    return 0;
}

int training_immune_connect_optimizer(
    training_immune_system_t* system,
    nimcp_optimizer_context_t* optimizer
) {
    if (!system || !optimizer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_connect_optimizer: required parameter is NULL (system, optimizer)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    system->optimizer = optimizer;
    system->config.has_optimizer = true;

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Connected to optimizer");
    }

    return 0;
}

int training_immune_connect_gradient_manager(
    training_immune_system_t* system,
    nimcp_gradient_manager_ctx_t* grad_manager
) {
    if (!system || !grad_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_connect_gradient_manager: required parameter is NULL (system, grad_manager)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    system->grad_manager = grad_manager;
    system->config.has_gradient_manager = true;

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Connected to gradient manager");
    }

    return 0;
}

int training_immune_connect_callbacks(
    training_immune_system_t* system,
    tcb_context_t* callbacks
) {
    if (!system || !callbacks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_connect_callbacks: required parameter is NULL (system, callbacks)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    system->callbacks = callbacks;
    system->config.has_callbacks = true;

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Connected to training callbacks");
    }

    return 0;
}

int training_immune_connect_perception_training(
    training_immune_system_t* system,
    perception_training_bridge_t* perception_training
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_connect_perception_training: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    system->perception_training = perception_training;
    system->config.has_perception_training = (perception_training != NULL);

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        if (perception_training) {
            LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                      "Connected to perception training bridge");
        } else {
            LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                      "Disconnected from perception training bridge");
        }
    }

    return 0;
}

int training_immune_connect_cortical_training(
    training_immune_system_t* system,
    cortical_training_bridge_t* cortical_training
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_connect_cortical_training: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    system->cortical_training = cortical_training;
    system->config.has_cortical_training = (cortical_training != NULL);

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        if (cortical_training) {
            LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                      "Connected to cortical training bridge");
        } else {
            LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                      "Disconnected from cortical training bridge");
        }
    }

    return 0;
}

/* ============================================================================
 * Immune → Training Implementation
 * ============================================================================ */

int training_immune_update_inflammation(
    training_immune_system_t* system,
    brain_inflammation_level_t inflammation
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_update_inflammation: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    brain_inflammation_level_t prev_inflammation = system->inflammation;
    system->inflammation = inflammation;

    /* Update EMA for smoothing */
    float alpha = system->config.inflammation_ema_alpha;
    system->inflammation_ema =
        alpha * (float)inflammation + (1.0f - alpha) * system->inflammation_ema;

    /* Compute new LR factor */
    float new_factor = compute_lr_factor(system, inflammation);

    /* Apply minimum LR factor constraint */
    if (new_factor < system->config.min_lr_factor) {
        new_factor = system->config.min_lr_factor;
    }

    system->current_lr_factor = new_factor;

    /* Update phase */
    if (inflammation > INFLAMMATION_NONE) {
        system->phase = TRAINING_IMMUNE_PHASE_RESPONDING;
        system->in_immune_response = true;

        if (!system->immune_response_start_ms) {
            system->immune_response_start_ms = get_timestamp_ms();
        }

        system->stats.inflamed_steps++;
    } else if (prev_inflammation > INFLAMMATION_NONE && inflammation == INFLAMMATION_NONE) {
        /* Check minimum response duration */
        uint64_t duration = get_timestamp_ms() - system->immune_response_start_ms;
        if (duration >= system->config.min_response_duration_ms) {
            system->phase = TRAINING_IMMUNE_PHASE_RECOVERING;
            system->in_immune_response = false;
            system->immune_response_start_ms = 0;
        }
    } else {
        system->stats.healthy_steps++;
    }

    /* Update statistics */
    if (inflammation != prev_inflammation) {
        system->stats.lr_modulations++;
    }

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging && inflammation != prev_inflammation) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Inflammation changed: %d -> %d, LR factor: %.3f",
                  prev_inflammation, inflammation, new_factor);
    }

    return 0;
}

float training_immune_get_effective_lr(
    const training_immune_system_t* system,
    float base_lr
) {
    if (!system || !system->config.enable_lr_modulation) {
        return base_lr;
    }

    return base_lr * system->current_lr_factor;
}

float training_immune_get_lr_factor(
    const training_immune_system_t* system
) {
    if (!system) return 1.0f;
    return system->current_lr_factor;
}

int training_immune_apply_lr_modulation(
    training_immune_system_t* system
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_apply_lr_modulation: system is NULL");
        return -1;
    }
    if (!system->config.enable_lr_modulation) return 0;
    if (!system->config.has_optimizer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_apply_lr_modulation: system->config is NULL");
        return -1;
    }

    /* Get current base LR from optimizer */
    float base_lr = nimcp_optimizer_get_lr(system->optimizer);

    /* Compute effective LR */
    float effective_lr = training_immune_get_effective_lr(system, base_lr);

    /* Set modulated LR */
    nimcp_optimizer_set_lr(system->optimizer, effective_lr);

    return 0;
}

float training_immune_get_gradient_scale(
    const training_immune_system_t* system
) {
    if (!system || !system->config.enable_grad_scaling) {
        return 1.0f;
    }

    /* Increase numerical stability during inflammation */
    /* Scale down gradients proportional to inflammation */
    return system->current_lr_factor;
}

/* ============================================================================
 * Training → Immune Implementation
 * ============================================================================ */

int training_immune_update_metrics(
    training_immune_system_t* system,
    float loss,
    float grad_norm,
    float learning_rate
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_update_metrics: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Update metrics */
    training_immune_metrics_t* m = &system->current_metrics;

    m->loss_prev = m->loss;
    m->loss = loss;
    m->grad_norm = grad_norm;
    m->learning_rate = learning_rate;
    m->effective_lr = training_immune_get_effective_lr(system, learning_rate);
    m->timestamp_ms = get_timestamp_ms();

    /* Update step counter */
    m->step++;

    /* Update min loss and check for improvement */
    if (loss < system->best_loss) {
        system->best_loss = loss;
        system->steps_without_improvement = 0;
    } else {
        system->steps_without_improvement++;
    }

    /* Check for NaN/Inf */
    m->has_nan = is_nan_value(loss) || is_nan_value(grad_norm);
    m->has_inf = is_inf_value(loss) || is_inf_value(grad_norm);

    /* Check for explosion/vanishing */
    m->is_exploding = grad_norm > system->config.grad_explosion_threshold;
    m->is_vanishing = grad_norm < system->config.grad_vanishing_threshold;
    m->is_plateau = system->steps_without_improvement >= system->config.plateau_steps;

    /* Update EMA */
    if (m->step == 1) {
        m->loss_ema = loss;
    } else {
        float alpha = 0.1f;
        m->loss_ema = alpha * loss + (1.0f - alpha) * m->loss_ema;
    }

    /* Add to history */
    add_metrics_to_history(system, m);

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

training_instability_type_t training_immune_check_stability(
    training_immune_system_t* system
) {
    if (!system) return TRAINING_INSTABILITY_NONE;

    nimcp_mutex_lock(system->mutex);

    const training_immune_metrics_t* m = &system->current_metrics;
    training_instability_type_t instability = TRAINING_INSTABILITY_NONE;

    /* Check for critical failures first */
    if (m->has_nan) {
        instability = TRAINING_INSTABILITY_LOSS_NAN;
    } else if (m->has_inf) {
        instability = TRAINING_INSTABILITY_LOSS_INF;
    } else if (m->loss_prev > 0 &&
               m->loss / m->loss_prev > system->config.loss_explosion_ratio) {
        instability = TRAINING_INSTABILITY_LOSS_EXPLOSION;
    } else if (m->is_exploding) {
        instability = TRAINING_INSTABILITY_GRAD_EXPLOSION;
    } else if (m->is_vanishing) {
        instability = TRAINING_INSTABILITY_GRAD_VANISHING;
    } else if (m->is_plateau) {
        instability = TRAINING_INSTABILITY_LOSS_PLATEAU;
    }

    nimcp_mutex_unlock(system->mutex);

    /* Auto-trigger immune response if enabled */
    if (instability != TRAINING_INSTABILITY_NONE &&
        system->config.enable_auto_immune_response) {

        uint32_t event_id;
        uint32_t severity = (instability <= TRAINING_INSTABILITY_LOSS_INF) ? 10 :
                           (instability <= TRAINING_INSTABILITY_LOSS_EXPLOSION) ? 8 :
                           (instability <= TRAINING_INSTABILITY_GRAD_EXPLOSION) ? 6 : 3;

        training_immune_report_instability(system, instability, severity, &event_id);
        training_immune_trigger_immune_response(system, event_id);
    }

    return instability;
}

int training_immune_report_instability(
    training_immune_system_t* system,
    training_instability_type_t type,
    uint32_t severity,
    uint32_t* event_id
) {
    if (!system || !event_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_report_instability: required parameter is NULL (system, event_id)");
        return -1;
    }
    if (type >= TRAINING_INSTABILITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "training_immune_report_instability: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Check capacity */
    if (system->event_count >= system->event_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "training_immune_report_instability: capacity exceeded");
        return -1;
    }

    /* Create event */
    training_instability_event_t* event = &system->events[system->event_count++];
    memset(event, 0, sizeof(training_instability_event_t));

    event->event_id = system->next_event_id++;
    event->type = type;
    event->severity = severity;
    event->confidence = 1.0f;
    event->metrics = system->current_metrics;
    event->detection_time_ms = get_timestamp_ms();

    *event_id = event->event_id;

    /* Update statistics */
    system->stats.total_instabilities++;
    system->stats.instabilities_by_type[type]++;

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging) {
        LOG_MODULE_WARN(TRAINING_IMMUNE_MODULE_NAME,
                  "Training instability detected: %s (severity=%u)",
                  training_instability_type_to_string(type), severity);
    }

    return 0;
}

int training_immune_trigger_immune_response(
    training_immune_system_t* system,
    uint32_t event_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_trigger_immune_response: system is NULL");
        return -1;
    }
    if (!system->config.has_brain_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_trigger_immune_response: system->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Find event */
    training_instability_event_t* event = NULL;
    for (size_t i = 0; i < system->event_count; i++) {
        if (system->events[i].event_id == event_id) {
            event = &system->events[i];
            break;
        }
    }

    if (!event) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_trigger_immune_response: event is NULL");
        return -1;
    }

    /* Create epitope */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    size_t epitope_len;
    create_instability_epitope(event->type, event->severity, epitope, &epitope_len);

    /* Present as antigen to brain immune system */
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        system->brain_immune,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        epitope_len,
        event->severity,
        0, /* No specific node */
        &antigen_id
    );

    if (result == 0) {
        event->antigen_id = antigen_id;
        event->immune_triggered = true;

        /* Update statistics */
        system->stats.immune_responses_triggered++;
        system->phase = TRAINING_IMMUNE_PHASE_RESPONDING;
    }

    nimcp_mutex_unlock(system->mutex);

    if (system->config.enable_logging && result == 0) {
        LOG_MODULE_INFO(TRAINING_IMMUNE_MODULE_NAME,
                  "Immune response triggered for instability event %u (antigen=%u)",
                  event_id, antigen_id);
    }

    return result;
}

training_instability_type_t training_immune_check_perception_cortical_stability(
    training_immune_system_t* system
) {
    if (!system) return TRAINING_INSTABILITY_NONE;

    training_instability_type_t instability = TRAINING_INSTABILITY_NONE;

    nimcp_mutex_lock(system->mutex);

    /* Check perception training bridge */
    if (system->perception_training) {
        perception_training_effects_t perception_effects;
        if (perception_training_get_effects(system->perception_training, &perception_effects) == 0 &&
            perception_effects.valid) {
            /* Check for perception collapse: all modality confidences near 0 */
            float combined_confidence = (perception_effects.visual_confidence +
                                         perception_effects.audio_quality +
                                         perception_effects.comprehension) / 3.0f;
            if (combined_confidence < system->config.perception_collapse_threshold) {
                instability = TRAINING_INSTABILITY_PERCEPTION_COLLAPSE;
                system->stats.perception_collapse_count++;
            }
        }
    }

    /* Check cortical training bridge (only if no perception instability found) */
    if (instability == TRAINING_INSTABILITY_NONE && system->cortical_training) {
        cortical_training_effects_t cortical_effects;
        if (cortical_training_get_effects(system->cortical_training, &cortical_effects) == 0 &&
            cortical_effects.valid) {
            /* Check for free energy explosion */
            if (cortical_effects.free_energy > system->config.cortical_fe_explosion_threshold) {
                instability = TRAINING_INSTABILITY_CORTICAL_EXPLOSION;
                system->stats.cortical_explosion_count++;
            }
            /* Check for burst rate collapse */
            else if (cortical_effects.burst_rate < system->config.cortical_burst_collapse_threshold) {
                instability = TRAINING_INSTABILITY_PREDICTION_FAILURE;
                system->stats.prediction_failure_count++;
            }
        }
    }

    nimcp_mutex_unlock(system->mutex);

    /* Auto-trigger immune response if enabled */
    if (instability != TRAINING_INSTABILITY_NONE &&
        system->config.enable_auto_immune_response) {

        uint32_t event_id;
        uint32_t severity = (instability == TRAINING_INSTABILITY_CORTICAL_EXPLOSION) ? 8 :
                           (instability == TRAINING_INSTABILITY_PERCEPTION_COLLAPSE) ? 6 : 5;

        training_immune_report_instability(system, instability, severity, &event_id);
        training_immune_trigger_immune_response(system, event_id);
    }

    return instability;
}

float training_immune_get_perception_sensitivity(
    const training_immune_system_t* system
) {
    if (!system) return 1.0f;

    /* Map inflammation to sensitivity factor
     * NONE      → 1.00 (full sensitivity)
     * LOCAL     → 0.90 (slight reduction)
     * REGIONAL  → 0.70 (moderate reduction)
     * SYSTEMIC  → 0.50 (significant reduction)
     * STORM     → 0.30 (conserve resources)
     */
    switch (system->inflammation) {
        case INFLAMMATION_NONE:
            return 1.0f;
        case INFLAMMATION_LOCAL:
            return 0.90f;
        case INFLAMMATION_REGIONAL:
            return 0.70f;
        case INFLAMMATION_SYSTEMIC:
            return 0.50f;
        case INFLAMMATION_STORM:
            return 0.30f;
        default:
            return 1.0f;
    }
}

/* ============================================================================
 * Query and Statistics Implementation
 * ============================================================================ */

training_immune_phase_t training_immune_get_phase(
    const training_immune_system_t* system
) {
    return system ? system->phase : TRAINING_IMMUNE_PHASE_HEALTHY;
}

brain_inflammation_level_t training_immune_get_inflammation(
    const training_immune_system_t* system
) {
    return system ? system->inflammation : INFLAMMATION_NONE;
}

bool training_immune_is_responding(
    const training_immune_system_t* system
) {
    return system ? system->in_immune_response : false;
}

int training_immune_get_stats(
    const training_immune_system_t* system,
    training_immune_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    *stats = system->stats;

    /* Update current state */
    stats->current_phase = system->phase;
    stats->current_inflammation = system->inflammation;
    stats->current_lr_factor = system->current_lr_factor;

    /* Compute averages */
    if (system->stats.lr_modulations > 0) {
        stats->avg_lr_reduction_factor =
            (float)system->stats.healthy_steps /
            (float)(system->stats.healthy_steps + system->stats.inflamed_steps);
    }

    uint64_t total_steps = system->stats.healthy_steps + system->stats.inflamed_steps;
    if (total_steps > 0) {
        stats->time_in_inflammation_pct =
            100.0f * (float)system->stats.inflamed_steps / (float)total_steps;
    }

    /* Update perception/cortical integration stats */
    stats->perception_training_connected = system->config.has_perception_training;
    stats->cortical_training_connected = system->config.has_cortical_training;

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

int training_immune_get_current_metrics(
    const training_immune_system_t* system,
    training_immune_metrics_t* metrics
) {
    if (!system || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_get_current_metrics: required parameter is NULL (system, metrics)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    *metrics = system->current_metrics;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

const training_instability_event_t* training_immune_get_event(
    const training_immune_system_t* system,
    uint32_t event_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    for (size_t i = 0; i < system->event_count; i++) {
        if (system->events[i].event_id == event_id) {
            return &system->events[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_immune_get_event: validation failed");
    return NULL;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* training_immune_phase_to_string(training_immune_phase_t phase) {
    switch (phase) {
        case TRAINING_IMMUNE_PHASE_HEALTHY:    return "HEALTHY";
        case TRAINING_IMMUNE_PHASE_MONITORING: return "MONITORING";
        case TRAINING_IMMUNE_PHASE_RESPONDING: return "RESPONDING";
        case TRAINING_IMMUNE_PHASE_RECOVERING: return "RECOVERING";
        case TRAINING_IMMUNE_PHASE_RESOLVED:   return "RESOLVED";
        default:                               return "UNKNOWN";
    }
}

const char* training_instability_type_to_string(training_instability_type_t type) {
    switch (type) {
        case TRAINING_INSTABILITY_NONE:           return "NONE";
        case TRAINING_INSTABILITY_LOSS_NAN:       return "LOSS_NAN";
        case TRAINING_INSTABILITY_LOSS_INF:       return "LOSS_INF";
        case TRAINING_INSTABILITY_LOSS_EXPLOSION: return "LOSS_EXPLOSION";
        case TRAINING_INSTABILITY_GRAD_EXPLOSION: return "GRAD_EXPLOSION";
        case TRAINING_INSTABILITY_GRAD_VANISHING: return "GRAD_VANISHING";
        case TRAINING_INSTABILITY_LOSS_PLATEAU:   return "LOSS_PLATEAU";
        case TRAINING_INSTABILITY_OSCILLATION:    return "OSCILLATION";
        case TRAINING_INSTABILITY_PERCEPTION_COLLAPSE: return "PERCEPTION_COLLAPSE";
        case TRAINING_INSTABILITY_CORTICAL_EXPLOSION:  return "CORTICAL_EXPLOSION";
        case TRAINING_INSTABILITY_PREDICTION_FAILURE:  return "PREDICTION_FAILURE";
        default:                                  return "UNKNOWN";
    }
}
