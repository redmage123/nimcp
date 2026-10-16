#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_circular_buffer.c - Lock-Free Circular Buffer Implementation
//=============================================================================

#include "middleware/buffering/nimcp_circular_buffer.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_circular_buffer"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(circular_buffer)

#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sched.h>  /* For sched_yield() */

// Cache line size for alignment (typical x86-64)
#define CACHE_LINE_SIZE 64

// Align pointer to cache line boundary
#define ALIGN_TO_CACHE_LINE(ptr) \
    ((void*)(((uintptr_t)(ptr) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1)))

/**
 * @brief Circular buffer structure
 *
 * WHAT: Internal representation with lock-free indices
 * WHY:  Enable concurrent single-producer single-consumer access
 * HOW:  Atomic read/write indices, cache-aligned to prevent false sharing
 */
struct circular_buffer {
    // Configuration
    size_t element_size;           /**< Size of each element in bytes */
    size_t capacity;               /**< Maximum number of elements */
    overflow_strategy_t strategy;  /**< Overflow handling */

    // Cache-aligned atomic indices (prevent false sharing)
    _Alignas(CACHE_LINE_SIZE) atomic_size_t write_pos;  /**< Write position */
    _Alignas(CACHE_LINE_SIZE) atomic_size_t read_pos;   /**< Read position */

    // Statistics (non-atomic version for API compatibility)
    // P2 note: avg_usage and peak_usage in stats are approximate (non-atomic).
    // This is a deliberate design trade-off: these values are informational
    // and don't need exact accuracy. The atomic counters below are the
    // authoritative counters for total_writes/reads/overflows/underflows.
    _Alignas(CACHE_LINE_SIZE) circular_buffer_stats_t stats;

    // THREAD SAFETY FIX: Atomic statistics counters for lock-free updates
    _Alignas(CACHE_LINE_SIZE) atomic_size_t atomic_total_writes;
    atomic_size_t atomic_total_reads;
    atomic_size_t atomic_overflows;
    atomic_size_t atomic_underflows;
    atomic_size_t atomic_peak_usage;

    // Data storage (allocated separately for alignment)
    void* data;  /**< Element storage */

    // Positional encoding support
    nimcp_pos_encoder_t* pe_encoder;  /**< Positional encoder instance (optional) */
};

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Update average utilization
 *
 * WHAT: Incremental average calculation
 * WHY:  Track buffer usage over time without storing all samples
 * HOW:  Running average: new_avg = old_avg + (sample - old_avg) / count
 */
static void update_avg_utilization(circular_buffer_t* buf, size_t current_size) {
    if (!buf) return;

    size_t total_ops = atomic_load(&buf->atomic_total_writes) + atomic_load(&buf->atomic_total_reads);
    if (total_ops == 0) {
        buf->stats.avg_usage = 0.0F;
        return;
    }

    float current_pct = (buf->capacity > 0) ?
        (100.0F * current_size / buf->capacity) : 0.0F;

    // Incremental average
    buf->stats.avg_usage = buf->stats.avg_usage +
        (current_pct - buf->stats.avg_usage) / total_ops;
}

/**
 * @brief Update peak utilization
 *
 * WHAT: Track maximum buffer occupancy
 * WHY:  Identify if buffer size is adequate
 * HOW:  Update peak if current size exceeds recorded peak
 */
static void update_peak_utilization(circular_buffer_t* buf, size_t current_size) {
    if (!buf) return;
    if (current_size > buf->stats.peak_usage) {
        buf->stats.peak_usage = current_size;
    }
}

//=============================================================================
// LIFECYCLE
//=============================================================================

circular_buffer_t* circular_buffer_create(
    size_t element_size,
    size_t capacity,
    overflow_strategy_t strategy
) {
    // Guard: validate inputs
    if (element_size == 0 || capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circular_buffer_create: element_size is zero");
        return NULL;
    }

    // Guard: check for integer overflow in data_size calculation
    // We need to compute element_size * (capacity + 1) safely
    // First check capacity + 1 won't overflow
    if (capacity > SIZE_MAX - 1) {
        LOG_ERROR("circular_buffer_create: capacity overflow (capacity=%zu)", capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "circular_buffer_create: validation failed");
        return NULL;
    }

    size_t adjusted_capacity = capacity + 1;  // +1 for full/empty distinction

    // Check that element_size * adjusted_capacity won't overflow
    // Safe check: if element_size > SIZE_MAX / adjusted_capacity, overflow would occur
    if (element_size > SIZE_MAX / adjusted_capacity) {
        LOG_ERROR("circular_buffer_create: data_size overflow (element_size=%zu, capacity=%zu)",
                  element_size, capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "circular_buffer_create: validation failed");
        return NULL;
    }

    size_t data_size = element_size * adjusted_capacity;

    // Allocate control structure (cache-aligned due to _Alignas members)
    circular_buffer_t* buf = nimcp_aligned_alloc(CACHE_LINE_SIZE, sizeof(circular_buffer_t));
    if (!buf) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buf is NULL");

        return NULL;

    }

    // Zero-initialize the structure
    memset(buf, 0, sizeof(circular_buffer_t));

    // Round up to cache line boundary for nimcp_aligned_alloc
    // Check for overflow when adding CACHE_LINE_SIZE - 1
    if (data_size > SIZE_MAX - (CACHE_LINE_SIZE - 1)) {
        LOG_ERROR("circular_buffer_create: aligned_size overflow");
        nimcp_free(buf);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "circular_buffer_create: validation failed");
        return NULL;
    }
    size_t aligned_size = ((data_size + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;

    buf->data = nimcp_aligned_alloc(CACHE_LINE_SIZE, aligned_size);
    if (!buf->data) {
        nimcp_free(buf);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "circular_buffer_create: buf->data is NULL");
        return NULL;
    }

    // Initialize configuration
    buf->element_size = element_size;
    buf->capacity = capacity + 1;  // Reserve one slot for full/empty distinction
    buf->strategy = strategy;

    // Initialize atomic indices
    atomic_store(&buf->write_pos, 0);
    atomic_store(&buf->read_pos, 0);

    // Initialize statistics
    memset(&buf->stats, 0, sizeof(circular_buffer_stats_t));

    // THREAD SAFETY FIX: Initialize atomic statistics counters
    atomic_init(&buf->atomic_total_writes, 0);
    atomic_init(&buf->atomic_total_reads, 0);
    atomic_init(&buf->atomic_overflows, 0);
    atomic_init(&buf->atomic_underflows, 0);
    atomic_init(&buf->atomic_peak_usage, 0);

    // Initialize PE encoder as NULL (not configured by default)
    buf->pe_encoder = NULL;

    return buf;
}

void circular_buffer_destroy(circular_buffer_t* buffer) {
    if (!buffer) return;

    // Free aligned data storage
    if (buffer->data) {
        nimcp_aligned_free(buffer->data);
    }

    // Free aligned buffer structure
    nimcp_aligned_free(buffer);
}

//=============================================================================
// CORE OPERATIONS
//=============================================================================

bool circular_buffer_push(circular_buffer_t* buffer, const void* element) {
    // Guard: validate inputs
    if (!buffer || !element) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_push: required parameter is NULL (buffer, element)");
        return false;
    }

    // Load current positions
    size_t write = atomic_load(&buffer->write_pos);
    size_t read = atomic_load(&buffer->read_pos);

    // Calculate next write position
    size_t next_write = (write + 1) % buffer->capacity;

    // Check if buffer full
    if (next_write == read) {
        atomic_fetch_add(&buffer->atomic_overflows, 1);  // THREAD SAFETY FIX

        switch (buffer->strategy) {
            case OVERFLOW_OVERWRITE:
                // Advance read pointer to overwrite oldest
                atomic_store(&buffer->read_pos, (read + 1) % buffer->capacity);
                break;

            case OVERFLOW_BLOCK: {
                // WHAT: Wait for space in buffer using spin-wait with yield
                // WHY:  Blocking strategy requires waiting for consumer to free space
                // HOW:  Busy-wait with sched_yield() to reduce CPU waste
                // NOTE: This is inefficient compared to condition variables, but maintains
                //       lock-free design. Consider using semaphores for production use.
                // P1 fix: Add timeout to prevent infinite blocking if consumer dies
                uint32_t spin_count = 0;
                const uint32_t max_spins = 1000000;  /* ~1 second at 1us per spin */
                while (next_write == atomic_load(&buffer->read_pos)) {
                    sched_yield();  // Yield CPU to allow consumer to run
                    spin_count++;
                    if (spin_count >= max_spins) {
                        atomic_fetch_add(&buffer->atomic_overflows, 1);  // THREAD SAFETY FIX
                        LOG_DEBUG("circular_buffer_push: OVERFLOW_BLOCK timeout after %u spins", max_spins);
                        return false;  /* Timeout - prevent infinite loop */
                    }
                }
                break;
            }

            case OVERFLOW_ERROR:
                // Report error — buffer full is an expected condition under OVERFLOW_ERROR strategy
                LOG_DEBUG("circular_buffer_push: buffer full (OVERFLOW_ERROR strategy)");
                return false;
        }
    }

    // Copy element to buffer
    void* dest = (char*)buffer->data + (write * buffer->element_size);
    memcpy(dest, element, buffer->element_size);

    // Advance write pointer (release semantics for synchronization)
    atomic_store(&buffer->write_pos, next_write);

    // Update statistics atomically (THREAD SAFETY FIX)
    atomic_fetch_add(&buffer->atomic_total_writes, 1);
    size_t current_size = circular_buffer_size(buffer);
    // Update peak usage atomically using compare-and-swap
    size_t current_peak = atomic_load(&buffer->atomic_peak_usage);
    while (current_size > current_peak) {
        if (atomic_compare_exchange_weak(&buffer->atomic_peak_usage, &current_peak, current_size)) {
            break;
        }
    }

    // Update average utilization tracking
    update_avg_utilization(buffer, current_size);

    return true;
}

bool circular_buffer_pop(circular_buffer_t* buffer, void* element) {
    // Guard: validate inputs
    if (!buffer || !element) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_pop: required parameter is NULL (buffer, element)");
        return false;
    }

    // Load current positions
    size_t read = atomic_load(&buffer->read_pos);
    size_t write = atomic_load(&buffer->write_pos);

    // Check if buffer empty
    if (read == write) {
        atomic_fetch_add(&buffer->atomic_underflows, 1);  // THREAD SAFETY FIX
        /* P2-MW-01 fix: Removed false positive NIMCP_THROW_TO_IMMUNE.
         * Popping an empty buffer is normal behavior (e.g., consumer drains faster
         * than producer). The caller checks the return value; the atomic underflow
         * counter tracks occurrences for monitoring. */
        return false;
    }

    // Copy element from buffer
    const void* src = (const char*)buffer->data + (read * buffer->element_size);
    memcpy(element, src, buffer->element_size);

    // Advance read pointer
    atomic_store(&buffer->read_pos, (read + 1) % buffer->capacity);

    // Update statistics atomically (THREAD SAFETY FIX)
    atomic_fetch_add(&buffer->atomic_total_reads, 1);

    return true;
}

/* NOTE: peek is subject to TOCTOU race in MPSC/MPMC scenarios.
 * The data may change between size check and actual read.
 * This is acceptable for SPSC use but callers using MPMC
 * should use external synchronization. */
bool circular_buffer_peek(
    const circular_buffer_t* buffer,
    size_t offset,
    void* element
) {
    // Guard: validate inputs
    if (!buffer || !element) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_peek: required parameter is NULL (buffer, element)");
        return false;
    }

    // Load current positions
    size_t read = atomic_load(&buffer->read_pos);
    size_t write = atomic_load(&buffer->write_pos);

    // Calculate current size
    size_t size = (write >= read) ?
        (write - read) : (buffer->capacity - read + write);

    // Check if offset valid
    if (offset >= size) {
        /* P2-MW-02 fix: Removed false positive NIMCP_THROW_TO_IMMUNE.
         * Peeking beyond current buffer size is a normal boundary check failure,
         * not an immune-level error. Caller checks the return value. */
        return false;
    }

    // Calculate peek position
    size_t peek_pos = (read + offset) % buffer->capacity;

    // Copy element
    const void* src = (const char*)buffer->data + (peek_pos * buffer->element_size);
    memcpy(element, src, buffer->element_size);

    return true;
}

//=============================================================================
// BATCH OPERATIONS
//=============================================================================

size_t circular_buffer_push_batch(
    circular_buffer_t* buffer,
    const void* elements,
    size_t count
) {
    // Guard: validate inputs
    if (!buffer || !elements || count == 0) return 0;

    size_t written = 0;
    const char* src = (const char*)elements;

    // Write elements one by one (could optimize with memcpy for contiguous space)
    for (size_t i = 0; i < count; i++) {
        if (!circular_buffer_push(buffer, src + (i * buffer->element_size))) {
            // Stop on overflow if strategy is ERROR
            if (buffer->strategy == OVERFLOW_ERROR) {
                break;
            }
            // For other strategies (OVERWRITE, BLOCK), push may fail (e.g.,
            // BLOCK timeout) - do NOT count as written
        } else {
            written++;  // Only increment on successful push
        }
    }

    return written;
}

size_t circular_buffer_pop_batch(
    circular_buffer_t* buffer,
    void* elements,
    size_t count
) {
    // Guard: validate inputs
    if (!buffer || !elements || count == 0) return 0;

    size_t read = 0;
    char* dest = (char*)elements;

    // Read elements one by one
    for (size_t i = 0; i < count; i++) {
        if (!circular_buffer_pop(buffer, dest + (i * buffer->element_size))) {
            break;  // Buffer empty
        }
        read++;
    }

    return read;
}

//=============================================================================
// QUERY OPERATIONS
//=============================================================================

size_t circular_buffer_size(const circular_buffer_t* buffer) {
    // Guard: validate input
    if (!buffer) return 0;

    size_t write = atomic_load(&buffer->write_pos);
    size_t read = atomic_load(&buffer->read_pos);

    // Calculate size handling wrap-around
    return (write >= read) ?
        (write - read) : (buffer->capacity - read + write);
}

size_t circular_buffer_capacity(const circular_buffer_t* buffer) {
    // Guard: validate input
    if (!buffer) return 0;

    return buffer->capacity - 1;  // Actual usable capacity (one slot reserved)
}

bool circular_buffer_is_empty(const circular_buffer_t* buffer) {
    // Guard: validate input
    if (!buffer) return true;

    return atomic_load(&buffer->read_pos) == atomic_load(&buffer->write_pos);
}

bool circular_buffer_is_full(const circular_buffer_t* buffer) {
    // Guard: validate input
    /* P2-MW-03 fix: Removed false positive NIMCP_THROW_TO_IMMUNE.
     * is_full is a query function - NULL input returns false consistently
     * with is_empty (which also returns early without throwing). */
    if (!buffer) return false;

    size_t write = atomic_load(&buffer->write_pos);
    size_t read = atomic_load(&buffer->read_pos);
    size_t next_write = (write + 1) % buffer->capacity;

    return next_write == read;
}

float circular_buffer_utilization(const circular_buffer_t* buffer) {
    // Guard: validate input
    if (!buffer || buffer->capacity <= 1) return 0.0F;

    size_t size = circular_buffer_size(buffer);
    size_t capacity = buffer->capacity - 1;

    return (100.0F * size) / capacity;
}

//=============================================================================
// MANAGEMENT
//=============================================================================

void circular_buffer_clear(circular_buffer_t* buffer) {
    // Guard: validate input
    if (!buffer) return;

    // Reset pointers
    atomic_store(&buffer->write_pos, 0);
    atomic_store(&buffer->read_pos, 0);
}

void circular_buffer_get_stats(
    const circular_buffer_t* buffer,
    circular_buffer_stats_t* stats
) {
    // Guard: validate inputs
    if (!buffer || !stats) return;

    // THREAD SAFETY FIX: Read from atomic counters to get accurate stats
    stats->total_writes = atomic_load(&buffer->atomic_total_writes);
    stats->total_reads = atomic_load(&buffer->atomic_total_reads);
    stats->overflows = atomic_load(&buffer->atomic_overflows);
    stats->underflows = atomic_load(&buffer->atomic_underflows);
    stats->peak_usage = atomic_load(&buffer->atomic_peak_usage);
    stats->avg_usage = buffer->stats.avg_usage;  // Keep non-atomic for float
}

void circular_buffer_reset_stats(circular_buffer_t* buffer) {
    // Guard: validate input
    if (!buffer) return;

    // Reset all statistics
    memset(&buffer->stats, 0, sizeof(circular_buffer_stats_t));

    // THREAD SAFETY FIX: Also reset atomic counters
    atomic_store(&buffer->atomic_total_writes, 0);
    atomic_store(&buffer->atomic_total_reads, 0);
    atomic_store(&buffer->atomic_overflows, 0);
    atomic_store(&buffer->atomic_underflows, 0);
    atomic_store(&buffer->atomic_peak_usage, 0);
}

//=============================================================================
// POSITIONAL ENCODING INTEGRATION
//=============================================================================

bool circular_buffer_set_pe_config(
    circular_buffer_t* buffer,
    nimcp_pos_encoder_t* encoder
) {
    // Guard: validate inputs
    if (!buffer || !encoder) {
        LOG_ERROR("circular_buffer_set_pe_config: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_set_pe_config: required parameter is NULL (buffer, encoder)");
        return false;
    }

    // Validate encoder type (should be SINUSOIDAL or ALIBI)
    nimcp_pos_encoding_type_t type = nimcp_pos_get_type(encoder);
    if (type != NIMCP_POS_SINUSOIDAL && type != NIMCP_POS_ALIBI) {
        LOG_ERROR("circular_buffer_set_pe_config: Invalid encoder type %d (expected SINUSOIDAL or ALIBI)",
                  type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circular_buffer_set_pe_config: validation failed");
        return false;
    }

    // Attach encoder to buffer
    buffer->pe_encoder = encoder;

    LOG_INFO("Circular buffer PE configured: type=%s, max_seq=%u, dim=%u",
             nimcp_pos_type_to_string(type),
             nimcp_pos_get_max_length(encoder),
             nimcp_pos_get_dim(encoder));

    return true;
}

bool circular_buffer_get_position_embedding(
    const circular_buffer_t* buffer,
    size_t index,
    float* output
) {
    // Guard: validate inputs
    if (!buffer || !output) {
        LOG_ERROR("circular_buffer_get_position_embedding: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_get_position_embedding: required parameter is NULL (buffer, output)");
        return false;
    }

    // Guard: check PE configured
    if (!buffer->pe_encoder) {
        LOG_WARNING("circular_buffer_get_position_embedding: PE not configured");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_get_position_embedding: buffer->pe_encoder is NULL");
        return false;
    }

    // Guard: validate index within buffer size
    size_t buffer_size = circular_buffer_size(buffer);
    if (index >= buffer_size) {
        LOG_ERROR("circular_buffer_get_position_embedding: index %zu out of range (size=%zu)",
                  index, buffer_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circular_buffer_get_position_embedding: capacity exceeded");
        return false;
    }

    // Encode position using PE encoder
    int result = nimcp_pos_encode_position(buffer->pe_encoder, (uint32_t)index, output);
    if (result != NIMCP_POS_SUCCESS) {
        LOG_ERROR("circular_buffer_get_position_embedding: encoding failed with error %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circular_buffer_get_position_embedding: validation failed");
        return false;
    }

    return true;
}

bool circular_buffer_apply_alibi_bias(
    const circular_buffer_t* buffer,
    uint32_t seq_length,
    float* bias_out
) {
    // Guard: validate inputs
    if (!buffer || !bias_out) {
        LOG_ERROR("circular_buffer_apply_alibi_bias: NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_apply_alibi_bias: required parameter is NULL (buffer, bias_out)");
        return false;
    }

    // Guard: check PE configured
    if (!buffer->pe_encoder) {
        LOG_WARNING("circular_buffer_apply_alibi_bias: PE not configured");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_apply_alibi_bias: buffer->pe_encoder is NULL");
        return false;
    }

    // Guard: validate encoder is ALiBi type
    nimcp_pos_encoding_type_t type = nimcp_pos_get_type(buffer->pe_encoder);
    if (type != NIMCP_POS_ALIBI) {
        LOG_ERROR("circular_buffer_apply_alibi_bias: encoder type is %s, expected ALIBI",
                  nimcp_pos_type_to_string(type));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circular_buffer_apply_alibi_bias: validation failed");
        return false;
    }

    // Guard: validate sequence length within buffer bounds
    size_t buffer_size = circular_buffer_size(buffer);
    if (seq_length > buffer_size) {
        LOG_ERROR("circular_buffer_apply_alibi_bias: seq_length %u exceeds buffer size %zu",
                  seq_length, buffer_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circular_buffer_apply_alibi_bias: validation failed");
        return false;
    }

    // Generate ALiBi bias matrix
    int result = nimcp_pos_alibi_get_bias(buffer->pe_encoder, seq_length, bias_out);
    if (result != NIMCP_POS_SUCCESS) {
        LOG_ERROR("circular_buffer_apply_alibi_bias: bias generation failed with error %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circular_buffer_apply_alibi_bias: validation failed");
        return false;
    }

    return true;
}
