/**
 * @file nimcp_exception_trace.c
 * @brief Distributed tracing and cross-module exception propagation implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Implementation of exception correlation IDs and KG-aware routing
 * WHY:  Enable distributed debugging and intelligent exception routing
 * HOW:  Atomic ID generation, thread-local context stacks, W3C format parsing
 *
 * THREAD SAFETY:
 * - ID generation uses atomic operations
 * - Trace stacks are thread-local (no sharing)
 * - Global state protected by mutex
 *
 * PERFORMANCE:
 * - ID generation: ~10ns (atomic increment)
 * - Trace create: ~50ns (timestamp + ID gen)
 * - Header format: ~200ns (hex conversion)
 *
 * @author NIMCP Development Team
 */

#include "utils/exception/nimcp_exception_trace.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(exception_trace)

/* ============================================================================
 * Module State
 * ============================================================================ */

static bool g_trace_system_initialized = false;
static nimcp_platform_mutex_t* g_trace_mutex = NULL;
static uint32_t g_node_id = 0;
static volatile uint64_t g_trace_counter = 0;
static volatile uint64_t g_span_counter = 0;

/* Thread-local trace context stack */
typedef struct {
    nimcp_exception_trace_t traces[NIMCP_MAX_TRACE_STACK];
    size_t depth;
} trace_stack_t;

static _Thread_local trace_stack_t tl_trace_stack = { .depth = 0 };

/* Extended exception data for trace/propagation attachment */
typedef struct {
    nimcp_exception_trace_t trace;
    bool has_trace;
    nimcp_propagation_context_t* propagation;
} exception_trace_data_t;

/* We store extended data in a simple hash table keyed by exception pointer */
#define TRACE_DATA_BUCKETS 256
static exception_trace_data_t* g_trace_data[TRACE_DATA_BUCKETS] = {0};
static nimcp_exception_t* g_trace_data_keys[TRACE_DATA_BUCKETS] = {0};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Hash function for exception pointer
 */
static size_t hash_exception_ptr(const nimcp_exception_t* ex) {
    uintptr_t ptr = (uintptr_t)ex;
    /* Simple multiplicative hash */
    return (size_t)((ptr * 2654435761UL) % TRACE_DATA_BUCKETS);
}

/**
 * @brief Find or create trace data slot for exception
 */
static exception_trace_data_t* get_or_create_trace_data(nimcp_exception_t* ex) {
    if (!ex || !g_trace_system_initialized) return NULL;

    size_t bucket = hash_exception_ptr(ex);
    size_t original_bucket = bucket;

    nimcp_platform_mutex_lock(g_trace_mutex);

    /* Linear probing to find slot */
    do {
        if (g_trace_data_keys[bucket] == ex) {
            /* Found existing */
            nimcp_platform_mutex_unlock(g_trace_mutex);
            return g_trace_data[bucket];
        }
        if (g_trace_data_keys[bucket] == NULL) {
            /* Found empty slot */
            g_trace_data[bucket] = nimcp_calloc(1, sizeof(exception_trace_data_t));
            if (!g_trace_data[bucket]) {
                nimcp_platform_mutex_unlock(g_trace_mutex);
                return NULL;
            }
            g_trace_data_keys[bucket] = ex;
            nimcp_platform_mutex_unlock(g_trace_mutex);
            return g_trace_data[bucket];
        }
        bucket = (bucket + 1) % TRACE_DATA_BUCKETS;
    } while (bucket != original_bucket);

    nimcp_platform_mutex_unlock(g_trace_mutex);
    return NULL; /* Table full */
}

/**
 * @brief Find trace data for exception (read-only)
 *
 * P2-U31 WARNING: This function returns a pointer to internal data AFTER
 * releasing the mutex. The returned pointer is stable (heap-allocated and
 * only freed during nimcp_trace_shutdown), but the DATA it points to could
 * be modified concurrently by another thread calling get_or_create_trace_data.
 * Callers should treat the returned data as a best-effort snapshot.
 * A copy-based API would fix this but would break the public interface.
 */
static const exception_trace_data_t* find_trace_data(const nimcp_exception_t* ex) {
    if (!ex || !g_trace_system_initialized) return NULL;

    size_t bucket = hash_exception_ptr(ex);
    size_t original_bucket = bucket;

    nimcp_platform_mutex_lock(g_trace_mutex);

    do {
        if (g_trace_data_keys[bucket] == ex) {
            nimcp_platform_mutex_unlock(g_trace_mutex);
            return g_trace_data[bucket];
        }
        if (g_trace_data_keys[bucket] == NULL) {
            break;
        }
        bucket = (bucket + 1) % TRACE_DATA_BUCKETS;
    } while (bucket != original_bucket);

    nimcp_platform_mutex_unlock(g_trace_mutex);
    return NULL;
}

/**
 * @brief Convert hex character to nibble value
 */
static int hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief Parse hex string to uint64
 */
static int parse_hex64(const char* str, size_t len, uint64_t* out) {
    if (!str || !out || len == 0 || len > 16) return -1;

    *out = 0;
    for (size_t i = 0; i < len; i++) {
        int nibble = hex_to_nibble(str[i]);
        if (nibble < 0) return -1;
        *out = (*out << 4) | (uint64_t)nibble;
    }
    return 0;
}

/**
 * @brief Format uint64 as hex string
 */
static size_t format_hex64(uint64_t val, char* buffer, size_t len) {
    static const char hex_chars[] = "0123456789abcdef";
    if (!buffer || len < 16) return 0;

    for (int i = 15; i >= 0; i--) {
        buffer[i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    return 16;
}

/* ============================================================================
 * Trace System Lifecycle
 * ============================================================================ */

int nimcp_trace_init(void) {
    if (g_trace_system_initialized) return 0;

    g_trace_mutex = nimcp_platform_mutex_create();
    if (!g_trace_mutex) {
        LOG_ERROR("Failed to create trace mutex");
        return -1;
    }

    /* Initialize counters with timestamp-based seed */
    uint64_t seed = get_timestamp_us();
    __atomic_store_n(&g_trace_counter, seed, __ATOMIC_SEQ_CST);
    __atomic_store_n(&g_span_counter, seed >> 16, __ATOMIC_SEQ_CST);

    /* Clear trace data table */
    memset(g_trace_data, 0, sizeof(g_trace_data));
    memset(g_trace_data_keys, 0, sizeof(g_trace_data_keys));

    g_trace_system_initialized = true;
    LOG_INFO("Exception trace system initialized (node_id=%u)", g_node_id);
    return 0;
}

void nimcp_trace_shutdown(void) {
    if (!g_trace_system_initialized) return;

    /* Free all trace data entries */
    for (size_t i = 0; i < TRACE_DATA_BUCKETS; i++) {
        if (g_trace_data[i]) {
            if (g_trace_data[i]->propagation) {
                nimcp_propagation_destroy(g_trace_data[i]->propagation);
            }
            nimcp_free(g_trace_data[i]);
            g_trace_data[i] = NULL;
            g_trace_data_keys[i] = NULL;
        }
    }

    if (g_trace_mutex) {
        nimcp_platform_mutex_destroy(g_trace_mutex);
        nimcp_free(g_trace_mutex);
        g_trace_mutex = NULL;
    }

    g_trace_system_initialized = false;
    LOG_INFO("Exception trace system shutdown");
}

/* ============================================================================
 * Trace Creation
 * ============================================================================ */

uint64_t nimcp_trace_generate_id(void) {
    uint64_t counter = __atomic_fetch_add(&g_trace_counter, 1, __ATOMIC_SEQ_CST);
    uint64_t timestamp = get_timestamp_us();

    /* Combine: node_id (16 bits) | timestamp (32 bits) | counter (16 bits) */
    uint64_t id = ((uint64_t)g_node_id << 48) |
                  ((timestamp & 0xFFFFFFFFULL) << 16) |
                  (counter & 0xFFFFULL);
    return id;
}

nimcp_exception_trace_t nimcp_trace_create(void) {
    nimcp_exception_trace_t trace = {0};

    trace.trace_id = nimcp_trace_generate_id();
    trace.span_id = __atomic_fetch_add(&g_span_counter, 1, __ATOMIC_SEQ_CST);
    trace.parent_span_id = 0; /* Root span */
    trace.node_id = g_node_id;
    trace.start_time_us = get_timestamp_us();
    trace.trace_flags = NIMCP_TRACE_FLAG_SAMPLED;

    return trace;
}

nimcp_exception_trace_t nimcp_trace_create_child(const nimcp_exception_trace_t* parent) {
    nimcp_exception_trace_t trace = {0};

    if (parent) {
        trace.trace_id = parent->trace_id;
        trace.parent_span_id = parent->span_id;
        trace.trace_flags = parent->trace_flags;
    } else {
        trace.trace_id = nimcp_trace_generate_id();
        trace.parent_span_id = 0;
        trace.trace_flags = NIMCP_TRACE_FLAG_SAMPLED;
    }

    trace.span_id = __atomic_fetch_add(&g_span_counter, 1, __ATOMIC_SEQ_CST);
    trace.node_id = g_node_id;
    trace.start_time_us = get_timestamp_us();

    return trace;
}

void nimcp_trace_set_node_id(uint32_t node_id) {
    g_node_id = node_id;
}

uint32_t nimcp_trace_get_node_id(void) {
    return g_node_id;
}

/* ============================================================================
 * Exception-Trace Association
 * ============================================================================ */

int nimcp_exception_set_trace(nimcp_exception_t* ex, const nimcp_exception_trace_t* trace) {
    if (!ex || !trace) return -1;

    exception_trace_data_t* data = get_or_create_trace_data(ex);
    if (!data) return -1;

    data->trace = *trace;
    data->has_trace = true;
    return 0;
}

const nimcp_exception_trace_t* nimcp_exception_get_trace(const nimcp_exception_t* ex) {
    const exception_trace_data_t* data = find_trace_data(ex);
    if (!data || !data->has_trace) return NULL;
    return &data->trace;
}

/* ============================================================================
 * Thread-Local Trace Stack
 * ============================================================================ */

int nimcp_trace_push(const nimcp_exception_trace_t* trace) {
    if (!trace) return -1;
    if (tl_trace_stack.depth >= NIMCP_MAX_TRACE_STACK) {
        LOG_WARNING("Trace stack overflow (depth=%zu)", tl_trace_stack.depth);
        return -1;
    }

    tl_trace_stack.traces[tl_trace_stack.depth] = *trace;
    tl_trace_stack.depth++;
    return 0;
}

nimcp_exception_trace_t* nimcp_trace_current(void) {
    if (tl_trace_stack.depth == 0) return NULL;
    return &tl_trace_stack.traces[tl_trace_stack.depth - 1];
}

void nimcp_trace_pop(void) {
    if (tl_trace_stack.depth > 0) {
        tl_trace_stack.depth--;
    }
}

/* ============================================================================
 * Propagation Context
 * ============================================================================ */

nimcp_propagation_context_t* nimcp_propagation_create(const char* origin_module) {
    nimcp_propagation_context_t* ctx = nimcp_calloc(1, sizeof(nimcp_propagation_context_t));
    if (!ctx) {
        return NULL;
    }

    ctx->origin_module = origin_module;
    ctx->target_module = NULL;
    ctx->path_length = 0;
    ctx->requires_coordination = false;

    return ctx;
}

void nimcp_propagation_destroy(nimcp_propagation_context_t* ctx) {
    if (!ctx) return;
    /* Note: module_name and message_type are not owned, so we don't free them */
    nimcp_free(ctx);
}

int nimcp_propagation_add_hop(
    nimcp_propagation_context_t* ctx,
    const char* module,
    const char* msg_type,
    uint32_t priority
) {
    if (!ctx || !module) return -1;
    if (ctx->path_length >= NIMCP_MAX_PROPAGATION_PATH) {
        LOG_WARNING("Propagation path full (length=%zu)", ctx->path_length);
        return -1;
    }

    nimcp_propagation_entry_t* entry = &ctx->path[ctx->path_length];
    entry->module_name = module;
    entry->message_type = msg_type;
    entry->timestamp_us = get_timestamp_us();
    entry->priority = priority;

    ctx->path_length++;

    /* High-priority messages (>=8) or multiple modules may need coordination */
    if (priority >= 8 || ctx->path_length >= 3) {
        ctx->requires_coordination = true;
    }

    return 0;
}

int nimcp_propagation_set_target(
    nimcp_propagation_context_t* ctx,
    const char* target
) {
    if (!ctx) return -1;
    ctx->target_module = target;
    return 0;
}

int nimcp_exception_set_propagation(
    nimcp_exception_t* ex,
    nimcp_propagation_context_t* ctx
) {
    if (!ex) return -1;

    exception_trace_data_t* data = get_or_create_trace_data(ex);
    if (!data) {
        nimcp_propagation_destroy(ctx);
        return -1;
    }

    /* Free existing propagation if present */
    if (data->propagation) {
        nimcp_propagation_destroy(data->propagation);
    }

    data->propagation = ctx; /* Takes ownership */
    return 0;
}

const nimcp_propagation_context_t* nimcp_exception_get_propagation(
    const nimcp_exception_t* ex
) {
    const exception_trace_data_t* data = find_trace_data(ex);
    return data ? data->propagation : NULL;
}

bool nimcp_propagation_requires_coordination(
    const nimcp_propagation_context_t* ctx
) {
    return ctx ? ctx->requires_coordination : false;
}

/* ============================================================================
 * W3C Trace-Context Format
 * ============================================================================ */

size_t nimcp_trace_to_header(
    const nimcp_exception_trace_t* trace,
    char* buffer,
    size_t size
) {
    if (!trace || !buffer || size < 56) return 0;

    /*
     * W3C traceparent format:
     * version (2) + "-" + trace_id (32) + "-" + span_id (16) + "-" + flags (2)
     * Total: 55 chars + null = 56 bytes minimum
     *
     * We use 64-bit trace_id, so we'll pad with zeros on the left to make 32 hex chars
     */

    char* p = buffer;

    /* Version: 00 */
    *p++ = '0';
    *p++ = '0';
    *p++ = '-';

    /* Trace ID: 32 hex chars (we have 64-bit, so 16 hex + 16 zeros padding) */
    memset(p, '0', 16); /* Leading zeros */
    p += 16;
    format_hex64(trace->trace_id, p, 16);
    p += 16;
    *p++ = '-';

    /* Span ID: 16 hex chars */
    format_hex64(trace->span_id, p, 16);
    p += 16;
    *p++ = '-';

    /* Flags: 2 hex chars */
    static const char hex_chars[] = "0123456789abcdef";
    *p++ = hex_chars[(trace->trace_flags >> 4) & 0x0F];
    *p++ = hex_chars[trace->trace_flags & 0x0F];
    *p = '\0';

    return (size_t)(p - buffer);
}

int nimcp_trace_from_header(
    const char* header,
    nimcp_exception_trace_t* trace
) {
    if (!header || !trace) return -1;

    size_t len = strlen(header);
    if (len < 55) return -1; /* Minimum valid length */

    memset(trace, 0, sizeof(nimcp_exception_trace_t));

    /* Parse version (should be "00") */
    if (header[0] != '0' || header[1] != '0' || header[2] != '-') {
        return -1;
    }

    /* Parse trace_id (32 hex chars, we use last 16 for our 64-bit ID) */
    uint64_t trace_id_high = 0, trace_id_low = 0;
    if (parse_hex64(header + 3, 16, &trace_id_high) < 0) return -1;
    if (parse_hex64(header + 19, 16, &trace_id_low) < 0) return -1;
    trace->trace_id = trace_id_low; /* Use low 64 bits */
    (void)trace_id_high; /* Ignore high bits for now */

    if (header[35] != '-') return -1;

    /* Parse span_id (16 hex chars) */
    if (parse_hex64(header + 36, 16, &trace->span_id) < 0) return -1;

    if (header[52] != '-') return -1;

    /* Parse flags (2 hex chars) */
    uint64_t flags = 0;
    if (parse_hex64(header + 53, 2, &flags) < 0) return -1;
    trace->trace_flags = (uint8_t)flags;

    /* Set defaults for fields not in header */
    trace->parent_span_id = 0;
    trace->node_id = g_node_id;
    trace->start_time_us = get_timestamp_us();

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

size_t nimcp_trace_to_string(
    const nimcp_exception_trace_t* trace,
    char* buffer,
    size_t size
) {
    if (!trace || !buffer || size == 0) return 0;

    int written = snprintf(buffer, size,
        "Trace{id=%016llx, span=%016llx, parent=%016llx, node=%u, flags=0x%02x}",
        (unsigned long long)trace->trace_id,
        (unsigned long long)trace->span_id,
        (unsigned long long)trace->parent_span_id,
        trace->node_id,
        trace->trace_flags
    );

    return (size_t)(written > 0 ? written : 0);
}

size_t nimcp_propagation_to_string(
    const nimcp_propagation_context_t* ctx,
    char* buffer,
    size_t size
) {
    if (!ctx || !buffer || size == 0) return 0;

    size_t offset = 0;
    int written = snprintf(buffer + offset, size - offset,
        "Propagation{\n  origin: %s\n  target: %s\n  requires_coordination: %s\n  path (%zu hops):\n",
        ctx->origin_module ? ctx->origin_module : "(null)",
        ctx->target_module ? ctx->target_module : "(null)",
        ctx->requires_coordination ? "yes" : "no",
        ctx->path_length
    );
    if (written > 0) offset += (size_t)written;

    for (size_t i = 0; i < ctx->path_length && offset < size - 1; i++) {
        const nimcp_propagation_entry_t* entry = &ctx->path[i];
        written = snprintf(buffer + offset, size - offset,
            "    [%zu] %s via %s (priority=%u)\n",
            i,
            entry->module_name ? entry->module_name : "(null)",
            entry->message_type ? entry->message_type : "(null)",
            entry->priority
        );
        if (written > 0) offset += (size_t)written;
    }

    if (offset < size - 1) {
        buffer[offset++] = '}';
        buffer[offset] = '\0';
    }

    return offset;
}
