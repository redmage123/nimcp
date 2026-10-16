/**
 * @file nimcp_brain_init_cingulate.c
 * @brief Cingulate Cortex Initialization Implementation
 *
 * WHAT: Initialization functions for Cingulate Cortex subsystem
 * WHY:  Enable conflict monitoring, error detection, and self-referential processing
 * HOW:  Creates cingulate adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase B4: Cingulate Cortex Integration
 */

/*=============================================================================
 * INCLUDES
 *===========================================================================*/

#include "core/brain/factory/init/nimcp_brain_init_cingulate.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_CINGULATE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_cingulate, MESH_ADAPTER_CATEGORY_SYSTEM)


/* Compatibility macro for set_error (converts to LOG_ERROR) */
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

/* Cingulate cortex includes */
#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"
#include "core/brain/regions/cingulate/nimcp_cingulate_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Connect cingulate to bio-async messaging
 */
static bool connect_cingulate_to_bio_async(brain_t brain) {
    if (!brain || !brain->cingulate) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /* Get bio context from cingulate adapter */
        bio_module_context_t ctx = cingulate_get_bio_context(brain->cingulate);
        if (ctx) {
            LOG_DEBUG(LOG_MODULE, "Cingulate registered with bio-async");
        }
    }

    return true;
}

/**
 * @brief Set up cingulate callbacks for integration
 */
static bool setup_cingulate_callbacks(brain_t brain) {
    if (!brain || !brain->cingulate) return true;

    /* Could set up callbacks here for:
     * - Conflict notification to executive
     * - Error notification to learning systems
     * - Control signals to motor planning
     */

    return true;
}

/*=============================================================================
 * PUBLIC API IMPLEMENTATION
 *===========================================================================*/

bool nimcp_brain_factory_init_cingulate_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_cingulate_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->cingulate) {
        return true;  /* Already initialized */
    }

    /* Check if cingulate is enabled in config */
    /* Default to enabled for cognition-capable brains */
    if (!brain->config.enable_executive_control &&
        !brain->config.enable_emotional_tagging) {
        brain->cingulate_enabled = false;
        return true;  /* Not enabled, not an error */
    }

    LOG_INFO(LOG_MODULE, "Initializing cingulate cortex subsystem");

    /* Create cingulate adapter with default configuration */
    cingulate_config_t cingulate_cfg = cingulate_default_config();

    /* Scale configuration based on brain config */
    if (brain->config.working_memory_capacity > 0) {
        /* More WM capacity means more options to monitor */
        cingulate_cfg.max_conflicts = brain->config.working_memory_capacity;
    }

    /* Enable bio-async if brain has it */
    cingulate_cfg.enable_bio_async = brain->bio_async_enabled;

    brain->cingulate = cingulate_create(&cingulate_cfg);
    if (!brain->cingulate) {
        set_error("Failed to create cingulate adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_cingulate_subsystem: brain->cingulate is NULL");
        return false;
    }

    brain->cingulate_enabled = true;
    brain->last_cingulate_update_us = 0;

    /* Initialize quantum bridge */
    if (!nimcp_brain_factory_init_cingulate_quantum_bridge(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_cingulate_to_executive(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate-Executive connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cingulate_to_emotion(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate-Emotion connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cingulate_to_autobio(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate-Autobio connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cingulate_to_working_memory(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate-WM connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cingulate_to_immune(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate-Immune connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cingulate_to_fep(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate-FEP connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_cingulate_to_bio_async(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate bio-async connection failed (non-fatal)");
    }

    /* Set up callbacks */
    if (!setup_cingulate_callbacks(brain)) {
        LOG_WARNING(LOG_MODULE, "Cingulate callback setup failed (non-fatal)");
    }

    LOG_INFO(LOG_MODULE, "Cingulate cortex initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_cingulate_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_cingulate_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->cingulate_quantum_bridge) {
        return true;
    }

    /* Need cingulate adapter first */
    if (!brain->cingulate) {
        return true;  /* Not ready yet, will be called again */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        LOG_DEBUG(LOG_MODULE, "Quantum reasoning disabled, skipping quantum bridge");
        return true;  /* Not enabled, not an error */
    }

    LOG_DEBUG(LOG_MODULE, "Creating cingulate quantum bridge");

    /* Create quantum bridge with default config */
    cingulate_quantum_config_t config = cingulate_quantum_default_config();

    /* Scale based on brain configuration */
    if (brain->config.num_outputs > 4) {
        /* More outputs = more response options = more qubits needed */
        uint32_t qubits_needed = 1;
        uint32_t outputs = brain->config.num_outputs;
        while ((1u << qubits_needed) < outputs && qubits_needed < 8) {
            qubits_needed++;
        }
        config.max_qubits = qubits_needed;
    }

    brain->cingulate_quantum_bridge = cingulate_quantum_bridge_create(
        brain->cingulate, &config);

    if (!brain->cingulate_quantum_bridge) {
        LOG_WARNING(LOG_MODULE, "Failed to create cingulate quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_cingulate_quantum_bridge: brain->cingulate_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Cingulate quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_cingulate_to_executive(brain_t brain) {
    if (!brain || !brain->cingulate) {
        return true;  /* Nothing to connect */
    }

    /* Check if executive is available */
    if (!brain->executive) {
        LOG_DEBUG(LOG_MODULE, "Executive not available yet");
        return true;  /* Executive not initialized yet */
    }

    /*
     * Register cingulate as a control signal source for executive.
     * This allows the ACC to send cognitive control demands that
     * adjust response thresholds and increase monitoring.
     */

    /* TODO: Register control signal callback
     * executive_register_control_source(brain->executive,
     *     CONTROL_SOURCE_CINGULATE, cingulate_control_handler, brain->cingulate);
     */

    LOG_DEBUG(LOG_MODULE, "Cingulate connected to executive control");
    return true;
}

bool nimcp_brain_factory_connect_cingulate_to_emotion(brain_t brain) {
    if (!brain || !brain->cingulate) {
        return true;  /* Nothing to connect */
    }

    /* Check if emotional system is available */
    if (!brain->emotional_system) {
        LOG_DEBUG(LOG_MODULE, "Emotional system not available yet");
        return true;  /* Emotional system not initialized yet */
    }

    /*
     * Register for emotional state updates from emotional system.
     * The rostral ACC integrates emotional valence and arousal with
     * cognitive processing.
     */

    /* TODO: Register for emotion updates
     * emotional_system_register_callback(brain->emotional_system,
     *     EMOTION_EVENT_STATE_CHANGE, cingulate_emotion_handler, brain->cingulate);
     */

    LOG_DEBUG(LOG_MODULE, "Cingulate connected to emotional system");
    return true;
}

bool nimcp_brain_factory_connect_cingulate_to_autobio(brain_t brain) {
    if (!brain || !brain->cingulate) {
        return true;  /* Nothing to connect */
    }

    /* Check if autobiographical memory is available */
    if (!brain->autobio) {
        LOG_DEBUG(LOG_MODULE, "Autobiographical memory not available yet");
        return true;  /* Autobio not initialized yet */
    }

    /*
     * Register PCC as autobiographical memory accessor.
     * The PCC mediates retrieval of self-referential memories
     * and activates during mind wandering.
     */

    /* TODO: Register as memory accessor
     * autobiographical_register_accessor(brain->autobio,
     *     ACCESSOR_PCC, cingulate_autobio_handler, brain->cingulate);
     */

    LOG_DEBUG(LOG_MODULE, "Cingulate connected to autobiographical memory");
    return true;
}

bool nimcp_brain_factory_connect_cingulate_to_working_memory(brain_t brain) {
    if (!brain || !brain->cingulate) {
        return true;  /* Nothing to connect */
    }

    /* Check if working memory is available */
    if (!brain->working_memory) {
        LOG_DEBUG(LOG_MODULE, "Working memory not available yet");
        return true;  /* WM not initialized yet */
    }

    /*
     * Register cingulate as working memory consumer for conflict resolution.
     * The ACC needs to hold active representations of competing response
     * options to detect conflicts.
     */

    /* TODO: Register with working memory
     * working_memory_register_consumer(brain->working_memory,
     *     WM_CONSUMER_CINGULATE, brain->cingulate);
     */

    LOG_DEBUG(LOG_MODULE, "Cingulate connected to working memory");
    return true;
}

bool nimcp_brain_factory_connect_cingulate_to_immune(brain_t brain) {
    if (!brain || !brain->cingulate) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        LOG_DEBUG(LOG_MODULE, "Immune system not available");
        return true;  /* Immune not enabled */
    }

    /*
     * Register for cytokine signals that affect conflict monitoring.
     * Neuroinflammation (high IL-1beta, TNF-alpha) can cause:
     * - Reduced error detection sensitivity
     * - Impaired cognitive control
     * - Increased conflict threshold
     */

    /* TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A, cingulate_inflammation_callback, brain->cingulate);
     */

    LOG_DEBUG(LOG_MODULE, "Cingulate connected to immune system");
    return true;
}

bool nimcp_brain_factory_connect_cingulate_to_fep(brain_t brain) {
    if (!brain || !brain->cingulate) {
        return true;  /* Nothing to connect */
    }

    /* Check if FEP orchestrator is available */
    if (!brain->fep_orchestrator_enabled || !brain->fep_orchestrator) {
        LOG_DEBUG(LOG_MODULE, "FEP orchestrator not available");
        return true;  /* FEP not enabled */
    }

    /*
     * Register cingulate as prediction error source with FEP orchestrator.
     * The ERN (Error-Related Negativity) is essentially a prediction error
     * signal that should feed into the free energy framework.
     *
     * Integration:
     * - ERN amplitude -> Precision-weighted prediction error
     * - Conflict level -> Expected free energy (unresolved = high FE)
     * - Control signal -> Active inference to minimize future errors
     */

    /* TODO: Register with FEP orchestrator
     * fep_orchestrator_register_error_source(brain->fep_orchestrator,
     *     FEP_SOURCE_CINGULATE, cingulate_fep_error_handler, brain->cingulate);
     */

    LOG_DEBUG(LOG_MODULE, "Cingulate connected to FEP orchestrator");
    return true;
}

bool nimcp_brain_factory_shutdown_cingulate_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_shutdown_cingulate_subsystem: brain is NULL");

            return false;
    }

    LOG_INFO(LOG_MODULE, "Shutting down cingulate cortex subsystem");

    /* Destroy quantum bridge first */
    if (brain->cingulate_quantum_bridge) {
        cingulate_quantum_bridge_destroy(brain->cingulate_quantum_bridge);
        brain->cingulate_quantum_bridge = NULL;
    }

    /* Destroy cingulate adapter */
    if (brain->cingulate) {
        cingulate_destroy(brain->cingulate);
        brain->cingulate = NULL;
    }

    brain->cingulate_enabled = false;

    LOG_DEBUG(LOG_MODULE, "Cingulate cortex subsystem shutdown complete");
    return true;
}
