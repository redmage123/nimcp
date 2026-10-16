/**
 * @file nimcp_wernicke_gpu_bio_bridge.c
 * @brief Bio-Async Bridge for GPU-Accelerated Wernicke's Area
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Connects Wernicke GPU module to bio-async messaging system
 * WHY:  Enable GPU-accelerated language comprehension in bio-async pipeline
 * HOW:  Bridge pattern wrapping GPU context with message handlers
 *
 * INTEGRATION ARCHITECTURE:
 * =========================
 *
 *   Bio-Async Router <---> GPU Bio Bridge <---> Wernicke GPU Context
 *                               |
 *                    +----------+----------+
 *                    |          |          |
 *              Phoneme     Lexical    Semantic
 *              Messages    Messages   Messages
 *
 * MESSAGE FLOW:
 * =============
 * 1. Audio spectral data arrives via bio-async
 * 2. Bridge dispatches to GPU for phoneme recognition
 * 3. GPU results trigger lexical access messages
 * 4. Semantic spreading activations sent to KG/memory
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_gpu_bio_bridge.h"
#include "gpu/cognitive/nimcp_wernicke_gpu.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "WERNICKE_GPU_BIO"

//=============================================================================
#include <stddef.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wernicke_gpu_bio_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Constants
 *=============================================================================*/

/** @brief Bio-async module ID for Wernicke GPU bridge */
#define BIO_MODULE_WERNICKE_GPU_BRIDGE    0x0E5A

/** @brief Default inbox capacity */
#define DEFAULT_INBOX_CAPACITY            64

/** @brief Maximum batch size for GPU processing */
#define MAX_GPU_BATCH_SIZE                NIMCP_LARGE_BATCH_SIZE

/*=============================================================================
 * Internal Structure
 *=============================================================================*/

struct wernicke_gpu_bio_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* GPU context */
    wernicke_gpu_context_t* gpu_ctx;
    bool owns_gpu_ctx;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_connected;

    /* Configuration */
    wernicke_gpu_bio_config_t config;

    /* Batch buffers */
    wernicke_gpu_spectral_frame_t* batch_frames;
    wernicke_gpu_phoneme_result_t* batch_phonemes;
    wernicke_gpu_word_candidate_t* batch_words;
    wernicke_gpu_activation_result_t* batch_activations;
    uint32_t batch_count;

    /* Statistics */
    uint64_t messages_processed;
    uint64_t gpu_dispatches;
    uint64_t results_sent;
};

/*=============================================================================
 * Message Handlers
 *=============================================================================*/

/**
 * @brief Handle spectral frame input for phoneme recognition
 */
static nimcp_error_t handle_spectral_input(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    wernicke_gpu_bio_bridge_t* bridge = (wernicke_gpu_bio_bridge_t*)user_data;
    if (!bridge || !msg || !bridge->gpu_ctx) return NIMCP_ERROR_INVALID_PARAM;

    /* Extract spectral frame from message */
    if (msg_size < sizeof(wernicke_gpu_spectral_frame_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const wernicke_gpu_spectral_frame_t* frame =
        (const wernicke_gpu_spectral_frame_t*)msg;

    /* Add to batch */
    if (bridge->batch_count < MAX_GPU_BATCH_SIZE) {
        memcpy(&bridge->batch_frames[bridge->batch_count], frame,
               sizeof(wernicke_gpu_spectral_frame_t));
        bridge->batch_count++;
    }

    /* Process batch if full or if processing requested */
    if (bridge->batch_count >= bridge->config.batch_threshold) {
        wernicke_gpu_bio_process_batch(bridge);
    }

    bridge->messages_processed++;
    (void)response_promise;
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle GPU comprehension request
 */
static nimcp_error_t handle_comprehension_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    wernicke_gpu_bio_bridge_t* bridge = (wernicke_gpu_bio_bridge_t*)user_data;
    if (!bridge || !bridge->gpu_ctx) return NIMCP_ERROR_INVALID_PARAM;

    /* Force batch processing */
    if (bridge->batch_count > 0) {
        wernicke_gpu_bio_process_batch(bridge);
    }

    bridge->messages_processed++;
    (void)msg;
    (void)msg_size;
    (void)response_promise;
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle precision update for GPU processing
 */
static nimcp_error_t handle_gpu_precision_update(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    wernicke_gpu_bio_bridge_t* bridge = (wernicke_gpu_bio_bridge_t*)user_data;
    if (!bridge) return NIMCP_ERROR_INVALID_PARAM;

    /* Precision update affects GPU batch threshold */
    bridge->messages_processed++;

    (void)msg;
    (void)msg_size;
    (void)response_promise;
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * Configuration API
 *=============================================================================*/

wernicke_gpu_bio_config_t wernicke_gpu_bio_default_config(void) {
    wernicke_gpu_bio_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_bio_async = true;
    config.inbox_capacity = DEFAULT_INBOX_CAPACITY;
    config.batch_threshold = 16;
    config.max_batch_size = MAX_GPU_BATCH_SIZE;
    config.auto_process_batch = true;
    config.send_phoneme_results = true;
    config.send_word_results = true;
    config.send_semantic_results = true;

    return config;
}

/*=============================================================================
 * Lifecycle API
 *=============================================================================*/

wernicke_gpu_bio_bridge_t* wernicke_gpu_bio_create(
    wernicke_gpu_context_t* gpu_ctx,
    const wernicke_gpu_bio_config_t* config)
{
    wernicke_gpu_bio_bridge_t* bridge = nimcp_calloc(1, sizeof(wernicke_gpu_bio_bridge_t));
    if (!bridge) {
        LOG_ERROR("Failed to allocate GPU bio bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->config = config ? *config : wernicke_gpu_bio_default_config();

    /* Use provided GPU context or note that we need one */
    if (gpu_ctx) {
        bridge->gpu_ctx = gpu_ctx;
        bridge->owns_gpu_ctx = false;
    } else {
        bridge->gpu_ctx = NULL;  /* Must be set later */
        bridge->owns_gpu_ctx = false;
    }

    /* Allocate batch buffers */
    bridge->batch_frames = nimcp_calloc(MAX_GPU_BATCH_SIZE,
                                   sizeof(wernicke_gpu_spectral_frame_t));
    bridge->batch_phonemes = nimcp_calloc(MAX_GPU_BATCH_SIZE,
                                     sizeof(wernicke_gpu_phoneme_result_t));
    bridge->batch_words = nimcp_calloc(bridge->config.max_batch_size,
                                  sizeof(wernicke_gpu_word_candidate_t));
    bridge->batch_activations = nimcp_calloc(bridge->config.max_batch_size,
                                        sizeof(wernicke_gpu_activation_result_t));

    if (!bridge->batch_frames || !bridge->batch_phonemes ||
        !bridge->batch_words || !bridge->batch_activations) {
        LOG_ERROR("Failed to allocate batch buffers");
        wernicke_gpu_bio_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_gpu_bio_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "wernicke_gpu_bio") != 0) {
        wernicke_gpu_bio_destroy(bridge);
        return NULL;
    }

    LOG_INFO("Created Wernicke GPU bio bridge");
    return bridge;
}

void wernicke_gpu_bio_destroy(wernicke_gpu_bio_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "wernicke_gpu_bio");

    /* Cleanup bridge base */
    bridge_base_cleanup(&bridge->base);

    /* Disconnect bio-async */
    if (bridge->bio_connected) {
        wernicke_gpu_bio_disconnect(bridge);
    }

    /* Free GPU context if we own it */
    if (bridge->owns_gpu_ctx && bridge->gpu_ctx) {
        wernicke_gpu_destroy(bridge->gpu_ctx);
    }

    /* Free batch buffers */
    nimcp_free(bridge->batch_frames);
    nimcp_free(bridge->batch_phonemes);
    nimcp_free(bridge->batch_words);
    nimcp_free(bridge->batch_activations);

    nimcp_free(bridge);
    LOG_DEBUG("Destroyed Wernicke GPU bio bridge");
}

/*=============================================================================
 * GPU Context Management
 *=============================================================================*/

int wernicke_gpu_bio_set_gpu_context(
    wernicke_gpu_bio_bridge_t* bridge,
    wernicke_gpu_context_t* gpu_ctx)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    if (bridge->owns_gpu_ctx && bridge->gpu_ctx) {
        wernicke_gpu_destroy(bridge->gpu_ctx);
    }

    bridge->gpu_ctx = gpu_ctx;
    bridge->owns_gpu_ctx = false;

    return 0;
}

wernicke_gpu_context_t* wernicke_gpu_bio_get_gpu_context(
    const wernicke_gpu_bio_bridge_t* bridge)
{
    return bridge ? bridge->gpu_ctx : NULL;
}

/*=============================================================================
 * Bio-Async Connection
 *=============================================================================*/

int wernicke_gpu_bio_connect(wernicke_gpu_bio_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->bio_connected) return 0;
    if (!bridge->config.enable_bio_async) return 0;

    if (!bio_router_is_initialized()) {
        LOG_WARN("Bio router not initialized, skipping connection");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_WERNICKE_GPU_BRIDGE,
        .module_name = "wernicke_gpu_bridge",
        .inbox_capacity = bridge->config.inbox_capacity,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        LOG_ERROR("Failed to register with bio router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_bio_connect: bridge->bio_ctx is NULL");
        return -1;
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->bio_ctx,
                                 BIO_MSG_OMNI_PREDICT_REQUEST,
                                 handle_comprehension_request);
    bio_router_register_handler(bridge->bio_ctx,
                                 BIO_MSG_OMNI_PRECISION_UPDATE,
                                 handle_gpu_precision_update);

    /* Custom spectral input message handler */
    /* Note: Using a generic message type for spectral data */
    bio_router_register_handler(bridge->bio_ctx,
                                 BIO_MSG_PRED_HIER_FORWARD,
                                 handle_spectral_input);

    bridge->bio_connected = true;
    LOG_INFO("Connected Wernicke GPU bridge to bio-async");

    return 0;
}

int wernicke_gpu_bio_disconnect(wernicke_gpu_bio_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->bio_connected) return 0;

    if (bridge->bio_ctx) {
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
    }

    bridge->bio_connected = false;
    LOG_DEBUG("Disconnected Wernicke GPU bridge from bio-async");

    return 0;
}

bool wernicke_gpu_bio_is_connected(const wernicke_gpu_bio_bridge_t* bridge) {
    return bridge ? bridge->bio_connected : false;
}

/*=============================================================================
 * Batch Processing
 *=============================================================================*/

int wernicke_gpu_bio_process_batch(wernicke_gpu_bio_bridge_t* bridge) {
    if (!bridge || !bridge->gpu_ctx || bridge->batch_count == 0) {
        return 0;
    }

    uint32_t num_words = 0;
    uint32_t num_activations = 0;

    /* Run full GPU comprehension pipeline */
    bool success = wernicke_gpu_comprehend(
        bridge->gpu_ctx,
        bridge->batch_frames,
        bridge->batch_count,
        bridge->batch_words,
        bridge->config.max_batch_size,
        &num_words,
        bridge->batch_activations,
        bridge->config.max_batch_size,
        &num_activations
    );

    if (!success) {
        LOG_WARN("GPU comprehension failed for batch of %u frames", bridge->batch_count);
        bridge->batch_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_gpu_bio_process_batch: GPU comprehension failed");
        return -1;
    }

    bridge->gpu_dispatches++;

    /* Send results via bio-async if connected */
    if (bridge->bio_connected && bridge->bio_ctx) {
        /* Word recognition results */
        if (bridge->config.send_word_results && num_words > 0) {
            /* Send word candidates to lexical processing */
            for (uint32_t i = 0; i < num_words; i++) {
                bio_router_send(bridge->bio_ctx,
                               &bridge->batch_words[i],
                               sizeof(wernicke_gpu_word_candidate_t),
                               0);  /* default timeout */
                bridge->results_sent++;
            }
        }

        /* Semantic activation results */
        if (bridge->config.send_semantic_results && num_activations > 0) {
            /* Send to semantic memory / KG */
            for (uint32_t i = 0; i < num_activations; i++) {
                bio_router_send(bridge->bio_ctx,
                               &bridge->batch_activations[i],
                               sizeof(wernicke_gpu_activation_result_t),
                               0);  /* default timeout */
                bridge->results_sent++;
            }
        }
    }

    /* Clear batch */
    bridge->batch_count = 0;

    return (int)(num_words + num_activations);
}

int wernicke_gpu_bio_add_frame(
    wernicke_gpu_bio_bridge_t* bridge,
    const wernicke_gpu_spectral_frame_t* frame)
{
    if (!bridge || !frame) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_bio_add_frame: required parameter is NULL (bridge, frame)");
        return -1;
    }

    if (bridge->batch_count >= MAX_GPU_BATCH_SIZE) {
        /* Auto-process if enabled */
        if (bridge->config.auto_process_batch) {
            wernicke_gpu_bio_process_batch(bridge);
        } else {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_gpu_bio_add_frame: validation failed");
            return -1;  /* Batch full */
        }
    }

    memcpy(&bridge->batch_frames[bridge->batch_count], frame,
           sizeof(wernicke_gpu_spectral_frame_t));
    bridge->batch_count++;

    /* Auto-process if threshold reached */
    if (bridge->config.auto_process_batch &&
        bridge->batch_count >= bridge->config.batch_threshold) {
        wernicke_gpu_bio_process_batch(bridge);
    }

    return 0;
}

/*=============================================================================
 * Update / Poll
 *=============================================================================*/

int wernicke_gpu_bio_update(wernicke_gpu_bio_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Process incoming bio-async messages */
    if (bridge->bio_connected && bridge->bio_ctx) {
        bio_router_process_inbox(bridge->bio_ctx, 10);
    }

    return 0;
}

/*=============================================================================
 * Statistics
 *=============================================================================*/

int wernicke_gpu_bio_get_stats(
    const wernicke_gpu_bio_bridge_t* bridge,
    wernicke_gpu_bio_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_bio_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(wernicke_gpu_bio_stats_t));
    stats->messages_processed = bridge->messages_processed;
    stats->gpu_dispatches = bridge->gpu_dispatches;
    stats->results_sent = bridge->results_sent;
    stats->current_batch_size = bridge->batch_count;
    stats->bio_connected = bridge->bio_connected;

    /* Get GPU stats if available */
    if (bridge->gpu_ctx) {
        wernicke_gpu_stats_t gpu_stats;
        if (wernicke_gpu_get_stats(bridge->gpu_ctx, &gpu_stats)) {
            stats->gpu_phoneme_recognitions = gpu_stats.phoneme_recognitions;
            stats->gpu_word_recognitions = gpu_stats.word_recognitions;
            stats->gpu_spreading_activations = gpu_stats.spreading_activations;
        }
    }

    return 0;
}

void wernicke_gpu_bio_reset_stats(wernicke_gpu_bio_bridge_t* bridge) {
    if (!bridge) return;

    bridge->messages_processed = 0;
    bridge->gpu_dispatches = 0;
    bridge->results_sent = 0;

    if (bridge->gpu_ctx) {
        wernicke_gpu_reset_stats(bridge->gpu_ctx);
    }
}
