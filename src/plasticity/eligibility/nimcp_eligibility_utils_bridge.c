/**
 * @file nimcp_eligibility_utils_bridge.c
 * @brief Eligibility Trace Utils Integration - Metrics, Memory Pools, RK4, Shannon
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "plasticity/eligibility/nimcp_eligibility_utils_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eligibility_utils_bridge)

#define LOG_MODULE "ELIGIBILITY_UTILS_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Memory pool for eligibility traces
 */
typedef struct {
    eligibility_trace_t* pool;      /**< Pre-allocated traces */
    uint32_t* free_list;            /**< Free index list */
    uint32_t free_head;             /**< Head of free list */
    uint32_t capacity;              /**< Total capacity */
    uint32_t used;                  /**< Currently allocated */
} elig_trace_pool_t;

/**
 * @brief Internal context for eligibility utils
 */
struct eligibility_utils_ctx_internal {
    /* Configuration */
    eligibility_utils_config_t config;

    /* Memory pool */
    elig_trace_pool_t trace_pool;

    /* Metrics */
    eligibility_metrics_t metrics;
    uint64_t update_count;
    uint64_t last_bottleneck_check;

    /* Timing */
    struct timespec start_time;

    /* Bottleneck tracking */
    float last_information_efficiency;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void pool_init(elig_trace_pool_t* pool, uint32_t capacity) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool_init: pool is NULL");
        return;
    }
    if (capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pool_init: capacity is zero");
        return;
    }

    pool->pool = (eligibility_trace_t*)nimcp_calloc(capacity, sizeof(eligibility_trace_t));
    if (!pool->pool) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, capacity * sizeof(eligibility_trace_t),
                           "pool_init: failed to allocate trace pool");
        return;
    }

    pool->free_list = (uint32_t*)nimcp_malloc(capacity * sizeof(uint32_t));
    if (!pool->free_list) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, capacity * sizeof(uint32_t),
                           "pool_init: failed to allocate free list");
        nimcp_free(pool->pool);
        pool->pool = NULL;
        return;
    }

    pool->capacity = capacity;
    pool->used = 0;
    pool->free_head = 0;

    /* Initialize free list */
    for (uint32_t i = 0; i < capacity; i++) {
        pool->free_list[i] = i;
    }
}

static void pool_destroy(elig_trace_pool_t* pool) {
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "eligibility_utils");
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool_destroy: pool is NULL");
        return;
    }

    nimcp_free(pool->pool);
    nimcp_free(pool->free_list);
    pool->pool = NULL;
    pool->free_list = NULL;
    pool->capacity = 0;
    pool->used = 0;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

eligibility_utils_config_t eligibility_utils_default_config(void) {
    eligibility_utils_config_t config = {
        .enable_metrics = true,
        .metrics_flush_interval_ms = 10000,
        .metrics_output_dir = "",

        .enable_trace_pool = true,
        .trace_pool_size = ELIG_TRACE_POOL_SIZE,

        .use_adaptive_integration = false,
        .integration_dt = ELIG_INTEGRATION_DT,
        .adaptive_error_tolerance = 1e-6f,

        .enable_bottleneck_detection = true,
        .bottleneck_threshold = ELIG_BOTTLENECK_THRESHOLD,
        .bottleneck_check_interval = 100
    };

    return config;
}

eligibility_utils_ctx_t eligibility_utils_create(const eligibility_utils_config_t* config) {
    struct eligibility_utils_ctx_internal* ctx =
        (struct eligibility_utils_ctx_internal*)nimcp_calloc(1, sizeof(struct eligibility_utils_ctx_internal));

    if (!ctx) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");


        return NULL;


    }

    /* Copy config */
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = eligibility_utils_default_config();
    }

    /* Initialize pool */
    if (ctx->config.enable_trace_pool) {
        pool_init(&ctx->trace_pool, ctx->config.trace_pool_size);
    }

    /* Initialize timing */
    clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);
    ctx->metrics.last_update_ms = get_current_time_ms();

    NIMCP_LOGGING_INFO("Created %s bridge", "eligibility_utils");
    return ctx;
}

void eligibility_utils_destroy(eligibility_utils_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_destroy: ctx is NULL");
        return;
    }

    pool_destroy(&ctx->trace_pool);
    nimcp_free(ctx);
}

void eligibility_utils_reset(eligibility_utils_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_reset: ctx is NULL");
        return;
    }

    /* Reset metrics */
    memset(&ctx->metrics, 0, sizeof(eligibility_metrics_t));
    ctx->update_count = 0;
    ctx->last_bottleneck_check = 0;
    ctx->last_information_efficiency = 0.0f;
    ctx->metrics.last_update_ms = get_current_time_ms();

    /* Reset pool (mark all as free) */
    if (ctx->config.enable_trace_pool && ctx->trace_pool.capacity > 0) {
        ctx->trace_pool.used = 0;
        ctx->trace_pool.free_head = 0;
        for (uint32_t i = 0; i < ctx->trace_pool.capacity; i++) {
            ctx->trace_pool.free_list[i] = i;
            memset(&ctx->trace_pool.pool[i], 0, sizeof(eligibility_trace_t));
        }
    }
}

/*=============================================================================
 * MEMORY POOL API
 *===========================================================================*/

eligibility_trace_t* eligibility_utils_alloc_trace(eligibility_utils_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_alloc_trace: ctx is NULL");
        return NULL;
    }
    if (!ctx->config.enable_trace_pool) {
        LOG_DEBUG("eligibility_utils_alloc_trace: trace pool not enabled");
        return NULL;
    }

    elig_trace_pool_t* pool = &ctx->trace_pool;

    if (pool->used >= pool->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "eligibility_utils_alloc_trace: pool exhausted");
        return NULL;
    }

    uint32_t idx = pool->free_list[pool->free_head++];
    pool->used++;

    eligibility_trace_t* trace = &pool->pool[idx];
    memset(trace, 0, sizeof(eligibility_trace_t));

    return trace;
}

void eligibility_utils_free_trace(eligibility_utils_ctx_t ctx, eligibility_trace_t* trace) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_free_trace: ctx is NULL");
        return;
    }
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_free_trace: trace is NULL");
        return;
    }
    if (!ctx->config.enable_trace_pool) {
        LOG_DEBUG("eligibility_utils_free_trace: trace pool not enabled");
        return;
    }

    elig_trace_pool_t* pool = &ctx->trace_pool;

    /* Calculate index from pointer */
    ptrdiff_t offset = trace - pool->pool;
    if (offset < 0 || offset >= (ptrdiff_t)pool->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_utils_free_trace: trace not from this pool");
        return;
    }

    uint32_t idx = (uint32_t)offset;

    /* Bounds check: prevent free_head underflow (would cause OOB write) */
    if (pool->free_head == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "eligibility_utils_free_trace: free_head underflow — pool is already fully free");
        return;
    }

    /* Add back to free list */
    pool->free_head--;
    pool->free_list[pool->free_head] = idx;
    pool->used--;
}

uint32_t eligibility_utils_alloc_trace_batch(
    eligibility_utils_ctx_t ctx,
    uint32_t count,
    eligibility_trace_t** out_traces
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_alloc_trace_batch: ctx is NULL");
        return 0;
    }
    if (!out_traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_alloc_trace_batch: out_traces is NULL");
        return 0;
    }
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_utils_alloc_trace_batch: count is zero");
        return 0;
    }

    uint32_t allocated = 0;
    for (uint32_t i = 0; i < count; i++) {
        out_traces[i] = eligibility_utils_alloc_trace(ctx);
        if (out_traces[i]) {
            allocated++;
        } else {
            break;
        }
    }

    return allocated;
}

void eligibility_utils_pool_stats(
    eligibility_utils_ctx_t ctx,
    uint32_t* total_capacity,
    uint32_t* used_count,
    uint32_t* free_count
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_pool_stats: ctx is NULL");
        if (total_capacity) *total_capacity = 0;
        if (used_count) *used_count = 0;
        if (free_count) *free_count = 0;
        return;
    }

    if (total_capacity) *total_capacity = ctx->trace_pool.capacity;
    if (used_count) *used_count = ctx->trace_pool.used;
    if (free_count) *free_count = ctx->trace_pool.capacity - ctx->trace_pool.used;
}

/*=============================================================================
 * METRICS API
 *===========================================================================*/

void eligibility_utils_record_update(
    eligibility_utils_ctx_t ctx,
    float trace_value,
    float weight_change
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_record_update: ctx is NULL");
        return;
    }
    if (!ctx->config.enable_metrics) return;

    ctx->metrics.total_traces_updated++;

    /* Update mean trace value (running average) */
    float n = (float)ctx->metrics.total_traces_updated;
    ctx->metrics.mean_trace_value =
        ctx->metrics.mean_trace_value * ((n - 1.0f) / n) + trace_value / n;

    /* Update max */
    if (trace_value > ctx->metrics.max_trace_value) {
        ctx->metrics.max_trace_value = trace_value;
    }

    /* Update weight change mean */
    ctx->metrics.mean_weight_change =
        ctx->metrics.mean_weight_change * ((n - 1.0f) / n) + weight_change / n;

    /* Update histogram */
    int bin = (int)(trace_value * 19.0f);
    if (bin < 0) bin = 0;
    if (bin > 19) bin = 19;
    ctx->metrics.trace_histogram[bin]++;

    /* Weight change histogram */
    float wc_norm = (weight_change + 1.0f) / 2.0f; /* Normalize -1 to 1 -> 0 to 1 */
    bin = (int)(wc_norm * 19.0f);
    if (bin < 0) bin = 0;
    if (bin > 19) bin = 19;
    ctx->metrics.weight_change_histogram[bin]++;

    ctx->metrics.last_update_ms = get_current_time_ms();
}

void eligibility_utils_record_consolidation(
    eligibility_utils_ctx_t ctx,
    uint32_t num_synapses,
    float total_weight_change,
    bool burst_triggered
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_record_consolidation: ctx is NULL");
        return;
    }
    if (!ctx->config.enable_metrics) return;

    ctx->metrics.total_consolidations++;
    if (burst_triggered) {
        ctx->metrics.burst_triggered_count++;
    }

    (void)num_synapses;
    (void)total_weight_change;

    ctx->metrics.last_update_ms = get_current_time_ms();
}

void eligibility_utils_update_trace_stats(
    eligibility_utils_ctx_t ctx,
    const eligibility_trace_t* traces,
    uint32_t num_traces
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_update_trace_stats: ctx is NULL");
        return;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_update_trace_stats: traces is NULL");
        return;
    }
    if (num_traces == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_utils_update_trace_stats: num_traces is zero");
        return;
    }

    float sum = 0.0f;
    float max_val = 0.0f;
    uint32_t above_threshold = 0;

    for (uint32_t i = 0; i < num_traces; i++) {
        float val = traces[i].trace;
        sum += val;
        if (val > max_val) max_val = val;
        if (val > 0.1f) above_threshold++; /* Simple threshold */
    }

    ctx->metrics.mean_trace_value = sum / (float)num_traces;
    ctx->metrics.max_trace_value = max_val;
    ctx->metrics.traces_above_threshold = above_threshold;
    ctx->metrics.last_update_ms = get_current_time_ms();
}

bool eligibility_utils_get_metrics(eligibility_utils_ctx_t ctx, eligibility_metrics_t* metrics) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_get_metrics: ctx is NULL");
        return false;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_get_metrics: metrics is NULL");
        return false;
    }

    *metrics = ctx->metrics;
    return true;
}

int32_t eligibility_utils_flush_metrics(eligibility_utils_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_flush_metrics: ctx is NULL");
        return -1;
    }

    /* In a real implementation, this would write to disk */
    /* For now, just reset the update count */
    ctx->update_count = 0;

    return 0;
}

bool eligibility_utils_export_csv(eligibility_utils_ctx_t ctx, const char* filename) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_export_csv: ctx is NULL");
        return false;
    }
    if (!filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_export_csv: filename is NULL");
        return false;
    }

    FILE* fp = fopen(filename, "w");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "eligibility_utils_export_csv: failed to open file");
        return false;
    }

    fprintf(fp, "metric,value\n");
    fprintf(fp, "total_traces_updated,%lu\n", (unsigned long)ctx->metrics.total_traces_updated);
    fprintf(fp, "total_consolidations,%lu\n", (unsigned long)ctx->metrics.total_consolidations);
    fprintf(fp, "mean_trace_value,%f\n", ctx->metrics.mean_trace_value);
    fprintf(fp, "max_trace_value,%f\n", ctx->metrics.max_trace_value);
    fprintf(fp, "mean_weight_change,%f\n", ctx->metrics.mean_weight_change);
    fprintf(fp, "information_efficiency,%f\n", ctx->metrics.information_efficiency);
    fprintf(fp, "bottleneck_count,%u\n", ctx->metrics.bottleneck_count);

    fclose(fp);
    return true;
}

/*=============================================================================
 * NUMERICAL INTEGRATION API
 *===========================================================================*/

/**
 * @brief Derivative function for trace decay: d(trace)/dt = -trace * (1-lambda) + spike
 *
 * Using decay_lambda from config, where lambda is per-timestep decay (e.g., 0.95)
 * Converted to continuous: decay_rate = -log(lambda) per unit time
 */
static float trace_derivative(float trace, float decay_rate, float spike) {
    return -trace * decay_rate + spike;
}

void eligibility_utils_rk4_update(
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    float dt,
    float spike_contribution
) {
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_rk4_update: trace is NULL");
        return;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_rk4_update: config is NULL");
        return;
    }
    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_utils_rk4_update: dt must be positive");
        return;
    }

    /* Convert decay_lambda to continuous decay rate */
    float lambda = config->decay_lambda;
    if (lambda <= 0.0f || lambda >= 1.0f) lambda = 0.95f; /* Default */
    float decay_rate = -logf(lambda); /* Per ms if dt is in ms */

    float y = trace->trace;

    /* RK4 integration */
    float k1 = trace_derivative(y, decay_rate, spike_contribution);
    float k2 = trace_derivative(y + 0.5f * dt * k1, decay_rate, spike_contribution);
    float k3 = trace_derivative(y + 0.5f * dt * k2, decay_rate, spike_contribution);
    float k4 = trace_derivative(y + dt * k3, decay_rate, spike_contribution);

    trace->trace = y + (dt / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

    /* Clamp to valid range */
    if (trace->trace < 0.0f) trace->trace = 0.0f;
    if (trace->trace > 1.0f) trace->trace = 1.0f;
}

float eligibility_utils_adaptive_update(
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    float dt_initial,
    float spike_contribution,
    eligibility_utils_ctx_t ctx
) {
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_adaptive_update: trace is NULL");
        return 0.0f;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_adaptive_update: config is NULL");
        return 0.0f;
    }

    float tolerance = 1e-6f;
    if (ctx && ctx->config.use_adaptive_integration) {
        tolerance = ctx->config.adaptive_error_tolerance;
    }

    float dt = dt_initial;

    /* Convert decay_lambda to continuous decay rate */
    float lambda = config->decay_lambda;
    if (lambda <= 0.0f || lambda >= 1.0f) lambda = 0.95f;
    float decay_rate = -logf(lambda);

    /* Try full step */
    float y0 = trace->trace;

    /* Full step with RK4 */
    float k1 = trace_derivative(y0, decay_rate, spike_contribution);
    float k2 = trace_derivative(y0 + 0.5f * dt * k1, decay_rate, spike_contribution);
    float k3 = trace_derivative(y0 + 0.5f * dt * k2, decay_rate, spike_contribution);
    float k4 = trace_derivative(y0 + dt * k3, decay_rate, spike_contribution);
    float y_full = y0 + (dt / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

    /* Two half steps */
    float dt_half = dt * 0.5f;
    k1 = trace_derivative(y0, decay_rate, spike_contribution);
    k2 = trace_derivative(y0 + 0.5f * dt_half * k1, decay_rate, spike_contribution);
    k3 = trace_derivative(y0 + 0.5f * dt_half * k2, decay_rate, spike_contribution);
    k4 = trace_derivative(y0 + dt_half * k3, decay_rate, spike_contribution);
    float y_half1 = y0 + (dt_half / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

    k1 = trace_derivative(y_half1, decay_rate, spike_contribution);
    k2 = trace_derivative(y_half1 + 0.5f * dt_half * k1, decay_rate, spike_contribution);
    k3 = trace_derivative(y_half1 + 0.5f * dt_half * k2, decay_rate, spike_contribution);
    k4 = trace_derivative(y_half1 + dt_half * k3, decay_rate, spike_contribution);
    float y_half2 = y_half1 + (dt_half / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

    /* Error estimate */
    float error = fabsf(y_full - y_half2);

    if (error < tolerance) {
        /* Accept step */
        trace->trace = y_half2;
    } else {
        /* Reduce timestep and retry */
        dt = dt * 0.5f;
        eligibility_utils_rk4_update(trace, config, dt, spike_contribution);
    }

    /* Clamp */
    if (trace->trace < 0.0f) trace->trace = 0.0f;
    if (trace->trace > 1.0f) trace->trace = 1.0f;

    return dt;
}

void eligibility_utils_batch_update(
    eligibility_trace_t* traces,
    const eligibility_config_t* configs,
    uint32_t num_traces,
    float dt,
    const uint8_t* spike_mask,
    eligibility_utils_ctx_t ctx
) {
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_batch_update: traces is NULL");
        return;
    }
    if (!configs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_batch_update: configs is NULL");
        return;
    }
    if (num_traces == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_utils_batch_update: num_traces is zero");
        return;
    }

    for (uint32_t i = 0; i < num_traces; i++) {
        float spike = 0.0f;
        if (spike_mask) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx = i % 8;
            spike = (spike_mask[byte_idx] & (1 << bit_idx)) ? 1.0f : 0.0f;
        }

        if (ctx && ctx->config.use_adaptive_integration) {
            eligibility_utils_adaptive_update(&traces[i], &configs[i], dt, spike, ctx);
        } else {
            eligibility_utils_rk4_update(&traces[i], &configs[i], dt, spike);
        }
    }
}

/*=============================================================================
 * INFORMATION THEORY API
 *===========================================================================*/

float eligibility_utils_compute_entropy(
    const eligibility_trace_t* traces,
    uint32_t num_traces
) {
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_compute_entropy: traces is NULL");
        return 0.0f;
    }
    if (num_traces == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_utils_compute_entropy: num_traces is zero");
        return 0.0f;
    }

    /* Compute histogram of trace values */
    uint32_t bins[20] = {0};

    for (uint32_t i = 0; i < num_traces; i++) {
        int bin = (int)(traces[i].trace * 19.0f);
        if (bin < 0) bin = 0;
        if (bin > 19) bin = 19;
        bins[bin]++;
    }

    /* Compute entropy */
    float entropy = 0.0f;
    float n = (float)num_traces;

    for (int i = 0; i < 20; i++) {
        if (bins[i] > 0) {
            float p = (float)bins[i] / n;
            entropy -= p * log2f(p);
        }
    }

    return entropy;
}

float eligibility_utils_analyze_information_flow(
    eligibility_utils_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_analyze_information_flow: ctx is NULL");
        return 0.0f;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_analyze_information_flow: traces is NULL");
        return 0.0f;
    }
    if (num_synapses == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_utils_analyze_information_flow: num_synapses is zero");
        return 0.0f;
    }

    /* Compute entropy of trace distribution */
    float entropy = eligibility_utils_compute_entropy(traces, num_synapses);

    /* Maximum entropy for 20 bins */
    float max_entropy = log2f(20.0f);

    /* Information efficiency = actual / maximum */
    float efficiency = entropy / max_entropy;

    /* Store for later use */
    ctx->last_information_efficiency = efficiency;
    ctx->metrics.information_efficiency = efficiency;

    (void)weights; /* Weight distribution could also be factored in */

    return efficiency;
}

bool eligibility_utils_detect_bottlenecks(
    eligibility_utils_ctx_t ctx,
    const eligibility_trace_t* traces,
    const float* weights,
    uint32_t num_synapses,
    eligibility_bottleneck_t* bottlenecks,
    uint32_t max_bottlenecks,
    uint32_t* num_found
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_detect_bottlenecks: ctx is NULL");
        return false;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_detect_bottlenecks: traces is NULL");
        return false;
    }
    if (!bottlenecks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_detect_bottlenecks: bottlenecks is NULL");
        return false;
    }
    if (!num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_detect_bottlenecks: num_found is NULL");
        return false;
    }

    *num_found = 0;

    float threshold = ctx->config.bottleneck_threshold;

    for (uint32_t i = 0; i < num_synapses && *num_found < max_bottlenecks; i++) {
        /* Simple bottleneck detection: high trace but low weight */
        float trace_val = traces[i].trace;
        float weight_val = weights ? weights[i] : 0.5f;

        /* Information deficit = trace value minus weight capacity */
        float deficit = trace_val - weight_val;

        if (deficit > threshold) {
            eligibility_bottleneck_t* bn = &bottlenecks[*num_found];
            bn->synapse_id = i;
            bn->information_deficit = deficit;
            bn->suggested_weight = weight_val + 0.1f * deficit;
            bn->current_trace = trace_val;
            (*num_found)++;
            ctx->metrics.bottleneck_count++;
        }
    }

    return true;
}

bool eligibility_utils_suggest_adjustments(
    eligibility_utils_ctx_t ctx,
    const eligibility_bottleneck_t* bottlenecks,
    uint32_t num_bottlenecks,
    float* weight_adjustments
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_suggest_adjustments: ctx is NULL");
        return false;
    }
    if (!bottlenecks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_suggest_adjustments: bottlenecks is NULL");
        return false;
    }
    if (!weight_adjustments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_suggest_adjustments: weight_adjustments is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_bottlenecks; i++) {
        /* Suggest weight increase proportional to deficit */
        weight_adjustments[i] = 0.1f * bottlenecks[i].information_deficit;
    }

    return true;
}

/*=============================================================================
 * ENHANCED ELIGIBILITY OPERATIONS
 *===========================================================================*/

void eligibility_utils_update_enhanced(
    eligibility_utils_ctx_t ctx,
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    uint64_t current_time,
    float spike_contribution
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_update_enhanced: ctx is NULL");
        return;
    }
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_update_enhanced: trace is NULL");
        return;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_update_enhanced: config is NULL");
        return;
    }

    float dt = (float)(current_time - trace->last_update) * ctx->config.integration_dt;
    if (dt <= 0.0f) dt = ctx->config.integration_dt;

    /* Update trace */
    if (ctx->config.use_adaptive_integration) {
        eligibility_utils_adaptive_update(trace, config, dt, spike_contribution, ctx);
    } else {
        eligibility_utils_rk4_update(trace, config, dt, spike_contribution);
    }

    /* Record metrics */
    eligibility_utils_record_update(ctx, trace->trace, 0.0f);

    trace->last_update = current_time;
    ctx->update_count++;
}

float eligibility_utils_apply_reward_enhanced(
    eligibility_utils_ctx_t ctx,
    void* synapse,
    const eligibility_trace_t* trace,
    const eligibility_config_t* config,
    float reward,
    float dopamine_level
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_apply_reward_enhanced: ctx is NULL");
        return 0.0f;
    }
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_apply_reward_enhanced: trace is NULL");
        return 0.0f;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_apply_reward_enhanced: config is NULL");
        return 0.0f;
    }

    (void)synapse;

    /* Compute weight change using eligibility trace */
    float weight_change = config->learning_rate * trace->trace * reward * dopamine_level;

    /* Record */
    eligibility_utils_record_update(ctx, trace->trace, weight_change);
    ctx->metrics.dopamine_sensitivity = dopamine_level;

    return weight_change;
}

int eligibility_utils_consolidate_enhanced(
    eligibility_utils_ctx_t ctx,
    void* synapses,
    const eligibility_trace_t* traces,
    int num_synapses,
    const eligibility_config_t* config,
    const void* phasic_tonic,
    float reward
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_consolidate_enhanced: ctx is NULL");
        return 0;
    }
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_utils_consolidate_enhanced: traces is NULL");
        return 0;
    }
    if (num_synapses <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "eligibility_utils_consolidate_enhanced: num_synapses must be positive");
        return 0;
    }

    (void)synapses;
    (void)config;
    (void)phasic_tonic;

    int consolidated = 0;
    float total_change = 0.0f;

    for (int i = 0; i < num_synapses; i++) {
        if (traces[i].trace > 0.1f) { /* Above threshold */
            consolidated++;
            total_change += traces[i].trace * reward;
        }
    }

    /* Record consolidation */
    eligibility_utils_record_consolidation(ctx, (uint32_t)num_synapses, total_change, false);

    return consolidated;
}
