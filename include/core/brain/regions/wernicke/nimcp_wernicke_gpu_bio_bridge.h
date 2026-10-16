/**
 * @file nimcp_wernicke_gpu_bio_bridge.h
 * @brief Bio-Async Bridge for GPU-Accelerated Wernicke's Area
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Connects Wernicke GPU module to bio-async messaging system
 * WHY:  Enable GPU-accelerated language comprehension in bio-async pipeline
 * HOW:  Bridge pattern wrapping GPU context with message handlers
 *
 * USAGE:
 * ======
 * @code
 * // Create GPU context
 * nimcp_gpu_context_t* gpu = nimcp_gpu_context_create(0);
 * wernicke_gpu_context_t* wernicke_gpu = wernicke_gpu_create(gpu, NULL);
 *
 * // Create bio bridge
 * wernicke_gpu_bio_bridge_t* bridge = wernicke_gpu_bio_create(wernicke_gpu, NULL);
 * wernicke_gpu_bio_connect(bridge);
 *
 * // Process frames
 * wernicke_gpu_bio_add_frame(bridge, &spectral_frame);
 *
 * // Update (processes bio-async messages)
 * wernicke_gpu_bio_update(bridge);
 *
 * // Cleanup
 * wernicke_gpu_bio_destroy(bridge);
 * @endcode
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WERNICKE_GPU_BIO_BRIDGE_H
#define NIMCP_WERNICKE_GPU_BIO_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include GPU types BEFORE extern "C" (contains C++ templates) */
#include "gpu/cognitive/nimcp_wernicke_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *=============================================================================*/

typedef struct wernicke_gpu_bio_bridge wernicke_gpu_bio_bridge_t;

/*=============================================================================
 * Constants
 *=============================================================================*/

/** @brief Bio-async module ID for Wernicke GPU bridge */
#define BIO_MODULE_WERNICKE_GPU_BRIDGE    0x0E5A

/*=============================================================================
 * Configuration
 *=============================================================================*/

/**
 * @brief Configuration for GPU bio bridge
 */
typedef struct {
    /* Bio-async settings */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    uint32_t inbox_capacity;         /**< Message inbox capacity */

    /* Batch processing */
    uint32_t batch_threshold;        /**< Frames before auto-processing */
    uint32_t max_batch_size;         /**< Maximum batch size */
    bool auto_process_batch;         /**< Auto-process when threshold reached */

    /* Result routing */
    bool send_phoneme_results;       /**< Send phoneme results via bio-async */
    bool send_word_results;          /**< Send word candidates via bio-async */
    bool send_semantic_results;      /**< Send semantic activations via bio-async */
} wernicke_gpu_bio_config_t;

/**
 * @brief Get default configuration
 */
wernicke_gpu_bio_config_t wernicke_gpu_bio_default_config(void);

/*=============================================================================
 * Statistics
 *=============================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t messages_processed;     /**< Total bio-async messages processed */
    uint64_t gpu_dispatches;         /**< Total GPU batch dispatches */
    uint64_t results_sent;           /**< Total results sent via bio-async */
    uint32_t current_batch_size;     /**< Current pending batch size */
    bool bio_connected;              /**< Bio-async connection status */

    /* GPU statistics */
    uint64_t gpu_phoneme_recognitions;   /**< GPU phoneme recognitions */
    uint64_t gpu_word_recognitions;      /**< GPU word recognitions */
    uint64_t gpu_spreading_activations;  /**< GPU spreading activations */
} wernicke_gpu_bio_stats_t;

/*=============================================================================
 * Lifecycle API
 *=============================================================================*/

/**
 * @brief Create GPU bio bridge
 *
 * @param gpu_ctx GPU Wernicke context (can be NULL, set later)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
wernicke_gpu_bio_bridge_t* wernicke_gpu_bio_create(
    wernicke_gpu_context_t* gpu_ctx,
    const wernicke_gpu_bio_config_t* config
);

/**
 * @brief Destroy GPU bio bridge
 *
 * @param bridge Bridge to destroy
 */
void wernicke_gpu_bio_destroy(wernicke_gpu_bio_bridge_t* bridge);

/*=============================================================================
 * GPU Context Management
 *=============================================================================*/

/**
 * @brief Set GPU context
 *
 * @param bridge Bridge handle
 * @param gpu_ctx GPU Wernicke context
 * @return 0 on success, -1 on error
 */
int wernicke_gpu_bio_set_gpu_context(
    wernicke_gpu_bio_bridge_t* bridge,
    wernicke_gpu_context_t* gpu_ctx
);

/**
 * @brief Get GPU context
 *
 * @param bridge Bridge handle
 * @return GPU context or NULL
 */
wernicke_gpu_context_t* wernicke_gpu_bio_get_gpu_context(
    const wernicke_gpu_bio_bridge_t* bridge
);

/*=============================================================================
 * Bio-Async Connection
 *=============================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int wernicke_gpu_bio_connect(wernicke_gpu_bio_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int wernicke_gpu_bio_disconnect(wernicke_gpu_bio_bridge_t* bridge);

/**
 * @brief Check connection status
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool wernicke_gpu_bio_is_connected(const wernicke_gpu_bio_bridge_t* bridge);

/*=============================================================================
 * Batch Processing
 *=============================================================================*/

/**
 * @brief Process current batch on GPU
 *
 * @param bridge Bridge handle
 * @return Number of results generated, or -1 on error
 */
int wernicke_gpu_bio_process_batch(wernicke_gpu_bio_bridge_t* bridge);

/**
 * @brief Add spectral frame to batch
 *
 * @param bridge Bridge handle
 * @param frame Spectral frame to add
 * @return 0 on success, -1 on error
 */
int wernicke_gpu_bio_add_frame(
    wernicke_gpu_bio_bridge_t* bridge,
    const wernicke_gpu_spectral_frame_t* frame
);

/*=============================================================================
 * Update / Poll
 *=============================================================================*/

/**
 * @brief Update bridge (process bio-async messages)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int wernicke_gpu_bio_update(wernicke_gpu_bio_bridge_t* bridge);

/*=============================================================================
 * Statistics
 *=============================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int wernicke_gpu_bio_get_stats(
    const wernicke_gpu_bio_bridge_t* bridge,
    wernicke_gpu_bio_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 */
void wernicke_gpu_bio_reset_stats(wernicke_gpu_bio_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_GPU_BIO_BRIDGE_H */
