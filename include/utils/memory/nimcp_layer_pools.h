/**
 * @file nimcp_layer_pools.h
 * @brief Unified Layer Memory Pool Manager (Phase 3)
 *
 * WHAT: Cross-layer memory pool integration for cognitive, middleware, training
 * WHY:  Provide O(1) allocation across all NIMCP layers with unified metrics
 * HOW:  Extend brain_pools with layer-specific pools and integration hooks
 *
 * MATHEMATICAL FOUNDATIONS:
 * 1. Cross-Entropy Loss: H(p,q) = -Σ p(i) log q(i)
 *    - Measures allocation pattern divergence between layers
 *    - Enables pool rebalancing based on actual usage
 *
 * 2. Multi-Queue M/M/c: ρ_total = Σ λ_i / (c × μ)
 *    - Aggregate utilization across layer pools
 *    - Prevents any single layer from starving others
 *
 * 3. Fairness Index (Jain's): F = (Σx_i)² / (n × Σx_i²)
 *    - Measures allocation fairness across layers (0-1)
 *    - F=1 means perfect fairness
 *
 * 4. Information Gain: IG = H(before) - H(after|pool_choice)
 *    - Guides adaptive pool sizing decisions
 *
 * PHASE: 3 (Cross-Layer Integration)
 *
 * LAYER HIERARCHY:
 * ┌─────────────────────────────────────────────────────────┐
 * │                    COGNITIVE LAYER                       │
 * │  (Global Workspace, Working Memory, Knowledge Base)      │
 * ├─────────────────────────────────────────────────────────┤
 * │                   MIDDLEWARE LAYER                       │
 * │  (Events, Routing, Patterns, Features, Encoding)         │
 * ├─────────────────────────────────────────────────────────┤
 * │                    TRAINING LAYER                        │
 * │  (Learning Signals, Weight Updates, Adapters)            │
 * ├─────────────────────────────────────────────────────────┤
 * │                     BRAIN LAYER                          │
 * │  (Decisions, Activations, Spikes, Features)              │
 * └─────────────────────────────────────────────────────────┘
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 3.0.0
 */

#ifndef NIMCP_LAYER_POOLS_H
#define NIMCP_LAYER_POOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/memory/nimcp_brain_pools.h"

//=============================================================================
// Constants
//=============================================================================

/** Layer identifiers */
#define LAYER_POOL_BRAIN        0
#define LAYER_POOL_MIDDLEWARE   1
#define LAYER_POOL_COGNITIVE    2
#define LAYER_POOL_TRAINING     3
#define LAYER_POOL_COUNT        4

/** Pool type identifiers within each layer */
#define POOL_TYPE_GENERIC       0
#define POOL_TYPE_EVENT         1
#define POOL_TYPE_PATTERN       2
#define POOL_TYPE_ROUTE         3
#define POOL_TYPE_FEATURE       4
#define POOL_TYPE_SIGNAL        5
#define POOL_TYPE_WORKSPACE     6
#define POOL_TYPE_KNOWLEDGE     7
#define POOL_TYPE_COUNT         8

/** Default capacities per layer */
#define LAYER_POOL_COGNITIVE_WORKSPACE  512
#define LAYER_POOL_COGNITIVE_KNOWLEDGE  1024
#define LAYER_POOL_MIDDLEWARE_EVENTS    2048
#define LAYER_POOL_MIDDLEWARE_PATTERNS  512
#define LAYER_POOL_MIDDLEWARE_ROUTES    1024
#define LAYER_POOL_TRAINING_SIGNALS     1024
#define LAYER_POOL_TRAINING_BUFFERS     512

//=============================================================================
// Type Definitions
//=============================================================================

/** Opaque layer pools handle */
typedef struct layer_pools* layer_pools_t;

/**
 * @brief Layer-specific pool configuration
 */
typedef struct {
    /* Cognitive layer pools */
    size_t workspace_pool_capacity;     /* Global workspace entries */
    size_t knowledge_pool_capacity;     /* Knowledge base entries */
    size_t working_memory_capacity;     /* Working memory items */
    size_t semantic_pool_capacity;      /* Semantic memory concepts */

    /* Middleware layer pools */
    size_t event_pool_capacity;         /* Event queue entries */
    size_t pattern_pool_capacity;       /* Pattern nodes */
    size_t route_pool_capacity;         /* Routing table nodes */
    size_t feature_pool_capacity;       /* Feature extraction buffers */
    size_t subscriber_pool_capacity;    /* Event subscribers */

    /* Training layer pools */
    size_t signal_pool_capacity;        /* Learning signals */
    size_t target_pool_capacity;        /* Target/prediction buffers */
    size_t gradient_pool_capacity;      /* Gradient buffers */
    size_t batch_pool_capacity;         /* Batch processing buffers */

    /* Block sizes */
    size_t workspace_entry_size;        /* Size of workspace entry */
    size_t knowledge_entry_size;        /* Size of knowledge entry */
    size_t event_entry_size;            /* Size of event entry */
    size_t pattern_node_size;           /* Size of pattern node */
    size_t route_node_size;             /* Size of route node */
    size_t signal_entry_size;           /* Size of learning signal */
} layer_pools_config_t;

/**
 * @brief Per-layer statistics
 */
typedef struct {
    uint64_t total_acquires;            /* Total acquisitions */
    uint64_t total_releases;            /* Total releases */
    uint64_t current_in_use;            /* Currently allocated */
    uint64_t peak_in_use;               /* Peak allocation */
    uint64_t failed_acquires;           /* Failed due to exhaustion */
    uint64_t cross_layer_borrows;       /* Borrows from other layers */
    float avg_acquire_ns;               /* Average acquire latency */
    float utilization;                  /* Current utilization (0-1) */
} layer_stats_t;

/**
 * @brief Cross-layer fairness metrics
 *
 * MATHEMATICAL DEFINITION (Jain's Fairness Index):
 * F = (Σ x_i)² / (n × Σ x_i²)
 * where x_i = allocation rate for layer i
 */
typedef struct {
    float jains_fairness_index;         /* F ∈ [1/n, 1] */
    float allocation_variance;          /* σ² of layer allocations */
    float max_layer_utilization;        /* Max util across layers */
    float min_layer_utilization;        /* Min util across layers */
    uint32_t most_active_layer;         /* Layer with highest activity */
    uint32_t least_active_layer;        /* Layer with lowest activity */
} fairness_metrics_t;

/**
 * @brief Cross-entropy metrics for allocation patterns
 *
 * MATHEMATICAL DEFINITION:
 * H(p,q) = -Σ p(i) log q(i)
 * where p = expected distribution, q = observed distribution
 */
typedef struct {
    float cross_entropy;                /* H(expected, observed) */
    float kl_divergence;                /* D_KL(p||q) = H(p,q) - H(p) */
    float expected_entropy;             /* H(expected) */
    float observed_entropy;             /* H(observed) */
    float efficiency;                   /* 1 - (KL / H_max) */
} cross_entropy_metrics_t;

/**
 * @brief Comprehensive layer pools metrics
 */
typedef struct {
    /* Per-layer statistics */
    layer_stats_t brain_stats;
    layer_stats_t middleware_stats;
    layer_stats_t cognitive_stats;
    layer_stats_t training_stats;

    /* Cross-layer metrics */
    fairness_metrics_t fairness;
    cross_entropy_metrics_t cross_entropy;

    /* Aggregate metrics */
    uint64_t total_memory_bytes;        /* Total pool memory */
    uint64_t used_memory_bytes;         /* Currently in use */
    uint64_t cross_layer_transfers;     /* Inter-layer borrows */
    float overall_utilization;          /* Aggregate utilization */
    float speedup_vs_malloc;            /* Measured speedup */

    /* Timing */
    uint64_t uptime_ms;                 /* Time since creation */
    uint64_t last_rebalance_ms;         /* Last pool rebalancing */
} layer_pools_metrics_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default layer pools configuration
 *
 * MATHEMATICAL BASIS:
 * Sizes computed using multi-queue M/M/c model with layer-specific
 * arrival rates: λ_cognitive=100/s, λ_middleware=1000/s, λ_training=500/s
 *
 * @return Default configuration
 */
layer_pools_config_t layer_pools_default_config(void);

/**
 * @brief Get configuration optimized for training workload
 *
 * @return Training-optimized configuration
 */
layer_pools_config_t layer_pools_training_config(void);

/**
 * @brief Get configuration optimized for inference workload
 *
 * @return Inference-optimized configuration
 */
layer_pools_config_t layer_pools_inference_config(void);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create layer pools with given configuration
 *
 * @param config Pool configuration (NULL for defaults)
 * @param brain_pools Existing brain pools to integrate (NULL to create new)
 * @return Layer pools handle or NULL on error
 */
layer_pools_t layer_pools_create(
    const layer_pools_config_t* config,
    brain_pools_t brain_pools);

/**
 * @brief Destroy layer pools and release all memory
 *
 * @param pools Layer pools handle
 */
void layer_pools_destroy(layer_pools_t pools);

/**
 * @brief Get the underlying brain pools handle
 *
 * @param pools Layer pools handle
 * @return Brain pools handle
 */
brain_pools_t layer_pools_get_brain_pools(layer_pools_t pools);

//=============================================================================
// Cognitive Layer Pool API
//=============================================================================

/**
 * @brief Acquire workspace entry for global workspace
 *
 * @param pools Layer pools handle
 * @return Pointer to workspace entry or NULL
 */
void* layer_pools_acquire_workspace_entry(layer_pools_t pools);

/**
 * @brief Release workspace entry
 *
 * @param pools Layer pools handle
 * @param entry Previously acquired entry
 */
void layer_pools_release_workspace_entry(layer_pools_t pools, void* entry);

/**
 * @brief Acquire knowledge base entry
 *
 * @param pools Layer pools handle
 * @return Pointer to knowledge entry or NULL
 */
void* layer_pools_acquire_knowledge_entry(layer_pools_t pools);

/**
 * @brief Release knowledge entry
 *
 * @param pools Layer pools handle
 * @param entry Previously acquired entry
 */
void layer_pools_release_knowledge_entry(layer_pools_t pools, void* entry);

/**
 * @brief Acquire working memory item
 *
 * @param pools Layer pools handle
 * @param item_size Size of item in bytes
 * @return Pointer to item or NULL
 */
void* layer_pools_acquire_working_memory_item(layer_pools_t pools, size_t item_size);

/**
 * @brief Release working memory item
 *
 * @param pools Layer pools handle
 * @param item Previously acquired item
 * @param item_size Size that was allocated
 */
void layer_pools_release_working_memory_item(layer_pools_t pools, void* item, size_t item_size);

//=============================================================================
// Middleware Layer Pool API
//=============================================================================

/**
 * @brief Acquire event queue entry
 *
 * @param pools Layer pools handle
 * @return Pointer to event entry or NULL
 */
void* layer_pools_acquire_event_entry(layer_pools_t pools);

/**
 * @brief Release event entry
 *
 * @param pools Layer pools handle
 * @param entry Previously acquired entry
 */
void layer_pools_release_event_entry(layer_pools_t pools, void* entry);

/**
 * @brief Acquire pattern node
 *
 * @param pools Layer pools handle
 * @return Pointer to pattern node or NULL
 */
void* layer_pools_acquire_pattern_node(layer_pools_t pools);

/**
 * @brief Release pattern node
 *
 * @param pools Layer pools handle
 * @param node Previously acquired node
 */
void layer_pools_release_pattern_node(layer_pools_t pools, void* node);

/**
 * @brief Acquire routing table node
 *
 * @param pools Layer pools handle
 * @return Pointer to route node or NULL
 */
void* layer_pools_acquire_route_node(layer_pools_t pools);

/**
 * @brief Release route node
 *
 * @param pools Layer pools handle
 * @param node Previously acquired node
 */
void layer_pools_release_route_node(layer_pools_t pools, void* node);

/**
 * @brief Acquire feature extraction buffer
 *
 * @param pools Layer pools handle
 * @param num_features Number of features
 * @return Pointer to float array or NULL
 */
float* layer_pools_acquire_feature_buffer(layer_pools_t pools, size_t num_features);

/**
 * @brief Release feature buffer
 *
 * @param pools Layer pools handle
 * @param buffer Previously acquired buffer
 */
void layer_pools_release_feature_buffer(layer_pools_t pools, float* buffer);

/**
 * @brief Acquire subscriber entry
 *
 * @param pools Layer pools handle
 * @return Pointer to subscriber entry or NULL
 */
void* layer_pools_acquire_subscriber_entry(layer_pools_t pools);

/**
 * @brief Release subscriber entry
 *
 * @param pools Layer pools handle
 * @param entry Previously acquired entry
 */
void layer_pools_release_subscriber_entry(layer_pools_t pools, void* entry);

//=============================================================================
// Training Layer Pool API
//=============================================================================

/**
 * @brief Acquire learning signal structure
 *
 * @param pools Layer pools handle
 * @return Pointer to learning signal or NULL
 */
void* layer_pools_acquire_learning_signal(layer_pools_t pools);

/**
 * @brief Release learning signal
 *
 * @param pools Layer pools handle
 * @param signal Previously acquired signal
 */
void layer_pools_release_learning_signal(layer_pools_t pools, void* signal);

/**
 * @brief Acquire target/prediction buffer pair
 *
 * @param pools Layer pools handle
 * @param num_outputs Number of output values
 * @param target Output: target buffer pointer
 * @param prediction Output: prediction buffer pointer
 * @return true on success
 */
bool layer_pools_acquire_target_prediction(
    layer_pools_t pools,
    size_t num_outputs,
    float** target,
    float** prediction);

/**
 * @brief Release target/prediction buffer pair
 *
 * @param pools Layer pools handle
 * @param target Target buffer
 * @param prediction Prediction buffer
 */
void layer_pools_release_target_prediction(
    layer_pools_t pools,
    float* target,
    float* prediction);

/**
 * @brief Acquire gradient buffer for backpropagation
 *
 * @param pools Layer pools handle
 * @param num_weights Number of weights
 * @return Pointer to gradient buffer or NULL
 */
float* layer_pools_acquire_gradient_buffer(layer_pools_t pools, size_t num_weights);

/**
 * @brief Release gradient buffer
 *
 * @param pools Layer pools handle
 * @param buffer Previously acquired buffer
 */
void layer_pools_release_gradient_buffer(layer_pools_t pools, float* buffer);

/**
 * @brief Acquire batch processing buffer
 *
 * @param pools Layer pools handle
 * @param batch_size Number of examples
 * @param example_size Size per example in bytes
 * @return Pointer to batch buffer or NULL
 */
void* layer_pools_acquire_batch_buffer(
    layer_pools_t pools,
    size_t batch_size,
    size_t example_size);

/**
 * @brief Release batch buffer
 *
 * @param pools Layer pools handle
 * @param buffer Previously acquired buffer
 * @param total_size Total buffer size
 */
void layer_pools_release_batch_buffer(
    layer_pools_t pools,
    void* buffer,
    size_t total_size);

//=============================================================================
// Cross-Layer Operations
//=============================================================================

/**
 * @brief Borrow capacity from another layer
 *
 * MATHEMATICAL NOTE: Cross-layer borrowing uses work-stealing algorithm
 * Borrow probability = (1 - util_source) × util_target
 *
 * @param pools Layer pools handle
 * @param target_layer Layer that needs capacity
 * @param source_layer Layer to borrow from
 * @param num_blocks Number of blocks to borrow
 * @return Number of blocks actually borrowed
 */
size_t layer_pools_borrow(
    layer_pools_t pools,
    uint32_t target_layer,
    uint32_t source_layer,
    size_t num_blocks);

/**
 * @brief Return borrowed capacity to source layer
 *
 * @param pools Layer pools handle
 * @param source_layer Original owning layer
 * @param num_blocks Number of blocks to return
 */
void layer_pools_return(
    layer_pools_t pools,
    uint32_t source_layer,
    size_t num_blocks);

/**
 * @brief Rebalance pools based on observed usage patterns
 *
 * Uses Shannon entropy and queuing theory to optimize allocation
 *
 * @param pools Layer pools handle
 * @return true if rebalancing occurred
 */
bool layer_pools_rebalance(layer_pools_t pools);

//=============================================================================
// Metrics API
//=============================================================================

/**
 * @brief Get comprehensive layer pools metrics
 *
 * @param pools Layer pools handle
 * @param metrics Output metrics structure
 * @return true on success
 */
bool layer_pools_get_metrics(layer_pools_t pools, layer_pools_metrics_t* metrics);

/**
 * @brief Get statistics for a specific layer
 *
 * @param pools Layer pools handle
 * @param layer Layer identifier (LAYER_POOL_*)
 * @param stats Output statistics
 * @return true on success
 */
bool layer_pools_get_layer_stats(
    layer_pools_t pools,
    uint32_t layer,
    layer_stats_t* stats);

/**
 * @brief Get fairness metrics across layers
 *
 * @param pools Layer pools handle
 * @param fairness Output fairness metrics
 * @return true on success
 */
bool layer_pools_get_fairness_metrics(
    layer_pools_t pools,
    fairness_metrics_t* fairness);

/**
 * @brief Get cross-entropy metrics for allocation patterns
 *
 * @param pools Layer pools handle
 * @param ce Output cross-entropy metrics
 * @return true on success
 */
bool layer_pools_get_cross_entropy_metrics(
    layer_pools_t pools,
    cross_entropy_metrics_t* ce);

/**
 * @brief Reset all metrics counters
 *
 * @param pools Layer pools handle
 */
void layer_pools_reset_metrics(layer_pools_t pools);

/**
 * @brief Check if all layers meet performance targets
 *
 * @param pools Layer pools handle
 * @return true if all layers performant
 */
bool layer_pools_is_performant(layer_pools_t pools);

/**
 * @brief Get recommended configuration based on observed patterns
 *
 * @param pools Layer pools handle
 * @param recommended Output recommended configuration
 * @return true on success
 */
bool layer_pools_get_recommended_config(
    layer_pools_t pools,
    layer_pools_config_t* recommended);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Calculate total memory required for configuration
 *
 * @param config Pool configuration
 * @return Total bytes required
 */
size_t layer_pools_calculate_memory(const layer_pools_config_t* config);

/**
 * @brief Get layer name string
 *
 * @param layer Layer identifier
 * @return Layer name string
 */
const char* layer_pools_get_layer_name(uint32_t layer);

/**
 * @brief Print layer pools status to stdout
 *
 * @param pools Layer pools handle
 */
void layer_pools_print_status(layer_pools_t pools);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LAYER_POOLS_H */
