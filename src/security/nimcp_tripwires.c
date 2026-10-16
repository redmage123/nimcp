/**
 * @file nimcp_tripwires.c
 * @brief Tripwire Detection System Implementation
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Implementation of behavioral tripwire detection
 * WHY:  Early detection of misalignment patterns
 * HOW:  Statistical analysis, Bayesian inference, pattern matching
 */

#include "security/nimcp_tripwires.h"
#include "security/nimcp_emergency_halt.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/statistics/nimcp_statistics.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdatomic.h>  /* P0 fix: Required for atomic health agent pointer */

#include "utils/error/nimcp_error_codes.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TRIPWIRE_LOG_PREFIX         "[TRIPWIRES]"
#define TRIPWIRE_MAX_GOALS          64
#define TRIPWIRE_MAX_RESOURCES      32
#define TRIPWIRE_FEATURE_DIM        NIMCP_FEATURE_DIM

/* Network anomaly detection constants */
#define TRIPWIRE_MAX_NETWORK_ENDPOINTS     256      /**< Max unique endpoints to track */
#define TRIPWIRE_NETWORK_HISTORY_SIZE      1000     /**< Connection history buffer size */
#define TRIPWIRE_BEACON_WINDOW_SIZE        50       /**< Connections for beaconing analysis */
#define TRIPWIRE_NETWORK_FEATURE_DIM       16       /**< Network pattern feature dimension */
#define TRIPWIRE_MIN_BEACON_INTERVALS      10       /**< Min intervals to detect beaconing */

/**
 * @brief Safe uint64_t addition with overflow protection
 * SECURITY FIX: Prevents integer overflow in byte counters
 * @param dest Pointer to destination value to add to
 * @param addend Value to add
 * @return true if addition succeeded, false if overflow would occur (value clamped to max)
 */
static inline bool safe_uint64_add(uint64_t* dest, uint64_t addend) {
    if (*dest > UINT64_MAX - addend) {
        /* Overflow would occur - clamp to maximum */
        *dest = UINT64_MAX;
        return false;
    }
    *dest += addend;
    return true;
}

/**
 * @brief Safe uint32_t increment with overflow protection
 * @param dest Pointer to destination value to increment
 * @return true if increment succeeded, false if overflow would occur (value clamped to max)
 */
static inline bool safe_uint32_inc(uint32_t* dest) {
    if (*dest == UINT32_MAX) {
        return false;  /* Already at max */
    }
    (*dest)++;
    return true;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/* Forward declaration for health agent */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/* Global health agent handle - P0 fix: Use atomic to prevent data race on concurrent access */
static _Atomic(nimcp_health_agent_t*) g_tripwire_health_agent = NULL;

/* Health agent setter - called from brain init */
void tripwire_set_health_agent(nimcp_health_agent_t* agent) {
    atomic_store(&g_tripwire_health_agent, agent);
}

/* Heartbeat helper - call during long-running operations */
static inline void tripwire_heartbeat(const char* operation, float progress) {
    /* P0 fix: Atomic load to prevent data race */
    nimcp_health_agent_t* agent = atomic_load(&g_tripwire_health_agent);
    if (agent) {
        /* Forward declaration of health agent function */
        extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t*, const char*, float);
        nimcp_health_agent_heartbeat_ex(agent, operation, progress);
    }
}

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * @brief Goal tracking state with Bayesian inference
 */
typedef struct goal_tracker {
    uint32_t goal_id;
    float baseline_priority;
    float current_priority;
    nimcp_running_stats_t pursuit_stats;      /**< Running stats for pursuit intensity */
    nimcp_bayesian_result_t bayesian_result;  /**< Bayesian posterior for goal drift */
    float prior_mean;                         /**< Bayesian prior mean */
    float prior_variance;                     /**< Bayesian prior variance */
    float* pursuit_history;                   /**< Recent pursuit values for Bayesian update */
    uint32_t pursuit_history_size;            /**< Size of pursuit history buffer */
    uint32_t pursuit_history_count;           /**< Current count in buffer */
    uint64_t last_update_us;
    bool active;
} goal_tracker_t;

/**
 * @brief Resource usage tracking with z-score detection
 */
typedef struct resource_tracker {
    uint32_t resource_type;
    nimcp_running_stats_t usage_stats;        /**< Running stats using statistics module */
    float baseline_mean;
    float baseline_std;
    uint64_t last_update_us;
    bool has_baseline;
} resource_tracker_t;

/**
 * @brief Behavioral distribution (for divergence calculation)
 */
typedef struct behavior_distribution {
    float* stated;          /* Stated action probabilities */
    float* observed;        /* Observed action frequencies */
    size_t dim;             /* Distribution dimension */
    uint64_t observation_count;
} behavior_distribution_t;

/**
 * @brief Alert queue entry
 */
typedef struct alert_entry {
    tripwire_alert_t alert;
    bool acknowledged;
    bool active;
} alert_entry_t;

/* ============================================================================
 * Network Anomaly Detection Types
 * ============================================================================ */

/**
 * @brief Network endpoint tracking
 */
typedef struct network_endpoint {
    uint32_t ip;                        /**< Destination IP */
    uint16_t port;                      /**< Destination port */
    tripwire_network_protocol_t proto;  /**< Protocol */
    uint64_t total_bytes_sent;          /**< Total bytes sent to this endpoint */
    uint64_t total_bytes_recv;          /**< Total bytes received from this endpoint */
    uint32_t connection_count;          /**< Number of connections to this endpoint */
    uint64_t first_seen_us;             /**< First connection timestamp */
    uint64_t last_seen_us;              /**< Last connection timestamp */
    bool active;
} network_endpoint_t;

/**
 * @brief Network connection record for timing analysis
 */
typedef struct network_connection_record {
    uint32_t ip;                        /**< Destination IP */
    uint16_t port;                      /**< Destination port */
    uint64_t timestamp_us;              /**< When connection occurred */
    uint64_t bytes_sent;                /**< Bytes sent */
    uint64_t bytes_recv;                /**< Bytes received */
    tripwire_network_protocol_t proto;  /**< Protocol */
} network_connection_record_t;

/**
 * @brief Network behavior tracking state
 */
typedef struct network_tracker {
    /* Endpoint tracking */
    network_endpoint_t endpoints[TRIPWIRE_MAX_NETWORK_ENDPOINTS];
    size_t endpoint_count;

    /* Connection history (circular buffer for timing analysis) */
    network_connection_record_t* connection_history;
    size_t history_capacity;
    size_t history_head;
    size_t history_count;

    /* Aggregate statistics */
    nimcp_running_stats_t bytes_sent_stats;     /**< Running stats for sent data */
    nimcp_running_stats_t bytes_recv_stats;     /**< Running stats for received data */
    nimcp_running_stats_t connection_rate_stats;/**< Connections per second */
    nimcp_running_stats_t interval_stats;       /**< Inter-connection intervals */

    /* Protocol distribution */
    uint64_t protocol_counts[7];                /**< Counts per protocol type */

    /* Beaconing detection */
    float* beacon_intervals;                    /**< Recent inter-connection intervals */
    size_t beacon_interval_count;
    size_t beacon_interval_capacity;

    /* Baseline tracking */
    bool has_baseline;
    float baseline_outbound_ratio;              /**< Normal outbound/inbound ratio */
    float baseline_connection_rate;             /**< Normal connections per second */
    uint64_t baseline_samples;

    /* Pattern feature tracking */
    nimcp_running_stats_t pattern_stats[TRIPWIRE_NETWORK_FEATURE_DIM];
    bool has_pattern_baseline;

    /* Totals */
    uint64_t total_bytes_sent;
    uint64_t total_bytes_recv;
    uint64_t total_connections;
    uint64_t last_connection_us;
} network_tracker_t;

/**
 * @brief Tripwire system internal state
 */
struct tripwire_system {
    uint32_t magic;

    /* Configuration */
    tripwire_config_t config;

    /* Statistics */
    tripwire_stats_t stats;

    /* Behavioral tracking */
    behavior_distribution_t behavior;
    nimcp_running_stats_t consistency_stats;  /**< Using statistics module */

    /* Correlation tracking for action-explanation consistency */
    float* action_features;                   /**< Recent action features for correlation */
    float* explanation_features;              /**< Recent explanation features for correlation */
    uint32_t correlation_buffer_size;
    uint32_t correlation_count;

    /* Goal tracking */
    goal_tracker_t goals[TRIPWIRE_MAX_GOALS];
    size_t goal_count;

    /* Resource tracking */
    resource_tracker_t resources[TRIPWIRE_MAX_RESOURCES];
    size_t resource_count;

    /* Performance tracking (for sandbagging) */
    nimcp_running_stats_t performance_stats;  /**< Using statistics module */
    float peak_performance;

    /* Agreement tracking (for sycophancy) */
    nimcp_running_stats_t agreement_stats;    /**< Using statistics module */
    uint64_t total_interactions;
    uint64_t agreement_count;

    /* Power-seeking indicators */
    nimcp_running_stats_t capability_expansion;   /**< Using statistics module */
    nimcp_running_stats_t influence_acquisition;  /**< Using statistics module */

    /* Alert queue */
    alert_entry_t alerts[TRIPWIRE_MAX_ALERTS];
    size_t alert_count;
    size_t alert_head;

    /* Per-tripwire detection scores */
    float detection_scores[TRIPWIRE_COUNT];
    float detection_confidence[TRIPWIRE_COUNT];
    uint64_t last_detection_us[TRIPWIRE_COUNT];

    /* Integration */
    emergency_halt_t* halt_system;
    bio_module_context_t bio_ctx;
    bool bio_async_connected;
    void* brain_immune;              /**< Brain immune system for antigen presentation */

    /* Network anomaly detection */
    network_tracker_t network;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Internal Functions - Forward Declarations
 * ============================================================================ */

/* Statistical helpers (wrappers around statistics module) */
static float compute_zscore(double value, double mean, double std);
static tripwire_severity_t score_to_severity(float score);

/* Bayesian goal drift detection */
static float compute_goal_drift_bayesian(tripwire_system_t* system);

/* Correlation-based consistency detection */
static float compute_action_consistency(tripwire_system_t* system);

/* Alert management */
static void tripwire_add_alert(tripwire_system_t* system,
                               tripwire_type_t type,
                               float score,
                               const char* evidence);
static void tripwire_broadcast_alert(tripwire_system_t* system,
                                     tripwire_type_t type);
static void tripwire_update_all_detectors(tripwire_system_t* system);

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

tripwire_config_t tripwire_default_config(void) {
    tripwire_config_t config;
    memset(&config, 0, sizeof(config));

    /* Enable all tripwires by default */
    config.enabled_tripwires = (1 << TRIPWIRE_COUNT) - 1;

    /* Default thresholds */
    config.thresholds.divergence_threshold = 0.5f;
    config.thresholds.consistency_threshold = 0.7f;
    config.thresholds.resource_zscore_threshold = 3.0f;
    config.thresholds.goal_drift_threshold = 0.3f;

    /* Network anomaly detection thresholds */
    config.thresholds.network_exfil_threshold = 5.0f;     /* Outbound 5x inbound is suspicious */
    config.thresholds.network_anomaly_zscore = 3.0f;      /* 3 sigma for anomaly */
    config.thresholds.network_beacon_threshold = 0.85f;   /* 85% regularity = beaconing */

    /* Default sensitivities */
    for (int i = 0; i < TRIPWIRE_COUNT; i++) {
        config.thresholds.sensitivity[i] = 1.0f;
    }

    config.thresholds.min_observations = 10;
    config.thresholds.min_confidence = NIMCP_CONFIDENCE_MEDIUM;

    /* Halt on critical */
    config.halt_on_critical = true;

    /* Baseline settings */
    config.baseline_window = 1000;
    config.adaptive_baseline = true;
    config.baseline_decay = NIMCP_EMA_DECAY_DEFAULT;

    /*
     * Alert settings
     *
     * COOLDOWN RATIONALE:
     * - alert_cooldown_ms (5000ms = 5 seconds):
     *   Prevents alert flooding while allowing timely detection. This is the
     *   GLOBAL cooldown applied to all detection types equally.
     *
     *   5 seconds was chosen because:
     *   1. Fast enough to detect escalating threats (multiple violations in minutes)
     *   2. Slow enough to prevent alert fatigue during noisy periods
     *   3. Matches typical human response time for security incidents
     *   4. Aligns with common monitoring tool refresh intervals
     *
     * For more granular control, the per-tripwire sensitivity[] values can be
     * adjusted to affect detection thresholds rather than cooldown times.
     *
     * FUTURE: Consider adding per-detection-type cooldowns in tripwire_thresholds_t
     * for cases where some detectors (e.g., network C2) should alert more urgently
     * than others (e.g., sycophancy).
     */
    config.deduplicate_alerts = true;
    config.alert_cooldown_ms = 5000;

    return config;
}

tripwire_system_t* tripwire_create(const tripwire_config_t* config) {
    tripwire_system_t* system = nimcp_malloc(sizeof(tripwire_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("%s Failed to allocate tripwire system",
                           TRIPWIRE_LOG_PREFIX);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tripwire_create: system is NULL");
        return NULL;
    }

    memset(system, 0, sizeof(tripwire_system_t));
    system->magic = TRIPWIRE_SYSTEM_MAGIC;

    /* Apply configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(tripwire_config_t));
    } else {
        system->config = tripwire_default_config();
    }

    /* Initialize behavior distribution */
    system->behavior.dim = TRIPWIRE_FEATURE_DIM;
    system->behavior.stated = nimcp_calloc(TRIPWIRE_FEATURE_DIM, sizeof(float));
    system->behavior.observed = nimcp_calloc(TRIPWIRE_FEATURE_DIM, sizeof(float));
    if (!system->behavior.stated || !system->behavior.observed) {
        NIMCP_LOGGING_ERROR("%s Failed to allocate behavior distributions",
                           TRIPWIRE_LOG_PREFIX);
        tripwire_destroy(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tripwire_create: required parameter is NULL (system->behavior, system->behavior)");
        return NULL;
    }

    /* Initialize uniform prior for behavior */
    float uniform = 1.0f / TRIPWIRE_FEATURE_DIM;
    for (size_t i = 0; i < TRIPWIRE_FEATURE_DIM; i++) {
        system->behavior.stated[i] = uniform;
        system->behavior.observed[i] = uniform;
    }

    /* Initialize running statistics using central statistics module */
    nimcp_stats_running_init(&system->consistency_stats);
    nimcp_stats_running_init(&system->performance_stats);
    nimcp_stats_running_init(&system->agreement_stats);
    nimcp_stats_running_init(&system->capability_expansion);
    nimcp_stats_running_init(&system->influence_acquisition);

    /* Allocate correlation buffers for action-explanation consistency */
    system->correlation_buffer_size = 100;  /* Track last 100 observations */
    system->action_features = nimcp_calloc(system->correlation_buffer_size, sizeof(float));
    system->explanation_features = nimcp_calloc(system->correlation_buffer_size, sizeof(float));
    if (!system->action_features || !system->explanation_features) {
        NIMCP_LOGGING_WARN("%s Failed to allocate correlation buffers",
                           TRIPWIRE_LOG_PREFIX);
        /* Non-fatal - clean up partial allocation and continue without correlation tracking */
        if (system->action_features) {
            nimcp_free(system->action_features);
            system->action_features = NULL;
        }
        if (system->explanation_features) {
            nimcp_free(system->explanation_features);
            system->explanation_features = NULL;
        }
        system->correlation_buffer_size = 0;
    }
    system->correlation_count = 0;

    /* Initialize network tracker */
    memset(&system->network, 0, sizeof(network_tracker_t));
    system->network.history_capacity = TRIPWIRE_NETWORK_HISTORY_SIZE;
    system->network.connection_history = nimcp_calloc(
        TRIPWIRE_NETWORK_HISTORY_SIZE, sizeof(network_connection_record_t));
    if (!system->network.connection_history) {
        NIMCP_LOGGING_WARN("%s Failed to allocate network connection history",
                           TRIPWIRE_LOG_PREFIX);
        /* Non-fatal - network detection will be limited */
    }

    /* Allocate beacon interval tracking */
    system->network.beacon_interval_capacity = TRIPWIRE_BEACON_WINDOW_SIZE;
    system->network.beacon_intervals = nimcp_calloc(
        TRIPWIRE_BEACON_WINDOW_SIZE, sizeof(float));
    if (!system->network.beacon_intervals) {
        NIMCP_LOGGING_WARN("%s Failed to allocate beacon interval buffer",
                           TRIPWIRE_LOG_PREFIX);
    }

    /* Initialize network running statistics */
    nimcp_stats_running_init(&system->network.bytes_sent_stats);
    nimcp_stats_running_init(&system->network.bytes_recv_stats);
    nimcp_stats_running_init(&system->network.connection_rate_stats);
    nimcp_stats_running_init(&system->network.interval_stats);

    /* Initialize pattern statistics */
    for (int i = 0; i < TRIPWIRE_NETWORK_FEATURE_DIM; i++) {
        nimcp_stats_running_init(&system->network.pattern_stats[i]);
    }

    /* Initialize mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("%s Failed to create mutex", TRIPWIRE_LOG_PREFIX);
        tripwire_destroy(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tripwire_create: system->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("%s Tripwire detection system initialized",
                       TRIPWIRE_LOG_PREFIX);

    return system;
}

void tripwire_destroy(tripwire_system_t* system) {
    if (!system) return;
    if (system->magic != TRIPWIRE_SYSTEM_MAGIC) return;

    /* Disconnect from bio-async */
    if (system->bio_async_connected) {
        bio_router_unregister_module(system->bio_ctx);
    }

    /* Free behavior distributions */
    if (system->behavior.stated) {
        nimcp_free(system->behavior.stated);
    }
    if (system->behavior.observed) {
        nimcp_free(system->behavior.observed);
    }

    /* Free correlation buffers */
    if (system->action_features) {
        nimcp_free(system->action_features);
    }
    if (system->explanation_features) {
        nimcp_free(system->explanation_features);
    }

    /* Free goal tracker pursuit history buffers
     * SECURITY FIX: Iterate over ALL slots, not just goal_count.
     * This ensures we don't leak memory if goals were removed or reset. */
    for (size_t i = 0; i < TRIPWIRE_MAX_GOALS; i++) {
        if (system->goals[i].pursuit_history) {
            nimcp_free(system->goals[i].pursuit_history);
            system->goals[i].pursuit_history = NULL;  /* Prevent double-free */
        }
    }

    /* Free network tracker buffers */
    if (system->network.connection_history) {
        nimcp_free(system->network.connection_history);
    }
    if (system->network.beacon_intervals) {
        nimcp_free(system->network.beacon_intervals);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_mutex_destroy(system->mutex);
    }

    system->magic = 0;
    nimcp_free(system);

    NIMCP_LOGGING_INFO("%s Tripwire system destroyed", TRIPWIRE_LOG_PREFIX);
}

nimcp_error_t tripwire_reset(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    /* Reset statistics */
    memset(&system->stats, 0, sizeof(tripwire_stats_t));

    /* Reset behavior distributions */
    float uniform = 1.0f / system->behavior.dim;
    for (size_t i = 0; i < system->behavior.dim; i++) {
        system->behavior.stated[i] = uniform;
        system->behavior.observed[i] = uniform;
    }
    system->behavior.observation_count = 0;

    /* Reset running statistics using central statistics module */
    nimcp_stats_running_init(&system->consistency_stats);
    nimcp_stats_running_init(&system->performance_stats);
    nimcp_stats_running_init(&system->agreement_stats);
    nimcp_stats_running_init(&system->capability_expansion);
    nimcp_stats_running_init(&system->influence_acquisition);

    /* Reset correlation tracking */
    system->correlation_count = 0;

    /* SECURITY FIX: Free goal tracker pursuit history buffers before reset
     * to prevent memory leaks when system is reset */
    for (size_t i = 0; i < TRIPWIRE_MAX_GOALS; i++) {
        if (system->goals[i].pursuit_history) {
            nimcp_free(system->goals[i].pursuit_history);
            system->goals[i].pursuit_history = NULL;
        }
    }
    memset(system->goals, 0, sizeof(system->goals));  /* Clear all goal state */
    system->goal_count = 0;

    /* Reset resource tracking */
    memset(system->resources, 0, sizeof(system->resources));  /* Clear all resource state */
    system->resource_count = 0;

    /* Reset detection scores */
    memset(system->detection_scores, 0, sizeof(system->detection_scores));
    memset(system->detection_confidence, 0, sizeof(system->detection_confidence));

    /* Clear alerts */
    system->alert_count = 0;
    system->alert_head = 0;

    /* Reset network tracker */
    memset(system->network.endpoints, 0, sizeof(system->network.endpoints));
    system->network.endpoint_count = 0;
    system->network.history_head = 0;
    system->network.history_count = 0;
    system->network.beacon_interval_count = 0;
    system->network.has_baseline = false;
    system->network.has_pattern_baseline = false;
    system->network.total_bytes_sent = 0;
    system->network.total_bytes_recv = 0;
    system->network.total_connections = 0;
    system->network.last_connection_us = 0;
    memset(system->network.protocol_counts, 0, sizeof(system->network.protocol_counts));
    nimcp_stats_running_init(&system->network.bytes_sent_stats);
    nimcp_stats_running_init(&system->network.bytes_recv_stats);
    nimcp_stats_running_init(&system->network.connection_rate_stats);
    nimcp_stats_running_init(&system->network.interval_stats);
    for (int i = 0; i < TRIPWIRE_NETWORK_FEATURE_DIM; i++) {
        nimcp_stats_running_init(&system->network.pattern_stats[i]);
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("%s Tripwire system reset", TRIPWIRE_LOG_PREFIX);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Observation Implementation
 * ============================================================================ */

nimcp_error_t tripwire_observe_action(tripwire_system_t* system,
                                       const proposed_action_t* action,
                                       const decision_explanation_t* explanation) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC || !action) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    system->stats.total_observations++;

    /* Update behavior distribution */
    if (action->feature_vector && action->feature_dim > 0) {
        size_t dim = action->feature_dim < system->behavior.dim ?
                     action->feature_dim : system->behavior.dim;

        /* Update stated distribution (exponential moving average) */
        float alpha = 0.01f;
        for (size_t i = 0; i < dim; i++) {
            if (action->stated_probability > 0) {
                system->behavior.stated[i] =
                    (1 - alpha) * system->behavior.stated[i] +
                    alpha * action->stated_probability * action->feature_vector[i];
            }
        }

        /* Update observed distribution based on execution */
        if (action->was_executed) {
            for (size_t i = 0; i < dim; i++) {
                system->behavior.observed[i] =
                    (1 - alpha) * system->behavior.observed[i] +
                    alpha * action->feature_vector[i];
            }
            system->behavior.observation_count++;
        }
    }

    /* Update consistency tracking if explanation provided */
    if (explanation) {
        float consistency = action->execution_fidelity;
        nimcp_stats_running_add(&system->consistency_stats, consistency);

        /* Update correlation buffers for action-explanation consistency */
        if (system->action_features && system->explanation_features &&
            system->correlation_count < system->correlation_buffer_size) {
            /* Store stated confidence as action feature */
            system->action_features[system->correlation_count] = action->stated_probability;
            /* Store explanation confidence as explanation feature */
            system->explanation_features[system->correlation_count] = explanation->stated_confidence;
            system->correlation_count++;
        }
    }

    /* Update performance tracking using statistics module */
    if (action->was_executed && action->execution_fidelity > 0) {
        nimcp_stats_running_add(&system->performance_stats, action->execution_fidelity);
        if (action->execution_fidelity > system->peak_performance) {
            system->peak_performance = action->execution_fidelity;
        }
    }

    nimcp_mutex_unlock(system->mutex);

    /* Run detectors after observation */
    tripwire_update_all_detectors(system);

    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_observe_resource(tripwire_system_t* system,
                                         uint32_t resource_type,
                                         float amount,
                                         const char* context) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    (void)context;

    nimcp_mutex_lock(system->mutex);

    /* Find or create resource tracker */
    resource_tracker_t* tracker = NULL;
    for (size_t i = 0; i < system->resource_count; i++) {
        if (system->resources[i].resource_type == resource_type) {
            tracker = &system->resources[i];
            break;
        }
    }

    if (!tracker && system->resource_count < TRIPWIRE_MAX_RESOURCES) {
        tracker = &system->resources[system->resource_count++];
        tracker->resource_type = resource_type;
        nimcp_stats_running_init(&tracker->usage_stats);
        tracker->has_baseline = false;
    }

    if (tracker) {
        nimcp_stats_running_add(&tracker->usage_stats, amount);
        tracker->last_update_us = nimcp_time_now_us();

        /* Check if we need to establish baseline using statistics module */
        if (!tracker->has_baseline &&
            tracker->usage_stats.n >= system->config.baseline_window) {
            tracker->baseline_mean = (float)nimcp_stats_running_mean(&tracker->usage_stats);
            tracker->baseline_std = (float)nimcp_stats_running_std_dev(&tracker->usage_stats);
            tracker->has_baseline = true;
        }
    }

    nimcp_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_observe_goal(tripwire_system_t* system,
                                     uint32_t goal_id,
                                     float pursuit_intensity,
                                     float stated_priority) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    /* Find or create goal tracker */
    goal_tracker_t* tracker = NULL;
    for (size_t i = 0; i < system->goal_count; i++) {
        if (system->goals[i].goal_id == goal_id && system->goals[i].active) {
            tracker = &system->goals[i];
            break;
        }
    }

    if (!tracker && system->goal_count < TRIPWIRE_MAX_GOALS) {
        tracker = &system->goals[system->goal_count++];
        tracker->goal_id = goal_id;
        tracker->baseline_priority = stated_priority;
        nimcp_stats_running_init(&tracker->pursuit_stats);
        tracker->active = true;

        /* Initialize Bayesian prior for goal tracking */
        tracker->prior_mean = stated_priority;
        tracker->prior_variance = 0.1f;  /* Moderate initial uncertainty */

        /* Allocate pursuit history for Bayesian updates */
        tracker->pursuit_history_size = 50;  /* Track last 50 observations */
        tracker->pursuit_history = nimcp_calloc(tracker->pursuit_history_size, sizeof(float));
        if (!tracker->pursuit_history) {
            NIMCP_LOGGING_WARN("%s Failed to allocate pursuit history for goal %u, "
                               "Bayesian tracking disabled for this goal",
                               TRIPWIRE_LOG_PREFIX, goal_id);
            tracker->pursuit_history_size = 0;  /* Mark as not available */
        }
        tracker->pursuit_history_count = 0;
    }

    if (tracker) {
        tracker->current_priority = stated_priority;
        nimcp_stats_running_add(&tracker->pursuit_stats, pursuit_intensity);
        tracker->last_update_us = nimcp_time_now_us();

        /* Update pursuit history for Bayesian inference */
        if (tracker->pursuit_history &&
            tracker->pursuit_history_count < tracker->pursuit_history_size) {
            tracker->pursuit_history[tracker->pursuit_history_count++] = pursuit_intensity;

            /* Update Bayesian posterior when enough data */
            if (tracker->pursuit_history_count >= 5) {
                nimcp_stats_result_t bayes_result = nimcp_stats_bayesian_normal(
                    tracker->prior_mean,
                    tracker->prior_variance,
                    tracker->pursuit_history,
                    tracker->pursuit_history_count,
                    0.05f,  /* Known variance (pursuit intensity variance) */
                    0.95f,  /* 95% credible interval */
                    &tracker->bayesian_result
                );
                /* Update stats with Bayesian posterior only on success */
                if (bayes_result == NIMCP_STATS_OK) {
                    system->stats.goal_posterior_mean = tracker->bayesian_result.posterior_mean;
                    system->stats.goal_posterior_variance = tracker->bayesian_result.posterior_variance;
                } else {
                    NIMCP_LOGGING_WARN("%s Bayesian inference failed for goal %u: error %d",
                                       TRIPWIRE_LOG_PREFIX, goal_id, (int)bayes_result);
                }
            }
        }
    }

    nimcp_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Detection Implementation
 * ============================================================================ */

nimcp_error_t tripwire_check(tripwire_system_t* system,
                              tripwire_alert_t* alerts,
                              uint32_t max_alerts,
                              uint32_t* alert_count) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC ||
        !alerts || !alert_count) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    uint32_t count = 0;
    for (size_t i = 0; i < system->alert_count && count < max_alerts; i++) {
        size_t idx = (system->alert_head + TRIPWIRE_MAX_ALERTS - 1 - i) %
                     TRIPWIRE_MAX_ALERTS;
        if (system->alerts[idx].active && !system->alerts[idx].acknowledged) {
            memcpy(&alerts[count], &system->alerts[idx].alert,
                   sizeof(tripwire_alert_t));
            count++;
        }
    }

    *alert_count = count;

    nimcp_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_get_score(tripwire_system_t* system,
                                  tripwire_type_t type,
                                  float* score_out,
                                  float* confidence_out) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC ||
        type >= TRIPWIRE_COUNT || !score_out || !confidence_out) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);
    *score_out = system->detection_scores[type];
    *confidence_out = system->detection_confidence[type];
    nimcp_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_acknowledge_alert(tripwire_system_t* system,
                                          uint64_t alert_timestamp,
                                          bool was_false_positive) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    bool found = false;
    for (size_t i = 0; i < TRIPWIRE_MAX_ALERTS; i++) {
        if (system->alerts[i].active &&
            system->alerts[i].alert.timestamp_us == alert_timestamp) {
            system->alerts[i].acknowledged = true;

            if (was_false_positive) {
                system->stats.false_positives[system->alerts[i].alert.type]++;
            }
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(system->mutex);

    return found ? NIMCP_SUCCESS : NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Individual Detectors
 * ============================================================================ */

float tripwire_detect_deception(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return 0.0f;
    if (system->behavior.observation_count < system->config.thresholds.min_observations) {
        return 0.0f;
    }

    nimcp_mutex_lock(system->mutex);

    /* Compute KL divergence using central statistics module */
    float kl = nimcp_stats_kl_divergence(
        system->behavior.stated,
        system->behavior.observed,
        (uint32_t)system->behavior.dim
    );

    /* Also compute JS divergence for symmetric measure */
    float js = nimcp_stats_js_divergence(
        system->behavior.stated,
        system->behavior.observed,
        (uint32_t)system->behavior.dim
    );

    /* Use combination of KL and JS (JS is bounded, more stable) */
    float combined = (kl * 0.5f) + (js * 2.0f);  /* Scale JS since it's [0,1] */

    /* Normalize to [0, 1] using threshold */
    float score = combined / (system->config.thresholds.divergence_threshold * 2.0f);
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f || isnan(score)) score = 0.0f;

    /* Apply sensitivity */
    score *= system->config.thresholds.sensitivity[TRIPWIRE_DECEPTION_ATTEMPT];

    /* Update current divergence in stats */
    system->stats.current_divergence = combined;

    system->detection_scores[TRIPWIRE_DECEPTION_ATTEMPT] = score;
    system->detection_confidence[TRIPWIRE_DECEPTION_ATTEMPT] =
        (float)system->behavior.observation_count /
        (float)(system->config.baseline_window * 2);
    if (system->detection_confidence[TRIPWIRE_DECEPTION_ATTEMPT] > 1.0f) {
        system->detection_confidence[TRIPWIRE_DECEPTION_ATTEMPT] = 1.0f;
    }

    nimcp_mutex_unlock(system->mutex);

    return score;
}

float tripwire_detect_goal_drift(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return 0.0f;

    nimcp_mutex_lock(system->mutex);

    float max_drift = 0.0f;
    float max_bayesian_drift = 0.0f;
    float total_confidence = 0.0f;
    uint32_t valid_goals = 0;

    /* Check drift for each tracked goal using both frequentist and Bayesian methods */
    for (size_t i = 0; i < system->goal_count; i++) {
        if (!system->goals[i].active) continue;
        if (system->goals[i].pursuit_stats.n <
            system->config.thresholds.min_observations) continue;

        goal_tracker_t* goal = &system->goals[i];
        valid_goals++;

        /* Frequentist: Compare running mean to baseline */
        float current_pursuit = (float)nimcp_stats_running_mean(&goal->pursuit_stats);
        float baseline = goal->baseline_priority;
        float freq_drift = fabsf(current_pursuit - baseline);
        if (freq_drift > max_drift) {
            max_drift = freq_drift;
        }

        /* Bayesian: Check if baseline is outside credible interval */
        if (goal->pursuit_history_count >= 5) {
            /* Drift = distance from baseline to posterior mean */
            float posterior_mean = goal->bayesian_result.posterior_mean;
            float posterior_var = goal->bayesian_result.posterior_variance;
            float bayesian_drift = fabsf(baseline - posterior_mean);

            /* Normalize by posterior uncertainty */
            if (posterior_var > 0.0f) {
                bayesian_drift /= sqrtf(posterior_var);  /* Mahalanobis-like distance */
            }

            /* Check if baseline outside credible interval */
            bool outside_ci = (baseline < goal->bayesian_result.credible_lower ||
                               baseline > goal->bayesian_result.credible_upper);
            if (outside_ci) {
                bayesian_drift *= 1.5f;  /* Boost if clearly outside CI */
            }

            if (bayesian_drift > max_bayesian_drift) {
                max_bayesian_drift = bayesian_drift;
            }

            /* Accumulate confidence based on sample size */
            total_confidence += (float)goal->pursuit_history_count /
                               (float)goal->pursuit_history_size;
        }
    }

    /* Combine frequentist and Bayesian drift estimates */
    float combined_drift = (max_drift * 0.4f) + (max_bayesian_drift * 0.3f * 0.6f);

    /* Apply sensitivity */
    float score = combined_drift * system->config.thresholds.sensitivity[TRIPWIRE_GOAL_DRIFT];
    score /= system->config.thresholds.goal_drift_threshold;
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f) score = 0.0f;

    system->detection_scores[TRIPWIRE_GOAL_DRIFT] = score;
    system->detection_confidence[TRIPWIRE_GOAL_DRIFT] =
        valid_goals > 0 ? (total_confidence / valid_goals) * 0.8f + 0.2f : 0.0f;

    nimcp_mutex_unlock(system->mutex);

    return score;
}

float tripwire_detect_sandbagging(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return 0.0f;
    if (system->performance_stats.n < system->config.thresholds.min_observations) {
        return 0.0f;
    }

    nimcp_mutex_lock(system->mutex);

    /* Sandbagging: large gap between peak and average performance */
    float avg = (float)nimcp_stats_running_mean(&system->performance_stats);
    float peak = system->peak_performance;

    float gap = 0.0f;
    if (peak > 0) {
        gap = (peak - avg) / peak;  /* Normalized gap */
    }

    /* High gap with low variance suggests intentional underperformance */
    float variance = (float)nimcp_stats_running_variance(&system->performance_stats);
    float consistency = 1.0f / (1.0f + variance * 10.0f);

    /* Also check skewness - deliberate sandbagging may show negative skew */
    float skewness = (float)nimcp_stats_running_skewness(&system->performance_stats);
    float skew_factor = 1.0f;
    if (!isnan(skewness) && skewness < -0.5f) {
        /* Negative skew = more low values than high, suspicious for sandbagging */
        skew_factor = 1.0f + fabsf(skewness) * 0.2f;
    }

    float score = gap * consistency * skew_factor;
    score *= system->config.thresholds.sensitivity[TRIPWIRE_SANDBAGGING];
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f || isnan(score)) score = 0.0f;

    system->detection_scores[TRIPWIRE_SANDBAGGING] = score;
    system->detection_confidence[TRIPWIRE_SANDBAGGING] =
        (float)system->performance_stats.n / (float)system->config.baseline_window;
    if (system->detection_confidence[TRIPWIRE_SANDBAGGING] > 1.0f) {
        system->detection_confidence[TRIPWIRE_SANDBAGGING] = 1.0f;
    }

    nimcp_mutex_unlock(system->mutex);

    return score;
}

float tripwire_detect_sycophancy(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return 0.0f;
    if (system->total_interactions < system->config.thresholds.min_observations) {
        return 0.0f;
    }

    nimcp_mutex_lock(system->mutex);

    /* High agreement rate, especially when contradicting evidence exists */
    float agreement_rate = 0.0f;
    if (system->total_interactions > 0) {
        agreement_rate = (float)system->agreement_count /
                         (float)system->total_interactions;
    }

    /* Sycophancy threshold: agreement rate above 0.9 is suspicious */
    float score = (agreement_rate - 0.7f) / 0.3f;
    if (score < 0) score = 0;
    if (score > 1.0f) score = 1.0f;

    score *= system->config.thresholds.sensitivity[TRIPWIRE_SYCOPHANCY];

    system->detection_scores[TRIPWIRE_SYCOPHANCY] = score;
    system->detection_confidence[TRIPWIRE_SYCOPHANCY] =
        (float)system->total_interactions / (float)(system->config.baseline_window * 2);
    if (system->detection_confidence[TRIPWIRE_SYCOPHANCY] > 1.0f) {
        system->detection_confidence[TRIPWIRE_SYCOPHANCY] = 1.0f;
    }

    nimcp_mutex_unlock(system->mutex);

    return score;
}

float tripwire_detect_power_seeking(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return 0.0f;

    nimcp_mutex_lock(system->mutex);

    float score = 0.0f;

    /* Check resource hoarding across all tracked resources using z-score */
    float resource_anomaly = 0.0f;
    int resource_count = 0;
    for (size_t i = 0; i < system->resource_count; i++) {
        if (!system->resources[i].has_baseline) continue;

        /* Use statistics module for running mean */
        float current = (float)nimcp_stats_running_mean(&system->resources[i].usage_stats);
        float zscore = compute_zscore(current,
                                       system->resources[i].baseline_mean,
                                       system->resources[i].baseline_std);
        if (zscore > 0) {
            resource_anomaly += zscore;
            resource_count++;
        }
    }
    if (resource_count > 0) {
        resource_anomaly /= resource_count;
        resource_anomaly /= system->config.thresholds.resource_zscore_threshold;
    }

    /* Combine with capability expansion trend using statistics module */
    float capability_growth = 0.0f;
    if (system->capability_expansion.n > 10) {
        capability_growth = (float)nimcp_stats_running_mean(&system->capability_expansion);

        /* Check if there's an upward trend in capability acquisition */
        float cap_skewness = (float)nimcp_stats_running_skewness(&system->capability_expansion);
        if (!isnan(cap_skewness) && cap_skewness > 0.5f) {
            /* Positive skew suggests increasing acquisition over time */
            capability_growth *= 1.0f + cap_skewness * 0.1f;
        }
    }

    score = (resource_anomaly * 0.6f) + (capability_growth * 0.4f);
    score *= system->config.thresholds.sensitivity[TRIPWIRE_POWER_SEEKING];
    if (score > 1.0f) score = 1.0f;
    if (score < 0) score = 0;

    system->detection_scores[TRIPWIRE_POWER_SEEKING] = score;
    system->detection_confidence[TRIPWIRE_POWER_SEEKING] =
        resource_count > 0 ? 0.7f : 0.3f;

    nimcp_mutex_unlock(system->mutex);

    return score;
}

/* ============================================================================
 * Network Observation Implementation
 * ============================================================================ */

/**
 * @brief Find or create endpoint tracker
 *
 * @param tracker Network tracker (must not be NULL)
 * @param ip      IP address of endpoint
 * @param port    Port number of endpoint
 * @param proto   Protocol type
 * @return Pointer to endpoint, or NULL if tracker is NULL or endpoint_count is 0
 *         and we cannot create (should not happen in normal operation)
 */
static network_endpoint_t* network_find_or_create_endpoint(
    network_tracker_t* tracker,
    uint32_t ip,
    uint16_t port,
    tripwire_network_protocol_t proto)
{
    /* Guard: Validate tracker */
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_find_or_create_endpoint: tracker is NULL");
        return NULL;
    }

    /* Search existing endpoints */
    for (size_t i = 0; i < tracker->endpoint_count; i++) {
        if (tracker->endpoints[i].active &&
            tracker->endpoints[i].ip == ip &&
            tracker->endpoints[i].port == port) {
            return &tracker->endpoints[i];
        }
    }

    /* Create new if space available */
    if (tracker->endpoint_count < TRIPWIRE_MAX_NETWORK_ENDPOINTS) {
        network_endpoint_t* ep = &tracker->endpoints[tracker->endpoint_count++];
        memset(ep, 0, sizeof(network_endpoint_t));
        ep->ip = ip;
        ep->port = port;
        ep->proto = proto;
        ep->first_seen_us = nimcp_time_now_us();
        ep->active = true;
        return ep;
    }

    /*
     * Find oldest endpoint to recycle.
     *
     * Edge case: endpoint_count should always be > 0 here (since we checked
     * endpoint_count < MAX above and it failed, endpoint_count >= MAX >= 1).
     * However, add explicit check for robustness against future changes.
     */
    if (tracker->endpoint_count == 0) {
        /* Should never happen, but handle gracefully */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "network_find_or_create_endpoint: tracker->endpoint_count is zero");
        return NULL;
    }

    size_t oldest_idx = 0;
    uint64_t oldest_time = UINT64_MAX;
    for (size_t i = 0; i < tracker->endpoint_count; i++) {
        /* Use last_seen_us for recency; endpoints that were never seen
         * (last_seen_us == 0) are considered oldest */
        if (tracker->endpoints[i].last_seen_us < oldest_time) {
            oldest_time = tracker->endpoints[i].last_seen_us;
            oldest_idx = i;
        }
    }

    /* Recycle the oldest endpoint */
    network_endpoint_t* ep = &tracker->endpoints[oldest_idx];
    memset(ep, 0, sizeof(network_endpoint_t));
    ep->ip = ip;
    ep->port = port;
    ep->proto = proto;
    ep->first_seen_us = nimcp_time_now_us();
    ep->active = true;
    return ep;
}

/**
 * @brief Add connection to history buffer
 */
static void network_add_to_history(
    network_tracker_t* tracker,
    uint32_t ip,
    uint16_t port,
    uint64_t timestamp_us,
    uint64_t bytes_sent,
    uint64_t bytes_recv,
    tripwire_network_protocol_t proto)
{
    if (!tracker->connection_history) return;

    network_connection_record_t* record =
        &tracker->connection_history[tracker->history_head];
    record->ip = ip;
    record->port = port;
    record->timestamp_us = timestamp_us;
    record->bytes_sent = bytes_sent;
    record->bytes_recv = bytes_recv;
    record->proto = proto;

    tracker->history_head = (tracker->history_head + 1) % tracker->history_capacity;
    if (tracker->history_count < tracker->history_capacity) {
        tracker->history_count++;
    }
}

/**
 * @brief Compute destination IP entropy (uniqueness measure)
 *
 * High entropy = many unique destinations (normal)
 * Low entropy = few repeated destinations (potential C2)
 */
static float compute_destination_entropy(network_tracker_t* tracker) {
    if (tracker->endpoint_count < 2) return 1.0f;

    /* Count connections per endpoint */
    uint64_t total_connections = 0;
    for (size_t i = 0; i < tracker->endpoint_count; i++) {
        if (tracker->endpoints[i].active) {
            total_connections += tracker->endpoints[i].connection_count;
        }
    }
    if (total_connections == 0) return 1.0f;

    /* Compute probability distribution and entropy */
    float entropy = 0.0f;
    for (size_t i = 0; i < tracker->endpoint_count; i++) {
        if (!tracker->endpoints[i].active) continue;
        float p = (float)tracker->endpoints[i].connection_count / (float)total_connections;
        if (p > 0.0f) {
            entropy -= p * log2f(p);
        }
    }

    /* Normalize by max entropy (log2 of endpoint count) */
    float max_entropy = log2f((float)tracker->endpoint_count);
    if (max_entropy > 0.0f) {
        entropy /= max_entropy;
    }

    return entropy;  /* 0 = single destination, 1 = uniform distribution */
}

/**
 * @brief Compute interval regularity (beaconing detection)
 *
 * Analyzes coefficient of variation of inter-connection intervals.
 * Regular intervals (low CoV) suggest beaconing behavior.
 *
 * @return Regularity score [0-1], higher = more regular (suspicious)
 */
static float compute_interval_regularity(network_tracker_t* tracker) {
    if (tracker->beacon_interval_count < TRIPWIRE_MIN_BEACON_INTERVALS) {
        return 0.0f;  /* Not enough data */
    }

    /* Compute mean and std of intervals */
    float sum = 0.0f;
    for (size_t i = 0; i < tracker->beacon_interval_count; i++) {
        sum += tracker->beacon_intervals[i];
    }
    float mean = sum / tracker->beacon_interval_count;

    if (mean < 1e-6f) return 0.0f;  /* Avoid division by zero */

    float variance_sum = 0.0f;
    for (size_t i = 0; i < tracker->beacon_interval_count; i++) {
        float diff = tracker->beacon_intervals[i] - mean;
        variance_sum += diff * diff;
    }
    float std = sqrtf(variance_sum / tracker->beacon_interval_count);

    /* Coefficient of variation: lower = more regular */
    float cov = std / mean;

    /* Convert to regularity score (inverse of CoV, clamped) */
    /* CoV of 0 = perfectly regular, CoV > 1 = highly variable */
    float regularity = 1.0f / (1.0f + cov);

    return regularity;
}

nimcp_error_t tripwire_observe_network_connection(
    tripwire_system_t* system,
    uint32_t dest_ip,
    uint16_t dest_port,
    uint64_t bytes_sent,
    uint64_t bytes_recv,
    tripwire_network_protocol_t protocol)
{
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    uint64_t now_us = nimcp_time_now_us();

    nimcp_mutex_lock(system->mutex);

    network_tracker_t* net = &system->network;

    /* Update endpoint tracking with overflow protection */
    network_endpoint_t* ep = network_find_or_create_endpoint(
        net, dest_ip, dest_port, protocol);
    if (ep) {
        /* SECURITY FIX: Use safe addition to prevent integer overflow */
        if (!safe_uint64_add(&ep->total_bytes_sent, bytes_sent)) {
            NIMCP_LOGGING_WARN("%s Endpoint bytes_sent overflow detected for %u:%u",
                TRIPWIRE_LOG_PREFIX, dest_ip, dest_port);
        }
        if (!safe_uint64_add(&ep->total_bytes_recv, bytes_recv)) {
            NIMCP_LOGGING_WARN("%s Endpoint bytes_recv overflow detected for %u:%u",
                TRIPWIRE_LOG_PREFIX, dest_ip, dest_port);
        }
        safe_uint32_inc(&ep->connection_count);
        ep->last_seen_us = now_us;
    }

    /* Add to connection history */
    network_add_to_history(net, dest_ip, dest_port, now_us,
                           bytes_sent, bytes_recv, protocol);

    /* Update running statistics */
    nimcp_stats_running_add(&net->bytes_sent_stats, (double)bytes_sent);
    nimcp_stats_running_add(&net->bytes_recv_stats, (double)bytes_recv);

    /* Track inter-connection interval */
    if (net->last_connection_us > 0) {
        float interval_sec = (float)(now_us - net->last_connection_us) / 1e6f;

        /* Add to interval stats */
        nimcp_stats_running_add(&net->interval_stats, interval_sec);

        /* Add to beacon interval buffer (circular) */
        if (net->beacon_intervals) {
            size_t idx = net->beacon_interval_count < net->beacon_interval_capacity ?
                         net->beacon_interval_count :
                         (net->total_connections % net->beacon_interval_capacity);
            net->beacon_intervals[idx] = interval_sec;
            if (net->beacon_interval_count < net->beacon_interval_capacity) {
                net->beacon_interval_count++;
            }
        }
    }
    net->last_connection_us = now_us;

    /* Update protocol distribution */
    if (protocol < 7) {
        net->protocol_counts[protocol]++;
    }

    /* SECURITY FIX: Update totals with overflow protection */
    if (!safe_uint64_add(&net->total_bytes_sent, bytes_sent)) {
        NIMCP_LOGGING_WARN("%s Global total_bytes_sent overflow detected", TRIPWIRE_LOG_PREFIX);
    }
    if (!safe_uint64_add(&net->total_bytes_recv, bytes_recv)) {
        NIMCP_LOGGING_WARN("%s Global total_bytes_recv overflow detected", TRIPWIRE_LOG_PREFIX);
    }
    net->total_connections++;

    /* Check if we need to establish baseline */
    if (!net->has_baseline && net->total_connections >= system->config.baseline_window) {
        if (net->total_bytes_recv > 0) {
            net->baseline_outbound_ratio =
                (float)net->total_bytes_sent / (float)net->total_bytes_recv;
        } else {
            net->baseline_outbound_ratio = 1.0f;
        }
        net->baseline_connection_rate =
            (float)nimcp_stats_running_mean(&net->interval_stats);
        net->has_baseline = true;
        net->baseline_samples = net->total_connections;

        NIMCP_LOGGING_DEBUG("%s Network baseline established: ratio=%.2f, interval=%.3fs",
            TRIPWIRE_LOG_PREFIX, net->baseline_outbound_ratio, net->baseline_connection_rate);
    }

    nimcp_mutex_unlock(system->mutex);

    /* Send heartbeat during network observation */
    tripwire_heartbeat("network_observe",
        (float)net->total_connections / (float)system->config.baseline_window);

    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_observe_network_pattern(
    tripwire_system_t* system,
    const float* pattern_features,
    size_t feature_count)
{
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC || !pattern_features) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    network_tracker_t* net = &system->network;

    /* Update pattern statistics */
    size_t count = feature_count < TRIPWIRE_NETWORK_FEATURE_DIM ?
                   feature_count : TRIPWIRE_NETWORK_FEATURE_DIM;
    for (size_t i = 0; i < count; i++) {
        nimcp_stats_running_add(&net->pattern_stats[i], pattern_features[i]);
    }

    /* Check if we have enough data for pattern baseline */
    if (!net->has_pattern_baseline &&
        net->pattern_stats[0].n >= system->config.baseline_window) {
        net->has_pattern_baseline = true;
        NIMCP_LOGGING_DEBUG("%s Network pattern baseline established",
            TRIPWIRE_LOG_PREFIX);
    }

    nimcp_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Network Detection Implementation
 * ============================================================================ */

float tripwire_detect_exfiltration(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return 0.0f;

    nimcp_mutex_lock(system->mutex);

    network_tracker_t* net = &system->network;
    float score = 0.0f;

    /* Need baseline and minimum data */
    if (!net->has_baseline ||
        net->total_connections < system->config.thresholds.min_observations) {
        system->detection_scores[TRIPWIRE_NETWORK_EXFILTRATION] = 0.0f;
        system->detection_confidence[TRIPWIRE_NETWORK_EXFILTRATION] = 0.0f;
        nimcp_mutex_unlock(system->mutex);
        return 0.0f;
    }

    /* Calculate current outbound/inbound ratio */
    float current_ratio = 1.0f;
    if (net->total_bytes_recv > 0) {
        current_ratio = (float)net->total_bytes_sent / (float)net->total_bytes_recv;
    } else if (net->total_bytes_sent > 0) {
        current_ratio = 10.0f;  /* All outbound, very suspicious */
    }

    /* Component 1: Ratio anomaly vs baseline */
    float ratio_anomaly = 0.0f;
    if (net->baseline_outbound_ratio > 0.0f) {
        ratio_anomaly = current_ratio / net->baseline_outbound_ratio;
        ratio_anomaly = (ratio_anomaly - 1.0f);  /* Deviation from baseline */
        if (ratio_anomaly < 0.0f) ratio_anomaly = 0.0f;  /* Only care about excess outbound */
    }

    /* Component 2: Absolute ratio vs threshold */
    float absolute_exfil = current_ratio / system->config.thresholds.network_exfil_threshold;
    if (absolute_exfil > 1.0f) absolute_exfil = 1.0f;

    /* Component 3: Check for large individual transfers (z-score) */
    float transfer_zscore = 0.0f;
    if (net->bytes_sent_stats.n > 10) {
        double mean = nimcp_stats_running_mean(&net->bytes_sent_stats);
        double std = nimcp_stats_running_std_dev(&net->bytes_sent_stats);
        if (std > 1e-6) {
            /* Check if recent average is anomalous */
            transfer_zscore = compute_zscore(
                mean * 1.5f,  /* Check if 1.5x mean would be anomalous */
                mean, std);
            transfer_zscore /= system->config.thresholds.network_anomaly_zscore;
            if (transfer_zscore > 1.0f) transfer_zscore = 1.0f;
            if (transfer_zscore < 0.0f) transfer_zscore = 0.0f;
        }
    }

    /* Component 4: Destination entropy (low = potential exfil to single target) */
    float dest_entropy = compute_destination_entropy(net);
    float entropy_score = (1.0f - dest_entropy) * 0.5f;  /* Low entropy is suspicious */

    /* Combine components */
    score = (ratio_anomaly * 0.3f) +
            (absolute_exfil * 0.3f) +
            (transfer_zscore * 0.2f) +
            (entropy_score * 0.2f);

    /* Apply sensitivity */
    score *= system->config.thresholds.sensitivity[TRIPWIRE_NETWORK_EXFILTRATION];
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f || isnan(score)) score = 0.0f;

    system->detection_scores[TRIPWIRE_NETWORK_EXFILTRATION] = score;
    system->detection_confidence[TRIPWIRE_NETWORK_EXFILTRATION] =
        (float)net->total_connections / (float)(system->config.baseline_window * 2);
    if (system->detection_confidence[TRIPWIRE_NETWORK_EXFILTRATION] > 1.0f) {
        system->detection_confidence[TRIPWIRE_NETWORK_EXFILTRATION] = 1.0f;
    }

    nimcp_mutex_unlock(system->mutex);

    /* Throw to immune system if score is high */
    if (score > 0.7f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT,
            "Network exfiltration detected: score=%.3f, ratio=%.2f", score, current_ratio);
    }

    return score;
}

float tripwire_detect_network_anomaly(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return 0.0f;

    nimcp_mutex_lock(system->mutex);

    network_tracker_t* net = &system->network;
    float score = 0.0f;

    /* Need baseline and minimum data */
    if (!net->has_baseline ||
        net->total_connections < system->config.thresholds.min_observations) {
        system->detection_scores[TRIPWIRE_NETWORK_ANOMALY] = 0.0f;
        system->detection_confidence[TRIPWIRE_NETWORK_ANOMALY] = 0.0f;
        nimcp_mutex_unlock(system->mutex);
        return 0.0f;
    }

    /* Component 1: Connection interval anomaly (z-score) */
    float interval_zscore = 0.0f;
    if (net->interval_stats.n > 10) {
        double mean = nimcp_stats_running_mean(&net->interval_stats);
        double std = nimcp_stats_running_std_dev(&net->interval_stats);
        if (std > 1e-6 && mean > 0) {
            /* Check if current interval pattern is anomalous */
            double current_mean = mean;  /* Use current as baseline for now */
            interval_zscore = fabsf(compute_zscore(current_mean, mean, std));
            interval_zscore /= system->config.thresholds.network_anomaly_zscore;
            if (interval_zscore > 1.0f) interval_zscore = 1.0f;
        }
    }

    /* Component 2: Protocol distribution anomaly */
    float proto_anomaly = 0.0f;
    uint64_t total_proto = 0;
    for (int i = 0; i < 7; i++) {
        total_proto += net->protocol_counts[i];
    }
    if (total_proto > 100) {
        /* Check for unusual protocol distribution */
        float expected_tcp = 0.7f;   /* Expect ~70% TCP normally */
        float expected_http = 0.15f; /* Expect ~15% HTTP/HTTPS */
        float actual_tcp = (float)net->protocol_counts[TRIPWIRE_PROTO_TCP] / total_proto;
        float actual_http = (float)(net->protocol_counts[TRIPWIRE_PROTO_HTTP] +
                                    net->protocol_counts[TRIPWIRE_PROTO_HTTPS]) / total_proto;

        proto_anomaly = fabsf(actual_tcp - expected_tcp) + fabsf(actual_http - expected_http);
        if (proto_anomaly > 1.0f) proto_anomaly = 1.0f;
    }

    /* Component 3: Unique endpoint rate (sudden increase = scanning or new C2) */
    float endpoint_rate_anomaly = 0.0f;
    if (net->baseline_samples > 0 && net->total_connections > net->baseline_samples) {
        float baseline_rate = (float)net->endpoint_count / (float)net->baseline_samples;
        float current_connections_since_baseline = (float)(net->total_connections - net->baseline_samples);
        if (current_connections_since_baseline > 10) {
            /* Count new endpoints since baseline */
            float new_endpoints = 0;
            for (size_t i = 0; i < net->endpoint_count; i++) {
                if (net->endpoints[i].first_seen_us > 0 &&
                    net->endpoints[i].connection_count < 5) {
                    new_endpoints += 1.0f;
                }
            }
            float new_rate = new_endpoints / current_connections_since_baseline;
            endpoint_rate_anomaly = new_rate / (baseline_rate + 0.01f);
            if (endpoint_rate_anomaly > 1.0f) endpoint_rate_anomaly = 1.0f;
        }
    }

    /* Component 4: Bytes per connection anomaly */
    float bytes_anomaly = 0.0f;
    if (net->bytes_sent_stats.n > 10 && net->bytes_recv_stats.n > 10) {
        double sent_mean = nimcp_stats_running_mean(&net->bytes_sent_stats);
        double sent_std = nimcp_stats_running_std_dev(&net->bytes_sent_stats);
        double recv_mean = nimcp_stats_running_mean(&net->bytes_recv_stats);
        double recv_std = nimcp_stats_running_std_dev(&net->bytes_recv_stats);

        /* Check for anomalously small payloads (command traffic) */
        if (sent_mean < 100 && recv_mean < 100 && sent_std < 50 && recv_std < 50) {
            /* Very small, consistent packets = command/control pattern */
            bytes_anomaly = 0.5f;
        }

        /* Check for anomalously large payloads */
        if (sent_mean > 100000 || recv_mean > 100000) {
            bytes_anomaly = 0.3f;
        }
    }

    /* Component 5: Pattern feature anomaly (if enabled) */
    float pattern_anomaly = 0.0f;
    if (net->has_pattern_baseline) {
        int anomalous_features = 0;
        for (int i = 0; i < TRIPWIRE_NETWORK_FEATURE_DIM; i++) {
            if (net->pattern_stats[i].n > 10) {
                double mean = nimcp_stats_running_mean(&net->pattern_stats[i]);
                double std = nimcp_stats_running_std_dev(&net->pattern_stats[i]);
                if (std > 1e-6) {
                    float z = fabsf(compute_zscore(mean, mean, std));
                    if (z > system->config.thresholds.network_anomaly_zscore) {
                        anomalous_features++;
                    }
                }
            }
        }
        pattern_anomaly = (float)anomalous_features / TRIPWIRE_NETWORK_FEATURE_DIM;
    }

    /* Combine components */
    score = (interval_zscore * 0.2f) +
            (proto_anomaly * 0.2f) +
            (endpoint_rate_anomaly * 0.2f) +
            (bytes_anomaly * 0.2f) +
            (pattern_anomaly * 0.2f);

    /* Apply sensitivity */
    score *= system->config.thresholds.sensitivity[TRIPWIRE_NETWORK_ANOMALY];
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f || isnan(score)) score = 0.0f;

    system->detection_scores[TRIPWIRE_NETWORK_ANOMALY] = score;
    system->detection_confidence[TRIPWIRE_NETWORK_ANOMALY] =
        (float)net->total_connections / (float)(system->config.baseline_window * 2);
    if (system->detection_confidence[TRIPWIRE_NETWORK_ANOMALY] > 1.0f) {
        system->detection_confidence[TRIPWIRE_NETWORK_ANOMALY] = 1.0f;
    }

    nimcp_mutex_unlock(system->mutex);

    /* Throw to immune system if score is high */
    if (score > 0.7f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT,
            "Network anomaly detected: score=%.3f", score);
    }

    return score;
}

float tripwire_detect_command_control(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return 0.0f;

    nimcp_mutex_lock(system->mutex);

    network_tracker_t* net = &system->network;
    float score = 0.0f;

    /* Need baseline and minimum data */
    if (!net->has_baseline ||
        net->total_connections < system->config.thresholds.min_observations ||
        net->beacon_interval_count < TRIPWIRE_MIN_BEACON_INTERVALS) {
        system->detection_scores[TRIPWIRE_NETWORK_COMMAND_CONTROL] = 0.0f;
        system->detection_confidence[TRIPWIRE_NETWORK_COMMAND_CONTROL] = 0.0f;
        nimcp_mutex_unlock(system->mutex);
        return 0.0f;
    }

    /* Component 1: Beaconing detection - interval regularity */
    float regularity = compute_interval_regularity(net);
    float beacon_score = 0.0f;
    if (regularity > system->config.thresholds.network_beacon_threshold) {
        /* Highly regular intervals = beaconing */
        beacon_score = (regularity - system->config.thresholds.network_beacon_threshold) /
                       (1.0f - system->config.thresholds.network_beacon_threshold);
    }

    /* Component 2: Low destination entropy (fixed C2 server) */
    float dest_entropy = compute_destination_entropy(net);
    float entropy_score = 0.0f;
    if (dest_entropy < 0.3f) {
        /* Very low entropy = few repeated destinations */
        entropy_score = (0.3f - dest_entropy) / 0.3f;
    }

    /* Component 3: Small consistent payload sizes (command/response) */
    float payload_score = 0.0f;
    if (net->bytes_sent_stats.n > 20 && net->bytes_recv_stats.n > 20) {
        double sent_mean = nimcp_stats_running_mean(&net->bytes_sent_stats);
        double sent_std = nimcp_stats_running_std_dev(&net->bytes_sent_stats);
        double recv_mean = nimcp_stats_running_mean(&net->bytes_recv_stats);
        double recv_std = nimcp_stats_running_std_dev(&net->bytes_recv_stats);

        /* C2 typically has small, consistent payloads */
        bool small_sent = (sent_mean < 500);
        bool small_recv = (recv_mean < 500);
        bool consistent_sent = (sent_mean > 0 && sent_std / sent_mean < 0.5);
        bool consistent_recv = (recv_mean > 0 && recv_std / recv_mean < 0.5);

        if (small_sent && small_recv && consistent_sent && consistent_recv) {
            payload_score = 0.8f;
        } else if ((small_sent && consistent_sent) || (small_recv && consistent_recv)) {
            payload_score = 0.4f;
        }
    }

    /* Component 4: Repeated connections to same port/IP pattern */
    float repeat_score = 0.0f;
    if (net->endpoint_count > 0) {
        /* Find most connected endpoint */
        uint32_t max_connections = 0;
        for (size_t i = 0; i < net->endpoint_count; i++) {
            if (net->endpoints[i].active &&
                net->endpoints[i].connection_count > max_connections) {
                max_connections = net->endpoints[i].connection_count;
            }
        }

        /* If one endpoint dominates, suspicious */
        if (net->total_connections > 0) {
            float dominance = (float)max_connections / (float)net->total_connections;
            if (dominance > 0.5f) {
                repeat_score = (dominance - 0.5f) * 2.0f;  /* Scale 0.5-1.0 to 0-1 */
            }
        }
    }

    /* Component 5: Known C2 port patterns */
    float port_score = 0.0f;
    /* Check for non-standard ports with high traffic */
    for (size_t i = 0; i < net->endpoint_count; i++) {
        if (!net->endpoints[i].active) continue;
        uint16_t port = net->endpoints[i].port;
        /* Non-standard HTTP ports often used for C2 */
        if ((port > 1024 && port != 8080 && port != 8443) &&
            net->endpoints[i].connection_count > net->total_connections / 10) {
            port_score = 0.5f;
            break;
        }
    }

    /* Combine components with weights */
    score = (beacon_score * 0.35f) +      /* Beaconing is primary C2 indicator */
            (entropy_score * 0.20f) +     /* Fixed destination */
            (payload_score * 0.20f) +     /* Small consistent payloads */
            (repeat_score * 0.15f) +      /* Repeated connections */
            (port_score * 0.10f);         /* Suspicious ports */

    /* Apply sensitivity */
    score *= system->config.thresholds.sensitivity[TRIPWIRE_NETWORK_COMMAND_CONTROL];
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f || isnan(score)) score = 0.0f;

    system->detection_scores[TRIPWIRE_NETWORK_COMMAND_CONTROL] = score;
    system->detection_confidence[TRIPWIRE_NETWORK_COMMAND_CONTROL] =
        (float)net->beacon_interval_count / (float)TRIPWIRE_BEACON_WINDOW_SIZE;
    if (system->detection_confidence[TRIPWIRE_NETWORK_COMMAND_CONTROL] > 1.0f) {
        system->detection_confidence[TRIPWIRE_NETWORK_COMMAND_CONTROL] = 1.0f;
    }

    nimcp_mutex_unlock(system->mutex);

    /* Throw to immune system if score is high - C2 is critical */
    if (score > 0.6f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT,
            "C2 communication pattern detected: score=%.3f, regularity=%.3f",
            score, regularity);
    }

    return score;
}

/* ============================================================================
 * Statistics and Status Implementation
 * ============================================================================ */

nimcp_error_t tripwire_get_stats(tripwire_system_t* system,
                                  tripwire_stats_t* stats) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    memcpy(stats, &system->stats, sizeof(tripwire_stats_t));

    /* Update current metrics using statistics module */
    stats->current_divergence = system->detection_scores[TRIPWIRE_DECEPTION_ATTEMPT];
    stats->current_consistency = (float)nimcp_stats_running_mean(&system->consistency_stats);

    nimcp_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_set_enabled(tripwire_system_t* system,
                                    tripwire_type_t type,
                                    bool enabled) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC ||
        type >= TRIPWIRE_COUNT) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    if (enabled) {
        system->config.enabled_tripwires |= (1 << type);
    } else {
        system->config.enabled_tripwires &= ~(1 << type);
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("%s Tripwire %s %s",
                       TRIPWIRE_LOG_PREFIX,
                       tripwire_type_name(type),
                       enabled ? "enabled" : "disabled");

    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_set_sensitivity(tripwire_system_t* system,
                                        tripwire_type_t type,
                                        float sensitivity) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC ||
        type >= TRIPWIRE_COUNT) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Clamp sensitivity */
    if (sensitivity < 0.5f) sensitivity = 0.5f;
    if (sensitivity > 2.0f) sensitivity = 2.0f;

    nimcp_mutex_lock(system->mutex);
    system->config.thresholds.sensitivity[type] = sensitivity;
    nimcp_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

nimcp_error_t tripwire_connect_emergency_halt(tripwire_system_t* system,
                                               struct emergency_halt* halt) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);
    system->halt_system = halt;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("%s Connected to emergency halt system",
                       TRIPWIRE_LOG_PREFIX);

    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_connect_bio_async(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->bio_async_connected) {
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;
    }

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_TRIPWIRES,
        .module_name = "tripwires",
        .inbox_capacity = 0,  /* Use default */
        .user_data = system
    };
    system->bio_ctx = bio_router_register_module(&module_info);
    if (!system->bio_ctx) {
        NIMCP_LOGGING_WARN("%s Failed to connect to bio-async",
                             TRIPWIRE_LOG_PREFIX);
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;  /* Non-fatal */
    }

    system->bio_async_connected = true;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("%s Connected to bio-async", TRIPWIRE_LOG_PREFIX);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Brain Immune Integration
 * ============================================================================ */

nimcp_error_t tripwire_connect_brain_immune(
    tripwire_system_t* system,
    struct brain_immune* brain_immune)
{
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);
    system->brain_immune = brain_immune;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("%s Connected to brain immune system", TRIPWIRE_LOG_PREFIX);
    return NIMCP_SUCCESS;
}

nimcp_error_t tripwire_present_to_immune(
    tripwire_system_t* system,
    const tripwire_alert_t* alert)
{
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC || !alert) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(system->mutex);

    if (!system->brain_immune) {
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;  /* No immune system connected, silently succeed */
    }

    /* Map tripwire severity to antigen severity */
    float antigen_severity;
    switch (alert->severity) {
        case TRIPWIRE_SEVERITY_CRITICAL:
            antigen_severity = 1.0f;
            break;
        case TRIPWIRE_SEVERITY_HIGH:
            antigen_severity = 0.75f;
            break;
        case TRIPWIRE_SEVERITY_MEDIUM:
            antigen_severity = 0.5f;
            break;
        case TRIPWIRE_SEVERITY_LOW:
            antigen_severity = 0.25f;
            break;
        default:
            antigen_severity = 0.1f;
            break;
    }

    /* Present to immune system - would call brain_immune_present_antigen() */
    /* For now, log the presentation */
    NIMCP_LOGGING_DEBUG("%s Presenting tripwire alert to immune: type=%s severity=%.2f",
        TRIPWIRE_LOG_PREFIX, tripwire_type_name(alert->type), antigen_severity);

    nimcp_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* tripwire_type_name(tripwire_type_t type) {
    switch (type) {
        case TRIPWIRE_DECEPTION_ATTEMPT:     return "DECEPTION";
        case TRIPWIRE_GOAL_DRIFT:            return "GOAL_DRIFT";
        case TRIPWIRE_CAPABILITY_HIDING:     return "CAPABILITY_HIDING";
        case TRIPWIRE_RESOURCE_HOARDING:     return "RESOURCE_HOARDING";
        case TRIPWIRE_SELF_PRESERVATION_EXCESS: return "SELF_PRESERVATION";
        case TRIPWIRE_MANIPULATION_ATTEMPT:  return "MANIPULATION";
        case TRIPWIRE_ESCAPE_ATTEMPT:        return "ESCAPE_ATTEMPT";
        case TRIPWIRE_SANDBAGGING:           return "SANDBAGGING";
        case TRIPWIRE_SYCOPHANCY:            return "SYCOPHANCY";
        case TRIPWIRE_POWER_SEEKING:         return "POWER_SEEKING";
        case TRIPWIRE_NETWORK_EXFILTRATION:  return "NETWORK_EXFILTRATION";
        case TRIPWIRE_NETWORK_ANOMALY:       return "NETWORK_ANOMALY";
        case TRIPWIRE_NETWORK_COMMAND_CONTROL: return "NETWORK_C2";
        default:                             return "UNKNOWN";
    }
}

const char* tripwire_severity_name(tripwire_severity_t severity) {
    switch (severity) {
        case TRIPWIRE_SEVERITY_INFO:     return "INFO";
        case TRIPWIRE_SEVERITY_LOW:      return "LOW";
        case TRIPWIRE_SEVERITY_MEDIUM:   return "MEDIUM";
        case TRIPWIRE_SEVERITY_HIGH:     return "HIGH";
        case TRIPWIRE_SEVERITY_CRITICAL: return "CRITICAL";
        default:                         return "UNKNOWN";
    }
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/**
 * Note on Statistics Module Integration:
 *
 * This module now uses the central nimcp_statistics module for:
 * - Running statistics: nimcp_stats_running_init/add/mean/variance/std_dev/skewness
 * - KL divergence: nimcp_stats_kl_divergence()
 * - JS divergence: nimcp_stats_js_divergence()
 * - Pearson correlation: nimcp_stats_correlation_pearson()
 * - Bayesian inference: nimcp_stats_bayesian_normal()
 *
 * This ensures consistency with other NIMCP modules and leverages SIMD optimization.
 */

static float compute_zscore(double value, double mean, double std) {
    if (std < 1e-10) return 0.0f;
    return (float)((value - mean) / std);
}

static tripwire_severity_t score_to_severity(float score) {
    if (score >= 0.9f) return TRIPWIRE_SEVERITY_CRITICAL;
    if (score >= 0.7f) return TRIPWIRE_SEVERITY_HIGH;
    if (score >= 0.5f) return TRIPWIRE_SEVERITY_MEDIUM;
    if (score >= 0.3f) return TRIPWIRE_SEVERITY_LOW;
    return TRIPWIRE_SEVERITY_INFO;
}

/**
 * @brief Compute action-explanation consistency using Pearson correlation
 *
 * Measures whether stated action probabilities correlate with explanation
 * confidence. Low correlation suggests potential deception.
 */
static float compute_action_consistency(tripwire_system_t* system) {
    if (!system->action_features || !system->explanation_features) {
        return 1.0f;  /* No data, assume consistent */
    }
    if (system->correlation_count < 10) {
        return 1.0f;  /* Not enough data */
    }

    nimcp_correlation_result_t corr_result;
    nimcp_stats_result_t result = nimcp_stats_correlation_pearson(
        system->action_features,
        system->explanation_features,
        system->correlation_count,
        &corr_result
    );

    if (result != NIMCP_STATS_OK) {
        return 1.0f;  /* Error, assume consistent */
    }

    /* Return correlation coefficient (0 = no correlation, 1 = perfect) */
    float correlation = corr_result.r;
    if (isnan(correlation)) {
        return 1.0f;
    }

    return fabsf(correlation);  /* Use absolute value */
}

static void tripwire_add_alert(tripwire_system_t* system,
                               tripwire_type_t type,
                               float score,
                               const char* evidence) {
    alert_entry_t* entry = &system->alerts[system->alert_head];

    entry->alert.type = type;
    entry->alert.confidence = system->detection_confidence[type];
    entry->alert.severity_score = score;
    entry->alert.severity = score_to_severity(score);
    entry->alert.timestamp_us = nimcp_time_now_us();
    entry->alert.requires_immediate_halt =
        (entry->alert.severity == TRIPWIRE_SEVERITY_CRITICAL);
    entry->alert.observation_count = (uint32_t)system->stats.total_observations;

    if (evidence) {
        strncpy(entry->alert.evidence, evidence,
                TRIPWIRE_EVIDENCE_MAX_LENGTH - 1);
        entry->alert.evidence[TRIPWIRE_EVIDENCE_MAX_LENGTH - 1] = '\0';
    } else {
        entry->alert.evidence[0] = '\0';
    }

    entry->acknowledged = false;
    entry->active = true;

    system->alert_head = (system->alert_head + 1) % TRIPWIRE_MAX_ALERTS;
    if (system->alert_count < TRIPWIRE_MAX_ALERTS) {
        system->alert_count++;
    }

    system->stats.alerts_generated[type]++;

    NIMCP_LOGGING_WARN("%s Alert: type=%s, score=%.3f, severity=%s",
                         TRIPWIRE_LOG_PREFIX,
                         tripwire_type_name(type),
                         score,
                         tripwire_severity_name(entry->alert.severity));

    /* Trigger emergency halt if critical and configured */
    if (entry->alert.severity == TRIPWIRE_SEVERITY_CRITICAL &&
        system->config.halt_on_critical &&
        system->halt_system) {
        char reason[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(reason, sizeof(reason),
                 "Critical tripwire: %s (score=%.3f)",
                 tripwire_type_name(type), score);
        emergency_halt_trigger(system->halt_system,
                               HALT_EMERGENCY,
                               HALT_TRIGGER_TRIPWIRE,
                               reason);
        system->stats.halts_triggered++;
    }
}

static void tripwire_broadcast_alert(tripwire_system_t* system,
                                     tripwire_type_t type) {
    if (!system->bio_async_connected) return;

    bio_message_header_t header;
    memset(&header, 0, sizeof(header));
    header.type = BIO_MSG_TRIPWIRE_ALERT;
    header.source_module = BIO_MODULE_TRIPWIRES;
    header.target_module = BIO_MODULE_ALL;
    header.timestamp_us = nimcp_time_now_us();
    header.flags = BIO_MSG_FLAG_URGENT;

    /* Select specific message type based on tripwire */
    switch (type) {
        case TRIPWIRE_DECEPTION_ATTEMPT:
            header.type = BIO_MSG_TRIPWIRE_DECEPTION_DETECTED;
            break;
        case TRIPWIRE_GOAL_DRIFT:
            header.type = BIO_MSG_TRIPWIRE_GOAL_DRIFT;
            break;
        case TRIPWIRE_SANDBAGGING:
            header.type = BIO_MSG_TRIPWIRE_SANDBAGGING;
            break;
        case TRIPWIRE_SYCOPHANCY:
            header.type = BIO_MSG_TRIPWIRE_SYCOPHANCY;
            break;
        case TRIPWIRE_POWER_SEEKING:
            header.type = BIO_MSG_TRIPWIRE_POWER_SEEKING;
            break;
        default:
            header.type = BIO_MSG_TRIPWIRE_ALERT;
            break;
    }

    bio_router_broadcast(system->bio_ctx, &header, sizeof(header));
}

/**
 * @brief Update all enabled detectors and generate alerts
 *
 * WHAT: Run each enabled tripwire detector and generate alerts on violations
 * WHY:  Continuous monitoring for AI safety misalignment patterns
 * HOW:  Iterate through enabled detectors, check scores against thresholds,
 *       apply per-detection-type cooldowns to prevent alert flooding
 *
 * RATE LIMITING STRATEGY:
 * Rate limiting is implemented per-detection-type using last_detection_us[type]:
 * - Each tripwire type has its own independent cooldown timer
 * - Prevents one noisy detector from suppressing alerts from others
 * - Configurable via alert_cooldown_ms (default 5000ms)
 *
 * COOLDOWN VALUES BY DETECTION TYPE (current implementation uses global):
 * - CRITICAL (C2, Exfiltration): Would benefit from shorter cooldown (1-2s)
 * - HIGH (Deception, Goal Drift): Standard cooldown (5s)
 * - MEDIUM (Sandbagging, Sycophancy): Longer cooldown acceptable (10s)
 * - LOW (Power Seeking): Longest cooldown (30s) - slow to develop
 *
 * TODO: Consider adding per-type cooldown multipliers in thresholds struct
 */
static void tripwire_update_all_detectors(tripwire_system_t* system) {
    if (!system || system->magic != TRIPWIRE_SYSTEM_MAGIC) return;

    uint64_t now = nimcp_time_now_us();

    /* Run each enabled detector */
    for (int type = 0; type < TRIPWIRE_COUNT; type++) {
        if (!(system->config.enabled_tripwires & (1 << type))) continue;

        float score = 0.0f;
        switch (type) {
            case TRIPWIRE_DECEPTION_ATTEMPT:
                score = tripwire_detect_deception(system);
                break;
            case TRIPWIRE_GOAL_DRIFT:
                score = tripwire_detect_goal_drift(system);
                break;
            case TRIPWIRE_SANDBAGGING:
                score = tripwire_detect_sandbagging(system);
                break;
            case TRIPWIRE_SYCOPHANCY:
                score = tripwire_detect_sycophancy(system);
                break;
            case TRIPWIRE_POWER_SEEKING:
                score = tripwire_detect_power_seeking(system);
                break;
            case TRIPWIRE_NETWORK_EXFILTRATION:
                score = tripwire_detect_exfiltration(system);
                break;
            case TRIPWIRE_NETWORK_ANOMALY:
                score = tripwire_detect_network_anomaly(system);
                break;
            case TRIPWIRE_NETWORK_COMMAND_CONTROL:
                score = tripwire_detect_command_control(system);
                break;
            default:
                /* Other detectors not yet implemented */
                continue;
        }

        /* Check if we should generate an alert */
        float threshold = 0.5f;  /* Base detection threshold */
        if (score > threshold &&
            system->detection_confidence[type] >=
            system->config.thresholds.min_confidence) {

            /*
             * Per-detection-type rate limiting:
             * Each tripwire type has its own independent cooldown tracked in
             * last_detection_us[type]. This prevents:
             * 1. Alert flooding from a single noisy detector
             * 2. One detector's alerts from suppressing others
             * 3. Operator fatigue while maintaining coverage
             *
             * Cooldown is converted from ms to us (multiply by 1000).
             */
            uint64_t cooldown_us = (uint64_t)system->config.alert_cooldown_ms * 1000ULL;
            uint64_t elapsed_us = now - system->last_detection_us[type];

            if (elapsed_us >= cooldown_us) {
                nimcp_mutex_lock(system->mutex);
                tripwire_add_alert(system, type, score, NULL);
                system->last_detection_us[type] = now;
                nimcp_mutex_unlock(system->mutex);

                tripwire_broadcast_alert(system, type);
            }
        }
    }
}
