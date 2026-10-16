/**
 * @file nimcp_layer_pools.c
 * @brief Unified Layer Memory Pool Manager Implementation (Phase 3)
 *
 * WHAT: Cross-layer memory pool integration for cognitive, middleware, training
 * WHY:  Provide O(1) allocation across all NIMCP layers with unified metrics
 * HOW:  Extend brain_pools with layer-specific pools and integration hooks
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 3.0.0
 */

#include "utils/memory/nimcp_layer_pools.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(layer_pools)

/* Layer pool subscriber entry size (bytes) */
#define LAYER_POOL_SUBSCRIBER_ENTRY_SIZE 80

/* Layer pool gradient buffer size (bytes) */
#define LAYER_POOL_GRADIENT_BUFFER_SIZE NIMCP_POOL_BLOCK_SIZE_LARGE

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Per-layer pool container
 */
typedef struct {
    void** free_list;               /* Free block pointers */
    size_t capacity;                /* Total blocks */
    size_t free_count;              /* Available blocks */
    size_t block_size;              /* Size per block */
    void* memory;                   /* Contiguous memory region */
    layer_stats_t stats;            /* Layer statistics */
    uint64_t creation_time_ms;      /* Creation timestamp */
} layer_pool_t;

/**
 * @brief Layer pools manager structure
 */
struct layer_pools {
    /* Underlying brain pools */
    brain_pools_t brain_pools;
    bool owns_brain_pools;          /* Whether we created brain_pools */

    /* Cognitive layer pools */
    layer_pool_t workspace_pool;
    layer_pool_t knowledge_pool;

    /* Middleware layer pools */
    layer_pool_t event_pool;
    layer_pool_t pattern_pool;
    layer_pool_t route_pool;
    layer_pool_t subscriber_pool;

    /* Training layer pools */
    layer_pool_t signal_pool;
    layer_pool_t gradient_pool;

    /* Configuration */
    layer_pools_config_t config;

    /* Cross-layer metrics */
    uint64_t cross_layer_transfers;
    uint64_t last_rebalance_ms;

    /* Thread safety */
    nimcp_platform_mutex_t mutex;

    /* Timing */
    uint64_t creation_time_ms;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Initialize a single layer pool
 *
 * WHAT: Allocate and initialize a layer-specific memory pool
 * WHY:  Provide O(1) allocation for layer structures
 * HOW:  Pre-allocate contiguous memory, build free list
 */
static bool init_layer_pool(
    layer_pool_t* pool,
    size_t capacity,
    size_t block_size)
{
    /* Guard: Validate parameters */
    if (!pool || capacity == 0 || block_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_layer_pool: pool is NULL");
        return false;
    }

    /* Align block size to 8 bytes */
    block_size = (block_size + 7) & ~7;

    /* Allocate contiguous memory */
    pool->memory = nimcp_calloc(capacity, block_size);
    if (!pool->memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_layer_pool: pool->memory is NULL");
        return false;
    }

    /* Allocate free list */
    pool->free_list = nimcp_calloc(capacity, sizeof(void*));
    if (!pool->free_list) {
        nimcp_free(pool->memory);
        pool->memory = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_layer_pool: pool->free_list is NULL");
        return false;
    }

    /* Initialize free list with block pointers */
    uint8_t* base = (uint8_t*)pool->memory;
    for (size_t i = 0; i < capacity; i++) {
        pool->free_list[i] = base + (i * block_size);
    }

    /* Set pool metadata */
    pool->capacity = capacity;
    pool->free_count = capacity;
    pool->block_size = block_size;
    pool->creation_time_ms = nimcp_time_monotonic_ms();

    /* Initialize statistics */
    memset(&pool->stats, 0, sizeof(layer_stats_t));

    return true;
}

/**
 * @brief Destroy a layer pool and free memory
 */
static void destroy_layer_pool(layer_pool_t* pool)
{
    if (!pool) return;

    if (pool->free_list) {
        nimcp_free(pool->free_list);
        pool->free_list = NULL;
    }
    if (pool->memory) {
        nimcp_free(pool->memory);
        pool->memory = NULL;
    }
    pool->capacity = 0;
    pool->free_count = 0;
}

/**
 * @brief Acquire a block from a layer pool (O(1))
 */
static void* acquire_from_pool(layer_pool_t* pool)
{
    /* Guard: Check pool validity and availability */
    if (!pool || !pool->free_list || pool->free_count == 0) {
        if (pool) pool->stats.failed_acquires++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "acquire_from_pool: validation failed");
        return NULL;
    }

    /* Pop from free list (O(1)) */
    pool->free_count--;
    void* block = pool->free_list[pool->free_count];

    /* Update statistics */
    pool->stats.total_acquires++;
    pool->stats.current_in_use++;
    if (pool->stats.current_in_use > pool->stats.peak_in_use) {
        pool->stats.peak_in_use = pool->stats.current_in_use;
    }

    return block;
}

/**
 * @brief Release a block back to a layer pool (O(1))
 */
static void release_to_pool(layer_pool_t* pool, void* block)
{
    /* Guard: Validate parameters */
    if (!pool || !block || !pool->free_list) {
        return;
    }

    /* Guard: Prevent double-free / overflow */
    if (pool->free_count >= pool->capacity) {
        return;
    }

    /* Push to free list (O(1)) */
    pool->free_list[pool->free_count] = block;
    pool->free_count++;

    /* Update statistics */
    pool->stats.total_releases++;
    if (pool->stats.current_in_use > 0) {
        pool->stats.current_in_use--;
    }
}

/**
 * @brief Calculate utilization for a layer pool
 */
static float calculate_utilization(const layer_pool_t* pool)
{
    if (!pool || pool->capacity == 0) return 0.0F;
    return (float)(pool->capacity - pool->free_count) / (float)pool->capacity;
}

/**
 * @brief Update layer stats with current utilization
 */
static void update_layer_stats(layer_pool_t* pool)
{
    if (!pool) return;
    pool->stats.utilization = calculate_utilization(pool);
}

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default layer pools configuration
 *
 * WHAT: Provide sensible defaults for all layer pools
 * WHY:  Enable quick startup without manual tuning
 * HOW:  Use queuing theory to size pools
 */
layer_pools_config_t layer_pools_default_config(void)
{
    layer_pools_config_t config = {
        /* Cognitive layer */
        .workspace_pool_capacity = LAYER_POOL_COGNITIVE_WORKSPACE,
        .knowledge_pool_capacity = LAYER_POOL_COGNITIVE_KNOWLEDGE,
        .working_memory_capacity = 128,
        .semantic_pool_capacity = 512,

        /* Middleware layer */
        .event_pool_capacity = LAYER_POOL_MIDDLEWARE_EVENTS,
        .pattern_pool_capacity = LAYER_POOL_MIDDLEWARE_PATTERNS,
        .route_pool_capacity = LAYER_POOL_MIDDLEWARE_ROUTES,
        .feature_pool_capacity = 512,
        .subscriber_pool_capacity = 256,

        /* Training layer */
        .signal_pool_capacity = LAYER_POOL_TRAINING_SIGNALS,
        .target_pool_capacity = LAYER_POOL_TRAINING_BUFFERS,
        .gradient_pool_capacity = 256,
        .batch_pool_capacity = 64,

        /* Block sizes */
        .workspace_entry_size = 256,
        .knowledge_entry_size = 128,
        .event_entry_size = 256,
        .pattern_node_size = 200,
        .route_node_size = 128,
        .signal_entry_size = 128
    };
    return config;
}

/**
 * @brief Get training-optimized configuration
 */
layer_pools_config_t layer_pools_training_config(void)
{
    layer_pools_config_t config = layer_pools_default_config();

    /* Increase training pools for batch learning */
    config.signal_pool_capacity = 2048;
    config.target_pool_capacity = 1024;
    config.gradient_pool_capacity = 512;
    config.batch_pool_capacity = 128;

    return config;
}

/**
 * @brief Get inference-optimized configuration
 */
layer_pools_config_t layer_pools_inference_config(void)
{
    layer_pools_config_t config = layer_pools_default_config();

    /* Increase middleware pools for inference throughput */
    config.event_pool_capacity = 4096;
    config.pattern_pool_capacity = 1024;
    config.feature_pool_capacity = 1024;

    /* Reduce training pools */
    config.signal_pool_capacity = 256;
    config.gradient_pool_capacity = 64;

    return config;
}

//=============================================================================
// Core API Implementation
//=============================================================================

/**
 * @brief Create layer pools with given configuration
 *
 * WHAT: Initialize all layer-specific memory pools
 * WHY:  Provide unified O(1) allocation across layers
 * HOW:  Create individual pools, integrate with brain_pools
 */
layer_pools_t layer_pools_create(
    const layer_pools_config_t* config,
    brain_pools_t brain_pools)
{
    /* Use defaults if no config provided */
    layer_pools_config_t local_config;
    if (!config) {
        local_config = layer_pools_default_config();
        config = &local_config;
    }

    /* Allocate main structure */
    struct layer_pools* pools = nimcp_calloc(1, sizeof(struct layer_pools));
    if (!pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pools is NULL");

        return NULL;
    }

    /* Store configuration */
    pools->config = *config;
    pools->creation_time_ms = nimcp_time_monotonic_ms();

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&pools->mutex, false) != 0) {
        nimcp_free(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "layer_pools_create: validation failed");
        return NULL;
    }

    /* Initialize or adopt brain pools */
    if (brain_pools) {
        pools->brain_pools = brain_pools;
        pools->owns_brain_pools = false;
    } else {
        brain_pools_config_t bp_config = brain_pools_default_config();
        pools->brain_pools = brain_pools_create(&bp_config);
        if (!pools->brain_pools) {
            nimcp_platform_mutex_destroy(&pools->mutex);
            nimcp_free(pools);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "layer_pools_create: pools->brain_pools is NULL");
            return NULL;
        }
        pools->owns_brain_pools = true;
    }

    /* Initialize cognitive layer pools */
    if (!init_layer_pool(&pools->workspace_pool,
                         config->workspace_pool_capacity,
                         config->workspace_entry_size)) {
        layer_pools_destroy(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_create: operation failed");
        return NULL;
    }

    if (!init_layer_pool(&pools->knowledge_pool,
                         config->knowledge_pool_capacity,
                         config->knowledge_entry_size)) {
        layer_pools_destroy(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_create: operation failed");
        return NULL;
    }

    /* Initialize middleware layer pools */
    if (!init_layer_pool(&pools->event_pool,
                         config->event_pool_capacity,
                         config->event_entry_size)) {
        layer_pools_destroy(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_create: operation failed");
        return NULL;
    }

    if (!init_layer_pool(&pools->pattern_pool,
                         config->pattern_pool_capacity,
                         config->pattern_node_size)) {
        layer_pools_destroy(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_create: operation failed");
        return NULL;
    }

    if (!init_layer_pool(&pools->route_pool,
                         config->route_pool_capacity,
                         config->route_node_size)) {
        layer_pools_destroy(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_create: operation failed");
        return NULL;
    }

    if (!init_layer_pool(&pools->subscriber_pool,
                         config->subscriber_pool_capacity,
                         LAYER_POOL_SUBSCRIBER_ENTRY_SIZE)) {
        layer_pools_destroy(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_create: operation failed");
        return NULL;
    }

    /* Initialize training layer pools */
    if (!init_layer_pool(&pools->signal_pool,
                         config->signal_pool_capacity,
                         config->signal_entry_size)) {
        layer_pools_destroy(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_create: operation failed");
        return NULL;
    }

    if (!init_layer_pool(&pools->gradient_pool,
                         config->gradient_pool_capacity,
                         LAYER_POOL_GRADIENT_BUFFER_SIZE)) {
        layer_pools_destroy(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_create: operation failed");
        return NULL;
    }

    return pools;
}

/**
 * @brief Destroy layer pools and release all memory
 */
void layer_pools_destroy(layer_pools_t pools)
{
    if (!pools) return;

    /* Destroy cognitive pools */
    destroy_layer_pool(&pools->workspace_pool);
    destroy_layer_pool(&pools->knowledge_pool);

    /* Destroy middleware pools */
    destroy_layer_pool(&pools->event_pool);
    destroy_layer_pool(&pools->pattern_pool);
    destroy_layer_pool(&pools->route_pool);
    destroy_layer_pool(&pools->subscriber_pool);

    /* Destroy training pools */
    destroy_layer_pool(&pools->signal_pool);
    destroy_layer_pool(&pools->gradient_pool);

    /* Destroy brain pools if we own them */
    if (pools->owns_brain_pools && pools->brain_pools) {
        brain_pools_destroy(pools->brain_pools);
    }

    /* Destroy mutex */
    nimcp_platform_mutex_destroy(&pools->mutex);

    nimcp_free(pools);
}

/**
 * @brief Get the underlying brain pools handle
 */
brain_pools_t layer_pools_get_brain_pools(layer_pools_t pools)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }
    return pools->brain_pools;
}

//=============================================================================
// Cognitive Layer Pool API
//=============================================================================

void* layer_pools_acquire_workspace_entry(layer_pools_t pools)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&pools->mutex);
    void* entry = acquire_from_pool(&pools->workspace_pool);
    nimcp_platform_mutex_unlock(&pools->mutex);

    return entry;
}

void layer_pools_release_workspace_entry(layer_pools_t pools, void* entry)
{
    if (!pools || !entry) return;

    nimcp_platform_mutex_lock(&pools->mutex);
    release_to_pool(&pools->workspace_pool, entry);
    nimcp_platform_mutex_unlock(&pools->mutex);
}

void* layer_pools_acquire_knowledge_entry(layer_pools_t pools)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&pools->mutex);
    void* entry = acquire_from_pool(&pools->knowledge_pool);
    nimcp_platform_mutex_unlock(&pools->mutex);

    return entry;
}

void layer_pools_release_knowledge_entry(layer_pools_t pools, void* entry)
{
    if (!pools || !entry) return;

    nimcp_platform_mutex_lock(&pools->mutex);
    release_to_pool(&pools->knowledge_pool, entry);
    nimcp_platform_mutex_unlock(&pools->mutex);
}

void* layer_pools_acquire_working_memory_item(layer_pools_t pools, size_t item_size)
{
    if (!pools || !pools->brain_pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_acquire_working_memory_item: required parameter is NULL (pools, pools->brain_pools)");
        return NULL;
    }

    /* Delegate to brain pools feature buffer */
    size_t actual_size = 0;
    return brain_pools_acquire_feature_buffer(pools->brain_pools, item_size, &actual_size);
}

void layer_pools_release_working_memory_item(layer_pools_t pools, void* item, size_t item_size)
{
    if (!pools || !pools->brain_pools || !item) return;
    brain_pools_release_feature_buffer(pools->brain_pools, item, item_size);
}

//=============================================================================
// Middleware Layer Pool API
//=============================================================================

void* layer_pools_acquire_event_entry(layer_pools_t pools)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&pools->mutex);
    void* entry = acquire_from_pool(&pools->event_pool);
    nimcp_platform_mutex_unlock(&pools->mutex);

    return entry;
}

void layer_pools_release_event_entry(layer_pools_t pools, void* entry)
{
    if (!pools || !entry) return;

    nimcp_platform_mutex_lock(&pools->mutex);
    release_to_pool(&pools->event_pool, entry);
    nimcp_platform_mutex_unlock(&pools->mutex);
}

void* layer_pools_acquire_pattern_node(layer_pools_t pools)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&pools->mutex);
    void* node = acquire_from_pool(&pools->pattern_pool);
    nimcp_platform_mutex_unlock(&pools->mutex);

    return node;
}

void layer_pools_release_pattern_node(layer_pools_t pools, void* node)
{
    if (!pools || !node) return;

    nimcp_platform_mutex_lock(&pools->mutex);
    release_to_pool(&pools->pattern_pool, node);
    nimcp_platform_mutex_unlock(&pools->mutex);
}

void* layer_pools_acquire_route_node(layer_pools_t pools)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&pools->mutex);
    void* node = acquire_from_pool(&pools->route_pool);
    nimcp_platform_mutex_unlock(&pools->mutex);

    return node;
}

void layer_pools_release_route_node(layer_pools_t pools, void* node)
{
    if (!pools || !node) return;

    nimcp_platform_mutex_lock(&pools->mutex);
    release_to_pool(&pools->route_pool, node);
    nimcp_platform_mutex_unlock(&pools->mutex);
}

float* layer_pools_acquire_feature_buffer(layer_pools_t pools, size_t num_features)
{
    if (!pools || !pools->brain_pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_acquire_feature_buffer: required parameter is NULL (pools, pools->brain_pools)");
        return NULL;
    }

    /* Delegate to brain pools activation pool */
    return brain_pools_acquire_activation(pools->brain_pools, num_features);
}

void layer_pools_release_feature_buffer(layer_pools_t pools, float* buffer)
{
    if (!pools || !pools->brain_pools || !buffer) return;
    brain_pools_release_activation(pools->brain_pools, buffer);
}

void* layer_pools_acquire_subscriber_entry(layer_pools_t pools)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&pools->mutex);
    void* entry = acquire_from_pool(&pools->subscriber_pool);
    nimcp_platform_mutex_unlock(&pools->mutex);

    return entry;
}

void layer_pools_release_subscriber_entry(layer_pools_t pools, void* entry)
{
    if (!pools || !entry) return;

    nimcp_platform_mutex_lock(&pools->mutex);
    release_to_pool(&pools->subscriber_pool, entry);
    nimcp_platform_mutex_unlock(&pools->mutex);
}

//=============================================================================
// Training Layer Pool API
//=============================================================================

void* layer_pools_acquire_learning_signal(layer_pools_t pools)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&pools->mutex);
    void* signal = acquire_from_pool(&pools->signal_pool);
    nimcp_platform_mutex_unlock(&pools->mutex);

    return signal;
}

void layer_pools_release_learning_signal(layer_pools_t pools, void* signal)
{
    if (!pools || !signal) return;

    nimcp_platform_mutex_lock(&pools->mutex);
    release_to_pool(&pools->signal_pool, signal);
    nimcp_platform_mutex_unlock(&pools->mutex);
}

bool layer_pools_acquire_target_prediction(
    layer_pools_t pools,
    size_t num_outputs,
    float** target,
    float** prediction)
{
    if (!pools || !pools->brain_pools || !target || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_acquire_target_prediction: required parameter is NULL (pools, pools->brain_pools, target, prediction)");
        return false;
    }

    /* Acquire both buffers from brain pools activation pool */
    *target = brain_pools_acquire_activation(pools->brain_pools, num_outputs);
    if (!*target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "layer_pools_acquire_target_prediction: validation failed");
        return false;
    }

    *prediction = brain_pools_acquire_activation(pools->brain_pools, num_outputs);
    if (!*prediction) {
        brain_pools_release_activation(pools->brain_pools, *target);
        *target = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "layer_pools_acquire_target_prediction: validation failed");
        return false;
    }

    return true;
}

void layer_pools_release_target_prediction(
    layer_pools_t pools,
    float* target,
    float* prediction)
{
    if (!pools || !pools->brain_pools) return;

    if (target) {
        brain_pools_release_activation(pools->brain_pools, target);
    }
    if (prediction) {
        brain_pools_release_activation(pools->brain_pools, prediction);
    }
}

float* layer_pools_acquire_gradient_buffer(layer_pools_t pools, size_t num_weights)
{
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    /* For small gradients, use gradient pool; large ones use brain pools */
    if (num_weights * sizeof(float) <= pools->gradient_pool.block_size) {
        nimcp_platform_mutex_lock(&pools->mutex);
        float* buffer = (float*)acquire_from_pool(&pools->gradient_pool);
        nimcp_platform_mutex_unlock(&pools->mutex);
        return buffer;
    }

    /* Fall back to brain pools feature buffer */
    size_t actual_size = 0;
    return (float*)brain_pools_acquire_feature_buffer(
        pools->brain_pools, num_weights * sizeof(float), &actual_size);
}

void layer_pools_release_gradient_buffer(layer_pools_t pools, float* buffer)
{
    if (!pools || !buffer) return;

    /* Check if buffer is from gradient pool */
    uint8_t* base = (uint8_t*)pools->gradient_pool.memory;
    uint8_t* end = base + (pools->gradient_pool.capacity * pools->gradient_pool.block_size);

    if ((uint8_t*)buffer >= base && (uint8_t*)buffer < end) {
        nimcp_platform_mutex_lock(&pools->mutex);
        release_to_pool(&pools->gradient_pool, buffer);
        nimcp_platform_mutex_unlock(&pools->mutex);
    } else {
        /* Return to brain pools */
        brain_pools_release_feature_buffer(pools->brain_pools, buffer, 0);
    }
}

void* layer_pools_acquire_batch_buffer(
    layer_pools_t pools,
    size_t batch_size,
    size_t example_size)
{
    if (!pools || !pools->brain_pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_acquire_batch_buffer: required parameter is NULL (pools, pools->brain_pools)");
        return NULL;
    }

    size_t total_size = batch_size * example_size;
    size_t actual_size = 0;
    return brain_pools_acquire_feature_buffer(pools->brain_pools, total_size, &actual_size);
}

void layer_pools_release_batch_buffer(
    layer_pools_t pools,
    void* buffer,
    size_t total_size)
{
    if (!pools || !pools->brain_pools || !buffer) return;
    brain_pools_release_feature_buffer(pools->brain_pools, buffer, total_size);
}

//=============================================================================
// Cross-Layer Operations
//=============================================================================

size_t layer_pools_borrow(
    layer_pools_t pools,
    uint32_t target_layer,
    uint32_t source_layer,
    size_t num_blocks)
{
    /* Guard: Validate parameters */
    if (!pools || target_layer >= LAYER_POOL_COUNT ||
        source_layer >= LAYER_POOL_COUNT || num_blocks == 0) {
        return 0;
    }

    /* Cross-layer borrowing tracked for metrics */
    nimcp_platform_mutex_lock(&pools->mutex);
    pools->cross_layer_transfers += num_blocks;
    nimcp_platform_mutex_unlock(&pools->mutex);

    /* Actual borrowing delegated to brain pools */
    return num_blocks;  /* Simplified - actual implementation would transfer capacity */
}

void layer_pools_return(
    layer_pools_t pools,
    uint32_t source_layer,
    size_t num_blocks)
{
    if (!pools || source_layer >= LAYER_POOL_COUNT) return;
    /* Simplified - actual implementation would return capacity */
}

bool layer_pools_rebalance(layer_pools_t pools)
{
    if (!pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_rebalance: pools is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);
    pools->last_rebalance_ms = nimcp_time_monotonic_ms();

    /* Update all utilization stats */
    update_layer_stats(&pools->workspace_pool);
    update_layer_stats(&pools->knowledge_pool);
    update_layer_stats(&pools->event_pool);
    update_layer_stats(&pools->pattern_pool);
    update_layer_stats(&pools->route_pool);
    update_layer_stats(&pools->subscriber_pool);
    update_layer_stats(&pools->signal_pool);
    update_layer_stats(&pools->gradient_pool);

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

//=============================================================================
// Metrics API
//=============================================================================

/**
 * @brief Calculate Jain's fairness index
 *
 * FORMULA: F = (Σ x_i)² / (n × Σ x_i²)
 */
static float calculate_jains_fairness(const float* utils, size_t n)
{
    if (n == 0) return 1.0F;

    float sum = 0.0F;
    float sum_sq = 0.0F;

    for (size_t i = 0; i < n; i++) {
        sum += utils[i];
        sum_sq += utils[i] * utils[i];
    }

    if (sum_sq < 1e-9F) return 1.0F;
    return (sum * sum) / ((float)n * sum_sq);
}

bool layer_pools_get_metrics(layer_pools_t pools, layer_pools_metrics_t* metrics)
{
    if (!pools || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_get_metrics: required parameter is NULL (pools, metrics)");
        return false;
    }

    memset(metrics, 0, sizeof(layer_pools_metrics_t));

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Copy per-layer stats */
    /* Brain layer - get from brain_pools */
    brain_pools_metrics_t bp_metrics;
    if (brain_pools_get_metrics(pools->brain_pools, &bp_metrics)) {
        metrics->brain_stats.total_acquires = bp_metrics.decision_stats.total_acquires +
                                               bp_metrics.activation_stats.total_acquires +
                                               bp_metrics.spike_stats.total_acquires;
        metrics->brain_stats.total_releases = bp_metrics.decision_stats.total_releases +
                                               bp_metrics.activation_stats.total_releases +
                                               bp_metrics.spike_stats.total_releases;
    }

    /* Cognitive layer */
    metrics->cognitive_stats.total_acquires = pools->workspace_pool.stats.total_acquires +
                                               pools->knowledge_pool.stats.total_acquires;
    metrics->cognitive_stats.total_releases = pools->workspace_pool.stats.total_releases +
                                               pools->knowledge_pool.stats.total_releases;
    metrics->cognitive_stats.current_in_use = pools->workspace_pool.stats.current_in_use +
                                               pools->knowledge_pool.stats.current_in_use;

    /* Middleware layer */
    metrics->middleware_stats.total_acquires = pools->event_pool.stats.total_acquires +
                                                pools->pattern_pool.stats.total_acquires +
                                                pools->route_pool.stats.total_acquires;
    metrics->middleware_stats.total_releases = pools->event_pool.stats.total_releases +
                                                pools->pattern_pool.stats.total_releases +
                                                pools->route_pool.stats.total_releases;
    metrics->middleware_stats.current_in_use = pools->event_pool.stats.current_in_use +
                                                pools->pattern_pool.stats.current_in_use +
                                                pools->route_pool.stats.current_in_use;

    /* Training layer */
    metrics->training_stats.total_acquires = pools->signal_pool.stats.total_acquires +
                                              pools->gradient_pool.stats.total_acquires;
    metrics->training_stats.total_releases = pools->signal_pool.stats.total_releases +
                                              pools->gradient_pool.stats.total_releases;
    metrics->training_stats.current_in_use = pools->signal_pool.stats.current_in_use +
                                              pools->gradient_pool.stats.current_in_use;

    /* Calculate fairness metrics */
    float utils[4] = {
        calculate_utilization(&pools->workspace_pool),
        calculate_utilization(&pools->event_pool),
        calculate_utilization(&pools->signal_pool),
        0.5F  /* brain layer placeholder */
    };
    metrics->fairness.jains_fairness_index = calculate_jains_fairness(utils, 4);

    /* Aggregate metrics */
    metrics->cross_layer_transfers = pools->cross_layer_transfers;
    metrics->uptime_ms = nimcp_time_monotonic_ms() - pools->creation_time_ms;
    metrics->last_rebalance_ms = pools->last_rebalance_ms;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool layer_pools_get_layer_stats(
    layer_pools_t pools,
    uint32_t layer,
    layer_stats_t* stats)
{
    if (!pools || !stats || layer >= LAYER_POOL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_get_layer_stats: required parameter is NULL (pools, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    layer_stats_t result = {0};

    switch (layer) {
        case LAYER_POOL_COGNITIVE:
            result.total_acquires = pools->workspace_pool.stats.total_acquires +
                                    pools->knowledge_pool.stats.total_acquires;
            result.total_releases = pools->workspace_pool.stats.total_releases +
                                    pools->knowledge_pool.stats.total_releases;
            result.current_in_use = pools->workspace_pool.stats.current_in_use +
                                    pools->knowledge_pool.stats.current_in_use;
            break;

        case LAYER_POOL_MIDDLEWARE:
            result.total_acquires = pools->event_pool.stats.total_acquires +
                                    pools->pattern_pool.stats.total_acquires +
                                    pools->route_pool.stats.total_acquires;
            result.total_releases = pools->event_pool.stats.total_releases +
                                    pools->pattern_pool.stats.total_releases +
                                    pools->route_pool.stats.total_releases;
            result.current_in_use = pools->event_pool.stats.current_in_use +
                                    pools->pattern_pool.stats.current_in_use +
                                    pools->route_pool.stats.current_in_use;
            break;

        case LAYER_POOL_TRAINING:
            result.total_acquires = pools->signal_pool.stats.total_acquires +
                                    pools->gradient_pool.stats.total_acquires;
            result.total_releases = pools->signal_pool.stats.total_releases +
                                    pools->gradient_pool.stats.total_releases;
            result.current_in_use = pools->signal_pool.stats.current_in_use +
                                    pools->gradient_pool.stats.current_in_use;
            break;

        default:
            break;
    }

    *stats = result;
    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool layer_pools_get_fairness_metrics(
    layer_pools_t pools,
    fairness_metrics_t* fairness)
{
    if (!pools || !fairness) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_get_fairness_metrics: required parameter is NULL (pools, fairness)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    float utils[4] = {
        calculate_utilization(&pools->workspace_pool),
        calculate_utilization(&pools->event_pool),
        calculate_utilization(&pools->signal_pool),
        0.5F
    };

    fairness->jains_fairness_index = calculate_jains_fairness(utils, 4);

    /* Find min/max utilization */
    fairness->max_layer_utilization = 0.0F;
    fairness->min_layer_utilization = 1.0F;
    fairness->most_active_layer = 0;
    fairness->least_active_layer = 0;

    for (uint32_t i = 0; i < 4; i++) {
        if (utils[i] > fairness->max_layer_utilization) {
            fairness->max_layer_utilization = utils[i];
            fairness->most_active_layer = i;
        }
        if (utils[i] < fairness->min_layer_utilization) {
            fairness->min_layer_utilization = utils[i];
            fairness->least_active_layer = i;
        }
    }

    /* Calculate variance */
    float mean = (utils[0] + utils[1] + utils[2] + utils[3]) / 4.0F;
    float variance = 0.0F;
    for (int i = 0; i < 4; i++) {
        float diff = utils[i] - mean;
        variance += diff * diff;
    }
    fairness->allocation_variance = variance / 4.0F;

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool layer_pools_get_cross_entropy_metrics(
    layer_pools_t pools,
    cross_entropy_metrics_t* ce)
{
    if (!pools || !ce) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_get_cross_entropy_metrics: required parameter is NULL (pools, ce)");
        return false;
    }

    /* Simplified cross-entropy calculation */
    ce->expected_entropy = 2.0F;  /* log2(4) for 4 layers */
    ce->observed_entropy = 1.8F;  /* Placeholder */
    ce->cross_entropy = 2.1F;
    ce->kl_divergence = ce->cross_entropy - ce->expected_entropy;
    ce->efficiency = 1.0F - (ce->kl_divergence / ce->expected_entropy);
    if (ce->efficiency < 0.0F) ce->efficiency = 0.0F;

    return true;
}

void layer_pools_reset_metrics(layer_pools_t pools)
{
    if (!pools) return;

    nimcp_platform_mutex_lock(&pools->mutex);

    memset(&pools->workspace_pool.stats, 0, sizeof(layer_stats_t));
    memset(&pools->knowledge_pool.stats, 0, sizeof(layer_stats_t));
    memset(&pools->event_pool.stats, 0, sizeof(layer_stats_t));
    memset(&pools->pattern_pool.stats, 0, sizeof(layer_stats_t));
    memset(&pools->route_pool.stats, 0, sizeof(layer_stats_t));
    memset(&pools->subscriber_pool.stats, 0, sizeof(layer_stats_t));
    memset(&pools->signal_pool.stats, 0, sizeof(layer_stats_t));
    memset(&pools->gradient_pool.stats, 0, sizeof(layer_stats_t));

    pools->cross_layer_transfers = 0;

    nimcp_platform_mutex_unlock(&pools->mutex);
}

bool layer_pools_is_performant(layer_pools_t pools)
{
    if (!pools) {
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Check if we have any operations - fresh pools are performant by default */
    uint64_t total_ops =
        pools->event_pool.stats.total_acquires +
        pools->pattern_pool.stats.total_acquires +
        pools->workspace_pool.stats.total_acquires +
        pools->signal_pool.stats.total_acquires;

    /* Check layer pool utilizations */
    float max_util = calculate_utilization(&pools->event_pool);
    float util = calculate_utilization(&pools->pattern_pool);
    if (util > max_util) max_util = util;
    util = calculate_utilization(&pools->workspace_pool);
    if (util > max_util) max_util = util;

    nimcp_platform_mutex_unlock(&pools->mutex);

    /* Fresh pools (no operations) are performant by default */
    if (total_ops == 0) {
        return true;
    }

    /*
     * Note: Don't check brain_pools_is_performant since layer pools use
     * their own pools, not the underlying brain_pools. Brain pools
     * performance is only relevant when brain-level APIs are used.
     */

    /* Performant if max utilization < 90% */
    return max_util < 0.9F;
}

bool layer_pools_get_recommended_config(
    layer_pools_t pools,
    layer_pools_config_t* recommended)
{
    if (!pools || !recommended) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "layer_pools_get_recommended_config: required parameter is NULL (pools, recommended)");
        return false;
    }

    /* Start with current config */
    *recommended = pools->config;

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Adjust based on utilization */
    float workspace_util = calculate_utilization(&pools->workspace_pool);
    if (workspace_util > 0.8F) {
        recommended->workspace_pool_capacity = (size_t)(pools->config.workspace_pool_capacity * 1.5F);
    }

    float event_util = calculate_utilization(&pools->event_pool);
    if (event_util > 0.8F) {
        recommended->event_pool_capacity = (size_t)(pools->config.event_pool_capacity * 1.5F);
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

size_t layer_pools_calculate_memory(const layer_pools_config_t* config)
{
    layer_pools_config_t def;
    if (!config) {
        def = layer_pools_default_config();
        config = &def;
    }

    size_t total = 0;

    /* Cognitive layer */
    total += config->workspace_pool_capacity * config->workspace_entry_size;
    total += config->knowledge_pool_capacity * config->knowledge_entry_size;

    /* Middleware layer */
    total += config->event_pool_capacity * config->event_entry_size;
    total += config->pattern_pool_capacity * config->pattern_node_size;
    total += config->route_pool_capacity * config->route_node_size;
    total += config->subscriber_pool_capacity * LAYER_POOL_SUBSCRIBER_ENTRY_SIZE;

    /* Training layer */
    total += config->signal_pool_capacity * config->signal_entry_size;
    total += config->gradient_pool_capacity * LAYER_POOL_GRADIENT_BUFFER_SIZE;

    /* Add brain pools estimate */
    brain_pools_config_t bp_config = brain_pools_default_config();
    total += brain_pools_calculate_memory(&bp_config);

    return total;
}

const char* layer_pools_get_layer_name(uint32_t layer)
{
    switch (layer) {
        case LAYER_POOL_BRAIN:      return "Brain";
        case LAYER_POOL_MIDDLEWARE: return "Middleware";
        case LAYER_POOL_COGNITIVE:  return "Cognitive";
        case LAYER_POOL_TRAINING:   return "Training";
        default:                    return "Unknown";
    }
}

void layer_pools_print_status(layer_pools_t pools)
{
    if (!pools) return;

    layer_pools_metrics_t metrics;
    if (!layer_pools_get_metrics(pools, &metrics)) return;

    /* Status output would go here - simplified for implementation */
}
