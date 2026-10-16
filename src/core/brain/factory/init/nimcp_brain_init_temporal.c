//=============================================================================
// nimcp_brain_init_temporal.c - Temporal Cortex Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_temporal.c
 * @brief Temporal Cortex Initialization Implementation
 *
 * WHAT: Initialization functions for temporal cortex
 * WHY:  Enable auditory, object recognition, and semantic memory capabilities
 * HOW:  Creates temporal adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase T1: Temporal Cortex Brain Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_temporal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_TEMPORAL"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_temporal, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Temporal cortex includes
#include "core/brain/regions/temporal/nimcp_temporal_adapter.h"
#include "core/brain/regions/temporal/nimcp_temporal_substrate_bridge.h"
#include "core/brain/regions/temporal/nimcp_temporal_thalamic_bridge.h"
#include "core/brain/regions/temporal/nimcp_temporal_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Connect temporal to bio-async messaging
 */
static bool connect_temporal_to_bio_async(brain_t brain) {
    if (!brain || !brain->temporal) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /*
         * TODO: Register temporal message handlers
         * bio_router_register_module(router, BIO_MODULE_TEMPORAL, brain->temporal);
         */
    }

    return true;
}

/**
 * @brief Connect temporal substrate bridge to neural substrate
 */
static bool connect_substrate_bridge(brain_t brain) {
    if (!brain || !brain->temporal_substrate_bridge) return true;

    /* Apply initial metabolic effects */
    if (temporal_substrate_bridge_update(brain->temporal_substrate_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Initial substrate bridge update failed");
    }

    return true;
}

/**
 * @brief Connect temporal thalamic bridge to thalamic router
 */
static bool connect_thalamic_bridge(brain_t brain) {
    if (!brain || !brain->temporal_thalamic_bridge) return true;

    /* Reset bridge to clean state */
    if (temporal_thalamic_bridge_reset(brain->temporal_thalamic_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Thalamic bridge reset failed");
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_temporal_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_temporal_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->temporal) {
        return true;  /* Already initialized */
    }

    /* Check if temporal is enabled in config */
    /* Default to enabled for perception-capable brains */
    if (!brain->config.enable_multimodal_integration &&
        !brain->visual_cortex && !brain->audio_cortex) {
        brain->temporal_enabled = false;
        return true;  /* Not enabled, not an error */
    }

    /* Create temporal adapter with default configuration */
    temporal_config_t temporal_cfg = temporal_default_config();

    /* Scale configuration based on brain config */
    if (brain->config.working_memory_capacity > 0) {
        temporal_cfg.working_memory_slots = brain->config.working_memory_capacity;
    }

    brain->temporal = temporal_create(&temporal_cfg);
    if (!brain->temporal) {
        set_error("Failed to create temporal adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_temporal_subsystem: brain->temporal is NULL");
        return false;
    }

    brain->temporal_enabled = true;
    brain->last_temporal_update_us = 0;

    /* Initialize integration bridges */
    if (!nimcp_brain_factory_init_temporal_substrate_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal substrate bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_temporal_thalamic_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal thalamic bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_temporal_quantum_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_temporal_to_working_memory(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal-WM connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_temporal_to_training(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal-Training connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_temporal_to_immune(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal-Immune connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_temporal_to_hippocampus(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal-Hippocampus connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_temporal_to_frontal(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal-Frontal connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_temporal_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Temporal bio-async connection failed (non-fatal)");
    }

    LOG_INFO(LOG_MODULE, "Temporal cortex initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_temporal_substrate_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_temporal_substrate_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->temporal_substrate_bridge) {
        return true;
    }

    /* Need temporal adapter first */
    if (!brain->temporal) {
        return true;  /* Not ready yet, will be called again */
    }

    /* Get neural substrate - may be NULL in simple configurations */
    neural_substrate_t* substrate = NULL;
    /*
     * TODO: Get substrate from brain if available
     * substrate = brain->substrate;
     */

    /* Create substrate bridge with default config */
    temporal_substrate_config_t config = temporal_substrate_default_config();

    brain->temporal_substrate_bridge = temporal_substrate_bridge_create(
        brain->temporal, substrate, &config);

    if (!brain->temporal_substrate_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create temporal substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_temporal_substrate_bridge: brain->temporal_substrate_bridge is NULL");
        return false;
    }

    /* Connect to substrate */
    connect_substrate_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Temporal substrate bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_temporal_thalamic_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_temporal_thalamic_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->temporal_thalamic_bridge) {
        return true;
    }

    /* Need temporal adapter first */
    if (!brain->temporal) {
        return true;  /* Not ready yet */
    }

    /* Get thalamic router - may be NULL in simple configurations */
    void* router = NULL;
    /*
     * TODO: Get thalamic router from brain if available
     * router = brain->thalamic_router;
     */

    /* Create thalamic bridge with default config */
    temporal_thalamic_config_t thal_config = temporal_thalamic_default_config();

    brain->temporal_thalamic_bridge = temporal_thalamic_bridge_create(
        brain->temporal, router, &thal_config);

    if (!brain->temporal_thalamic_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create temporal thalamic bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_temporal_thalamic_bridge: brain->temporal_thalamic_bridge is NULL");
        return false;
    }

    /* Connect to thalamus */
    connect_thalamic_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Temporal thalamic bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_temporal_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_temporal_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->temporal_quantum_bridge) {
        return true;
    }

    /* Need temporal adapter first */
    if (!brain->temporal) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        return true;  /* Not enabled, not an error */
    }

    /* Create quantum bridge with default config */
    temporal_quantum_config_t config = temporal_quantum_default_config();

    /* Scale search depth based on configuration */
    if (brain->config.num_outputs > 100) {
        config.concept_search_depth = brain->config.num_outputs * 2;
    }

    brain->temporal_quantum_bridge = temporal_quantum_bridge_create(
        brain->temporal, &config);

    if (!brain->temporal_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create temporal quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_temporal_quantum_bridge: brain->temporal_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Temporal quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_temporal_to_working_memory(brain_t brain) {
    if (!brain || !brain->temporal) {
        return true;  /* Nothing to connect */
    }

    /* Check if working memory is available */
    if (!brain->working_memory) {
        return true;  /* WM not initialized yet */
    }

    /*
     * Register temporal as a working memory consumer for concept access.
     * This allows temporal cortex to maintain active concepts during
     * perception and semantic processing.
     */

    /* TODO: Register with working memory
     * working_memory_register_consumer(brain->working_memory,
     *     WM_CONSUMER_TEMPORAL, brain->temporal);
     */

    LOG_DEBUG(LOG_MODULE, "Temporal connected to working memory");
    return true;
}

bool nimcp_brain_factory_connect_temporal_to_training(brain_t brain) {
    if (!brain || !brain->temporal) {
        return true;  /* Nothing to connect */
    }

    /* Check if training is enabled */
    if (!brain->enable_training_integration || !brain->training_ctx) {
        return true;  /* Training not enabled */
    }

    /*
     * Register temporal adapter with training context.
     * This allows perception and memory learning through:
     * - Object recognition improvement
     * - Semantic association learning
     * - Auditory pattern recognition
     */

    /* TODO: Register with training
     * nimcp_brain_training_register_module(brain->training_ctx,
     *     TRAIN_MODULE_TEMPORAL, brain->temporal);
     */

    LOG_DEBUG(LOG_MODULE, "Temporal connected to training system");
    return true;
}

bool nimcp_brain_factory_connect_temporal_to_immune(brain_t brain) {
    if (!brain || !brain->temporal) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        return true;  /* Immune not enabled */
    }

    /*
     * Register for cytokine signals that affect perception and memory.
     * Neuroinflammation (high IL-1b, TNF-a) can cause:
     * - Reduced auditory acuity
     * - Impaired object recognition
     * - Slowed semantic retrieval
     */

    /* TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A, temporal_inflammation_callback, brain->temporal);
     */

    LOG_DEBUG(LOG_MODULE, "Temporal connected to immune system");
    return true;
}

bool nimcp_brain_factory_connect_temporal_to_hippocampus(brain_t brain) {
    if (!brain || !brain->temporal) {
        return true;  /* Nothing to connect */
    }

    /* Check if hippocampal memory is available */
    if (!brain->engram_system) {
        return true;  /* Hippocampus not initialized yet */
    }

    /*
     * Connect temporal cortex to hippocampal memory system.
     * This enables:
     * - Semantic memory encoding (temporal -> hippocampus)
     * - Memory retrieval (hippocampus -> temporal)
     * - Consolidation pathways
     */

    /* TODO: Connect to hippocampus
     * engram_system_connect_cortical_area(brain->engram_system,
     *     CORTICAL_AREA_TEMPORAL, brain->temporal);
     */

    LOG_DEBUG(LOG_MODULE, "Temporal connected to hippocampus");
    return true;
}

bool nimcp_brain_factory_connect_temporal_to_frontal(brain_t brain) {
    if (!brain || !brain->temporal) {
        return true;  /* Nothing to connect */
    }

    /* Check if executive functions are available */
    if (!brain->executive) {
        return true;  /* Executive not initialized yet */
    }

    /*
     * Connect temporal cortex to prefrontal cortex.
     * This enables:
     * - Top-down attention modulation
     * - Goal-directed semantic retrieval
     * - Executive control of perception
     */

    /* TODO: Connect to frontal
     * executive_controller_connect_temporal(brain->executive, brain->temporal);
     */

    LOG_DEBUG(LOG_MODULE, "Temporal connected to frontal cortex");
    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy temporal cortex subsystem
 *
 * WHAT: Clean up all temporal resources and bridges
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_temporal_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_temporal_subsystem: brain is NULL"); return; }

    LOG_DEBUG(LOG_MODULE, "Destroying temporal cortex subsystem");

    /* Destroy quantum bridge first (depends on temporal) */
    if (brain->temporal_quantum_bridge) {
        temporal_quantum_bridge_destroy(brain->temporal_quantum_bridge);
        brain->temporal_quantum_bridge = NULL;
    }

    /* Destroy thalamic bridge */
    if (brain->temporal_thalamic_bridge) {
        temporal_thalamic_bridge_destroy(brain->temporal_thalamic_bridge);
        brain->temporal_thalamic_bridge = NULL;
    }

    /* Destroy substrate bridge */
    if (brain->temporal_substrate_bridge) {
        temporal_substrate_bridge_destroy(brain->temporal_substrate_bridge);
        brain->temporal_substrate_bridge = NULL;
    }

    /* Destroy temporal adapter */
    if (brain->temporal) {
        temporal_destroy(brain->temporal);
        brain->temporal = NULL;
    }

    brain->temporal_enabled = false;
    brain->last_temporal_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Temporal cortex subsystem destroyed");
}
