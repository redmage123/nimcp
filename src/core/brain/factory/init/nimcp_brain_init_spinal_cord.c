//=============================================================================
// nimcp_brain_init_spinal_cord.c - Spinal Cord Factory Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_spinal_cord.c
 * @brief Spinal cord subsystem initialization for brain
 *
 * WHAT: Spinal cord initialization and bridge wiring during brain_create()
 * WHY:  Final common pathway for motor output with CPGs, reflexes, and
 *       descending tract integration
 * HOW:  Creates spinal cord system, wires bridges to motor cortex,
 *       cerebellum, somatosensory, thalamus, training, immune
 *
 * BIOLOGICAL BASIS:
 * The spinal cord is the final relay for motor commands. It contains:
 * - Motor neuron pools organized by muscle group (ventral horn)
 * - Central pattern generators for locomotion (lumbar enlargement)
 * - Reflex arcs for fast protective responses (dorsal horn circuits)
 * - Ascending tracts (spinothalamic for pain/temp, dorsal columns for touch)
 * - Descending tracts (corticospinal, rubrospinal, vestibulospinal)
 *
 * DESIGN PATTERNS:
 * - Factory: Created by brain factory during lifecycle init
 * - Bridge: Connects to motor cortex, cerebellum, somatosensory, etc.
 * - Idempotent: Safe to call multiple times (checks brain->spinal_cord)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_spinal_cord.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/spinal/nimcp_spinal_cord.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_SPINAL_CORD"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_spinal_cord, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize spinal cord subsystem
 *
 * WHAT: Creates and configures spinal cord for brain
 * WHY:  Enables motor output via spinal cord processing
 * HOW:  Guard clause checks, create spinal cord, wire bridges
 *
 * @param brain Brain instance to initialize spinal cord for
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_spinal_cord_subsystem(brain_t brain) {
    /* Guard clause: NULL check */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_spinal_cord_subsystem: brain is NULL");
        return false;
    }

    /* Idempotency: already initialized */
    if (brain->spinal_cord) {
        NIMCP_LOGGING_DEBUG("Spinal cord subsystem already initialized");
        return true;
    }

    /* Initialize fields to defaults */
    brain->spinal_cord = NULL;
    brain->spinal_cord_enabled = false;

    /* Create default configuration */
    spinal_config_t config = spinal_default_config();

    /* Create spinal cord system */
    spinal_cord_t* sc = spinal_create(&config);
    if (!sc) {
        NIMCP_LOGGING_WARN("Failed to create spinal cord - "
                          "continuing without spinal motor output");
        return true;  /* Non-fatal */
    }

    /* Store in brain */
    brain->spinal_cord = sc;
    brain->spinal_cord_enabled = true;

    NIMCP_LOGGING_INFO("Spinal cord subsystem initialized: "
                       "%u motor pools, %u CPGs, %u reflexes",
                       config.num_motor_pools,
                       config.num_cpgs,
                       config.num_reflexes);

    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy spinal cord subsystem
 *
 * @param brain Brain instance to clean up
 */
void nimcp_brain_factory_destroy_spinal_cord_subsystem(brain_t brain) {
    if (!brain) {
        return;
    }

    if (brain->spinal_cord) {
        spinal_destroy(brain->spinal_cord);
        brain->spinal_cord = NULL;
    }

    brain->spinal_cord_enabled = false;
}
