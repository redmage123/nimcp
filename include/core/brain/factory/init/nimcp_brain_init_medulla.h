//=============================================================================
// nimcp_brain_init_medulla.h - Medulla Oblongata Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_medulla.h
 * @brief Medulla Oblongata Subsystem Initialization
 *
 * WHAT: Initialization and update functions for medulla oblongata integration
 * WHY:  Integrates brainstem autonomic regulation into brain lifecycle
 * HOW:  Creates, updates, and destroys medulla during brain operations
 *
 * BIOLOGICAL BASIS:
 * The medulla oblongata is the lowest part of the brainstem, controlling:
 * - Arousal and alertness (reticular formation)
 * - Protective reflexes (gag, cough, sneeze)
 * - Circadian rhythms (suprachiasmatic nucleus connection)
 * - Autonomic functions (heart rate, breathing, blood pressure)
 *
 * INTEGRATION POINTS:
 * - Sleep/Wake System: Circadian phase affects sleep pressure
 * - Neuromodulators: Arousal modulates catecholamine release
 * - Immune System: Inflammation affects arousal, triggers protection
 * - Bio-Async: Publishes state changes for system coordination
 * - Mesh Network: Coordinates arousal/protection via distributed consensus
 *
 * @version 2.7.1
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#ifndef NIMCP_BRAIN_INIT_MEDULLA_H
#define NIMCP_BRAIN_INIT_MEDULLA_H

#include "core/brain/nimcp_brain.h"
#include "core/medulla/nimcp_medulla.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Mesh Network Integration Forward Declarations
//=============================================================================

struct mesh_bootstrap;
typedef struct mesh_bootstrap mesh_bootstrap_t;

struct mesh_medulla_integration;
typedef struct mesh_medulla_integration mesh_medulla_integration_t;

//=============================================================================
// Medulla Subsystem Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize the medulla oblongata subsystem for a brain
 *
 * WHAT: Creates and starts the medulla with brain-appropriate configuration
 * WHY:  Provides foundational autonomic regulation for higher functions
 * HOW:  Creates medulla with default config, connects bio-async, starts it
 *
 * BIOLOGICAL BASIS:
 * The medulla must be operational before higher brain functions can work,
 * as it provides the baseline arousal and protection mechanisms.
 *
 * @param brain The brain to initialize medulla for
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Safe, uses internal locking
 */
bool nimcp_brain_factory_init_medulla_subsystem(brain_t brain);

/**
 * @brief Update the medulla oblongata during brain update cycle
 *
 * WHAT: Steps the medulla simulation forward
 * WHY:  Maintains continuous autonomic regulation
 * HOW:  Calls medulla_update with appropriate delta time
 *
 * BIOLOGICAL BASIS:
 * The medulla operates continuously, even during sleep. This function
 * should be called every brain update cycle for proper regulation.
 *
 * @param brain The brain containing the medulla
 * @param delta_time_s Time since last update in seconds
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1) per subsystem update
 * THREAD-SAFETY: Safe, uses internal locking
 */
int nimcp_brain_update_medulla_subsystem(brain_t brain, float delta_time_s);

/**
 * @brief Destroy the medulla oblongata subsystem
 *
 * WHAT: Stops and destroys the medulla
 * WHY:  Clean resource release during brain destruction
 * HOW:  Calls medulla_stop and medulla_destroy
 *
 * @param brain The brain containing the medulla to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Safe, uses internal locking
 */
void nimcp_brain_destroy_medulla_subsystem(brain_t brain);

//=============================================================================
// Medulla State Query Functions
//=============================================================================

/**
 * @brief Get current arousal level from brain's medulla
 *
 * WHAT: Returns the current arousal level [0.0 - 1.0]
 * WHY:  Other brain systems need arousal for modulation
 * HOW:  Queries medulla stats for current arousal
 *
 * @param brain The brain to query
 * @return Current arousal level, or 0.5 on error
 */
float nimcp_brain_get_arousal_level(brain_t brain);

/**
 * @brief Get current circadian phase from brain's medulla
 *
 * WHAT: Returns the current circadian phase
 * WHY:  Sleep/wake and other systems need circadian timing
 * HOW:  Queries medulla for circadian phase
 *
 * @param brain The brain to query
 * @return Current circadian phase, or CIRCADIAN_PHASE_MORNING on error
 */
circadian_phase_t nimcp_brain_get_circadian_phase(brain_t brain);

/**
 * @brief Get current protection level from brain's medulla
 *
 * WHAT: Returns the current protection level
 * WHY:  Security and immune systems need protection state
 * HOW:  Queries medulla for protection level
 *
 * @param brain The brain to query
 * @return Current protection level, or PROTECTION_LEVEL_NORMAL on error
 */
protection_level_t nimcp_brain_get_protection_level(brain_t brain);

/**
 * @brief Check if brain's medulla is in emergency state
 *
 * WHAT: Returns whether medulla is in emergency shutdown
 * WHY:  Other systems need to know if emergency protocols are active
 * HOW:  Checks medulla state for emergency conditions
 *
 * @param brain The brain to query
 * @return true if in emergency state, false otherwise
 */
bool nimcp_brain_is_medulla_emergency(brain_t brain);

//=============================================================================
// Medulla Control Functions
//=============================================================================

/**
 * @brief Trigger emergency shutdown via brain's medulla
 *
 * WHAT: Initiates emergency shutdown sequence
 * WHY:  Critical failures need immediate system-wide response
 * HOW:  Calls medulla_emergency_shutdown with reason
 *
 * BIOLOGICAL BASIS:
 * The medulla can trigger protective reflexes that override higher
 * cognitive functions - this is analogous to a vasovagal syncope
 * or protective unconsciousness.
 *
 * @param brain The brain to trigger emergency on
 * @param reason Human-readable reason for shutdown
 * @return 0 on success, negative error code on failure
 */
int nimcp_brain_trigger_emergency(brain_t brain, const char* reason);

/**
 * @brief Request medulla state change via brain
 *
 * WHAT: Requests a state transition in the medulla
 * WHY:  Allows higher cognitive functions to influence arousal state
 * HOW:  Calls medulla_request_state_change
 *
 * @param brain The brain to modify
 * @param new_state The requested new state
 * @return 0 on success, negative error code on failure
 */
int nimcp_brain_request_medulla_state(brain_t brain, medulla_state_t new_state);

//=============================================================================
// Mesh Network Integration Functions
//=============================================================================

/**
 * @brief Set the mesh bootstrap handle for medulla mesh registration
 *
 * WHAT: Configures medulla init to register with mesh network
 * WHY:  Enable coordinated arousal/protection via mesh consensus
 * HOW:  Stores bootstrap handle, used during medulla init
 *
 * When set before nimcp_brain_factory_init_medulla_subsystem is called,
 * the medulla will automatically register as a subcortical participant
 * in the mesh network and can coordinate arousal/protection changes
 * through distributed consensus.
 *
 * @param bootstrap Mesh bootstrap handle (NULL to disable)
 */
void nimcp_brain_medulla_set_mesh_bootstrap(mesh_bootstrap_t* bootstrap);

/**
 * @brief Get the medulla mesh integration handle
 *
 * WHAT: Returns the current mesh integration for the medulla
 * WHY:  Allows external code to interact with medulla via mesh
 * HOW:  Returns cached integration handle
 *
 * @return Current mesh integration or NULL if not registered
 */
mesh_medulla_integration_t* nimcp_brain_medulla_get_mesh_integration(void);

/**
 * @brief Get medulla module from brain (for mesh registration)
 *
 * @param brain The brain instance
 * @return Medulla handle or NULL if not initialized
 */
medulla_t brain_get_medulla(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_MEDULLA_H
