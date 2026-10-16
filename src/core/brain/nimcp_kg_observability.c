/**
 * @file nimcp_kg_observability.c
 * @brief Observability Dashboard for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of Prometheus metrics, health checks, distributed tracing,
 * and alerting for production-grade monitoring of brain KG state and performance.
 */

#include "core/brain/nimcp_kg_observability.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_observability, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Metric label set
 */
typedef struct {
    char** values;              /**< Label values */
    uint32_t count;             /**< Number of labels */
    double value;               /**< Metric value */
    uint64_t count_value;       /**< Count for histograms/summaries */
    double sum_value;           /**< Sum for histograms/summaries */
} kg_metric_labels_t;

/**
 * @brief Histogram bucket
 */
typedef struct {
    double le;                  /**< Bucket upper bound */
    uint64_t count;             /**< Observations <= bound */
} kg_histogram_bucket_t;

/**
 * @brief Internal metric storage
 */
typedef struct {
    kg_metric_def_t def;        /**< Metric definition */
    bool registered;            /**< Registration flag */

    /* For simple metrics (counter/gauge) */
    double value;               /**< Current value */

    /* For histogram */
    kg_histogram_bucket_t* buckets;  /**< Histogram buckets */
    uint32_t bucket_count;           /**< Number of buckets */
    uint64_t histogram_count;        /**< Total observations */
    double histogram_sum;            /**< Sum of observations */

    /* For summary */
    double* observations;       /**< Observation window */
    uint32_t obs_head;          /**< Window head */
    uint32_t obs_count;         /**< Observations in window */
    uint32_t obs_capacity;      /**< Window capacity */
} kg_metric_t;

/**
 * @brief Health check registration
 */
typedef struct {
    char component[KG_OBS_MAX_COMPONENT_LEN];  /**< Component name */
    kg_health_check_fn check_fn;               /**< Check function */
    void* user_data;                           /**< User context */
    bool registered;                           /**< Registration flag */
    kg_health_result_t last_result;            /**< Last check result */
} kg_health_check_t;

/**
 * @brief Observability implementation
 */
struct kg_observability {
    kg_observability_config_t config;  /**< Configuration */

    /* Metrics */
    kg_metric_t* metrics;              /**< Metric array */
    uint32_t metric_count;             /**< Registered metric count */
    uint32_t max_metrics;              /**< Maximum metrics */

    /* Health checks */
    kg_health_check_t* health_checks;  /**< Health check array */
    uint32_t health_count;             /**< Registered check count */
    uint32_t max_health_checks;        /**< Maximum checks */

    /* Thread safety */
    nimcp_mutex_t* mutex;              /**< Global mutex */

    /* State */
    bool running;                      /**< Endpoints running */

    /* Tracing state */
    uint64_t trace_counter;            /**< Counter for span IDs */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns_internal(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms_internal(void) {
    return get_timestamp_ns_internal() / 1000000ULL;
}

/**
 * @brief Find metric by name
 */
static kg_metric_t* find_metric(kg_observability_t* obs, const char* name) {
    if (!obs || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_metric: required parameter is NULL (obs, name)");
        return NULL;
    }

    for (uint32_t i = 0; i < obs->max_metrics; i++) {
        if (obs->metrics[i].registered &&
            strncmp(obs->metrics[i].def.name, name, KG_OBS_MAX_NAME_LEN) == 0) {
            return &obs->metrics[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_metric: operation failed");
    return NULL;
}

/**
 * @brief Find health check by component name
 */
static kg_health_check_t* find_health_check(kg_observability_t* obs, const char* component) {
    if (!obs || !component) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_health_check: required parameter is NULL (obs, component)");
        return NULL;
    }

    for (uint32_t i = 0; i < obs->max_health_checks; i++) {
        if (obs->health_checks[i].registered &&
            strncmp(obs->health_checks[i].component, component, KG_OBS_MAX_COMPONENT_LEN) == 0) {
            return &obs->health_checks[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_health_check: operation failed");
    return NULL;
}

/**
 * @brief Generate random hex string
 */
static void generate_random_hex(char* buf, size_t bytes) {
    static const char hex[] = "0123456789abcdef";

    for (size_t i = 0; i < bytes; i++) {
        uint8_t r = (uint8_t)(nimcp_tl_rand() & 0xFF);
        buf[i * 2] = hex[(r >> 4) & 0x0F];
        buf[i * 2 + 1] = hex[r & 0x0F];
    }
    buf[bytes * 2] = '\0';
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int kg_observability_default_config(kg_observability_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Prometheus defaults */
    config->enable_prometheus = true;
    config->prometheus_port = KG_OBS_DEFAULT_PROMETHEUS_PORT;
    strncpy(config->prometheus_path, "/metrics", KG_OBS_MAX_PATH_LEN - 1);

    /* Health check defaults */
    config->enable_health_endpoints = true;
    config->health_port = KG_OBS_DEFAULT_HEALTH_PORT;
    strncpy(config->liveness_path, "/healthz", KG_OBS_MAX_PATH_LEN - 1);
    strncpy(config->readiness_path, "/readyz", KG_OBS_MAX_PATH_LEN - 1);

    /* Tracing defaults */
    config->enable_tracing = false;
    config->trace_sample_rate = KG_OBS_DEFAULT_TRACE_SAMPLE_RATE;

    /* Alerting defaults */
    config->enable_alerting = false;

    return 0;
}

kg_observability_t* kg_observability_create(const kg_observability_config_t* config) {
    kg_observability_t* obs = nimcp_calloc(1, sizeof(kg_observability_t));
    if (!obs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obs is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        obs->config = *config;
    } else {
        kg_observability_default_config(&obs->config);
    }

    /* Allocate metrics */
    obs->max_metrics = KG_OBS_MAX_METRICS;
    obs->metrics = nimcp_calloc(obs->max_metrics, sizeof(kg_metric_t));
    if (!obs->metrics) {
        nimcp_free(obs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_observability_create: obs->metrics is NULL");
        return NULL;
    }

    /* Allocate health checks */
    obs->max_health_checks = KG_OBS_MAX_HEALTH_CHECKS;
    obs->health_checks = nimcp_calloc(obs->max_health_checks, sizeof(kg_health_check_t));
    if (!obs->health_checks) {
        nimcp_free(obs->metrics);
        nimcp_free(obs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_observability_create: obs->health_checks is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    obs->mutex = nimcp_mutex_create(&attr);
    if (!obs->mutex) {
        nimcp_free(obs->health_checks);
        nimcp_free(obs->metrics);
        nimcp_free(obs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_observability_create: obs->mutex is NULL");
        return NULL;
    }

    /* Seed random for trace IDs */
    srand((unsigned int)time(NULL));

    return obs;
}

void kg_observability_destroy(kg_observability_t* obs) {
    if (!obs) {
        return;
    }

    /* Stop if running */
    if (obs->running) {
        kg_observability_stop(obs);
    }

    /* Free metrics */
    if (obs->metrics) {
        for (uint32_t i = 0; i < obs->max_metrics; i++) {
            if (obs->metrics[i].registered) {
                /* Free label names */
                if (obs->metrics[i].def.label_names) {
                    for (uint32_t j = 0; j < obs->metrics[i].def.label_count; j++) {
                        if (obs->metrics[i].def.label_names[j]) {
                            nimcp_free(obs->metrics[i].def.label_names[j]);
                        }
                    }
                    nimcp_free(obs->metrics[i].def.label_names);
                }

                /* Free histogram buckets */
                if (obs->metrics[i].buckets) {
                    nimcp_free(obs->metrics[i].buckets);
                }

                /* Free summary observations */
                if (obs->metrics[i].observations) {
                    nimcp_free(obs->metrics[i].observations);
                }
            }
        }
        nimcp_free(obs->metrics);
    }

    /* Free health checks */
    if (obs->health_checks) {
        nimcp_free(obs->health_checks);
    }

    /* Destroy mutex */
    if (obs->mutex) {
        nimcp_mutex_free(obs->mutex);
    }

    nimcp_free(obs);
}

int kg_observability_start(kg_observability_t* obs) {
    if (!obs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obs is NULL");

        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    if (obs->running) {
        nimcp_mutex_unlock(obs->mutex);
        return 0; /* Already running */
    }

    /* In a full implementation, we would start HTTP servers here */
    /* For now, we just set the running flag */
    obs->running = true;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_observability_stop(kg_observability_t* obs) {
    if (!obs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obs is NULL");

        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    if (!obs->running) {
        nimcp_mutex_unlock(obs->mutex);
        return 0; /* Already stopped */
    }

    /* In a full implementation, we would stop HTTP servers here */
    obs->running = false;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

/* ============================================================================
 * Metrics API
 * ============================================================================ */

int kg_obs_register_metric(kg_observability_t* obs, const kg_metric_def_t* def) {
    if (!obs || !def || def->name[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_register_metric: required parameter is NULL (obs, def)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    /* Check for duplicate */
    if (find_metric(obs, def->name)) {
        nimcp_mutex_unlock(obs->mutex);
        return -2; /* Already exists */
    }

    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < obs->max_metrics; i++) {
        if (!obs->metrics[i].registered) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_obs_register_metric: validation failed");
        return -1; /* No space */
    }

    /* Copy definition */
    kg_metric_t* metric = &obs->metrics[slot];
    memset(metric, 0, sizeof(*metric));

    strncpy(metric->def.name, def->name, KG_OBS_MAX_NAME_LEN - 1);
    strncpy(metric->def.help, def->help, KG_OBS_MAX_HELP_LEN - 1);
    metric->def.type = def->type;
    metric->def.label_count = def->label_count;

    /* Copy label names */
    if (def->label_count > 0 && def->label_names) {
        metric->def.label_names = nimcp_calloc(def->label_count + 1, sizeof(char*));
        if (!metric->def.label_names) {
            nimcp_mutex_unlock(obs->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_obs_register_metric: metric->def is NULL");
            return -1;
        }

        for (uint32_t i = 0; i < def->label_count; i++) {
            if (def->label_names[i]) {
                metric->def.label_names[i] = nimcp_strdup(def->label_names[i]);
            }
        }
    }

    /* Initialize type-specific data */
    if (def->type == KG_METRIC_HISTOGRAM) {
        /* Default histogram buckets */
        static const double default_buckets[] = {
            0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0
        };
        metric->bucket_count = sizeof(default_buckets) / sizeof(default_buckets[0]);
        metric->buckets = nimcp_calloc(metric->bucket_count, sizeof(kg_histogram_bucket_t));
        if (metric->buckets) {
            for (uint32_t i = 0; i < metric->bucket_count; i++) {
                metric->buckets[i].le = default_buckets[i];
            }
        }
    } else if (def->type == KG_METRIC_SUMMARY) {
        /* Summary observation window */
        metric->obs_capacity = 1000;
        metric->observations = nimcp_calloc(metric->obs_capacity, sizeof(double));
    }

    metric->registered = true;
    obs->metric_count++;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_obs_counter_inc(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
) {
    (void)labels; /* TODO: implement per-label values */

    if (!obs || !name || value < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_counter_inc: required parameter is NULL (obs, name)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    kg_metric_t* metric = find_metric(obs, name);
    if (!metric || metric->def.type != KG_METRIC_COUNTER) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_obs_counter_inc: metric is NULL");
        return -1;
    }

    metric->value += value;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_obs_gauge_set(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
) {
    (void)labels;

    if (!obs || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_gauge_set: required parameter is NULL (obs, name)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    kg_metric_t* metric = find_metric(obs, name);
    if (!metric || metric->def.type != KG_METRIC_GAUGE) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_obs_gauge_set: metric is NULL");
        return -1;
    }

    metric->value = value;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_obs_gauge_inc(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
) {
    (void)labels;

    if (!obs || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_gauge_inc: required parameter is NULL (obs, name)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    kg_metric_t* metric = find_metric(obs, name);
    if (!metric || metric->def.type != KG_METRIC_GAUGE) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_obs_gauge_inc: metric is NULL");
        return -1;
    }

    metric->value += value;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_obs_histogram_observe(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
) {
    (void)labels;

    if (!obs || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_histogram_observe: required parameter is NULL (obs, name)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    kg_metric_t* metric = find_metric(obs, name);
    if (!metric || metric->def.type != KG_METRIC_HISTOGRAM) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_obs_histogram_observe: metric is NULL");
        return -1;
    }

    /* Update buckets */
    if (metric->buckets) {
        for (uint32_t i = 0; i < metric->bucket_count; i++) {
            if (value <= metric->buckets[i].le) {
                metric->buckets[i].count++;
            }
        }
    }

    /* Update sum and count */
    metric->histogram_sum += value;
    metric->histogram_count++;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_obs_summary_observe(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
) {
    (void)labels;

    if (!obs || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_summary_observe: required parameter is NULL (obs, name)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    kg_metric_t* metric = find_metric(obs, name);
    if (!metric || metric->def.type != KG_METRIC_SUMMARY) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_obs_summary_observe: metric is NULL");
        return -1;
    }

    /* Add to observation window */
    if (metric->observations) {
        metric->observations[metric->obs_head] = value;
        metric->obs_head = (metric->obs_head + 1) % metric->obs_capacity;
        if (metric->obs_count < metric->obs_capacity) {
            metric->obs_count++;
        }
    }

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_obs_get_metric(
    const kg_observability_t* obs,
    const char* name,
    const char** labels,
    double* value
) {
    (void)labels;

    if (!obs || !name || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_get_metric: required parameter is NULL (obs, name, value)");
        return -1;
    }

    nimcp_mutex_lock(((kg_observability_t*)obs)->mutex);

    kg_metric_t* metric = find_metric((kg_observability_t*)obs, name);
    if (!metric) {
        nimcp_mutex_unlock(((kg_observability_t*)obs)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_get_metric: metric is NULL");
        return -1;
    }

    *value = metric->value;

    nimcp_mutex_unlock(((kg_observability_t*)obs)->mutex);

    return 0;
}

int kg_obs_unregister_metric(kg_observability_t* obs, const char* name) {
    if (!obs || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_unregister_metric: required parameter is NULL (obs, name)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    kg_metric_t* metric = find_metric(obs, name);
    if (!metric) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_unregister_metric: metric is NULL");
        return -1;
    }

    /* Free resources */
    if (metric->def.label_names) {
        for (uint32_t i = 0; i < metric->def.label_count; i++) {
            if (metric->def.label_names[i]) {
                nimcp_free(metric->def.label_names[i]);
            }
        }
        nimcp_free(metric->def.label_names);
    }

    if (metric->buckets) {
        nimcp_free(metric->buckets);
    }

    if (metric->observations) {
        nimcp_free(metric->observations);
    }

    memset(metric, 0, sizeof(*metric));
    obs->metric_count--;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

/* ============================================================================
 * Health Check API
 * ============================================================================ */

int kg_obs_register_health_check(
    kg_observability_t* obs,
    const char* component,
    kg_health_check_fn check_fn,
    void* user_data
) {
    if (!obs || !component || !check_fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_register_health_check: required parameter is NULL (obs, component, check_fn)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    /* Check for duplicate */
    if (find_health_check(obs, component)) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_obs_register_health_check: validation failed");
        return -1;
    }

    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < obs->max_health_checks; i++) {
        if (!obs->health_checks[i].registered) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_obs_register_health_check: validation failed");
        return -1;
    }

    /* Register check */
    kg_health_check_t* check = &obs->health_checks[slot];
    strncpy(check->component, component, KG_OBS_MAX_COMPONENT_LEN - 1);
    check->check_fn = check_fn;
    check->user_data = user_data;
    check->registered = true;
    obs->health_count++;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_obs_unregister_health_check(
    kg_observability_t* obs,
    const char* component
) {
    if (!obs || !component) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_unregister_health_check: required parameter is NULL (obs, component)");
        return -1;
    }

    nimcp_mutex_lock(obs->mutex);

    kg_health_check_t* check = find_health_check(obs, component);
    if (!check) {
        nimcp_mutex_unlock(obs->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_unregister_health_check: check is NULL");
        return -1;
    }

    memset(check, 0, sizeof(*check));
    obs->health_count--;

    nimcp_mutex_unlock(obs->mutex);

    return 0;
}

int kg_obs_get_health(
    const kg_observability_t* obs,
    kg_health_result_t* results,
    uint32_t* count
) {
    if (!obs || !results || !count || *count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_get_health: required parameter is NULL (obs, results, count)");
        return -1;
    }

    nimcp_mutex_lock(((kg_observability_t*)obs)->mutex);

    uint32_t result_count = 0;

    for (uint32_t i = 0; i < obs->max_health_checks && result_count < *count; i++) {
        const kg_health_check_t* check = &obs->health_checks[i];
        if (!check->registered) {
            continue;
        }

        kg_health_result_t* result = &results[result_count];
        uint64_t start = get_timestamp_ms_internal();

        /* Execute check */
        int rc = check->check_fn(check->component, check->user_data, result);

        result->latency_ms = get_timestamp_ms_internal() - start;
        result->last_check = get_timestamp_ms_internal();

        if (rc != 0) {
            result->healthy = false;
            snprintf(result->message, KG_OBS_MAX_MESSAGE_LEN,
                     "Health check function failed");
        }

        strncpy(result->component, check->component, KG_OBS_MAX_COMPONENT_LEN - 1);
        result_count++;
    }

    *count = result_count;

    nimcp_mutex_unlock(((kg_observability_t*)obs)->mutex);

    return 0;
}

int kg_obs_get_component_health(
    const kg_observability_t* obs,
    const char* component,
    kg_health_result_t* result
) {
    if (!obs || !component || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_get_component_health: required parameter is NULL (obs, component, result)");
        return -1;
    }

    nimcp_mutex_lock(((kg_observability_t*)obs)->mutex);

    kg_health_check_t* check = find_health_check((kg_observability_t*)obs, component);
    if (!check) {
        nimcp_mutex_unlock(((kg_observability_t*)obs)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_get_component_health: check is NULL");
        return -1;
    }

    uint64_t start = get_timestamp_ms_internal();

    int rc = check->check_fn(check->component, check->user_data, result);

    result->latency_ms = get_timestamp_ms_internal() - start;
    result->last_check = get_timestamp_ms_internal();
    strncpy(result->component, component, KG_OBS_MAX_COMPONENT_LEN - 1);

    if (rc != 0) {
        result->healthy = false;
        snprintf(result->message, KG_OBS_MAX_MESSAGE_LEN,
                 "Health check function failed");
    }

    nimcp_mutex_unlock(((kg_observability_t*)obs)->mutex);

    return 0;
}

bool kg_obs_is_live(const kg_observability_t* obs) {
    if (!obs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_is_live: obs is NULL");
        return false;
    }

    /* Basic liveness: system is alive if this code runs */
    return true;
}

bool kg_obs_is_ready(const kg_observability_t* obs) {
    if (!obs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_is_ready: obs is NULL");
        return false;
    }

    nimcp_mutex_lock(((kg_observability_t*)obs)->mutex);

    bool all_healthy = true;

    for (uint32_t i = 0; i < obs->max_health_checks && all_healthy; i++) {
        const kg_health_check_t* check = &obs->health_checks[i];
        if (!check->registered) {
            continue;
        }

        kg_health_result_t result;
        int rc = check->check_fn(check->component, check->user_data, &result);

        if (rc != 0 || !result.healthy) {
            all_healthy = false;
        }
    }

    nimcp_mutex_unlock(((kg_observability_t*)obs)->mutex);

    return all_healthy;
}

/* ============================================================================
 * Tracing API
 * ============================================================================ */

kg_trace_span_t* kg_obs_start_span(
    kg_observability_t* obs,
    const char* operation,
    const kg_trace_span_t* parent
) {
    if (!obs || !operation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_start_span: required parameter is NULL (obs, operation)");
        return NULL;
    }

    /* Check sampling */
    if (obs->config.trace_sample_rate < 1.0f) {
        float sample = (float)nimcp_tl_rand() / (float)RAND_MAX;
        if (sample > obs->config.trace_sample_rate) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_start_span: validation failed");
            return NULL; /* Not sampled */
        }
    }

    kg_trace_span_t* span = nimcp_calloc(1, sizeof(kg_trace_span_t));
    if (!span) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "span is NULL");

        return NULL;
    }

    strncpy(span->operation, operation, KG_OBS_MAX_OPERATION_LEN - 1);
    span->start_time_ns = get_timestamp_ns_internal();

    if (parent) {
        /* Child span */
        strncpy(span->trace_id, parent->trace_id, KG_OBS_TRACE_ID_LEN - 1);
        strncpy(span->parent_span_id, parent->span_id, KG_OBS_SPAN_ID_LEN - 1);
    } else {
        /* Root span */
        kg_obs_generate_trace_id(span->trace_id);
    }

    kg_obs_generate_span_id(span->span_id);

    return span;
}

int kg_obs_end_span(kg_observability_t* obs, kg_trace_span_t* span) {
    if (!obs || !span) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_end_span: required parameter is NULL (obs, span)");
        return -1;
    }

    span->duration_ns = get_timestamp_ns_internal() - span->start_time_ns;

    /* In a full implementation, we would export the span to the OTLP collector here */

    return 0;
}

int kg_obs_add_span_tag(kg_trace_span_t* span, const char* key, const char* value) {
    if (!span || !key || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_add_span_tag: required parameter is NULL (span, key, value)");
        return -1;
    }

    /* Allocate or expand tags array */
    uint32_t new_count = span->tag_count + 2; /* key + value */
    char** new_tags = nimcp_realloc(span->tags, (new_count + 1) * sizeof(char*));
    if (!new_tags) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "new_tags is NULL");

        return -1;
    }

    span->tags = new_tags;
    span->tags[span->tag_count] = nimcp_strdup(key);
    span->tags[span->tag_count + 1] = nimcp_strdup(value);
    span->tag_count = new_count;
    span->tags[new_count] = NULL;

    return 0;
}

int kg_obs_add_span_event(
    kg_observability_t* obs,
    kg_trace_span_t* span,
    const char* event_name
) {
    (void)obs;

    if (!span || !event_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_add_span_event: required parameter is NULL (span, event_name)");
        return -1;
    }

    /* In a full implementation, we would record the event with timestamp */
    /* For now, just add as a tag */
    char timestamp_str[32];
    snprintf(timestamp_str, sizeof(timestamp_str), "%lu",
             (unsigned long)get_timestamp_ns_internal());

    return kg_obs_add_span_tag(span, event_name, timestamp_str);
}

int kg_obs_set_span_status(
    kg_trace_span_t* span,
    bool is_error,
    const char* message
) {
    if (!span) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "span is NULL");

        return -1;
    }

    /* Add status as tags */
    if (is_error) {
        kg_obs_add_span_tag(span, "error", "true");
        if (message) {
            kg_obs_add_span_tag(span, "error.message", message);
        }
    } else {
        kg_obs_add_span_tag(span, "error", "false");
    }

    return 0;
}

void kg_obs_free_span(kg_trace_span_t* span) {
    if (!span) {
        return;
    }

    if (span->tags) {
        for (uint32_t i = 0; i < span->tag_count; i++) {
            if (span->tags[i]) {
                nimcp_free(span->tags[i]);
            }
        }
        nimcp_free(span->tags);
    }

    nimcp_free(span);
}

int kg_obs_extract_trace_context(
    const kg_trace_span_t* span,
    char* trace_id,
    char* span_id
) {
    if (!span || !trace_id || !span_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_extract_trace_context: required parameter is NULL (span, trace_id, span_id)");
        return -1;
    }

    strncpy(trace_id, span->trace_id, KG_OBS_TRACE_ID_LEN - 1);
    trace_id[KG_OBS_TRACE_ID_LEN - 1] = '\0';

    strncpy(span_id, span->span_id, KG_OBS_SPAN_ID_LEN - 1);
    span_id[KG_OBS_SPAN_ID_LEN - 1] = '\0';

    return 0;
}

kg_trace_span_t* kg_obs_create_span_from_context(
    kg_observability_t* obs,
    const char* operation,
    const char* trace_id,
    const char* parent_span_id
) {
    if (!obs || !operation || !trace_id || !parent_span_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_create_span_from_context: required parameter is NULL (obs, operation, trace_id, parent_span_id)");
        return NULL;
    }

    kg_trace_span_t* span = nimcp_calloc(1, sizeof(kg_trace_span_t));
    if (!span) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "span is NULL");

        return NULL;
    }

    strncpy(span->operation, operation, KG_OBS_MAX_OPERATION_LEN - 1);
    strncpy(span->trace_id, trace_id, KG_OBS_TRACE_ID_LEN - 1);
    strncpy(span->parent_span_id, parent_span_id, KG_OBS_SPAN_ID_LEN - 1);

    kg_obs_generate_span_id(span->span_id);

    span->start_time_ns = get_timestamp_ns_internal();

    return span;
}

/* ============================================================================
 * Alerting API
 * ============================================================================ */

int kg_obs_send_alert(
    kg_observability_t* obs,
    const char* alert_name,
    const char* severity,
    const char* message
) {
    return kg_obs_send_alert_with_labels(obs, alert_name, severity, message,
                                          NULL, NULL, 0);
}

int kg_obs_send_alert_with_labels(
    kg_observability_t* obs,
    const char* alert_name,
    const char* severity,
    const char* message,
    const char** label_keys,
    const char** label_values,
    uint32_t label_count
) {
    (void)label_keys;
    (void)label_values;
    (void)label_count;

    if (!obs || !alert_name || !severity || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_send_alert_with_labels: required parameter is NULL (obs, alert_name, severity, message)");
        return -1;
    }

    if (!obs->config.enable_alerting || obs->config.alertmanager_url[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_send_alert_with_labels: obs->config is NULL");
        return -1;
    }

    /* In a full implementation, we would POST the alert to Alertmanager */
    /* For now, just log it */
    fprintf(stderr, "[ALERT][%s] %s: %s\n", severity, alert_name, message);

    return 0;
}

int kg_obs_resolve_alert(kg_observability_t* obs, const char* alert_name) {
    if (!obs || !alert_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_resolve_alert: required parameter is NULL (obs, alert_name)");
        return -1;
    }

    if (!obs->config.enable_alerting) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_obs_resolve_alert: obs->config is NULL");
        return -1;
    }

    /* In a full implementation, we would send resolution to Alertmanager */
    fprintf(stderr, "[ALERT][resolved] %s\n", alert_name);

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static const char* metric_type_strings[] = {
    "counter",
    "gauge",
    "histogram",
    "summary"
};

const char* kg_metric_type_to_string(kg_metric_type_t type) {
    if (type >= 0 && type <= KG_METRIC_SUMMARY) {
        return metric_type_strings[type];
    }
    return "unknown";
}

int kg_metric_type_from_string(const char* str) {
    if (!str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "str is NULL");

        return -1;
    }

    for (int i = 0; i <= KG_METRIC_SUMMARY; i++) {
        if (strcmp(str, metric_type_strings[i]) == 0) {
            return i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_metric_type_from_string: validation failed");
    return -1;
}

uint64_t kg_obs_timestamp_ns(void) {
    return get_timestamp_ns_internal();
}

uint64_t kg_obs_timestamp_ms(void) {
    return get_timestamp_ms_internal();
}

int kg_obs_generate_trace_id(char* trace_id) {
    if (!trace_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trace_id is NULL");

        return -1;
    }

    /* 128-bit = 16 bytes = 32 hex chars */
    generate_random_hex(trace_id, 16);
    return 0;
}

int kg_obs_generate_span_id(char* span_id) {
    if (!span_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "span_id is NULL");

        return -1;
    }

    /* 64-bit = 8 bytes = 16 hex chars */
    generate_random_hex(span_id, 8);
    return 0;
}
