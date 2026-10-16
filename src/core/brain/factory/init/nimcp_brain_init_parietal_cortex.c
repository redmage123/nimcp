/**
 * @file nimcp_brain_init_parietal_cortex.c
 * @brief Parietal Cortex Region Initialization Implementation
 *
 * WHAT: Initialization functions for parietal cortex region (spatial/sensorimotor)
 * WHY:  Enable spatial processing capabilities in the brain
 * HOW:  Creates parietal adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase PC1: Parietal Cortex Brain Integration
 * @author NIMCP Development Team
 */

/*=============================================================================
 * Includes
 *===========================================================================*/

#include "core/brain/factory/init/nimcp_brain_init_parietal_cortex.h"
#include "core/brain/nimcp_brain.h"
/* Include quantum bridge BEFORE brain_internal to get full struct definition */
#include "core/brain/regions/parietal/nimcp_parietal_quantum_bridge.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_PARIETAL_CORTEX"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_parietal_cortex, MESH_ADAPTER_CATEGORY_SYSTEM)


/* Compatibility macro for set_error */
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

/* Parietal cortex includes */
/* NOTE: nimcp_parietal_quantum_bridge.h must be included BEFORE nimcp_brain_internal.h
 * to get the full struct definition instead of just the forward declaration */
#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

/*=============================================================================
 * Forward Declarations for Bridge Types
 *===========================================================================*/

/* Substrate bridge types - to avoid header conflicts */
struct parietal_substrate_bridge;
typedef struct parietal_substrate_bridge parietal_substrate_bridge_t;

/* Thalamic bridge types */
struct parietal_thalamic_bridge;
typedef struct parietal_thalamic_bridge parietal_thalamic_bridge_t;

/*=============================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Connect parietal to bio-async messaging
 */
static bool connect_parietal_to_bio_async(brain_t brain) {
    if (!brain || !brain->parietal_cortex) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /*
         * TODO: Register parietal message handlers
         * bio_router_register_module(router, BIO_MODULE_PARIETAL, brain->parietal_cortex);
         */
        LOG_DEBUG(LOG_MODULE, "Parietal cortex bio-async integration prepared");
    }

    return true;
}

/**
 * @brief Connect parietal substrate bridge to neural substrate
 */
static bool connect_substrate_bridge(brain_t brain) {
    if (!brain || !brain->parietal_cortex_substrate_bridge) return true;

    /*
     * TODO: Apply initial metabolic effects
     * if (parietal_substrate_bridge_update(brain->parietal_cortex_substrate_bridge) != 0) {
     *     LOG_WARN(LOG_MODULE, "Initial substrate bridge update failed");
     * }
     */

    return true;
}

/**
 * @brief Connect parietal thalamic bridge to thalamic router
 */
static bool connect_thalamic_bridge(brain_t brain) {
    if (!brain || !brain->parietal_cortex_thalamic_bridge) return true;

    /*
     * TODO: Reset bridge to clean state
     * if (parietal_thalamic_bridge_reset(brain->parietal_cortex_thalamic_bridge) != 0) {
     *     LOG_WARN(LOG_MODULE, "Thalamic bridge reset failed");
     * }
     */

    return true;
}

/*=============================================================================
 * Public API Implementation
 *===========================================================================*/

bool nimcp_brain_factory_init_parietal_cortex_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_parietal_cortex_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->parietal_cortex) {
        return true;  /* Already initialized */
    }

    /* Check if parietal cortex is enabled in config */
    /* Default to enabled for spatial-capable brains */
    if (!brain->config.enable_parietal && !brain->config.enable_multimodal_integration) {
        brain->parietal_cortex_enabled = false;
        LOG_INFO(LOG_MODULE, "Parietal cortex disabled by configuration");
        return true;  /* Not enabled, not an error */
    }

    LOG_INFO(LOG_MODULE, "Initializing parietal cortex subsystem");

    /* Create parietal adapter with default configuration */
    parietal_cortex_config_t parietal_cfg = parietal_cortex_adapter_default_config();

    /* Scale configuration based on brain config */
    if (brain->config.working_memory_capacity > 0) {
        parietal_cfg.max_spatial_targets = brain->config.working_memory_capacity * 2;
    }

    /* Enable features based on brain capabilities */
    parietal_cfg.enable_bio_async = brain->bio_async_enabled;
    parietal_cfg.enable_training = brain->enable_training_integration;

    brain->parietal_cortex = parietal_cortex_adapter_create(&parietal_cfg);
    if (!brain->parietal_cortex) {
        set_error("Failed to create parietal cortex adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_parietal_cortex_subsystem: brain->parietal_cortex is NULL");
        return false;
    }

    brain->parietal_cortex_enabled = true;
    brain->last_parietal_cortex_update_us = 0;

    /* Skip bridge inits and subsystem connections on loaded brains.
     * The bridges assume a fresh brain with all subsystems in known state.
     * On checkpoint-loaded brains, connecting bridges to partially-loaded
     * subsystems causes stack smashing (buffer overflow in bridge handlers
     * processing async messages with stale/mismatched state).
     * The adapter itself is safe — only bridges cause crashes. */
    if (brain->stats.total_learning_steps == 0) {
        /* Fresh brain — safe to connect everything */
        if (!nimcp_brain_factory_init_parietal_cortex_substrate_bridge(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal cortex substrate bridge init failed (non-fatal)");
        }
        if (!nimcp_brain_factory_init_parietal_cortex_thalamic_bridge(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal cortex thalamic bridge init failed (non-fatal)");
        }
        if (!nimcp_brain_factory_init_parietal_cortex_quantum_bridge(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal cortex quantum bridge init failed (non-fatal)");
        }
        if (!nimcp_brain_factory_connect_parietal_to_motor(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal-Motor connection failed (non-fatal)");
        }
        if (!nimcp_brain_factory_connect_parietal_to_visual(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal-Visual connection failed (non-fatal)");
        }
        if (!nimcp_brain_factory_connect_parietal_to_frontal(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal-Frontal connection failed (non-fatal)");
        }
        if (!nimcp_brain_factory_connect_parietal_to_working_memory(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal-WM connection failed (non-fatal)");
        }
        if (!nimcp_brain_factory_connect_parietal_to_training(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal-Training connection failed (non-fatal)");
        }
        if (!nimcp_brain_factory_connect_parietal_to_immune(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal-Immune connection failed (non-fatal)");
        }
        if (!connect_parietal_to_bio_async(brain)) {
            LOG_WARN(LOG_MODULE, "Parietal cortex bio-async connection failed (non-fatal)");
        }
    } else {
        LOG_INFO(LOG_MODULE, "Parietal cortex: skipping bridges on loaded brain (steps=%lu)",
                 (unsigned long)brain->stats.total_learning_steps);
    }

    LOG_INFO(LOG_MODULE, "Parietal cortex region initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_parietal_cortex_substrate_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_parietal_cortex_substrate_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->parietal_cortex_substrate_bridge) {
        return true;
    }

    /* Need parietal adapter first */
    if (!brain->parietal_cortex) {
        return true;  /* Not ready yet, will be called again */
    }

    /*
     * TODO: Create substrate bridge when implementation available
     * neural_substrate_t* substrate = brain->substrate;
     * parietal_substrate_config_t config = parietal_substrate_default_config();
     * brain->parietal_cortex_substrate_bridge = parietal_substrate_bridge_create(
     *     brain->parietal_cortex, substrate, &config);
     */

    /* For now, just mark as prepared */
    brain->parietal_cortex_substrate_bridge = NULL;

    /* Connect to substrate */
    connect_substrate_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Parietal cortex substrate bridge prepared");
    return true;
}

bool nimcp_brain_factory_init_parietal_cortex_thalamic_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_parietal_cortex_thalamic_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->parietal_cortex_thalamic_bridge) {
        return true;
    }

    /* Need parietal adapter first */
    if (!brain->parietal_cortex) {
        return true;  /* Not ready yet */
    }

    /*
     * TODO: Create thalamic bridge when implementation available
     * thalamic_router_t* router = brain->thalamic_router;
     * parietal_thalamic_config_t config = parietal_thalamic_default_config();
     * brain->parietal_cortex_thalamic_bridge = parietal_thalamic_bridge_create(
     *     brain->parietal_cortex, router, &config);
     */

    /* For now, just mark as prepared */
    brain->parietal_cortex_thalamic_bridge = NULL;

    /* Connect to thalamus */
    connect_thalamic_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Parietal cortex thalamic bridge prepared");
    return true;
}

bool nimcp_brain_factory_init_parietal_cortex_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_parietal_cortex_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->parietal_cortex_quantum_bridge) {
        return true;
    }

    /* Need parietal adapter first */
    if (!brain->parietal_cortex) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        LOG_DEBUG(LOG_MODULE, "Quantum reasoning disabled, skipping quantum bridge");
        return true;  /* Not enabled, not an error */
    }

    LOG_DEBUG(LOG_MODULE, "Creating parietal cortex quantum bridge");

    /* Create quantum bridge with default config */
    parietal_region_quantum_config_t config = parietal_region_quantum_default_config();

    /* Scale based on brain configuration */
    if (brain->config.num_inputs > 64) {
        config.spatial_grid_size = NIMCP_DEFAULT_GRID_DIM;  /* Larger grid for more inputs */
    }

    brain->parietal_cortex_quantum_bridge = parietal_region_quantum_bridge_create(
        brain->parietal_cortex, &config);

    if (!brain->parietal_cortex_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create parietal cortex quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_parietal_cortex_quantum_bridge: brain->parietal_cortex_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Parietal cortex quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_parietal_to_motor(brain_t brain) {
    if (!brain || !brain->parietal_cortex) {
        return true;  /* Nothing to connect */
    }

    /*
     * Connect parietal to motor cortex for sensorimotor integration.
     * This allows parietal motor plans to be executed.
     */

    /*
     * TODO: Register motor callback
     * if (brain->motor_cortex) {
     *     parietal_set_motor_callback(brain->parietal_cortex,
     *         motor_cortex_execute_plan, brain->motor_cortex);
     * }
     */

    LOG_DEBUG(LOG_MODULE, "Parietal connected to motor cortex");
    return true;
}

bool nimcp_brain_factory_connect_parietal_to_visual(brain_t brain) {
    if (!brain || !brain->parietal_cortex) {
        return true;  /* Nothing to connect */
    }

    /* Check if visual cortex is available */
    if (!brain->visual_cortex) {
        return true;  /* Visual not initialized yet */
    }

    /*
     * Connect parietal to visual cortex for visuospatial processing.
     * Spatial attention modulates visual processing.
     */

    /*
     * TODO: Register attention callback for visual selection
     * parietal_set_attention_callback(brain->parietal_cortex,
     *     visual_cortex_apply_attention, brain->visual_cortex);
     */

    LOG_DEBUG(LOG_MODULE, "Parietal connected to visual cortex");
    return true;
}

bool nimcp_brain_factory_connect_parietal_to_frontal(brain_t brain) {
    if (!brain || !brain->parietal_cortex) {
        return true;  /* Nothing to connect */
    }

    /* Check if frontal cortex is available (via executive functions) */
    if (!brain->executive) {
        return true;  /* Frontal not initialized yet */
    }

    /*
     * Connect parietal to prefrontal for executive spatial control.
     * Bidirectional for goal-directed spatial behavior.
     */

    /*
     * TODO: Register bidirectional connection
     * executive_register_spatial_module(brain->executive, brain->parietal_cortex);
     */

    LOG_DEBUG(LOG_MODULE, "Parietal connected to frontal cortex");
    return true;
}

bool nimcp_brain_factory_connect_parietal_to_working_memory(brain_t brain) {
    if (!brain || !brain->parietal_cortex) {
        return true;  /* Nothing to connect */
    }

    /* Check if working memory is available */
    if (!brain->working_memory) {
        return true;  /* WM not initialized yet */
    }

    /*
     * Register parietal as a working memory consumer for spatial representations.
     * This allows parietal to maintain spatial buffers.
     */

    /*
     * TODO: Register with working memory
     * working_memory_register_consumer(brain->working_memory,
     *     WM_CONSUMER_PARIETAL, brain->parietal_cortex);
     */

    LOG_DEBUG(LOG_MODULE, "Parietal connected to working memory");
    return true;
}

bool nimcp_brain_factory_connect_parietal_to_training(brain_t brain) {
    if (!brain || !brain->parietal_cortex) {
        return true;  /* Nothing to connect */
    }

    /* Check if training is enabled */
    if (!brain->enable_training_integration || !brain->training_ctx) {
        return true;  /* Training not enabled */
    }

    /*
     * Register parietal adapter with training context.
     * This allows sensorimotor learning through:
     * - Coordinate transform refinement
     * - Reaching accuracy improvement
     * - Attention optimization
     *
     * The parietal-training bridge handles:
     * - Loss-based learning signals for spatial processing
     * - Gradient routing to plasticity systems
     * - Reward modulation for skill learning
     */

    /* Register training event callback for parietal learning signals */
    nimcp_result_t result = nimcp_brain_training_register_callback(
        brain->training_ctx,
        NULL,  /* Will be set when parietal_training_bridge is created */
        brain->parietal_cortex
    );

    if (result != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Failed to register parietal with training: %d", result);
        /* Non-fatal - parietal can work without training */
    }

    LOG_DEBUG(LOG_MODULE, "Parietal connected to training system");
    return true;
}

bool nimcp_brain_factory_connect_parietal_to_immune(brain_t brain) {
    if (!brain || !brain->parietal_cortex) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        return true;  /* Immune not enabled */
    }

    /*
     * Register for cytokine signals that affect spatial processing.
     * Neuroinflammation (high IL-1b, TNF-a) can cause:
     * - Reduced spatial acuity
     * - Slower coordinate transforms
     * - Attention deficits
     */

    /*
     * TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A, parietal_inflammation_callback, brain->parietal_cortex);
     */

    LOG_DEBUG(LOG_MODULE, "Parietal connected to immune system");
    return true;
}

/*=============================================================================
 * Runtime Functions
 *===========================================================================*/

parietal_adapter_t* brain_get_parietal_cortex(brain_t brain) {
    if (!brain || !brain->parietal_cortex_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_parietal_cortex: invalid parameters");

            return NULL;
    }
    return brain->parietal_cortex;
}

parietal_quantum_bridge_t* brain_get_parietal_cortex_quantum_bridge(brain_t brain) {
    if (!brain || !brain->parietal_cortex_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_parietal_cortex_quantum_bridge: invalid parameters");

            return NULL;
    }
    return brain->parietal_cortex_quantum_bridge;
}

int brain_step_parietal_cortex(brain_t brain, uint64_t delta_t) {
    if (!brain || !brain->parietal_cortex_enabled || !brain->parietal_cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_step_parietal_cortex: invalid parameters");

            return -1;
    }

    /* Process integration */
    parietal_cortex_integration_result_t result;
    if (!parietal_cortex_process_integration(brain->parietal_cortex, &result)) {
        LOG_WARN(LOG_MODULE, "Parietal integration step failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_step_parietal_cortex: parietal_cortex_process_integration is NULL");
        return -1;
    }

    /* Process quantum if enabled */
    if (brain->parietal_cortex_quantum_bridge &&
        parietal_region_quantum_bridge_is_enabled(brain->parietal_cortex_quantum_bridge)) {
        /* Would run quantum operations here */
    }

    brain->last_parietal_cortex_update_us += delta_t;

    return 0;
}

int brain_update_parietal_cortex_from_immune(brain_t brain) {
    if (!brain || !brain->parietal_cortex_enabled || !brain->parietal_cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_update_parietal_cortex_from_immune: invalid parameters");

            return -1;
    }

    if (!brain->immune_enabled || !brain->immune_system) {
        return 0;  /* Immune not available, not an error */
    }

    /*
     * TODO: Get inflammation level and update parietal
     * float inflammation = brain_immune_get_inflammation(brain->immune_system);
     * parietal_set_inflammation_modulation(brain->parietal_cortex, inflammation);
     */

    return 0;
}

int brain_update_parietal_cortex_from_sleep(brain_t brain) {
    if (!brain || !brain->parietal_cortex_enabled || !brain->parietal_cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_update_parietal_cortex_from_sleep: invalid parameters");

            return -1;
    }

    if (!brain->medulla_enabled) {
        return 0;  /* Sleep system not available */
    }

    /*
     * TODO: Get fatigue level and update parietal
     * float fatigue = medulla_get_fatigue_level(brain->medulla);
     * parietal_set_fatigue_modulation(brain->parietal_cortex, fatigue);
     */

    return 0;
}
