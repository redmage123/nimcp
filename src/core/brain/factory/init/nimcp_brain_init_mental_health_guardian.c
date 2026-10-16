//=============================================================================
// nimcp_brain_init_mental_health_guardian.c - Mental Health Guardian Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_mental_health_guardian.c
 * @brief Mental Health Guardian subsystem initialization for brain factory
 *
 * WHAT: Initializes the Mental Health Guardian background monitoring agent
 * WHY:  Proactively detect and correct mental health abnormalities
 * HOW:  Create guardian, connect to integrations, auto-start if configured
 *
 * INITIALIZATION ORDER:
 * This init must run AFTER:
 * - Mental health monitor (provides disorder detection)
 * - Immune system (optional, for threat reporting)
 * - Internal KG (optional, for topology awareness)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/mental_health/nimcp_mental_health_guardian.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_mental_health_guardian, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Logging
//=============================================================================

#define LOG_TAG "[BRAIN_INIT_GUARDIAN]"

//=============================================================================
// Integration Helper Functions
//=============================================================================

/**
 * @brief Connect guardian to immune system for threat reporting
 */
static void connect_guardian_to_immune(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->immune_system || !brain->immune_enabled) {
        LOG_DEBUG(LOG_TAG " Immune system not available - skipping connection");
        return;
    }

    if (mental_health_guardian_connect_immune(guardian, brain->immune_system)) {
        LOG_DEBUG(LOG_TAG " Connected to immune system");
    } else {
        LOG_WARN(LOG_TAG " Failed to connect to immune system");
    }
}

/**
 * @brief Connect guardian to internal knowledge graph
 */
static void connect_guardian_to_kg(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->internal_kg || !brain->internal_kg_enabled) {
        LOG_DEBUG(LOG_TAG " Internal KG not available - skipping connection");
        return;
    }

    if (mental_health_guardian_connect_kg(
            guardian,
            brain->internal_kg,
            brain->internal_kg_admin_token)) {
        LOG_DEBUG(LOG_TAG " Connected to internal knowledge graph");
    } else {
        LOG_WARN(LOG_TAG " Failed to connect to internal KG");
    }
}

/**
 * @brief Connect guardian to bio-async orchestrator
 */
static void connect_guardian_to_bio_async(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->bio_async_ctx || !brain->bio_async_enabled) {
        LOG_DEBUG(LOG_TAG " Bio-async not available - skipping connection");
        return;
    }

    if (mental_health_guardian_connect_bio_async(guardian, brain->bio_async_ctx)) {
        LOG_DEBUG(LOG_TAG " Connected to bio-async orchestrator");
    } else {
        LOG_WARN(LOG_TAG " Failed to connect to bio-async");
    }
}

/**
 * @brief Connect guardian to FEP orchestrator
 */
static void connect_guardian_to_fep(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->fep_orchestrator) {
        LOG_DEBUG(LOG_TAG " FEP orchestrator not available - skipping registration");
        return;
    }

    if (mental_health_guardian_register_fep(guardian, brain->fep_orchestrator)) {
        LOG_DEBUG(LOG_TAG " Registered with FEP orchestrator");
    } else {
        LOG_WARN(LOG_TAG " Failed to register with FEP orchestrator");
    }
}

/**
 * @brief Connect guardian to brainstem/medulla
 */
static void connect_guardian_to_medulla(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->medulla) {
        LOG_DEBUG(LOG_TAG " Medulla not available - skipping connection");
        return;
    }

    if (mental_health_guardian_connect_brainstem(guardian, brain->medulla)) {
        LOG_DEBUG(LOG_TAG " Connected to brainstem/medulla");
    } else {
        LOG_WARN(LOG_TAG " Failed to connect to medulla");
    }
}

/**
 * @brief Connect guardian to sleep system
 */
static void connect_guardian_to_sleep(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->sleep_system) {
        LOG_DEBUG(LOG_TAG " Sleep system not available - skipping connection");
        return;
    }

    if (mental_health_guardian_connect_sleep(guardian, brain->sleep_system)) {
        LOG_DEBUG(LOG_TAG " Connected to sleep system");
    } else {
        LOG_WARN(LOG_TAG " Failed to connect to sleep system");
    }
}

/**
 * @brief Connect guardian to plasticity coordinator
 */
static void connect_guardian_to_plasticity(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->plasticity_coordinator || !brain->plasticity_coordinator_enabled) {
        LOG_DEBUG(LOG_TAG " Plasticity coordinator not available - skipping connection");
        return;
    }

    if (mental_health_guardian_connect_plasticity(guardian, brain->plasticity_coordinator)) {
        LOG_DEBUG(LOG_TAG " Connected to plasticity coordinator");
    } else {
        LOG_WARN(LOG_TAG " Failed to connect to plasticity coordinator");
    }
}

/**
 * @brief Connect guardian to working memory
 */
static void connect_guardian_to_working_memory(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->working_memory) {
        LOG_DEBUG(LOG_TAG " Working memory not available - skipping connection");
        return;
    }

    if (mental_health_guardian_connect_working_memory(guardian, brain->working_memory)) {
        LOG_DEBUG(LOG_TAG " Connected to working memory");
    } else {
        LOG_WARN(LOG_TAG " Failed to connect to working memory");
    }
}

/**
 * @brief Connect guardian to executive controller
 */
static void connect_guardian_to_executive(
    mental_health_guardian_t* guardian,
    brain_t brain)
{
    if (!brain->executive) {
        LOG_DEBUG(LOG_TAG " Executive controller not available - skipping connection");
        return;
    }

    if (mental_health_guardian_connect_executive(guardian, brain->executive)) {
        LOG_DEBUG(LOG_TAG " Connected to executive controller");
    } else {
        LOG_WARN(LOG_TAG " Failed to connect to executive controller");
    }
}

//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize mental health guardian subsystem for brain
 *
 * Creates the Mental Health Guardian background agent and connects
 * it to relevant subsystems (immune, KG).
 *
 * @param brain Brain instance to initialize guardian for
 * @return true on success, false on failure
 *
 * PROCESS:
 * 1. Check if guardian is enabled in brain config
 * 2. Verify mental health monitor exists
 * 3. Create guardian with default config
 * 4. Connect to immune system (if available)
 * 5. Connect to internal KG (if available)
 * 6. Auto-start if brain is in running state
 */
bool nimcp_brain_factory_init_mental_health_guardian_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_mental_health_guardian_subsystem: brain is NULL");
        return false;
    }

    /* Check if guardian is disabled in config */
    if (!brain->config.enable_mental_health_guardian) {
        brain->mental_health_guardian = NULL;
        LOG_INFO(LOG_TAG " Mental Health Guardian disabled by config");
        return true;  /* Success - disabled by config */
    }

    /* Verify mental health monitor exists */
    if (!brain->mental_health_monitor) {
        LOG_ERROR(LOG_TAG " Mental health monitor not initialized");
        brain->mental_health_guardian = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_mental_health_guardian_subsystem: brain->mental_health_monitor is NULL");
        return false;
    }

    LOG_INFO(LOG_TAG " Initializing Mental Health Guardian...");

    /* ====================================================================== */
    /* CREATE GUARDIAN WITH DEFAULT CONFIG                                    */
    /* ====================================================================== */

    mental_health_guardian_config_t config = mental_health_guardian_default_config();

    /* Adjust config based on brain settings */
    config.immune_integration = brain->immune_enabled;
    config.kg_integration = brain->internal_kg_enabled;

    mental_health_guardian_t* guardian = mental_health_guardian_create(brain, &config);
    if (!guardian) {
        LOG_ERROR(LOG_TAG " Failed to create guardian");
        brain->mental_health_guardian = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_mental_health_guardian_subsystem: guardian is NULL");
        return false;
    }

    /* Store in brain */
    brain->mental_health_guardian = guardian;

    LOG_INFO(LOG_TAG " Guardian created (interval=%ums)",
            config.monitoring_interval_ms);

    /* ====================================================================== */
    /* CONNECT TO RELEVANT SUBSYSTEMS                                         */
    /* ====================================================================== */
    /* Connection order follows initialization dependencies:
     * 1. Infrastructure (immune, KG, bio-async)
     * 2. Low-level (medulla, sleep, plasticity)
     * 3. Coordination (FEP orchestrator)
     * 4. Cognitive (working memory, executive)
     */

    /* 1. Immune System - Threat reporting at QUARANTINE level */
    connect_guardian_to_immune(guardian, brain);

    /* 2. Internal KG - Topology awareness and state tracking */
    connect_guardian_to_kg(guardian, brain);

    /* 3. Bio-Async - Inter-module messaging for status broadcasts */
    connect_guardian_to_bio_async(guardian, brain);

    /* 4. Brainstem/Medulla - Arousal control at REGULATE level */
    connect_guardian_to_medulla(guardian, brain);

    /* 5. Sleep System - Sleep trigger at REGULATE level */
    connect_guardian_to_sleep(guardian, brain);

    /* 6. Plasticity Coordinator - Synaptic reset at REGULATE level */
    connect_guardian_to_plasticity(guardian, brain);

    /* 7. FEP Orchestrator - Coordinated free energy updates */
    connect_guardian_to_fep(guardian, brain);

    /* 8. Working Memory - Attention/memory reset during interventions */
    connect_guardian_to_working_memory(guardian, brain);

    /* 9. Executive Controller - Cognitive control adjustments */
    connect_guardian_to_executive(guardian, brain);

    /* ====================================================================== */
    /* AUTO-START IF BRAIN IS RUNNING                                         */
    /* ====================================================================== */

    /* Note: Brain state check would go here if we had a running state */
    /* For now, we don't auto-start - caller should start explicitly */

    LOG_INFO(LOG_TAG " Mental Health Guardian initialization complete");
    LOG_DEBUG(LOG_TAG "   Call mental_health_guardian_start() to begin monitoring");

    return true;
}

//=============================================================================
// Accessor Functions
//=============================================================================

/**
 * @brief Get mental health guardian from brain
 *
 * @param brain Brain instance
 * @return Guardian handle or NULL if not enabled
 */
mental_health_guardian_t* brain_get_mental_health_guardian(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }
    return brain->mental_health_guardian;
}

/**
 * @brief Start the mental health guardian
 *
 * Convenience function to start the guardian from brain reference.
 *
 * @param brain Brain instance
 * @return true on success
 */
bool brain_start_mental_health_guardian(brain_t brain) {
    if (!brain || !brain->mental_health_guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_start_mental_health_guardian: required parameter is NULL (brain, brain->mental_health_guardian)");
        return false;
    }
    return mental_health_guardian_start(brain->mental_health_guardian);
}

/**
 * @brief Stop the mental health guardian
 *
 * @param brain Brain instance
 * @return true on success
 */
bool brain_stop_mental_health_guardian(brain_t brain) {
    if (!brain || !brain->mental_health_guardian) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_stop_mental_health_guardian: required parameter is NULL (brain, brain->mental_health_guardian)");
        return false;
    }
    return mental_health_guardian_stop(brain->mental_health_guardian);
}

/**
 * @brief Get guardian status
 *
 * @param brain Brain instance
 * @param status Output status structure
 * @return true on success
 */
bool brain_get_mental_health_guardian_status(
    brain_t brain,
    mental_health_guardian_status_t* status)
{
    if (!brain || !brain->mental_health_guardian || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_mental_health_guardian_status: required parameter is NULL (brain, brain->mental_health_guardian, status)");
        return false;
    }
    return mental_health_guardian_get_status(brain->mental_health_guardian, status);
}

//=============================================================================
// Destruction (called during brain_destroy)
//=============================================================================

/**
 * @brief Destroy mental health guardian subsystem
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_mental_health_guardian_subsystem(brain_t brain) {
    if (!brain) {
        return;
    }

    if (brain->mental_health_guardian) {
        fprintf(stderr, LOG_TAG " Destroying Mental Health Guardian\n");
        mental_health_guardian_destroy(brain->mental_health_guardian);
        brain->mental_health_guardian = NULL;
    }
}
