//=============================================================================
// nimcp_cloud_inference.h - Edge-Cloud Hybrid Inference Bridge
//=============================================================================
/**
 * @file nimcp_cloud_inference.h
 * @brief Confidence-gated edge-cloud inference with online distillation
 *
 * WHAT: Routes inference between local (edge) and remote (cloud) brains
 * WHY:  Small devices can do fast local inference for easy cases, escalating
 *       to a powerful backend only when uncertain — reducing latency and
 *       network traffic while maintaining accuracy
 * HOW:  After local brain_decide(), check confidence against threshold.
 *       If below threshold, call cloud backend (callback-based, supports
 *       any transport: in-process, HTTP, gRPC, shared memory).
 *       Cloud responses optionally distill back into the local brain.
 *
 * ARCHITECTURE:
 *   Input → Local Brain → confidence ≥ threshold? → Return local result
 *                        → confidence < threshold? → Cloud Backend
 *                                                  → Return cloud result
 *                                                  → Distill into local brain
 *
 * BIOLOGICAL ANALOGY:
 *   Analogous to fast vs. slow thinking (Kahneman System 1/2):
 *   - System 1 (local): Fast, automatic, low-confidence triggers escalation
 *   - System 2 (cloud): Slow, deliberate, high-accuracy
 *   - Distillation: Repeated System 2 patterns become System 1 habits
 */

#ifndef NIMCP_CLOUD_INFERENCE_H
#define NIMCP_CLOUD_INFERENCE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

struct brain_struct;
typedef struct brain_struct* brain_t;

// brain_decision_t forward declaration (full def in nimcp_brain.h)
typedef struct brain_decision brain_decision_t;

//=============================================================================
// Types
//=============================================================================

/** How the decision was routed */
typedef enum {
    CLOUD_ROUTE_LOCAL       = 0,  /**< Handled entirely by local brain */
    CLOUD_ROUTE_CLOUD       = 1,  /**< Escalated to cloud backend */
    CLOUD_ROUTE_DISTILLED   = 2,  /**< Cloud answered + local brain learned */
    CLOUD_ROUTE_FALLBACK    = 3   /**< Cloud failed, used local result anyway */
} cloud_route_t;

/**
 * Cloud backend callback type.
 *
 * The bridge calls this when local confidence is below threshold.
 * Implementer should:
 *   1. Send features to the backend (another brain, HTTP endpoint, etc.)
 *   2. Receive and return the backend's decision
 *   3. Return NULL on failure (bridge will fall back to local result)
 *
 * The caller owns the returned decision and must free it with brain_free_decision().
 *
 * @param features      Input feature vector
 * @param num_features  Feature count
 * @param user_data     User-provided context (endpoint URL, brain pointer, etc.)
 * @return Backend decision (caller frees) or NULL on failure
 */
typedef brain_decision_t* (*cloud_backend_fn)(
    const float* features,
    uint32_t num_features,
    void* user_data
);

/** Configuration for cloud inference bridge */
typedef struct cloud_inference_config {
    float confidence_threshold;       /**< Escalate if local confidence below this (default: 0.5) */
    float distillation_lr;            /**< Learning rate for distillation updates (default: 0.001) */
    bool enable_distillation;         /**< Train local brain from cloud answers (default: true) */
    uint32_t distillation_buffer_size; /**< Max buffered distillation examples (default: 64) */
    uint32_t compression_dim;         /**< Query compression dimension (0 = disabled) */
    uint32_t max_cloud_failures;      /**< Disable cloud after N consecutive failures (default: 10) */
    float local_improvement_threshold; /**< Disable cloud when local accuracy exceeds this (default: 0.95) */
} cloud_inference_config_t;

/** Statistics tracked by the bridge */
typedef struct cloud_inference_stats {
    uint64_t total_queries;           /**< Total inference requests */
    uint64_t local_handled;           /**< Handled by local brain */
    uint64_t cloud_escalated;         /**< Sent to cloud backend */
    uint64_t cloud_succeeded;         /**< Cloud returned valid answer */
    uint64_t cloud_failed;            /**< Cloud returned NULL */
    uint64_t distillation_steps;      /**< Times local brain was trained from cloud */
    float avg_local_confidence;       /**< Running average local confidence */
    float avg_cloud_confidence;       /**< Running average cloud confidence */
    float local_accuracy_estimate;    /**< Estimated local accuracy (from distillation agreement) */
    uint32_t consecutive_cloud_failures; /**< Current failure streak */
} cloud_inference_stats_t;

/** Distillation buffer entry */
typedef struct distillation_example {
    float* input;                     /**< Input features [num_features] */
    float* cloud_output;              /**< Cloud output vector [num_outputs] */
    char label[64];                   /**< Cloud-assigned label */
    float cloud_confidence;           /**< Cloud decision confidence */
    uint32_t num_features;            /**< Input dimension */
    uint32_t num_outputs;             /**< Output dimension */
} distillation_example_t;

/** Cloud inference bridge (opaque internals) */
typedef struct cloud_inference_bridge {
    cloud_inference_config_t config;
    cloud_inference_stats_t stats;

    // Backend
    cloud_backend_fn backend_fn;      /**< Cloud backend callback */
    void* backend_user_data;          /**< User data passed to callback */
    bool backend_available;           /**< Cloud is reachable/enabled */

    // Distillation buffer (circular)
    distillation_example_t* distill_buffer;
    uint32_t distill_head;
    uint32_t distill_count;
    uint32_t distill_capacity;

    // Query compression (optional learned projection)
    float* compression_matrix;        /**< [input_dim × compression_dim] row-major */
    float* compressed_buf;            /**< [compression_dim] scratch buffer */
    uint32_t input_dim;
    uint32_t compress_dim;

    // Confidence EMA tracking
    float ema_local_conf;
    float ema_cloud_conf;
} cloud_inference_bridge_t;

//=============================================================================
// API
//=============================================================================

/**
 * @brief Get default cloud inference configuration
 * @return Default config with sensible values
 */
cloud_inference_config_t cloud_inference_default_config(void);

/**
 * @brief Create a cloud inference bridge
 *
 * @param config Configuration (use cloud_inference_default_config() as starting point)
 * @param backend_fn Callback for cloud inference (required)
 * @param user_data Context passed to backend_fn (e.g., brain pointer, URL string)
 * @return Bridge handle or NULL on failure
 */
cloud_inference_bridge_t* cloud_inference_create(
    const cloud_inference_config_t* config,
    cloud_backend_fn backend_fn,
    void* user_data
);

/**
 * @brief Destroy cloud inference bridge and free all resources
 * @param bridge Bridge to destroy
 */
void cloud_inference_destroy(cloud_inference_bridge_t* bridge);

/**
 * @brief Route a decision through the edge-cloud bridge
 *
 * If local_decision->confidence >= threshold, returns LOCAL route.
 * Otherwise, calls cloud backend, optionally distills result, and returns
 * the cloud decision (replacing local_decision contents).
 *
 * @param bridge Cloud inference bridge
 * @param local_brain Local brain (for distillation learning)
 * @param local_decision Decision from local brain (may be upgraded in-place)
 * @param features Original input features
 * @param num_features Feature count
 * @return Route taken (LOCAL, CLOUD, DISTILLED, or FALLBACK)
 */
cloud_route_t cloud_inference_route(
    cloud_inference_bridge_t* bridge,
    brain_t local_brain,
    brain_decision_t* local_decision,
    const float* features,
    uint32_t num_features
);

/**
 * @brief Process buffered distillation examples
 *
 * Call periodically (e.g., every N training steps) to train the local brain
 * from accumulated cloud responses. This is the "background learning" path
 * that gradually makes the local brain more capable.
 *
 * @param bridge Cloud inference bridge
 * @param local_brain Brain to train
 * @param max_examples Maximum examples to process (0 = all)
 * @return Number of examples processed
 */
uint32_t cloud_inference_distill_batch(
    cloud_inference_bridge_t* bridge,
    brain_t local_brain,
    uint32_t max_examples
);

/**
 * @brief Get bridge statistics
 * @param bridge Cloud inference bridge
 * @return Statistics snapshot
 */
cloud_inference_stats_t cloud_inference_get_stats(const cloud_inference_bridge_t* bridge);

/**
 * @brief Reset bridge statistics
 * @param bridge Cloud inference bridge
 */
void cloud_inference_reset_stats(cloud_inference_bridge_t* bridge);

/**
 * @brief Set/change the cloud backend
 *
 * @param bridge Cloud inference bridge
 * @param backend_fn New backend callback
 * @param user_data New user data
 */
void cloud_inference_set_backend(
    cloud_inference_bridge_t* bridge,
    cloud_backend_fn backend_fn,
    void* user_data
);

/**
 * @brief In-process backend: wraps another brain_t as the "cloud"
 *
 * Convenience function — use as backend_fn with a brain_t as user_data.
 * Calls brain_decide() on the backend brain and returns the result.
 */
brain_decision_t* cloud_backend_local_brain(
    const float* features,
    uint32_t num_features,
    void* user_data  /* brain_t */
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CLOUD_INFERENCE_H */
