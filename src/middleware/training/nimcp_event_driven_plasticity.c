/**
 * @file nimcp_event_driven_plasticity.c
 * @brief Event-Driven Plasticity Adapter Implementation
 *
 * Phase EDP-1: Connects Event Bus to Plasticity Bridge for Dynamic Learning
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 */

#include "middleware/training/nimcp_event_driven_plasticity.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/platform/nimcp_platform_thread.h"
#include "utils/platform/nimcp_platform_cond.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/math/nimcp_math_helpers.h"

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(event_driven_plasticity)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Spike buffer for STDP processing (circular buffer)
 */
typedef struct {
    edp_spike_record_t* spikes;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    nimcp_platform_mutex_t mutex;
} spike_buffer_t;

/**
 * @brief Eligibility trace table (hash-based)
 */
typedef struct {
    edp_eligibility_entry_t* entries;
    uint32_t capacity;
    uint32_t count;
    nimcp_platform_rwlock_t rwlock;
} eligibility_table_t;

/**
 * @brief Event subscription record
 */
typedef struct {
    event_subscription_handle_t handle;
    brain_event_type_t event_type;
    bool active;
} subscription_record_t;

/**
 * @brief EDP context structure
 */
struct edp_context {
    /* Magic number for dangling pointer detection */
    uint32_t magic;
#define EDP_CONTEXT_MAGIC 0xED90CAFE

    /* Configuration */
    edp_config_t config;

    /* Connected components */
    tpb_context_t* plasticity_bridge;
    event_bus_t event_bus;
    learning_signal_adapter_t learning_adapter;

    /* Subscriptions */
    subscription_record_t subscriptions[EDP_MAX_SUBSCRIPTIONS];
    uint32_t num_subscriptions;

    /* Spike processing */
    spike_buffer_t spike_buffer;
    float stdp_window_ns;  /* Converted to nanoseconds */

    /* Eligibility traces */
    eligibility_table_t eligibility;

    /* State */
    bool active;
    bool running;
    nimcp_platform_mutex_t state_mutex;

    /* Async processing */
    nimcp_platform_thread_t async_thread;
    nimcp_platform_cond_t async_cond;
    nimcp_platform_mutex_t async_mutex;
    bool async_shutdown;

    /* Statistics */
    edp_stats_t stats;
    nimcp_platform_mutex_t stats_mutex;

    /* Security */
    nimcp_sec_integration_t* security_ctx;
    uint32_t security_module_id;
    bool security_registered;

    /* Memory */
    unified_mem_manager_t memory_mgr;
};

/* ============================================================================
 * Category and Mode Names
 * ============================================================================ */

static const char* const category_names[] = {
    "Spike",
    "Pattern",
    "Error",
    "Reward",
    "Novelty",
    "Attention"
};

static const char* const mode_names[] = {
    "Immediate",
    "Batched",
    "Async",
    "Hybrid"
};

const char* edp_category_name(edp_event_category_t category) {
    if (category >= EDP_CATEGORY_COUNT) return "Unknown";
    return category_names[category];
}

const char* edp_mode_name(edp_processing_mode_t mode) {
    if (mode > EDP_MODE_HYBRID) return "Unknown";
    return mode_names[mode];
}

/* ============================================================================
 * Spike Buffer Operations
 * ============================================================================ */

static bool spike_buffer_init(spike_buffer_t* buf, uint32_t capacity)
{
    buf->spikes = (edp_spike_record_t*)nimcp_calloc(capacity, sizeof(edp_spike_record_t));
    if (!buf->spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "spike_buffer_init: buf->spikes is NULL");
        return false;
    }

    buf->capacity = capacity;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;

    if (nimcp_platform_mutex_init(&buf->mutex, false) != 0) {
        nimcp_free(buf->spikes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "spike_buffer_init: validation failed");
        return false;
    }

    return true;
}

static void spike_buffer_destroy(spike_buffer_t* buf)
{
    if (buf->spikes) {
        nimcp_free(buf->spikes);
        buf->spikes = NULL;
    }
    nimcp_platform_mutex_destroy(&buf->mutex);
}

static bool spike_buffer_push(spike_buffer_t* buf, const edp_spike_record_t* spike)
{
    nimcp_platform_mutex_lock(&buf->mutex);

    if (buf->count >= buf->capacity) {
        /* Buffer full - overwrite oldest */
        buf->tail = (buf->tail + 1) % buf->capacity;
    } else {
        buf->count++;
    }

    buf->spikes[buf->head] = *spike;
    buf->head = (buf->head + 1) % buf->capacity;

    nimcp_platform_mutex_unlock(&buf->mutex);
    return true;
}

static uint32_t spike_buffer_get_recent(spike_buffer_t* buf,
                                         edp_spike_record_t* out,
                                         uint32_t max_count,
                                         uint64_t since_timestamp)
{
    nimcp_platform_mutex_lock(&buf->mutex);

    uint32_t copied = 0;
    uint32_t idx = buf->tail;

    for (uint32_t i = 0; i < buf->count && copied < max_count; i++) {
        if (buf->spikes[idx].timestamp_ns >= since_timestamp) {
            out[copied++] = buf->spikes[idx];
        }
        idx = (idx + 1) % buf->capacity;
    }

    nimcp_platform_mutex_unlock(&buf->mutex);
    return copied;
}

/* ============================================================================
 * Eligibility Table Operations
 * ============================================================================ */

static uint32_t eligibility_hash(uint32_t pre, uint32_t post, uint32_t capacity)
{
    /* Simple hash combining pre and post neuron IDs */
    uint64_t combined = ((uint64_t)pre << 32) | post;
    return (uint32_t)((combined * 2654435761ULL) % capacity);
}

static bool eligibility_table_init(eligibility_table_t* table, uint32_t capacity)
{
    table->entries = (edp_eligibility_entry_t*)nimcp_calloc(capacity, sizeof(edp_eligibility_entry_t));
    if (!table->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "eligibility_table_init: table->entries is NULL");
        return false;
    }

    table->capacity = capacity;
    table->count = 0;

    if (nimcp_platform_rwlock_init(&table->rwlock) != 0) {
        nimcp_free(table->entries);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "eligibility_table_init: validation failed");
        return false;
    }

    /* Initialize entries as empty */
    for (uint32_t i = 0; i < capacity; i++) {
        table->entries[i].eligibility = 0.0F;
    }

    return true;
}

static void eligibility_table_destroy(eligibility_table_t* table)
{
    if (table->entries) {
        nimcp_free(table->entries);
        table->entries = NULL;
    }
    nimcp_platform_rwlock_destroy(&table->rwlock);
}

static edp_eligibility_entry_t* eligibility_find_or_create(
    eligibility_table_t* table,
    uint32_t pre_neuron,
    uint32_t post_neuron)
{
    uint32_t hash = eligibility_hash(pre_neuron, post_neuron, table->capacity);
    uint32_t probe = 0;

    /* Linear probing */
    while (probe < table->capacity) {
        uint32_t idx = (hash + probe) % table->capacity;
        edp_eligibility_entry_t* entry = &table->entries[idx];

        if (entry->eligibility == 0.0F && entry->pre_neuron == 0 && entry->post_neuron == 0) {
            /* Empty slot - create new entry */
            entry->pre_neuron = pre_neuron;
            entry->post_neuron = post_neuron;
            entry->eligibility = 0.0F;
            entry->last_update_ns = 0;
            entry->accumulated_delta = 0.0F;
            table->count++;
            return entry;
        }

        if (entry->pre_neuron == pre_neuron && entry->post_neuron == post_neuron) {
            /* Found existing entry */
            return entry;
        }

        probe++;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "eligibility_find_or_create: eligibility table full");
    return NULL;  /* Table full */
}

/* ============================================================================
 * Event Callback
 * ============================================================================ */

static void edp_event_callback(const brain_event_t* event, void* context)
{
    edp_context_t* ctx = (edp_context_t*)context;
    if (!ctx || !ctx->active) return;

    /* Process the event */
    edp_process_event(ctx, event);
}

/* ============================================================================
 * STDP Processing
 * ============================================================================ */

static edp_spike_timing_t classify_spike_timing(int64_t delta_ns, float window_ns)
{
    float delta_ms = (float)delta_ns / 1000000.0F;
    float window_ms = window_ns / 1000000.0F;

    if (fabsf(delta_ms) < 1.0F) {
        return EDP_SPIKE_SYNCHRONOUS;
    } else if (delta_ms > 0 && delta_ms < window_ms) {
        return EDP_SPIKE_PRE_BEFORE_POST;  /* LTP */
    } else if (delta_ms < 0 && delta_ms > -window_ms) {
        return EDP_SPIKE_POST_BEFORE_PRE;  /* LTD */
    }
    return EDP_SPIKE_UNCORRELATED;
}

static float compute_stdp_weight_change(edp_context_t* ctx,
                                        int64_t delta_ns,
                                        edp_spike_timing_t timing)
{
    float delta_ms = (float)delta_ns / 1000000.0F;
    float window_ms = ctx->stdp_window_ns / 1000000.0F;

    switch (timing) {
        case EDP_SPIKE_PRE_BEFORE_POST:
            /* LTP: exponential decay with positive dt */
            return ctx->config.ltp_rate * expf(-fabsf(delta_ms) / (window_ms * 0.5F));

        case EDP_SPIKE_POST_BEFORE_PRE:
            /* LTD: exponential decay with negative dt */
            return -ctx->config.ltd_rate * expf(-fabsf(delta_ms) / (window_ms * 0.5F));

        case EDP_SPIKE_SYNCHRONOUS:
            /* Slight LTP for synchronous firing */
            return ctx->config.ltp_rate * 0.5F;

        default:
            return 0.0F;
    }
}

static nimcp_result_t process_spike_pair(edp_context_t* ctx,
                                          const edp_spike_record_t* pre,
                                          const edp_spike_record_t* post)
{
    if (!ctx->plasticity_bridge) {
        return NIMCP_NOT_INITIALIZED;
    }

    int64_t delta_ns = (int64_t)post->timestamp_ns - (int64_t)pre->timestamp_ns;
    edp_spike_timing_t timing = classify_spike_timing(delta_ns, ctx->stdp_window_ns);

    if (timing == EDP_SPIKE_UNCORRELATED) {
        return NIMCP_SUCCESS;  /* No learning for uncorrelated spikes */
    }

    float weight_change = compute_stdp_weight_change(ctx, delta_ns, timing);

    /* Update statistics */
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->stats.spike_pairs_evaluated++;
    if (weight_change > 0) {
        ctx->stats.ltp_events++;
    } else if (weight_change < 0) {
        ctx->stats.ltd_events++;
    }
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    /* If eligibility traces enabled, accumulate for later reward */
    if (ctx->config.enable_eligibility) {
        nimcp_platform_rwlock_wrlock(&ctx->eligibility.rwlock);
        edp_eligibility_entry_t* entry = eligibility_find_or_create(
            &ctx->eligibility, pre->neuron_id, post->neuron_id);
        if (entry) {
            entry->accumulated_delta += weight_change;
            entry->eligibility = 1.0F;  /* Reset eligibility on activity */
            entry->last_update_ns = nimcp_time_monotonic_ns();
        }
        nimcp_platform_rwlock_wrunlock(&ctx->eligibility.rwlock);
    } else {
        /* Apply weight change immediately through plasticity bridge */
        float weight_delta = 0.0F;
        nimcp_result_t res = tpb_route_weight_update(
            ctx->plasticity_bridge,
            post->neuron_id,
            pre->amplitude,
            post->amplitude,
            (float)delta_ns / 1000000.0F,  /* Convert to ms */
            &weight_delta
        );

        if (res == NIMCP_SUCCESS) {
            nimcp_platform_mutex_lock(&ctx->stats_mutex);
            ctx->stats.total_plasticity_updates++;
            ctx->stats.category_stats[EDP_CATEGORY_SPIKE].plasticity_updates++;
            ctx->stats.category_stats[EDP_CATEGORY_SPIKE].total_weight_change += fabsf(weight_delta);
            nimcp_platform_mutex_unlock(&ctx->stats_mutex);
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Process pending spikes in buffer for STDP
 *
 * Scans recent spikes and forms pairs for STDP processing.
 * Uses sliding window to find temporally correlated spike pairs.
 */
static void process_pending_spikes(edp_context_t* ctx)
{
    if (!ctx || ctx->spike_buffer.count < 2) return;

    nimcp_platform_mutex_lock(&ctx->spike_buffer.mutex);

    /* Process recent spike pairs within STDP window */
    uint32_t count = ctx->spike_buffer.count;
    uint32_t tail = ctx->spike_buffer.tail;
    uint32_t capacity = ctx->spike_buffer.capacity;

    /* Look at the most recent spike (head points past newest, so go back by 1) */
    uint32_t newest_idx = (ctx->spike_buffer.head + capacity - 1) % capacity;
    edp_spike_record_t* newest = &ctx->spike_buffer.spikes[newest_idx];

    /* Compare with older spikes within the window */
    for (uint32_t i = 1; i < count && i < 10; i++) {  /* Limit comparisons */
        uint32_t older_idx = (newest_idx + capacity - i) % capacity;
        edp_spike_record_t* older = &ctx->spike_buffer.spikes[older_idx];

        /* Check if within STDP window */
        int64_t delta_ns = (int64_t)newest->timestamp_ns - (int64_t)older->timestamp_ns;
        if (llabs(delta_ns) > (int64_t)(ctx->stdp_window_ns)) {
            break;  /* Outside window, no more pairs */
        }

        /* Different neurons - valid pair for STDP */
        if (older->neuron_id != newest->neuron_id) {
            nimcp_platform_mutex_unlock(&ctx->spike_buffer.mutex);
            process_spike_pair(ctx, older, newest);
            nimcp_platform_mutex_lock(&ctx->spike_buffer.mutex);

            /* Re-validate after reacquiring lock - buffer may have been modified */
            if (ctx->spike_buffer.count == 0) break;  /* Buffer was drained while unlocked */
            count = ctx->spike_buffer.count;
            newest_idx = (ctx->spike_buffer.head + ctx->spike_buffer.capacity - 1) % ctx->spike_buffer.capacity;
            newest = &ctx->spike_buffer.spikes[newest_idx];
        }
    }

    nimcp_platform_mutex_unlock(&ctx->spike_buffer.mutex);
}

/* ============================================================================
 * Default Configurations
 * ============================================================================ */

edp_config_t edp_config_default(void)
{
    edp_config_t config;
    memset(&config, 0, sizeof(config));

    config.mode = EDP_MODE_IMMEDIATE;
    config.batch_size = NIMCP_DEFAULT_BATCH_SIZE;
    config.batch_timeout_ms = 10;

    config.stdp_window_ms = 40.0F;  /* ±40ms STDP window */
    config.ltp_rate = 0.01F;
    config.ltd_rate = 0.012F;       /* Slightly stronger LTD */
    config.spike_threshold = 0.1F;

    config.enable_eligibility = true;
    config.eligibility_tau_ms = EDP_DEFAULT_ELIGIBILITY_TAU;
    config.eligibility_threshold = 0.01F;

    config.error_gain = 1.0F;
    config.reward_gain = 1.0F;
    config.novelty_gain = 0.5F;

    config.filter_by_region = false;
    config.region_filter = NULL;
    config.num_region_filters = 0;

    config.use_memory_pool = false;
    config.memory_mgr = NULL;

    config.enable_security = true;
    config.security_ctx = NULL;

    config.enable_async_processing = false;
    config.async_queue_size = 1024;

    return config;
}

edp_config_t edp_config_high_performance(void)
{
    edp_config_t config = edp_config_default();

    /* Fast processing mode with minimal overhead */
    config.mode = EDP_MODE_IMMEDIATE;
    config.stdp_window_ms = 15.0F;  /* Shorter window for speed */

    config.enable_eligibility = false;  /* Faster without traces */
    config.eligibility_tau_ms = 50.0F;
    config.enable_async_processing = true;
    config.async_queue_size = 4096;

    return config;
}

edp_config_t edp_config_biological(void)
{
    edp_config_t config = edp_config_default();

    /* Biologically realistic parameters */
    config.mode = EDP_MODE_BATCHED;      /* Batch for biological coherence */
    config.stdp_window_ms = 50.0F;       /* Longer biological STDP window */
    config.ltp_rate = 0.005F;            /* More conservative LTP */
    config.ltd_rate = 0.0055F;           /* Balanced with LTP */

    config.enable_eligibility = true;
    config.eligibility_tau_ms = 1000.0F; /* Long trace (1s) for credit assignment */

    return config;
}

/* ============================================================================
 * Configuration Validation
 * ============================================================================ */

nimcp_result_t edp_validate_config(const edp_config_t* config)
{
    NIMCP_CHECK_THROW(config != NULL, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(config->stdp_window_ms > 0.0F && config->stdp_window_ms <= 1000.0F,
        NIMCP_ERROR_INVALID_PARAM, "Invalid STDP window: %.2f (must be 0-1000ms)", config->stdp_window_ms);
    NIMCP_CHECK_THROW(config->ltp_rate >= 0.0F && config->ltp_rate <= 1.0F,
        NIMCP_ERROR_INVALID_PARAM, "Invalid LTP rate: %.4f (must be 0-1)", config->ltp_rate);
    NIMCP_CHECK_THROW(config->ltd_rate >= 0.0F && config->ltd_rate <= 1.0F,
        NIMCP_ERROR_INVALID_PARAM, "Invalid LTD rate: %.4f (must be 0-1)", config->ltd_rate);
    NIMCP_CHECK_THROW(config->eligibility_tau_ms > 0.0F,
        NIMCP_ERROR_INVALID_PARAM, "Invalid eligibility tau: %.2f (must be > 0)", config->eligibility_tau_ms);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

edp_context_t* edp_create(const edp_config_t* config)
{
    edp_config_t local_config = config ? *config : edp_config_default();

    if (edp_validate_config(&local_config) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "edp_create: invalid configuration");
        return NULL;
    }

    edp_context_t* ctx = (edp_context_t*)nimcp_calloc(1, sizeof(edp_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "edp_create: failed to allocate EDP context");
        LOG_ERROR("Failed to allocate EDP context");
        return NULL;
    }

    ctx->config = local_config;
    ctx->stdp_window_ns = local_config.stdp_window_ms * 1000000.0F;

    /* Initialize spike buffer */
    if (!spike_buffer_init(&ctx->spike_buffer, EDP_SPIKE_BUFFER_SIZE)) {
        LOG_ERROR("Failed to initialize spike buffer");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "edp_create: spike_buffer_init is NULL");
        return NULL;
    }

    /* Initialize eligibility table */
    if (!eligibility_table_init(&ctx->eligibility, EDP_MAX_ELIGIBILITY_ENTRIES)) {
        LOG_ERROR("Failed to initialize eligibility table");
        spike_buffer_destroy(&ctx->spike_buffer);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "edp_create: eligibility_table_init is NULL");
        return NULL;
    }

    /* Initialize mutexes */
    if (nimcp_platform_mutex_init(&ctx->state_mutex, false) != 0 ||
        nimcp_platform_mutex_init(&ctx->stats_mutex, false) != 0 ||
        nimcp_platform_mutex_init(&ctx->async_mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutexes");
        eligibility_table_destroy(&ctx->eligibility);
        spike_buffer_destroy(&ctx->spike_buffer);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "edp_create: validation failed");
        return NULL;
    }

    if (nimcp_platform_cond_init(&ctx->async_cond) != 0) {
        LOG_ERROR("Failed to initialize condition variable");
        nimcp_platform_mutex_destroy(&ctx->state_mutex);
        nimcp_platform_mutex_destroy(&ctx->stats_mutex);
        nimcp_platform_mutex_destroy(&ctx->async_mutex);
        eligibility_table_destroy(&ctx->eligibility);
        spike_buffer_destroy(&ctx->spike_buffer);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "edp_create: validation failed");
        return NULL;
    }

    ctx->magic = EDP_CONTEXT_MAGIC;
    ctx->memory_mgr = local_config.memory_mgr;
    ctx->active = false;
    ctx->running = false;
    ctx->async_shutdown = false;

    LOG_INFO("Event-Driven Plasticity adapter created (mode=%s, stdp_window=%.1fms)",
             edp_mode_name(local_config.mode), local_config.stdp_window_ms);

    return ctx;
}

nimcp_result_t edp_connect_bridge(edp_context_t* ctx, tpb_context_t* bridge)
{
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_platform_mutex_lock(&ctx->state_mutex);
    ctx->plasticity_bridge = bridge;
    nimcp_platform_mutex_unlock(&ctx->state_mutex);

    if (bridge) {
        LOG_INFO("EDP connected to plasticity bridge");
    } else {
        LOG_INFO("EDP disconnected from plasticity bridge");
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t edp_connect_event_bus(edp_context_t* ctx, event_bus_t bus)
{
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_platform_mutex_lock(&ctx->state_mutex);

    /* Unsubscribe from previous bus if any */
    if (ctx->event_bus && ctx->num_subscriptions > 0) {
        for (uint32_t i = 0; i < ctx->num_subscriptions; i++) {
            if (ctx->subscriptions[i].active) {
                event_bus_unsubscribe(ctx->event_bus, ctx->subscriptions[i].handle);
                ctx->subscriptions[i].active = false;
            }
        }
        ctx->num_subscriptions = 0;
    }

    ctx->event_bus = bus;

    if (bus) {
        /* Subscribe to learning-relevant events individually */
        brain_event_type_t learning_events[] = {
            EVENT_NEURON_SPIKE,               /* Spike events for STDP */
            EVENT_LONG_TERM_POTENTIATION,     /* LTP triggers */
            EVENT_LONG_TERM_DEPRESSION,       /* LTD triggers */
            EVENT_PLASTICITY_UPDATE,          /* Plasticity changes */
            EVENT_WEIGHT_UPDATE,              /* Weight changes */
            EVENT_TRAINING_BATCH_COMPLETE,    /* Batch completion (reward timing) */
            EVENT_ATTENTION_COMPUTED,         /* Attention-gated learning */
            EVENT_ANOMALY_DETECTED            /* Novelty/surprise detection */
        };

        uint32_t num_events = sizeof(learning_events) / sizeof(learning_events[0]);
        uint32_t subscribed = 0;

        for (uint32_t i = 0; i < num_events && subscribed < EDP_MAX_SUBSCRIPTIONS; i++) {
            event_subscription_handle_t handle = event_bus_subscribe(
                bus, learning_events[i], edp_event_callback, ctx);

            if (handle != INVALID_SUBSCRIPTION_HANDLE) {
                ctx->subscriptions[subscribed].handle = handle;
                ctx->subscriptions[subscribed].event_type = learning_events[i];
                ctx->subscriptions[subscribed].active = true;
                subscribed++;
            } else {
                LOG_WARNING("EDP: Failed to subscribe to event type 0x%x", learning_events[i]);
            }
        }

        ctx->num_subscriptions = subscribed;
        LOG_INFO("EDP subscribed to event bus for %u/%u event types", subscribed, num_events);
    }

    nimcp_platform_mutex_unlock(&ctx->state_mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t edp_connect_learning_signals(edp_context_t* ctx, learning_signal_adapter_t adapter)
{
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_platform_mutex_lock(&ctx->state_mutex);
    ctx->learning_adapter = adapter;
    nimcp_platform_mutex_unlock(&ctx->state_mutex);

    LOG_INFO("EDP connected to learning signal adapter");
    return NIMCP_SUCCESS;
}

nimcp_result_t edp_start(edp_context_t* ctx)
{
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_platform_mutex_lock(&ctx->state_mutex);

    if (ctx->running) {
        nimcp_platform_mutex_unlock(&ctx->state_mutex);
        return NIMCP_SUCCESS;  /* Already running */
    }

    ctx->active = true;
    ctx->running = true;

    nimcp_platform_mutex_unlock(&ctx->state_mutex);

    LOG_INFO("Event-Driven Plasticity adapter started");
    return NIMCP_SUCCESS;
}

nimcp_result_t edp_stop(edp_context_t* ctx)
{
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_platform_mutex_lock(&ctx->state_mutex);

    ctx->active = false;
    ctx->running = false;

    nimcp_platform_mutex_unlock(&ctx->state_mutex);

    LOG_INFO("Event-Driven Plasticity adapter stopped");
    return NIMCP_SUCCESS;
}

void edp_destroy(edp_context_t* ctx)
{
    if (!ctx) return;

    /* Stop processing */
    edp_stop(ctx);

    /* Unregister from security if registered */
    if (ctx->security_registered) {
        edp_unregister_security(ctx);
    }

    /* Unsubscribe from event bus */
    if (ctx->event_bus) {
        for (uint32_t i = 0; i < ctx->num_subscriptions; i++) {
            if (ctx->subscriptions[i].active) {
                event_bus_unsubscribe(ctx->event_bus, ctx->subscriptions[i].handle);
            }
        }
    }

    /* Destroy buffers and tables */
    spike_buffer_destroy(&ctx->spike_buffer);
    eligibility_table_destroy(&ctx->eligibility);

    /* Destroy synchronization primitives */
    nimcp_platform_cond_destroy(&ctx->async_cond);
    nimcp_platform_mutex_destroy(&ctx->async_mutex);
    nimcp_platform_mutex_destroy(&ctx->stats_mutex);
    nimcp_platform_mutex_destroy(&ctx->state_mutex);

    ctx->magic = 0;  /* Invalidate before free */
    LOG_INFO("Event-Driven Plasticity adapter destroyed");
    nimcp_free(ctx);
}

/* ============================================================================
 * Event Processing
 * ============================================================================ */

nimcp_result_t edp_process_event(edp_context_t* ctx, const brain_event_t* event)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(event != NULL, NIMCP_ERROR_INVALID_PARAM, "event is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_NOT_INITIALIZED, "EDP context is not active");

    uint64_t start_time = nimcp_time_monotonic_ns();
    nimcp_result_t result = NIMCP_SUCCESS;
    edp_event_category_t category = EDP_CATEGORY_COUNT;

    /* Categorize and process event based on type */
    switch (event->type) {
        case EVENT_NEURON_SPIKE:
            category = EDP_CATEGORY_SPIKE;
            /* Extract spike data from event data payload */
            if (event->data.size >= sizeof(uint32_t) * 2) {
                /* Expect: neuron_id (uint32_t), amplitude (float) */
                const uint32_t* spike_data = (const uint32_t*)event->data.data;
                uint32_t neuron_id = spike_data[0];
                float amplitude = *(const float*)(&spike_data[1]);

                edp_spike_record_t spike = {
                    .neuron_id = neuron_id,
                    .timestamp_ns = event->timestamp_us * 1000,
                    .amplitude = amplitude,
                    .region_id = 0
                };
                spike_buffer_push(&ctx->spike_buffer, &spike);

                /* Process STDP if we have spike pairs */
                process_pending_spikes(ctx);
            }
            break;

        case EVENT_LONG_TERM_POTENTIATION:
            category = EDP_CATEGORY_SPIKE;
            /* LTP event - boost relevant synapses */
            if (ctx->plasticity_bridge) {
                result = edp_process_reward(ctx, 0.5F);  /* Positive reinforcement */
            }
            break;

        case EVENT_LONG_TERM_DEPRESSION:
            category = EDP_CATEGORY_SPIKE;
            /* LTD event - weaken relevant synapses */
            if (ctx->plasticity_bridge) {
                result = edp_process_reward(ctx, -0.3F);  /* Negative adjustment */
            }
            break;

        case EVENT_PLASTICITY_UPDATE:
        case EVENT_WEIGHT_UPDATE:
            category = EDP_CATEGORY_PATTERN;
            /* Weight/plasticity changes - track for eligibility traces */
            break;

        case EVENT_TRAINING_BATCH_COMPLETE:
            category = EDP_CATEGORY_REWARD;
            /* Batch complete - this is a reward signal timing point */
            if (event->data.size >= sizeof(float)) {
                float batch_loss = *(const float*)event->data.data;
                /* Convert loss improvement to reward signal */
                float reward = (batch_loss < ctx->stats.last_batch_loss) ? 0.2F : -0.1F;
                ctx->stats.last_batch_loss = batch_loss;
                result = edp_process_reward(ctx, reward);
            }
            break;

        case EVENT_ATTENTION_COMPUTED:
            category = EDP_CATEGORY_ATTENTION;
            /* Attention modulates learning via acetylcholine */
            if (ctx->plasticity_bridge && event->data.size >= sizeof(float)) {
                float attention = *(const float*)event->data.data;
                /* Boost acetylcholine for attention-gated learning */
                tpb_set_neuromod_levels(ctx->plasticity_bridge, -1.0F, attention, -1.0F, -1.0F);
            }
            break;

        case EVENT_ANOMALY_DETECTED:
            category = EDP_CATEGORY_NOVELTY;
            /* Anomaly = novelty signal → curiosity-driven learning */
            if (event->data.size >= sizeof(float)) {
                float anomaly_score = *(const float*)event->data.data;
                result = edp_process_novelty(ctx, anomaly_score, 0);
            }
            break;

        default:
            /* Unhandled event type - no error, just skip */
            return NIMCP_SUCCESS;
    }

    /* Update statistics */
    uint64_t elapsed = nimcp_time_monotonic_ns() - start_time;

    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->stats.total_events_received++;
    if (result == NIMCP_SUCCESS) {
        ctx->stats.total_events_processed++;
    }
    ctx->stats.total_processing_time_ns += elapsed;

    if (category < EDP_CATEGORY_COUNT) {
        ctx->stats.category_stats[category].events_received++;
        if (result == NIMCP_SUCCESS) {
            ctx->stats.category_stats[category].events_processed++;
        }
        ctx->stats.category_stats[category].processing_time_ns += elapsed;
    }
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return result;
}

nimcp_result_t edp_process_spike_burst(edp_context_t* ctx,
                                        const spike_burst_data_t* burst,
                                        uint32_t region_id)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(burst != NULL, NIMCP_ERROR_INVALID_PARAM, "burst is NULL");
    if (burst->num_neurons == 0) return NIMCP_SUCCESS;  /* Nothing to process */

    uint64_t start_time = nimcp_time_monotonic_ns();
    uint64_t now = start_time;

    /* Add spikes to buffer */
    for (uint32_t i = 0; i < burst->num_neurons; i++) {
        edp_spike_record_t record = {
            .neuron_id = burst->neuron_ids[i],
            .region_id = region_id,
            .timestamp_ns = now,
            .amplitude = burst->synchrony_score,
            .is_presynaptic = (i % 2 == 0)  /* Alternate for pairing */
        };
        spike_buffer_push(&ctx->spike_buffer, &record);
    }

    /* Get recent spikes for STDP pairing */
    uint64_t window_start = now - (uint64_t)ctx->stdp_window_ns;
    edp_spike_record_t recent[256];
    uint32_t num_recent = spike_buffer_get_recent(&ctx->spike_buffer, recent, 256, window_start);

    /* Find spike pairs and compute STDP */
    for (uint32_t i = 0; i < num_recent; i++) {
        if (!recent[i].is_presynaptic) continue;

        for (uint32_t j = 0; j < num_recent; j++) {
            if (i == j || recent[j].is_presynaptic) continue;
            if (recent[i].neuron_id == recent[j].neuron_id) continue;

            /* Found a pre-post pair */
            process_spike_pair(ctx, &recent[i], &recent[j]);
        }
    }

    /* Update statistics */
    uint64_t elapsed = nimcp_time_monotonic_ns() - start_time;
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->stats.total_events_received++;
    ctx->stats.total_events_processed++;
    ctx->stats.total_processing_time_ns += elapsed;
    ctx->stats.category_stats[EDP_CATEGORY_SPIKE].events_received++;
    ctx->stats.category_stats[EDP_CATEGORY_SPIKE].events_processed++;
    ctx->stats.category_stats[EDP_CATEGORY_SPIKE].processing_time_ns += elapsed;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t edp_process_prediction_error(edp_context_t* ctx,
                                             float prediction_error,
                                             uint32_t region_id)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(ctx->plasticity_bridge != NULL, NIMCP_NOT_INITIALIZED,
        "plasticity bridge not connected");

    uint64_t start_time = nimcp_time_monotonic_ns();

    float scaled_error = prediction_error * ctx->config.error_gain;

    /* Report error as negative reward (error = bad) */
    float rpe = 0.0F;
    nimcp_result_t res = tpb_report_loss(ctx->plasticity_bridge, fabsf(scaled_error), &rpe);

    uint64_t elapsed = nimcp_time_monotonic_ns() - start_time;
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->stats.total_events_received++;
    if (res == NIMCP_SUCCESS) {
        ctx->stats.total_events_processed++;
    }
    ctx->stats.total_processing_time_ns += elapsed;
    ctx->stats.avg_prediction_error = (ctx->stats.avg_prediction_error * 0.99F) +
                                      (fabsf(prediction_error) * 0.01F);
    NIMCP_EMA_GUARD_ZERO(ctx->stats.avg_prediction_error);
    ctx->stats.category_stats[EDP_CATEGORY_ERROR].events_received++;
    if (res == NIMCP_SUCCESS) {
        ctx->stats.category_stats[EDP_CATEGORY_ERROR].events_processed++;
    }
    ctx->stats.category_stats[EDP_CATEGORY_ERROR].plasticity_updates++;
    ctx->stats.category_stats[EDP_CATEGORY_ERROR].processing_time_ns += elapsed;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return res;
}

nimcp_result_t edp_process_reward(edp_context_t* ctx, float reward_signal)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(ctx->plasticity_bridge != NULL, NIMCP_NOT_INITIALIZED,
        "plasticity bridge not connected");

    uint64_t start_time = nimcp_time_monotonic_ns();

    float scaled_reward = reward_signal * ctx->config.reward_gain;

    /* Inject reward as dopamine modulation */
    nimcp_result_t res = tpb_inject_reward(ctx->plasticity_bridge, scaled_reward);

    /* Consolidate eligibility traces if enabled */
    if (ctx->config.enable_eligibility && fabsf(scaled_reward) > 0.01F) {
        uint32_t consolidated = edp_consolidate_eligibility(ctx, scaled_reward);
        if (consolidated > 0) {
            LOG_DEBUG("Consolidated %u eligibility traces with reward %.3f",
                     consolidated, scaled_reward);
        }
    }

    uint64_t elapsed = nimcp_time_monotonic_ns() - start_time;
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->stats.total_events_received++;
    if (res == NIMCP_SUCCESS) {
        ctx->stats.total_events_processed++;
    }
    ctx->stats.total_processing_time_ns += elapsed;
    ctx->stats.avg_reward_signal = (ctx->stats.avg_reward_signal * 0.99F) +
                                   (reward_signal * 0.01F);
    NIMCP_EMA_GUARD_ZERO(ctx->stats.avg_reward_signal);
    ctx->stats.cumulative_reward += reward_signal;
    ctx->stats.category_stats[EDP_CATEGORY_REWARD].events_received++;
    if (res == NIMCP_SUCCESS) {
        ctx->stats.category_stats[EDP_CATEGORY_REWARD].events_processed++;
    }
    ctx->stats.category_stats[EDP_CATEGORY_REWARD].plasticity_updates++;
    ctx->stats.category_stats[EDP_CATEGORY_REWARD].processing_time_ns += elapsed;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return res;
}

nimcp_result_t edp_process_novelty(edp_context_t* ctx,
                                    float novelty_score,
                                    uint32_t region_id)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(ctx->plasticity_bridge != NULL, NIMCP_NOT_INITIALIZED,
        "plasticity bridge not connected");

    uint64_t start_time = nimcp_time_monotonic_ns();

    float scaled_novelty = novelty_score * ctx->config.novelty_gain;

    /* Novelty boosts norepinephrine (arousal/attention) */
    float ne_boost = scaled_novelty * 0.5F;
    tpb_set_neuromod_levels(ctx->plasticity_bridge, -1.0F, -1.0F, -1.0F,
                            0.5F + ne_boost);  /* Boost NE */

    /* Mild positive reward for novel stimuli (curiosity) */
    if (scaled_novelty > 0.5F) {
        tpb_inject_reward(ctx->plasticity_bridge, scaled_novelty * 0.2F);
    }

    uint64_t elapsed = nimcp_time_monotonic_ns() - start_time;
    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->stats.total_events_received++;
    ctx->stats.total_events_processed++;
    ctx->stats.total_processing_time_ns += elapsed;
    ctx->stats.category_stats[EDP_CATEGORY_NOVELTY].events_received++;
    ctx->stats.category_stats[EDP_CATEGORY_NOVELTY].events_processed++;
    ctx->stats.category_stats[EDP_CATEGORY_NOVELTY].plasticity_updates++;
    ctx->stats.category_stats[EDP_CATEGORY_NOVELTY].processing_time_ns += elapsed;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Eligibility Trace Operations
 * ============================================================================ */

nimcp_result_t edp_update_eligibility(edp_context_t* ctx, float dt)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    if (!ctx->config.enable_eligibility) return NIMCP_SUCCESS;

    float decay = expf(-dt / ctx->config.eligibility_tau_ms);
    uint64_t now = nimcp_time_monotonic_ns();

    nimcp_platform_rwlock_wrlock(&ctx->eligibility.rwlock);

    uint32_t active_count = 0;
    for (uint32_t i = 0; i < ctx->eligibility.capacity; i++) {
        edp_eligibility_entry_t* entry = &ctx->eligibility.entries[i];
        if (entry->eligibility > ctx->config.eligibility_threshold) {
            entry->eligibility *= decay;
            entry->last_update_ns = now;
            if (entry->eligibility > ctx->config.eligibility_threshold) {
                active_count++;
            }
        }
    }

    nimcp_platform_rwlock_wrunlock(&ctx->eligibility.rwlock);

    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->stats.active_eligibility_traces = active_count;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

uint32_t edp_consolidate_eligibility(edp_context_t* ctx, float reward)
{
    if (!ctx || !ctx->plasticity_bridge) return 0;
    if (!ctx->config.enable_eligibility) return 0;

    uint32_t consolidated = 0;

    nimcp_platform_rwlock_wrlock(&ctx->eligibility.rwlock);

    for (uint32_t i = 0; i < ctx->eligibility.capacity; i++) {
        edp_eligibility_entry_t* entry = &ctx->eligibility.entries[i];

        if (entry->eligibility > ctx->config.eligibility_threshold &&
            fabsf(entry->accumulated_delta) > 0.0001F) {

            /* Three-factor learning: eligibility × reward × accumulated_delta */
            float weight_change = entry->eligibility * reward * entry->accumulated_delta;

            /* Apply through plasticity bridge */
            float actual_delta = 0.0F;
            nimcp_result_t res = tpb_route_weight_update(
                ctx->plasticity_bridge,
                entry->post_neuron,
                1.0F,  /* Pre activity placeholder */
                1.0F,  /* Post activity placeholder */
                0.0F,  /* Timing handled by accumulated_delta */
                &actual_delta
            );

            if (res == NIMCP_SUCCESS) {
                consolidated++;
                entry->accumulated_delta = 0.0F;
                entry->eligibility *= 0.5F;  /* Partial decay on consolidation */
            }
        }
    }

    nimcp_platform_rwlock_wrunlock(&ctx->eligibility.rwlock);

    if (consolidated > 0) {
        nimcp_platform_mutex_lock(&ctx->stats_mutex);
        ctx->stats.eligibility_consolidations += consolidated;
        ctx->stats.total_plasticity_updates += consolidated;
        nimcp_platform_mutex_unlock(&ctx->stats_mutex);
    }

    return consolidated;
}

nimcp_result_t edp_clear_eligibility(edp_context_t* ctx)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    nimcp_platform_rwlock_wrlock(&ctx->eligibility.rwlock);

    for (uint32_t i = 0; i < ctx->eligibility.capacity; i++) {
        ctx->eligibility.entries[i].eligibility = 0.0F;
        ctx->eligibility.entries[i].accumulated_delta = 0.0F;
    }
    ctx->eligibility.count = 0;

    nimcp_platform_rwlock_wrunlock(&ctx->eligibility.rwlock);

    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    ctx->stats.active_eligibility_traces = 0;
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

nimcp_result_t edp_get_stats(const edp_context_t* ctx, edp_stats_t* stats)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats != NULL, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&ctx->stats_mutex);
    *stats = ctx->stats;

    /* Compute derived metrics */
    if (stats->total_processing_time_ns > 0) {
        float total_time_sec = (float)stats->total_processing_time_ns / 1e9F;
        stats->events_per_second = (float)stats->total_events_processed / total_time_sec;
        stats->avg_latency_us = (float)stats->total_processing_time_ns /
                                (float)(stats->total_events_processed + 1) / 1000.0F;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t edp_reset_stats(edp_context_t* ctx)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    nimcp_platform_mutex_lock(&ctx->stats_mutex);
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    nimcp_platform_mutex_unlock(&ctx->stats_mutex);

    return NIMCP_SUCCESS;
}

void edp_print_status(const edp_context_t* ctx)
{
    if (!ctx) return;

    edp_stats_t stats;
    edp_get_stats(ctx, &stats);

    LOG_INFO("=== Event-Driven Plasticity Status ===");
    LOG_INFO("Active: %s, Mode: %s",
             ctx->active ? "Yes" : "No",
             edp_mode_name(ctx->config.mode));
    LOG_INFO("Events: received=%lu, processed=%lu, dropped=%lu",
             (unsigned long)stats.total_events_received,
             (unsigned long)stats.total_events_processed,
             (unsigned long)stats.dropped_events);
    LOG_INFO("Performance: %.1f events/sec, avg latency=%.2f us",
             stats.events_per_second, stats.avg_latency_us);
    LOG_INFO("STDP: pairs=%lu, LTP=%lu, LTD=%lu",
             (unsigned long)stats.spike_pairs_evaluated,
             (unsigned long)stats.ltp_events,
             (unsigned long)stats.ltd_events);
    LOG_INFO("Eligibility: active=%u, consolidations=%lu",
             stats.active_eligibility_traces,
             (unsigned long)stats.eligibility_consolidations);
    LOG_INFO("Signals: avg_error=%.4f, avg_reward=%.4f, cumulative=%.2f",
             stats.avg_prediction_error, stats.avg_reward_signal, stats.cumulative_reward);
}

bool edp_is_active(const edp_context_t* ctx)
{
    if (!ctx || ctx->magic != EDP_CONTEXT_MAGIC) {
        return false;
    }
    return ctx->active && ctx->running;
}

/* ============================================================================
 * Security Integration
 * ============================================================================ */

nimcp_result_t edp_register_security(edp_context_t* ctx, nimcp_sec_integration_t* security_ctx)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(security_ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "security_ctx is NULL");
    if (ctx->security_registered) return NIMCP_SUCCESS;

    /* Register using the simple API */
    nimcp_result_t res = nimcp_sec_register_module(
        security_ctx,
        EDP_SECURITY_MODULE_NAME,
        NIMCP_SEC_CAT_PLASTICITY,
        &ctx->security_module_id
    );

    if (res == NIMCP_SUCCESS) {
        ctx->security_ctx = security_ctx;
        ctx->security_registered = true;
        LOG_INFO("EDP registered with security system (module_id=%u)", ctx->security_module_id);
    } else {
        LOG_WARNING("Failed to register EDP with security system: %d", res);
    }

    return res;
}

nimcp_result_t edp_unregister_security(edp_context_t* ctx)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    if (!ctx->security_registered) return NIMCP_SUCCESS;

    if (ctx->security_ctx) {
        nimcp_sec_unregister_module(ctx->security_ctx, ctx->security_module_id);
    }

    ctx->security_ctx = NULL;
    ctx->security_module_id = 0;
    ctx->security_registered = false;

    LOG_INFO("EDP unregistered from security system");
    return NIMCP_SUCCESS;
}
