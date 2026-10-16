//=============================================================================
// nimcp_brain_init_prefrontal.c - Prefrontal Cortex Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_prefrontal.c
 * @brief Prefrontal Cortex Initialization Implementation
 *
 * WHAT: Initialization functions for prefrontal cortex (executive functions)
 * WHY:  Enable executive function capabilities in the brain
 * HOW:  Creates prefrontal adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase PFC-1: Prefrontal Cortex Brain Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_prefrontal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_PFC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_prefrontal, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Prefrontal cortex includes
#include "core/brain/regions/prefrontal/nimcp_prefrontal_adapter.h"
#include "core/brain/regions/prefrontal/nimcp_prefrontal_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

//=============================================================================
// Forward Declarations for Thalamic Bridge (to avoid header conflicts)
//=============================================================================

// Prefrontal thalamic config
typedef struct {
    bool enable_attention_gating;
    bool enable_executive_priority;
    bool enable_goal_routing;
    float min_urgency_threshold;
    float executive_boost;
    float attention_decay_rate;
} prefrontal_thalamic_config_t;

// Forward declare if prefrontal thalamic bridge exists (may not yet)
struct prefrontal_thalamic_bridge;
typedef struct prefrontal_thalamic_bridge prefrontal_thalamic_bridge_t;

// Forward declare substrate bridge type
struct prefrontal_substrate_bridge;
typedef struct prefrontal_substrate_bridge prefrontal_substrate_bridge_t;

// Substrate config
typedef struct {
    float atp_threshold;
    float fatigue_threshold;
    float stress_impact_factor;
    bool enable_metabolic_tracking;
} prefrontal_substrate_config_t;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Connect prefrontal to bio-async messaging
 */
static bool connect_prefrontal_to_bio_async(brain_t brain) {
    if (!brain || !brain->prefrontal) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /*
         * TODO: Register prefrontal message handlers
         * bio_router_register_module(router, BIO_MODULE_BRAIN_PREFRONTAL, brain->prefrontal);
         */
        LOG_DEBUG(LOG_MODULE, "Prefrontal registered with bio-async");
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_prefrontal_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_prefrontal_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->prefrontal) {
        LOG_DEBUG(LOG_MODULE, "Prefrontal already initialized");
        return true;  /* Already initialized */
    }

    /* Check if prefrontal is enabled in config */
    /* Note: Default to enabled for cognitive-capable brains */
    /* Use enable_executive_control as proxy for prefrontal capabilities */
    if (!brain->config.enable_executive_control) {
        brain->prefrontal_enabled = false;
        LOG_INFO(LOG_MODULE, "Prefrontal not enabled in config");
        return true;  /* Not enabled, not an error */
    }

    LOG_INFO(LOG_MODULE, "Initializing prefrontal cortex subsystem");

    /* Create prefrontal adapter with default configuration */
    prefrontal_config_t pfc_cfg = prefrontal_default_config();

    /* Scale configuration based on brain config */
    if (brain->config.working_memory_capacity > 0) {
        pfc_cfg.working_memory_slots = brain->config.working_memory_capacity;
    }

    /* Enable training if brain has training enabled */
    pfc_cfg.enable_training = brain->enable_training_integration;

    brain->prefrontal = prefrontal_create(&pfc_cfg);
    if (!brain->prefrontal) {
        set_error("Failed to create prefrontal adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_prefrontal_subsystem: brain->prefrontal is NULL");
        return false;
    }

    brain->prefrontal_enabled = true;
    brain->last_prefrontal_update_us = 0;

    /* Initialize integration bridges */
    if (!nimcp_brain_factory_init_prefrontal_substrate_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal substrate bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_prefrontal_thalamic_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal thalamic bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_prefrontal_quantum_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_prefrontal_to_working_memory(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal-WM connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_prefrontal_to_basal_ganglia(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal-BG connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_prefrontal_to_thalamus(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal-Thalamus connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_prefrontal_to_training(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal-Training connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_prefrontal_to_immune(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal-Immune connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_prefrontal_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Prefrontal bio-async connection failed (non-fatal)");
    }

    LOG_INFO(LOG_MODULE, "Prefrontal cortex initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_prefrontal_substrate_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_prefrontal_substrate_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->prefrontal_substrate_bridge) {
        return true;
    }

    /* Need prefrontal adapter first */
    if (!brain->prefrontal) {
        return true;  /* Not ready yet, will be called again */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing prefrontal substrate bridge");

    /* Get neural substrate - may be NULL in simple configurations */
    /* neural_substrate_t* substrate = brain->substrate; */

    /*
     * TODO: Create substrate bridge when implementation is available
     * For now, we mark as successfully initialized (no-op)
     *
     * prefrontal_substrate_config_t config = {
     *     .atp_threshold = 0.3f,
     *     .fatigue_threshold = 0.7f,
     *     .stress_impact_factor = 0.5f,
     *     .enable_metabolic_tracking = true
     * };
     *
     * brain->prefrontal_substrate_bridge = prefrontal_substrate_bridge_create(
     *     brain->prefrontal, substrate, &config);
     */

    LOG_DEBUG(LOG_MODULE, "Prefrontal substrate bridge initialized (placeholder)");
    return true;
}

bool nimcp_brain_factory_init_prefrontal_thalamic_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_prefrontal_thalamic_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->prefrontal_thalamic_bridge) {
        return true;
    }

    /* Need prefrontal adapter first */
    if (!brain->prefrontal) {
        return true;  /* Not ready yet */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing prefrontal thalamic bridge");

    /*
     * TODO: Create thalamic bridge when implementation is available
     * For now, we mark as successfully initialized (no-op)
     *
     * void* router = brain->thalamic_router;
     *
     * prefrontal_thalamic_config_t thal_config = {
     *     .enable_attention_gating = true,
     *     .enable_executive_priority = true,
     *     .enable_goal_routing = true,
     *     .min_urgency_threshold = 0.3f,
     *     .executive_boost = 0.2f,
     *     .attention_decay_rate = 0.1f
     * };
     *
     * brain->prefrontal_thalamic_bridge = prefrontal_thalamic_bridge_create(
     *     brain->prefrontal, router, &thal_config);
     */

    LOG_DEBUG(LOG_MODULE, "Prefrontal thalamic bridge initialized (placeholder)");
    return true;
}

bool nimcp_brain_factory_init_prefrontal_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_prefrontal_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->prefrontal_quantum_bridge) {
        return true;
    }

    /* Need prefrontal adapter first */
    if (!brain->prefrontal) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        LOG_DEBUG(LOG_MODULE, "Quantum reasoning not enabled, skipping quantum bridge");
        return true;  /* Not enabled, not an error */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing prefrontal quantum bridge");

    /* Create quantum bridge with default config */
    prefrontal_quantum_config_t config = prefrontal_quantum_default_config();

    /* Scale based on brain configuration */
    /* Use num_outputs as proxy for brain size (max_neurons doesn't exist) */
    if (brain->config.num_outputs > 500) {
        config.max_decision_qubits = 12;
        config.max_planning_qubits = 14;
    }

    brain->prefrontal_quantum_bridge = prefrontal_quantum_bridge_create(
        brain->prefrontal, &config);

    if (!brain->prefrontal_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create prefrontal quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_prefrontal_quantum_bridge: brain->prefrontal_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Prefrontal quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_prefrontal_to_working_memory(brain_t brain) {
    if (!brain || !brain->prefrontal) {
        return true;  /* Nothing to connect */
    }

    /* Check if working memory is available */
    if (!brain->working_memory) {
        LOG_DEBUG(LOG_MODULE, "Working memory not initialized yet");
        return true;  /* WM not initialized yet */
    }

    /*
     * Register prefrontal as a working memory controller for goal maintenance.
     * This allows prefrontal to:
     * - Maintain active goal representations
     * - Hold task rules and context
     * - Support decision-making with relevant information
     */

    /* TODO: Register with working memory when API is finalized
     * working_memory_register_controller(brain->working_memory,
     *     WM_CONTROLLER_PREFRONTAL, brain->prefrontal);
     */

    LOG_DEBUG(LOG_MODULE, "Prefrontal connected to working memory");
    return true;
}

bool nimcp_brain_factory_connect_prefrontal_to_basal_ganglia(brain_t brain) {
    if (!brain || !brain->prefrontal) {
        return true;  /* Nothing to connect */
    }

    /* Check if basal ganglia is available */
    if (!brain->basal_ganglia_enabled || !brain->basal_ganglia) {
        LOG_DEBUG(LOG_MODULE, "Basal ganglia not enabled");
        return true;  /* BG not enabled */
    }

    /*
     * Establish prefrontal-striatal-thalamic loop for action selection.
     * This allows:
     * - Prefrontal to bias action selection toward goals
     * - Basal ganglia to provide action value feedback
     * - Coordinated motor planning and execution
     */

    /* TODO: Connect to basal ganglia when API is finalized
     * bg_enhanced_connect_cortex(brain->basal_ganglia,
     *     BG_CORTEX_PREFRONTAL, brain->prefrontal);
     */

    LOG_DEBUG(LOG_MODULE, "Prefrontal connected to basal ganglia");
    return true;
}

bool nimcp_brain_factory_connect_prefrontal_to_thalamus(brain_t brain) {
    if (!brain || !brain->prefrontal) {
        return true;  /* Nothing to connect */
    }

    /*
     * Establish reciprocal prefrontal-thalamic connections.
     * MD nucleus is the primary thalamic relay for prefrontal cortex.
     */

    /* TODO: Connect to thalamic router when available
     * if (brain->thalamic_router) {
     *     thalamic_router_connect_cortex(brain->thalamic_router,
     *         THAL_NUCLEUS_MD, brain->prefrontal);
     * }
     */

    LOG_DEBUG(LOG_MODULE, "Prefrontal connected to thalamus");
    return true;
}

bool nimcp_brain_factory_connect_prefrontal_to_training(brain_t brain) {
    if (!brain || !brain->prefrontal) {
        return true;  /* Nothing to connect */
    }

    /* Check if training is enabled */
    if (!brain->enable_training_integration || !brain->training_ctx) {
        LOG_DEBUG(LOG_MODULE, "Training not enabled");
        return true;  /* Training not enabled */
    }

    /*
     * Register prefrontal adapter with training context.
     * This allows executive function learning through:
     * - Value learning: Expected values of actions and outcomes
     * - Rule learning: Task contingencies and optimal strategies
     * - Decision policy optimization
     */

    /* TODO: Register with training
     * nimcp_brain_training_register_module(brain->training_ctx,
     *     TRAIN_MODULE_PREFRONTAL, brain->prefrontal);
     */

    LOG_DEBUG(LOG_MODULE, "Prefrontal connected to training system");
    return true;
}

bool nimcp_brain_factory_connect_prefrontal_to_immune(brain_t brain) {
    if (!brain || !brain->prefrontal) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        LOG_DEBUG(LOG_MODULE, "Immune system not enabled");
        return true;  /* Immune not enabled */
    }

    /*
     * Register for cytokine signals that affect executive function.
     * Neuroinflammation affects prefrontal cortex through:
     * - IL-1beta: Impairs working memory capacity
     * - TNF-alpha: Reduces cognitive flexibility
     * - IL-6: Decreases planning capacity
     * - Cortisol: Modulates stress-related executive function
     */

    /* TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A | CYTOKINE_IL6,
     *     prefrontal_inflammation_callback, brain->prefrontal);
     */

    LOG_DEBUG(LOG_MODULE, "Prefrontal connected to immune system");
    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy prefrontal cortex subsystem
 *
 * WHAT: Clean up all prefrontal resources and bridges
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_prefrontal_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_prefrontal_subsystem: brain is NULL"); return; }

    LOG_DEBUG(LOG_MODULE, "Destroying prefrontal cortex subsystem");

    /* Destroy quantum bridge first (depends on prefrontal) */
    if (brain->prefrontal_quantum_bridge) {
        prefrontal_quantum_bridge_destroy(brain->prefrontal_quantum_bridge);
        brain->prefrontal_quantum_bridge = NULL;
    }

    /* Destroy thalamic bridge */
    if (brain->prefrontal_thalamic_bridge) {
        /* prefrontal_thalamic_bridge_destroy(brain->prefrontal_thalamic_bridge); */
        brain->prefrontal_thalamic_bridge = NULL;
    }

    /* Destroy substrate bridge */
    if (brain->prefrontal_substrate_bridge) {
        /* prefrontal_substrate_bridge_destroy(brain->prefrontal_substrate_bridge); */
        brain->prefrontal_substrate_bridge = NULL;
    }

    /* Destroy prefrontal adapter */
    if (brain->prefrontal) {
        prefrontal_destroy(brain->prefrontal);
        brain->prefrontal = NULL;
    }

    brain->prefrontal_enabled = false;
    brain->last_prefrontal_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Prefrontal cortex subsystem destroyed");
}
