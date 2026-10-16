//=============================================================================
// nimcp_circular_buffer.h - Lock-Free Circular Buffer for Temporal Data
//=============================================================================

#ifndef NIMCP_CIRCULAR_BUFFER_H
#define NIMCP_CIRCULAR_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declaration for positional encoding
typedef struct nimcp_pos_encoder_s nimcp_pos_encoder_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_circular_buffer.h
 * @brief Lock-free circular buffer with cache-aligned memory
 *
 * WHAT: High-performance ring buffer for temporal neural data
 * WHY:  Essential for buffering spike trains, activations, and sensor data
 * HOW:  Lock-free SPSC (single-producer single-consumer) with atomic operations
 *
 * FEATURES:
 * - Lock-free for single producer/single consumer
 * - Cache-aligned to prevent false sharing
 * - Multiple overflow strategies (overwrite, block, error)
 * - Batch operations for efficiency
 * - Zero-copy peek operations
 */

// Overflow handling strategies
typedef enum {
    OVERFLOW_OVERWRITE,  /**< Overwrite oldest data (default for continuous streams) */
    OVERFLOW_BLOCK,      /**< Block until space available (for critical data) */
    OVERFLOW_ERROR       /**< Return error on overflow (for strict bounds) */
} overflow_strategy_t;

// Circular buffer statistics
typedef struct {
    size_t total_writes;    /**< Total number of write operations */
    size_t total_reads;     /**< Total number of read operations */
    size_t overflows;       /**< Number of overflow events */
    size_t underflows;      /**< Number of underflow events (read from empty) */
    size_t peak_usage;      /**< Peak buffer utilization */
    float avg_usage;        /**< Average buffer utilization */
} circular_buffer_stats_t;

// Opaque circular buffer type
typedef struct circular_buffer circular_buffer_t;

//=============================================================================
// LIFECYCLE
//=============================================================================

/**
 * @brief Create circular buffer
 *
 * WHAT: Allocate and initialize a circular buffer
 * WHY:  Need efficient temporal storage for neural signals
 * HOW:  Allocate cache-aligned memory, initialize atomic indices
 *
 * @param element_size Size of each element in bytes
 * @param capacity Maximum number of elements
 * @param strategy Overflow handling strategy
 * @return Circular buffer or NULL on failure
 */
circular_buffer_t* circular_buffer_create(
    size_t element_size,
    size_t capacity,
    overflow_strategy_t strategy
);

/**
 * @brief Destroy circular buffer
 *
 * WHAT: Free all buffer resources
 * WHY:  Prevent memory leaks
 * HOW:  Free data array and control structure
 *
 * @param buffer Buffer to destroy (can be NULL)
 */
void circular_buffer_destroy(circular_buffer_t* buffer);

//=============================================================================
// CORE OPERATIONS
//=============================================================================

/**
 * @brief Push element to buffer
 *
 * WHAT: Add single element to buffer tail
 * WHY:  Store new temporal data point
 * HOW:  Copy data to current write position, advance write pointer
 *
 * @param buffer Buffer to write to
 * @param element Data to write
 * @return true on success, false on overflow (if strategy is ERROR)
 */
bool circular_buffer_push(circular_buffer_t* buffer, const void* element);

/**
 * @brief Pop element from buffer
 *
 * WHAT: Remove and return element from buffer head
 * WHY:  Retrieve oldest buffered data
 * HOW:  Copy data from read position, advance read pointer
 *
 * @param buffer Buffer to read from
 * @param element Output for element data
 * @return true on success, false if buffer empty
 */
bool circular_buffer_pop(circular_buffer_t* buffer, void* element);

/**
 * @brief Peek at element without removing
 *
 * WHAT: Read element at given offset without consuming
 * WHY:  Inspect buffered data without modifying buffer state
 * HOW:  Calculate offset from read pointer, copy data
 *
 * @param buffer Buffer to peek into
 * @param offset Offset from head (0 = oldest element)
 * @param element Output for element data
 * @return true on success, false if offset out of range
 */
bool circular_buffer_peek(
    const circular_buffer_t* buffer,
    size_t offset,
    void* element
);

//=============================================================================
// BATCH OPERATIONS
//=============================================================================

/**
 * @brief Push multiple elements
 *
 * WHAT: Write batch of elements atomically
 * WHY:  More efficient than individual pushes for streams
 * HOW:  Calculate space needed, memcpy in chunks, update pointer once
 *
 * @param buffer Buffer to write to
 * @param elements Array of elements to write
 * @param count Number of elements
 * @return Number of elements actually written
 */
size_t circular_buffer_push_batch(
    circular_buffer_t* buffer,
    const void* elements,
    size_t count
);

/**
 * @brief Pop multiple elements
 *
 * WHAT: Read batch of elements atomically
 * WHY:  More efficient for batch processing
 * HOW:  Calculate available data, memcpy in chunks, update pointer once
 *
 * @param buffer Buffer to read from
 * @param elements Output array for elements
 * @param count Maximum number of elements to read
 * @return Number of elements actually read
 */
size_t circular_buffer_pop_batch(
    circular_buffer_t* buffer,
    void* elements,
    size_t count
);

//=============================================================================
// QUERY OPERATIONS
//=============================================================================

/**
 * @brief Get number of elements in buffer
 *
 * WHAT: Return current buffer occupancy
 * WHY:  Check available data before reading
 * HOW:  Calculate (write_pos - read_pos) mod capacity
 *
 * @param buffer Buffer to query
 * @return Number of elements currently buffered
 */
size_t circular_buffer_size(const circular_buffer_t* buffer);

/**
 * @brief Get buffer capacity
 *
 * WHAT: Return maximum buffer size
 * WHY:  Determine buffer limits
 * HOW:  Return capacity field
 *
 * @param buffer Buffer to query
 * @return Maximum number of elements
 */
size_t circular_buffer_capacity(const circular_buffer_t* buffer);

/**
 * @brief Check if buffer is empty
 *
 * WHAT: Test if buffer contains no data
 * WHY:  Avoid underflow when reading
 * HOW:  Compare read and write pointers
 *
 * @param buffer Buffer to check
 * @return true if empty
 */
bool circular_buffer_is_empty(const circular_buffer_t* buffer);

/**
 * @brief Check if buffer is full
 *
 * WHAT: Test if buffer is at capacity
 * WHY:  Detect potential overflow before writing
 * HOW:  Check if (write + 1) mod capacity == read
 *
 * @param buffer Buffer to check
 * @return true if full
 */
bool circular_buffer_is_full(const circular_buffer_t* buffer);

/**
 * @brief Get buffer utilization
 *
 * WHAT: Calculate percentage of buffer filled
 * WHY:  Monitor buffer health and tune sizes
 * HOW:  Return (size / capacity) * 100
 *
 * @param buffer Buffer to query
 * @return Utilization percentage (0.0-100.0)
 */
float circular_buffer_utilization(const circular_buffer_t* buffer);

//=============================================================================
// MANAGEMENT
//=============================================================================

/**
 * @brief Clear buffer contents
 *
 * WHAT: Reset buffer to empty state
 * WHY:  Discard old data on event or reset
 * HOW:  Reset read/write pointers to 0
 *
 * @param buffer Buffer to clear
 */
void circular_buffer_clear(circular_buffer_t* buffer);

/**
 * @brief Get buffer statistics
 *
 * WHAT: Return runtime performance metrics
 * WHY:  Monitor buffer health, detect issues
 * HOW:  Copy internal statistics structure
 *
 * @param buffer Buffer to query
 * @param stats Output for statistics
 */
void circular_buffer_get_stats(
    const circular_buffer_t* buffer,
    circular_buffer_stats_t* stats
);

/**
 * @brief Reset buffer statistics
 *
 * WHAT: Clear all accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero out statistics structure
 *
 * @param buffer Buffer to reset stats for
 */
void circular_buffer_reset_stats(circular_buffer_t* buffer);

//=============================================================================
// POSITIONAL ENCODING INTEGRATION
//=============================================================================

/**
 * @brief Set positional encoding configuration for buffer
 *
 * WHAT: Configure sinusoidal PE and ALiBi for buffer position tracking
 * WHY:  Enable position-aware buffer access patterns for temporal context
 * HOW:  Create and attach PE encoder to buffer instance
 *
 * @param buffer Buffer to configure
 * @param encoder Positional encoder instance (SINUSOIDAL or ALIBI type)
 * @return true on success, false on failure
 */
bool circular_buffer_set_pe_config(
    circular_buffer_t* buffer,
    nimcp_pos_encoder_t* encoder
);

/**
 * @brief Get position embedding for buffer index
 *
 * WHAT: Retrieve sinusoidal positional encoding for specific buffer position
 * WHY:  Add temporal context to buffered data at read time
 * HOW:  Calculate relative position and encode using attached PE encoder
 *
 * @param buffer Buffer to query
 * @param index Buffer index (relative to read position)
 * @param output Output buffer for embedding (must be encoder's embedding_dim size)
 * @return true on success, false if PE not configured or index out of range
 */
bool circular_buffer_get_position_embedding(
    const circular_buffer_t* buffer,
    size_t index,
    float* output
);

/**
 * @brief Apply ALiBi attention bias for buffer lookups
 *
 * WHAT: Compute linear position bias matrix for buffer sequence
 * WHY:  Enable position-aware attention over buffered temporal data
 * HOW:  Use ALiBi encoder to generate bias based on buffer positions
 *
 * @param buffer Buffer to query
 * @param seq_length Length of sequence to compute bias for
 * @param bias_out Output bias matrix (num_heads * seq_length * seq_length floats)
 * @return true on success, false if ALiBi not configured
 */
bool circular_buffer_apply_alibi_bias(
    const circular_buffer_t* buffer,
    uint32_t seq_length,
    float* bias_out
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CIRCULAR_BUFFER_H
