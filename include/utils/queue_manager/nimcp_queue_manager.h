/**
 * @file nimcp_queue_manager.h
 * @brief Thread pool-based queue management system for NIMCP
 */

#ifndef NIMCP_QUEUE_MANAGER_H
#define NIMCP_QUEUE_MANAGER_H

#include "utils/validation/nimcp_common.h"
#include "utils/containers/nimcp_queue.h"
#include "utils/thread/nimcp_thread_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

// Constants
#define NIMCP_QUEUE_MAX_CHANNELS 1024
#define NIMCP_QUEUE_DEFAULT_TIMEOUT 1000
#define NIMCP_QUEUE_DEFAULT_SIZE 1000
#define NIMCP_QUEUE_MIN_SIZE 10
#define NIMCP_QUEUE_MAX_SIZE 1000000

// Queue priority levels
typedef enum {
    NIMCP_QUEUE_PRIORITY_HIGH = 0,
    NIMCP_QUEUE_PRIORITY_NORMAL = 1,
    NIMCP_QUEUE_PRIORITY_LOW = 2,
    NIMCP_QUEUE_PRIORITY_COUNT
} nimcp_queue_priority_t;

// Queue operation types
typedef enum {
    NIMCP_QUEUE_OP_ENQUEUE,
    NIMCP_QUEUE_OP_DEQUEUE,
    NIMCP_QUEUE_OP_CLEAR,
    NIMCP_QUEUE_OP_GET_STATS
} nimcp_queue_operation_t;

// Queue operation context
typedef struct {
    nimcp_queue_operation_t op_type;
    uint32_t channel_id;
    nimcp_queue_priority_t priority;
    nimcp_message_t* message;
    uint32_t timeout_ms;
    void* result;
    void* manager_handle;  // Internal: manager reference for handler
    nimcp_result_t status;
    volatile bool completed;  // Accessed atomically in implementation
    volatile bool abandoned;  // Set to true on timeout - worker must not access result
} nimcp_queue_operation_ctx_t;

// Queue manager configuration
typedef struct {
    struct {
        size_t high;    // Size of high priority queue
        size_t normal;  // Size of normal priority queue
        size_t low;     // Size of low priority queue
    } queue_sizes;
    uint32_t default_timeout;  // Default operation timeout in ms
    bool blocking_mode;        // Whether queues should block when full/empty
    size_t max_channels;       // Maximum number of channels supported
    size_t worker_threads;     // Number of worker threads for queue operations
} nimcp_queue_manager_config_t;

// Queue statistics
typedef struct {
    struct {
        uint_least64_t enqueued;  // Total messages enqueued (accessed atomically in implementation)
        uint_least64_t dequeued;  // Total messages dequeued (accessed atomically in implementation)
        uint_least64_t
            dropped;  // Messages dropped due to overflow (accessed atomically in implementation)
        size_t current_size;  // Current number of messages (accessed atomically in implementation)
        size_t peak_size;     // Maximum size reached (accessed atomically in implementation)
        uint_least64_t
            op_latency_sum;  // Sum of operation latencies (accessed atomically in implementation)
        uint_least64_t op_count;  // Total operation count (accessed atomically in implementation)
    } priorities[NIMCP_QUEUE_PRIORITY_COUNT];
} nimcp_queue_manager_stats_t;

// Forward declarations
typedef struct nimcp_queue_channel nimcp_queue_channel_t;
typedef struct nimcp_queue_manager nimcp_queue_manager_t;
typedef nimcp_queue_manager_t* nimcp_queue_manager_handle_t;

// Public API
nimcp_result_t nimcp_queue_manager_create(const nimcp_queue_manager_config_t* config,
                                          nimcp_queue_manager_handle_t* manager);

nimcp_result_t nimcp_queue_manager_destroy(nimcp_queue_manager_handle_t manager);

nimcp_result_t nimcp_queue_manager_enqueue(nimcp_queue_manager_handle_t manager,
                                           uint32_t channel_id, const nimcp_message_t* message,
                                           uint32_t timeout_ms);

nimcp_result_t nimcp_queue_manager_dequeue(nimcp_queue_manager_handle_t manager,
                                           uint32_t channel_id, nimcp_message_t** message,
                                           uint32_t timeout_ms);

nimcp_result_t nimcp_queue_manager_get_stats(nimcp_queue_manager_handle_t manager,
                                             uint32_t channel_id,
                                             nimcp_queue_manager_stats_t* stats);

nimcp_result_t nimcp_queue_manager_clear(nimcp_queue_manager_handle_t manager, uint32_t channel_id);

nimcp_result_t nimcp_queue_manager_set_timeout(nimcp_queue_manager_handle_t manager,
                                               uint32_t timeout_ms);

bool nimcp_queue_manager_is_empty(nimcp_queue_manager_handle_t manager, uint32_t channel_id,
                                  nimcp_queue_priority_t priority);

bool nimcp_queue_manager_is_full(nimcp_queue_manager_handle_t manager, uint32_t channel_id,
                                 nimcp_queue_priority_t priority);

size_t nimcp_queue_manager_get_size(nimcp_queue_manager_handle_t manager, uint32_t channel_id,
                                    nimcp_queue_priority_t priority);

// Internal structures and functions
#ifdef NIMCP_INTERNAL

// Channel structure containing priority queues
struct nimcp_queue_channel {
    nimcp_queue_handle_t queues[NIMCP_QUEUE_PRIORITY_COUNT];
    nimcp_queue_manager_stats_t stats;
};

// Queue manager structure
struct nimcp_queue_manager {
    nimcp_queue_channel_t* channels;
    nimcp_queue_manager_config_t config;
    nimcp_thread_pool_t* thread_pool;
    bool initialized;
    atomic_bool shutting_down;
};

// Internal helper function declarations
static bool is_valid_channel(nimcp_queue_manager_handle_t manager, uint32_t channel_id);
static size_t get_queue_size_for_priority(const nimcp_queue_manager_config_t* config,
                                          nimcp_queue_priority_t priority);
static nimcp_result_t init_channel(nimcp_queue_channel_t* channel,
                                   const nimcp_queue_manager_config_t* config);
static void destroy_channel(nimcp_queue_channel_t* channel);
static nimcp_result_t validate_config(const nimcp_queue_manager_config_t* config);
static void queue_operation_handler(void* arg);
static nimcp_result_t submit_queue_operation(nimcp_queue_manager_handle_t manager,
                                             nimcp_queue_operation_ctx_t* op_ctx);

#endif  // NIMCP_INTERNAL

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_QUEUE_MANAGER_H
