/**
 * @file nimcp_stdp_utils_bridge.c
 * @brief STDP Utils Integration Implementation
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "plasticity/stdp/nimcp_stdp_utils_bridge.h"
#include "utils/containers/nimcp_ring_buffer.h"
#include "utils/metrics/nimcp_metrics.h"
#include "utils/memory/nimcp_memory.h"
/* Memory pool API not available - using simple allocation */
#include "utils/numerical/nimcp_integration.h"
#include "utils/math/nimcp_complex_math.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Simple pool structure for synapse allocation */
typedef struct {
    stdp_synapse_t* synapses;
    uint32_t capacity;
    uint32_t used;
    uint32_t* free_list;
    uint32_t free_count;
} simple_synapse_pool_t;

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_utils_bridge)

#define LOG_MODULE "STDP_UTILS_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct stdp_utils_ctx_internal {
    /* Configuration */
    stdp_utils_config_t config;

    /* Ring buffer for spike history */
    nimcp_ring_buffer_t* spike_buffer;

    /* Metrics collector */
    nimcp_metrics_collector_t metrics_collector;

    /* Memory pool for synapses */
    simple_synapse_pool_t* synapse_pool;

    /* Metrics state */
    stdp_metrics_t current_metrics;
    uint64_t metrics_start_time;

    /* Phase gating state */
    float encoding_phase_center;
    float encoding_phase_width;

    /* Statistics tracking */
    double total_weight;
    double total_weight_sq;
    uint64_t weight_count;
};

/*=============================================================================
 * DERIVATIVE FUNCTION FOR RK4 TRACE DECAY
 *===========================================================================*/

typedef struct {
    float tau;
} trace_params_t;

static void trace_derivative(const float* state, float t, void* params, float* derivatives) {
    (void)t;  /* Unused */
    trace_params_t* p = (trace_params_t*)params;
    /* de/dt = -e/tau */
    derivatives[0] = -state[0] / p->tau;
}

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *===========================================================================*/

stdp_utils_config_t stdp_utils_default_config(void) {
    stdp_utils_config_t config = {
        .spike_history_size = STDP_SPIKE_HISTORY_SIZE,
        .enable_spike_buffering = true,
        .enable_metrics = true,
        .metrics_flush_interval_ms = 10000,  /* 10 seconds */
        .metrics_output_dir = "./nimcp_stdp_metrics",
        .enable_synapse_pool = true,
        .synapse_pool_size = STDP_SYNAPSE_POOL_SIZE,
        .use_rk4_trace_decay = true,
        .trace_integration_dt = STDP_TRACE_INTEGRATION_DT,
        .enable_phase_gating = false,
        .encoding_phase_start = 0.0f,
        .encoding_phase_end = 90.0f
    };
    return config;
}

stdp_utils_ctx_t stdp_utils_create(const stdp_utils_config_t* config) {
    struct stdp_utils_ctx_internal* ctx = nimcp_calloc(1, sizeof(*ctx));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_utils_create: failed to allocate context");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = stdp_utils_default_config();
    }

    /* Create spike ring buffer */
    if (ctx->config.enable_spike_buffering) {
        ctx->spike_buffer = nimcp_ring_buffer_create(
            sizeof(stdp_spike_event_t),
            ctx->config.spike_history_size
        );
        if (!ctx->spike_buffer) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_utils_create: failed to create spike buffer");
            nimcp_free(ctx);
            return NULL;
        }
    }

    /* Create metrics collector */
    if (ctx->config.enable_metrics) {
        nimcp_metrics_config_t metrics_config;
        nimcp_metrics_get_default_config(&metrics_config);
        strncpy(metrics_config.output_directory, ctx->config.metrics_output_dir,
                NIMCP_METRICS_MAX_PATH - 1);
        metrics_config.flush_interval_ms = ctx->config.metrics_flush_interval_ms;
        ctx->metrics_collector = nimcp_metrics_create_with_config(&metrics_config);
    }

    /* Create synapse memory pool */
    if (ctx->config.enable_synapse_pool) {
        ctx->synapse_pool = nimcp_calloc(1, sizeof(simple_synapse_pool_t));
        if (!ctx->synapse_pool) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_utils_create: failed to allocate synapse pool");
            if (ctx->spike_buffer) nimcp_ring_buffer_destroy(ctx->spike_buffer);
            nimcp_free(ctx);
            return NULL;
        }
        ctx->synapse_pool->capacity = ctx->config.synapse_pool_size;
        ctx->synapse_pool->synapses = nimcp_calloc(ctx->config.synapse_pool_size, sizeof(stdp_synapse_t));
        if (!ctx->synapse_pool->synapses) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_utils_create: failed to allocate synapses");
            nimcp_free(ctx->synapse_pool);
            if (ctx->spike_buffer) nimcp_ring_buffer_destroy(ctx->spike_buffer);
            nimcp_free(ctx);
            return NULL;
        }
        ctx->synapse_pool->free_list = nimcp_malloc(ctx->config.synapse_pool_size * sizeof(uint32_t));
        if (!ctx->synapse_pool->free_list) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_utils_create: failed to allocate free list");
            nimcp_free(ctx->synapse_pool->synapses);
            nimcp_free(ctx->synapse_pool);
            if (ctx->spike_buffer) nimcp_ring_buffer_destroy(ctx->spike_buffer);
            nimcp_free(ctx);
            return NULL;
        }
        ctx->synapse_pool->free_count = ctx->config.synapse_pool_size;
        for (uint32_t i = 0; i < ctx->config.synapse_pool_size; i++) {
            ctx->synapse_pool->free_list[i] = i;
        }
    }

    /* Initialize phase gating */
    ctx->encoding_phase_center = (ctx->config.encoding_phase_start +
                                   ctx->config.encoding_phase_end) / 2.0f;
    ctx->encoding_phase_width = ctx->config.encoding_phase_end -
                                 ctx->config.encoding_phase_start;

    /* Initialize metrics */
    memset(&ctx->current_metrics, 0, sizeof(ctx->current_metrics));
    ctx->metrics_start_time = (uint64_t)time(NULL) * 1000;

    NIMCP_LOGGING_INFO("Created %s bridge", "stdp_utils");
    return ctx;
}

void stdp_utils_destroy(stdp_utils_ctx_t ctx) {
    /* Silent return for destroy - idempotent operation */
    if (!ctx) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "stdp_utils");

    /* Flush any remaining metrics */
    if (ctx->metrics_collector) {
        stdp_utils_flush_metrics(ctx);
        nimcp_metrics_destroy(ctx->metrics_collector);
    }

    /* Destroy ring buffer */
    if (ctx->spike_buffer) {
        nimcp_ring_buffer_destroy(ctx->spike_buffer);
    }

    /* Destroy memory pool */
    if (ctx->synapse_pool) {
        nimcp_free(ctx->synapse_pool->synapses);
        nimcp_free(ctx->synapse_pool->free_list);
        nimcp_free(ctx->synapse_pool);
    }

    nimcp_free(ctx);
}

void stdp_utils_reset(stdp_utils_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_reset: ctx is NULL");
        return;
    }

    /* Clear spike buffer */
    if (ctx->spike_buffer) {
        nimcp_ring_buffer_clear(ctx->spike_buffer);
    }

    /* Reset metrics */
    memset(&ctx->current_metrics, 0, sizeof(ctx->current_metrics));
    ctx->total_weight = 0.0;
    ctx->total_weight_sq = 0.0;
    ctx->weight_count = 0;
}

/*=============================================================================
 * SPIKE BUFFER IMPLEMENTATION
 *===========================================================================*/

bool stdp_utils_record_spike(stdp_utils_ctx_t ctx, const stdp_spike_event_t* event) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_record_spike: ctx is NULL");
        return false;
    }
    if (!event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_record_spike: event is NULL");
        return false;
    }
    if (!ctx->spike_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_utils_record_spike: spike buffer not initialized");
        return false;
    }

    return nimcp_ring_buffer_push(ctx->spike_buffer, event);
}

bool stdp_utils_get_spikes_in_window(
    stdp_utils_ctx_t ctx,
    float t_start,
    float t_end,
    stdp_spike_event_t* out_events,
    uint32_t max_events,
    uint32_t* num_found
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_get_spikes_in_window: ctx is NULL");
        if (num_found) *num_found = 0;
        return false;
    }
    if (!out_events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_get_spikes_in_window: out_events is NULL");
        if (num_found) *num_found = 0;
        return false;
    }
    if (!num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_get_spikes_in_window: num_found is NULL");
        return false;
    }
    if (!ctx->spike_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_utils_get_spikes_in_window: spike buffer not initialized");
        *num_found = 0;
        return false;
    }

    *num_found = 0;
    size_t buffer_size = nimcp_ring_buffer_size(ctx->spike_buffer);

    for (size_t i = 0; i < buffer_size && *num_found < max_events; i++) {
        const stdp_spike_event_t* event =
            (const stdp_spike_event_t*)nimcp_ring_buffer_at_const(ctx->spike_buffer, i);
        if (event && event->timestamp >= t_start && event->timestamp <= t_end) {
            out_events[(*num_found)++] = *event;
        }
    }

    return true;
}

bool stdp_utils_get_recent_spikes(
    stdp_utils_ctx_t ctx,
    uint32_t n,
    stdp_spike_event_t* out_events,
    uint32_t* num_found
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_get_recent_spikes: ctx is NULL");
        if (num_found) *num_found = 0;
        return false;
    }
    if (!out_events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_get_recent_spikes: out_events is NULL");
        if (num_found) *num_found = 0;
        return false;
    }
    if (!num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_get_recent_spikes: num_found is NULL");
        return false;
    }
    if (!ctx->spike_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_utils_get_recent_spikes: spike buffer not initialized");
        *num_found = 0;
        return false;
    }

    size_t copied = nimcp_ring_buffer_copy_last_n(ctx->spike_buffer, out_events, n);
    *num_found = (uint32_t)copied;
    return true;
}

bool stdp_utils_find_spike_pairs(
    stdp_utils_ctx_t ctx,
    uint32_t pre_id,
    uint32_t post_id,
    float time_window_ms,
    stdp_spike_event_t* out_events,
    uint32_t max_events,
    uint32_t* num_found
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_find_spike_pairs: ctx is NULL");
        if (num_found) *num_found = 0;
        return false;
    }
    if (!out_events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_find_spike_pairs: out_events is NULL");
        if (num_found) *num_found = 0;
        return false;
    }
    if (!num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_find_spike_pairs: num_found is NULL");
        return false;
    }
    if (!ctx->spike_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_utils_find_spike_pairs: spike buffer not initialized");
        *num_found = 0;
        return false;
    }

    *num_found = 0;
    size_t buffer_size = nimcp_ring_buffer_size(ctx->spike_buffer);

    /* Get current time from most recent spike */
    float current_time = 0.0f;
    const stdp_spike_event_t* newest =
        (const stdp_spike_event_t*)nimcp_ring_buffer_back_const(ctx->spike_buffer);
    if (newest) {
        current_time = newest->timestamp;
    }

    float t_start = current_time - time_window_ms;

    for (size_t i = 0; i < buffer_size && *num_found < max_events; i++) {
        const stdp_spike_event_t* event =
            (const stdp_spike_event_t*)nimcp_ring_buffer_at_const(ctx->spike_buffer, i);
        if (event && event->timestamp >= t_start) {
            if ((event->source_id == pre_id || event->target_id == pre_id) ||
                (event->source_id == post_id || event->target_id == post_id)) {
                out_events[(*num_found)++] = *event;
            }
        }
    }

    return true;
}

/*=============================================================================
 * METRICS IMPLEMENTATION
 *===========================================================================*/

void stdp_utils_record_ltp(stdp_utils_ctx_t ctx, float weight_change, float timing_delta) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_record_ltp: ctx is NULL");
        return;
    }

    ctx->current_metrics.total_ltp_events++;

    /* Update LTP/LTD ratio */
    if (ctx->current_metrics.total_ltd_events > 0) {
        ctx->current_metrics.ltp_ltd_ratio = (float)ctx->current_metrics.total_ltp_events /
                                              (float)ctx->current_metrics.total_ltd_events;
    } else {
        ctx->current_metrics.ltp_ltd_ratio = (float)ctx->current_metrics.total_ltp_events;
    }

    /* Update timing histogram */
    int bin = (int)((timing_delta + 50.0f) / 5.0f);  /* -50ms to +50ms in 5ms bins */
    if (bin >= 0 && bin < 20) {
        ctx->current_metrics.timing_histogram[bin]++;
    }

    /* Record to metrics collector */
    if (ctx->metrics_collector) {
        nimcp_metrics_record_counter(ctx->metrics_collector, "stdp.ltp_events",
                                      ctx->current_metrics.total_ltp_events,
                                      NIMCP_METRIC_CATEGORY_LEARNING);
        nimcp_metrics_record_gauge(ctx->metrics_collector, "stdp.ltp_weight_change",
                                    weight_change, NIMCP_METRIC_CATEGORY_LEARNING);
    }
}

void stdp_utils_record_ltd(stdp_utils_ctx_t ctx, float weight_change, float timing_delta) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_record_ltd: ctx is NULL");
        return;
    }

    ctx->current_metrics.total_ltd_events++;

    /* Update LTP/LTD ratio */
    if (ctx->current_metrics.total_ltd_events > 0) {
        ctx->current_metrics.ltp_ltd_ratio = (float)ctx->current_metrics.total_ltp_events /
                                              (float)ctx->current_metrics.total_ltd_events;
    }

    /* Update timing histogram */
    int bin = (int)((timing_delta + 50.0f) / 5.0f);
    if (bin >= 0 && bin < 20) {
        ctx->current_metrics.timing_histogram[bin]++;
    }

    /* Record to metrics collector */
    if (ctx->metrics_collector) {
        nimcp_metrics_record_counter(ctx->metrics_collector, "stdp.ltd_events",
                                      ctx->current_metrics.total_ltd_events,
                                      NIMCP_METRIC_CATEGORY_LEARNING);
        nimcp_metrics_record_gauge(ctx->metrics_collector, "stdp.ltd_weight_change",
                                    weight_change, NIMCP_METRIC_CATEGORY_LEARNING);
    }
}

void stdp_utils_update_weight_stats(
    stdp_utils_ctx_t ctx,
    const float* weights,
    uint32_t num_weights
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_update_weight_stats: ctx is NULL");
        return;
    }
    if (!weights || num_weights == 0) return;

    /* Compute statistics */
    double sum = 0.0;
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < num_weights; i++) {
        sum += weights[i];
        sum_sq += weights[i] * weights[i];
    }

    ctx->current_metrics.mean_weight = (float)(sum / num_weights);
    float mean_sq = (float)(sum_sq / num_weights);
    ctx->current_metrics.weight_variance = mean_sq -
        ctx->current_metrics.mean_weight * ctx->current_metrics.mean_weight;

    /* Update weight histogram */
    memset(ctx->current_metrics.weight_histogram, 0, sizeof(ctx->current_metrics.weight_histogram));
    for (uint32_t i = 0; i < num_weights; i++) {
        int bin = (int)(weights[i] * 20.0f);  /* 0-1 in 20 bins */
        if (bin >= 0 && bin < 20) {
            ctx->current_metrics.weight_histogram[bin]++;
        }
    }

    /* Compute LTP/LTD ratio */
    if (ctx->current_metrics.total_ltd_events > 0) {
        ctx->current_metrics.ltp_ltd_ratio = (float)ctx->current_metrics.total_ltp_events /
                                              (float)ctx->current_metrics.total_ltd_events;
    }

    /* Record to metrics collector */
    if (ctx->metrics_collector) {
        nimcp_metrics_record_gauge(ctx->metrics_collector, "stdp.mean_weight",
                                    ctx->current_metrics.mean_weight,
                                    NIMCP_METRIC_CATEGORY_LEARNING);
        nimcp_metrics_record_gauge(ctx->metrics_collector, "stdp.weight_variance",
                                    ctx->current_metrics.weight_variance,
                                    NIMCP_METRIC_CATEGORY_LEARNING);
    }
}

bool stdp_utils_get_metrics(stdp_utils_ctx_t ctx, stdp_metrics_t* metrics) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_get_metrics: ctx is NULL");
        return false;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_get_metrics: metrics is NULL");
        return false;
    }

    ctx->current_metrics.last_update_ms = (uint64_t)time(NULL) * 1000;
    *metrics = ctx->current_metrics;
    return true;
}

int32_t stdp_utils_flush_metrics(stdp_utils_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_flush_metrics: ctx is NULL");
        return 0;
    }
    if (!ctx->metrics_collector) return 0;
    return nimcp_metrics_flush(ctx->metrics_collector);
}

bool stdp_utils_export_csv(stdp_utils_ctx_t ctx, const char* filename) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_export_csv: ctx is NULL");
        return false;
    }
    if (!filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_export_csv: filename is NULL");
        return false;
    }
    if (!ctx->metrics_collector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_utils_export_csv: metrics collector not initialized");
        return false;
    }
    return nimcp_metrics_export_tableau_csv(ctx->metrics_collector, filename);
}

bool stdp_utils_export_json(stdp_utils_ctx_t ctx, const char* filename) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_export_json: ctx is NULL");
        return false;
    }
    if (!filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_export_json: filename is NULL");
        return false;
    }
    if (!ctx->metrics_collector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_utils_export_json: metrics collector not initialized");
        return false;
    }
    return nimcp_metrics_export_powerbi_json(ctx->metrics_collector, filename);
}

/*=============================================================================
 * MEMORY POOL IMPLEMENTATION
 *===========================================================================*/

stdp_synapse_t* stdp_utils_alloc_synapse(stdp_utils_ctx_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_alloc_synapse: ctx is NULL");
        return NULL;
    }
    if (!ctx->synapse_pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_utils_alloc_synapse: synapse pool not initialized");
        return NULL;
    }

    simple_synapse_pool_t* pool = ctx->synapse_pool;
    if (pool->free_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_utils_alloc_synapse: pool exhausted");
        return NULL;
    }

    uint32_t idx = pool->free_list[--pool->free_count];
    stdp_synapse_t* synapse = &pool->synapses[idx];
    pool->used++;
    stdp_synapse_init(synapse);
    return synapse;
}

void stdp_utils_free_synapse(stdp_utils_ctx_t ctx, stdp_synapse_t* synapse) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_free_synapse: ctx is NULL");
        return;
    }
    if (!synapse || !ctx->synapse_pool) return;

    simple_synapse_pool_t* pool = ctx->synapse_pool;
    /* Calculate index from pointer offset */
    uint32_t idx = (uint32_t)(synapse - pool->synapses);
    if (idx < pool->capacity) {
        pool->free_list[pool->free_count++] = idx;
        pool->used--;
    }
}

uint32_t stdp_utils_alloc_synapse_batch(
    stdp_utils_ctx_t ctx,
    uint32_t count,
    stdp_synapse_t** out_synapses
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_alloc_synapse_batch: ctx is NULL");
        return 0;
    }
    if (!out_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_alloc_synapse_batch: out_synapses is NULL");
        return 0;
    }
    if (!ctx->synapse_pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_utils_alloc_synapse_batch: synapse pool not initialized");
        return 0;
    }

    uint32_t allocated = 0;
    for (uint32_t i = 0; i < count; i++) {
        out_synapses[i] = stdp_utils_alloc_synapse(ctx);
        if (out_synapses[i]) {
            allocated++;
        } else {
            break;  /* Pool exhausted */
        }
    }
    return allocated;
}

void stdp_utils_pool_stats(
    stdp_utils_ctx_t ctx,
    uint32_t* total_capacity,
    uint32_t* used_count,
    uint32_t* free_count
) {
    if (!ctx || !ctx->synapse_pool) {
        if (total_capacity) *total_capacity = 0;
        if (used_count) *used_count = 0;
        if (free_count) *free_count = 0;
        return;
    }

    simple_synapse_pool_t* pool = ctx->synapse_pool;
    if (total_capacity) *total_capacity = pool->capacity;
    if (used_count) *used_count = pool->used;
    if (free_count) *free_count = pool->free_count;
}

/*=============================================================================
 * NUMERICAL INTEGRATION IMPLEMENTATION
 *===========================================================================*/

void stdp_utils_rk4_trace_update(
    float* trace,
    float tau,
    float dt,
    float spike_contribution
) {
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_rk4_trace_update: trace is NULL");
        return;
    }
    if (tau <= 0.0f || dt <= 0.0f) return;

    /* Use RK4 for accurate exponential decay */
    trace_params_t params = { .tau = tau };

    /* Integrate trace decay */
    integration_rk4_step(trace_derivative, trace, 0.0f, dt, 1, &params);

    /* Add spike contribution after decay */
    *trace += spike_contribution;
}

void stdp_utils_rk4_trace_batch(
    float* traces,
    const float* taus,
    uint32_t num_traces,
    float dt,
    const uint8_t* spike_mask
) {
    if (!traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_rk4_trace_batch: traces is NULL");
        return;
    }
    if (!taus || num_traces == 0 || dt <= 0.0f) return;

    for (uint32_t i = 0; i < num_traces; i++) {
        trace_params_t params = { .tau = taus[i] };
        integration_rk4_step(trace_derivative, &traces[i], 0.0f, dt, 1, &params);

        /* Add spike if in mask */
        if (spike_mask && (spike_mask[i / 8] & (1 << (i % 8)))) {
            traces[i] += 1.0f;
        }
    }
}

/*=============================================================================
 * PHASE-GATED STDP IMPLEMENTATION
 *===========================================================================*/

bool stdp_utils_in_encoding_window(stdp_utils_ctx_t ctx, float theta_phase) {
    if (!ctx || !ctx->config.enable_phase_gating) return true;  /* Always learn if disabled */

    /* Normalize phase to 0-360 */
    while (theta_phase < 0.0f) theta_phase += 360.0f;
    while (theta_phase >= 360.0f) theta_phase -= 360.0f;

    return theta_phase >= ctx->config.encoding_phase_start &&
           theta_phase <= ctx->config.encoding_phase_end;
}

float stdp_utils_phase_modulation(stdp_utils_ctx_t ctx, float theta_phase) {
    if (!ctx || !ctx->config.enable_phase_gating) return 1.0f;

    /* Normalize phase */
    while (theta_phase < 0.0f) theta_phase += 360.0f;
    while (theta_phase >= 360.0f) theta_phase -= 360.0f;

    /* Compute distance from encoding center */
    float dist = fabsf(theta_phase - ctx->encoding_phase_center);
    if (dist > 180.0f) dist = 360.0f - dist;  /* Handle wrap-around */

    /* Gaussian modulation */
    float sigma = ctx->encoding_phase_width / 2.0f;
    float modulation = expf(-(dist * dist) / (2.0f * sigma * sigma));

    return modulation;
}

float stdp_utils_compute_pac(
    const float* theta_phases,
    const float* gamma_amplitudes,
    uint32_t num_samples
) {
    if (!theta_phases || !gamma_amplitudes || num_samples == 0) return 0.0f;

    /* Compute mean resultant length (PAC modulation index) */
    float sum_cos = 0.0f;
    float sum_sin = 0.0f;
    float sum_amp = 0.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        float phase_rad = theta_phases[i] * (float)M_PI / 180.0f;
        float amp = gamma_amplitudes[i];
        sum_cos += amp * cosf(phase_rad);
        sum_sin += amp * sinf(phase_rad);
        sum_amp += amp;
    }

    if (sum_amp <= 0.0f) return 0.0f;

    /* Normalize */
    sum_cos /= sum_amp;
    sum_sin /= sum_amp;

    /* Mean resultant length */
    float mrl = sqrtf(sum_cos * sum_cos + sum_sin * sum_sin);
    return mrl;
}

bool stdp_utils_extract_phase(
    const float* signal,
    uint32_t num_samples,
    float* phases
) {
    if (!signal || !phases || num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_extract_phase: required parameter is NULL (signal, phases)");
        return false;
    }

    /* Simple phase extraction using zero-crossing */
    /* For production, use full Hilbert transform from complex_math */
    float prev_val = signal[0];
    float current_phase = 0.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        float val = signal[i];

        /* Detect zero crossing (positive going) */
        if (prev_val < 0.0f && val >= 0.0f) {
            current_phase = 0.0f;
        } else if (prev_val >= 0.0f && val < 0.0f) {
            current_phase = (float)M_PI;
        } else {
            /* Linear interpolation between crossings */
            current_phase += (float)M_PI / (float)(num_samples / 4);
        }

        phases[i] = current_phase;
        prev_val = val;
    }

    return true;
}

/*=============================================================================
 * ENHANCED STDP OPERATIONS
 *===========================================================================*/

float stdp_utils_pre_spike_enhanced(
    stdp_utils_ctx_t ctx,
    stdp_synapse_t* synapse,
    float timestamp,
    float theta_phase
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_pre_spike_enhanced: ctx is NULL");
        return 0.0f;
    }
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_pre_spike_enhanced: synapse is NULL");
        return 0.0f;
    }

    /* Record spike to buffer */
    stdp_spike_event_t event = {
        .source_id = 0,  /* Pre-synaptic */
        .target_id = 0,
        .timestamp = timestamp,
        .amplitude = 1.0f,
        .is_pre = true
    };
    stdp_utils_record_spike(ctx, &event);

    /* Check phase gating */
    float phase_mod = stdp_utils_phase_modulation(ctx, theta_phase);
    if (phase_mod < 0.1f) return 0.0f;  /* Skip learning outside encoding window */

    /* Update traces with RK4 if enabled */
    if (ctx->config.use_rk4_trace_decay) {
        float dt = ctx->config.trace_integration_dt;
        stdp_utils_rk4_trace_update(&synapse->pre_trace, synapse->tau_plus, dt, 1.0f);
    } else {
        /* Standard Euler update */
        synapse->pre_trace += 1.0f;
    }

    /* Compute weight change (LTD: pre before post → depression) */
    float dw = -synapse->a_minus * synapse->post_trace * synapse->learning_rate * phase_mod;

    /* Apply weight change */
    synapse->weight += dw;
    if (synapse->weight < synapse->w_min) synapse->weight = synapse->w_min;
    if (synapse->weight > synapse->w_max) synapse->weight = synapse->w_max;

    /* Record metrics */
    if (dw < 0.0f) {
        stdp_utils_record_ltd(ctx, dw, timestamp);
    }

    return dw;
}

float stdp_utils_post_spike_enhanced(
    stdp_utils_ctx_t ctx,
    stdp_synapse_t* synapse,
    float timestamp,
    float theta_phase
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_post_spike_enhanced: ctx is NULL");
        return 0.0f;
    }
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_post_spike_enhanced: synapse is NULL");
        return 0.0f;
    }

    /* Record spike to buffer */
    stdp_spike_event_t event = {
        .source_id = 0,
        .target_id = 0,
        .timestamp = timestamp,
        .amplitude = 1.0f,
        .is_pre = false
    };
    stdp_utils_record_spike(ctx, &event);

    /* Check phase gating */
    float phase_mod = stdp_utils_phase_modulation(ctx, theta_phase);
    if (phase_mod < 0.1f) return 0.0f;

    /* Update traces with RK4 if enabled */
    if (ctx->config.use_rk4_trace_decay) {
        float dt = ctx->config.trace_integration_dt;
        stdp_utils_rk4_trace_update(&synapse->post_trace, synapse->tau_minus, dt, 1.0f);
    } else {
        synapse->post_trace += 1.0f;
    }

    /* Compute weight change (LTP: post after pre → potentiation) */
    float dw = synapse->a_plus * synapse->pre_trace * synapse->learning_rate * phase_mod;

    /* Apply weight change */
    synapse->weight += dw;
    if (synapse->weight < synapse->w_min) synapse->weight = synapse->w_min;
    if (synapse->weight > synapse->w_max) synapse->weight = synapse->w_max;

    /* Record metrics */
    if (dw > 0.0f) {
        stdp_utils_record_ltp(ctx, dw, timestamp);
    }

    return dw;
}

uint32_t stdp_utils_batch_process(
    stdp_utils_ctx_t ctx,
    stdp_synapse_t* synapses,
    const stdp_spike_event_t* events,
    uint32_t num_events,
    float theta_phase,
    float* weight_changes
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_batch_process: ctx is NULL");
        return 0;
    }
    if (!synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_batch_process: synapses is NULL");
        return 0;
    }
    if (!events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_utils_batch_process: events is NULL");
        return 0;
    }
    if (num_events == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_utils_batch_process: num_events is 0");
        return 0;
    }

    uint32_t processed = 0;

    for (uint32_t i = 0; i < num_events; i++) {
        const stdp_spike_event_t* event = &events[i];
        stdp_synapse_t* synapse = &synapses[i];  /* Assume 1:1 mapping */

        float dw;
        if (event->is_pre) {
            dw = stdp_utils_pre_spike_enhanced(ctx, synapse, event->timestamp, theta_phase);
        } else {
            dw = stdp_utils_post_spike_enhanced(ctx, synapse, event->timestamp, theta_phase);
        }

        if (weight_changes) {
            weight_changes[i] = dw;
        }
        processed++;
    }

    ctx->current_metrics.total_spikes_processed += processed;
    return processed;
}
