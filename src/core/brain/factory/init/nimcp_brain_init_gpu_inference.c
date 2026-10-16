/**
 * @file nimcp_brain_init_gpu_inference.c
 * @brief GPU Inference Initialization for Brain Factory
 *
 * WHAT: Initializes GPU weight cache for inference during brain creation
 * WHY:  The GPU weight cache was only created during adaptive_network_create()
 *       which uses the adaptive network's own GPU context. But brain_decide()
 *       needs the adaptive network to have gpu_enabled=true and a valid cache.
 *       For brains where the adaptive network didn't auto-init GPU (e.g., layer
 *       config wasn't ready), this ensures GPU inference is available.
 * HOW:  After brain creation, check if adaptive network already has GPU enabled.
 *       If not, and brain has a GPU context, create and upload the weight cache.
 *
 * @version 1.0
 * @date 2026
 */

#include "core/brain/factory/init/nimcp_brain_init.h"
#include "core/brain/nimcp_brain_internal.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/execution/nimcp_gpu_detect.h"
#include "gpu/training/nimcp_training_bridge.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_GPU_INFERENCE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_gpu_inference, MESH_ADAPTER_CATEGORY_SYSTEM)

/**
 * @brief Initialize GPU inference for brain's adaptive network
 *
 * WHAT: Ensures the adaptive network has GPU weight cache for inference
 * WHY:  adaptive_network_create() may have already set up GPU, but if not,
 *       the brain's GPU context can be used to create the cache
 * HOW:  Check adaptive network state, create cache if missing, upload weights
 *
 * @param brain Brain instance (must have network and gpu_ctx initialized)
 * @return true on success (including CPU fallback), false on fatal error
 */
bool nimcp_brain_factory_init_gpu_inference(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_gpu_inference: brain is NULL");
        return false;
    }

    // No network means nothing to accelerate
    if (!brain->network) {
        return true;
    }

    // Skip for COW clones — they share the original's network
    if (brain->can_use_readonly) {
        return true;
    }

    // Check if adaptive network already has GPU enabled
    // (adaptive_network_create() does its own GPU init)
    neural_network_t base_net = adaptive_network_get_base_network(brain->network);
    if (!base_net) {
        return true;  // No base network, nothing to do
    }

    /* On a freshly-created brain, adaptive_network_create() has already
     * built the weight cache and we're done. On a loaded brain,
     * gpu_enabled is serialized as true but gpu_weight_cache is NOT
     * (pointers can't cross a checkpoint), so the "already enabled"
     * early return bypassed cache rebuild and the hot path silently
     * fell back to CPU. Check BOTH the flag and the cache pointer so
     * post-load brains actually rebuild the cache instead of lying to
     * callers via an unchecked flag. */
    if (adaptive_network_is_gpu_enabled(brain->network) &&
        adaptive_network_get_gpu_weight_cache(brain->network) != NULL) {
        LOG_INFO("Adaptive network already has GPU inference enabled");
        return true;
    }

    // Need brain-level GPU context to proceed
    if (!brain->gpu_ctx || !brain->gpu_enabled) {
        LOG_DEBUG("No brain GPU context — inference stays on CPU");
        return true;
    }

    // Get layer config from adaptive network
    const adaptive_network_config_t* config = adaptive_network_get_config(brain->network);
    if (!config || config->base_config.num_layers < 2 || !config->base_config.layer_sizes) {
        LOG_DEBUG("Adaptive network has no layer config — GPU inference skipped");
        return true;
    }

    // Create GPU weight cache using brain's GPU context
    nimcp_gpu_weight_cache_t* cache = nimcp_gpu_weight_cache_create(
        brain->gpu_ctx,
        base_net,
        config->base_config.layer_sizes,
        config->base_config.num_layers);

    if (!cache) {
        LOG_WARN("Failed to create GPU weight cache — inference stays on CPU");
        return true;  // Graceful fallback
    }

    // Upload initial weights
    if (!nimcp_gpu_weight_cache_upload(cache, base_net)) {
        LOG_WARN("Failed to upload weights to GPU — inference stays on CPU");
        nimcp_gpu_weight_cache_destroy(cache);
        return true;  // Graceful fallback
    }

    // Set GPU state on adaptive network
    adaptive_network_set_gpu_context(brain->network, brain->gpu_ctx);
    adaptive_network_set_gpu_weight_cache(brain->network, cache);
    adaptive_network_set_gpu_enabled(brain->network, true);

    LOG_INFO("GPU inference enabled for adaptive network (%u layers)",
             config->base_config.num_layers);

    return true;
}
