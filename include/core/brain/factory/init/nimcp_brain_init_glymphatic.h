/**
 * @file nimcp_brain_init_glymphatic.h
 * @brief Glymphatic Subsystem Factory Initialization Header
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Initialization, update, and destruction functions for glymphatic integration
 * WHY:  Integrates brain waste clearance into the brain lifecycle
 * HOW:  Creates, updates, and destroys glymphatic system during brain operations
 *
 * BIOLOGICAL BASIS:
 * The glymphatic system is a brain-wide waste clearance pathway that operates
 * primarily during sleep. It uses CSF flow through perivascular channels,
 * facilitated by AQP4 water channels on astrocytic endfeet, to flush metabolic
 * waste products including beta-amyloid and tau protein. Impaired glymphatic
 * function is implicated in neurodegeneration.
 *
 * INTEGRATION POINTS:
 * - Sleep/Wake System: Sleep state drives clearance efficiency
 * - Immune System: High waste triggers neuroinflammation alerts
 * - Training Pipeline: Waste penalizes learning rate
 * - Inference Pipeline: Waste reduces output confidence
 * - Glial System: AQP4 expression from astrocytes
 * - Hypothalamus: Circadian modulation of clearance windows
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_GLYMPHATIC_H
#define NIMCP_BRAIN_INIT_GLYMPHATIC_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Glymphatic Subsystem Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Initialize the glymphatic waste clearance subsystem for a brain
 *
 * WHAT: Creates and configures the glymphatic system for the brain
 * WHY:  Provides metabolic waste management during sleep/wake cycles
 * HOW:  Creates glymphatic system with default config, stores on brain struct
 *
 * IDEMPOTENCY: Safe to call multiple times; skips if already initialized.
 *
 * @param brain The brain to initialize glymphatic for
 * @return true on success or non-fatal skip, false on critical error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Safe, uses internal locking
 */
bool nimcp_brain_factory_init_glymphatic_subsystem(brain_t brain);

/**
 * @brief Update the glymphatic system during brain update cycle
 *
 * WHAT: Steps the glymphatic simulation forward
 * WHY:  Maintains continuous waste accumulation and clearance dynamics
 * HOW:  Calls glymphatic_update with appropriate delta time
 *
 * @param brain        The brain containing the glymphatic system
 * @param delta_time_s Time since last update in seconds
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Safe, uses internal locking
 */
int nimcp_brain_update_glymphatic_subsystem(brain_t brain, float delta_time_s);

/**
 * @brief Destroy the glymphatic subsystem
 *
 * WHAT: Destroys and frees the glymphatic system
 * WHY:  Clean resource release during brain destruction
 * HOW:  Calls glymphatic_destroy and NULLs pointer on brain
 *
 * @param brain The brain containing the glymphatic system to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Safe (final call during brain destruction)
 */
void nimcp_brain_destroy_glymphatic_subsystem(brain_t brain);

/*=============================================================================
 * Glymphatic Query Functions (via brain handle)
 *===========================================================================*/

/**
 * @brief Get current waste level from brain's glymphatic system
 *
 * @param brain The brain to query
 * @return Waste level (0.0-1.0), or 0.0 if unavailable
 */
float nimcp_brain_get_waste_level(brain_t brain);

/**
 * @brief Get current clearance rate from brain's glymphatic system
 *
 * @param brain The brain to query
 * @return Clearance rate (0.0-1.0), or 0.0 if unavailable
 */
float nimcp_brain_get_clearance_rate(brain_t brain);

/**
 * @brief Notify brain's glymphatic system of sleep state change
 *
 * @param brain       The brain to notify
 * @param sleep_state New sleep state (GLYM_SLEEP_* constant)
 * @return 0 on success, -1 on error
 */
int nimcp_brain_glymphatic_sleep_transition(brain_t brain, uint32_t sleep_state);

/**
 * @brief Force a waste flush on the brain's glymphatic system
 *
 * @param brain The brain to flush
 * @return 0 on success, -1 on error
 */
int nimcp_brain_glymphatic_flush(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_GLYMPHATIC_H */
