/**
 * @file nimcp_brain_init_brainstem.h
 * @brief Brain factory initialization for Brainstem subsystem
 *
 * WHAT: Brain factory initialization for full brainstem integration
 * WHY:  Provides unified brainstem initialization during brain_create()
 * HOW:  Initializes brainstem adapter, quantum bridge, and connections
 *
 * INITIALIZATION ORDER:
 * 1. Create brainstem adapter (wraps midbrain, pons, medulla, reticular formation)
 * 2. Connect to existing medulla (if already initialized)
 * 3. Create quantum bridge for accelerated processing
 * 4. Connect to thalamus for sensory relay
 * 5. Connect to cerebellum for motor coordination
 * 6. Register with bio-async system
 *
 * DEPENDENCIES:
 * - Medulla (should be initialized before brainstem)
 * - Thalamus (for sensory relay)
 * - Cerebellum (for motor coordination)
 * - Bio-async router (for inter-module communication)
 *
 * @version Phase BS-3: Brainstem Factory Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_BRAINSTEM_H
#define NIMCP_BRAIN_INIT_BRAINSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
struct brain_struct;

/**
 * @brief Initialize brainstem subsystem in brain
 *
 * Creates and configures the full brainstem integration:
 * - Brainstem adapter (midbrain, pons, medulla, reticular formation)
 * - Quantum bridge for accelerated processing
 * - Connections to thalamus, cerebellum, and spinal pathways
 *
 * DEPENDENCIES:
 * - Medulla should be initialized first (will use if available)
 * - Bio-async router should be initialized
 *
 * @param brain Brain structure to initialize
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (called during brain creation)
 */
bool nimcp_brain_init_brainstem(struct brain_struct* brain);

/**
 * @brief Destroy brainstem subsystem in brain
 *
 * Cleans up all brainstem resources:
 * - Destroys quantum bridge
 * - Destroys brainstem adapter
 * - Disconnects from connected systems
 *
 * @param brain Brain structure to clean up
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (called during brain destruction)
 */
void nimcp_brain_destroy_brainstem(struct brain_struct* brain);

/**
 * @brief Check if brainstem is enabled in brain
 *
 * @param brain Brain structure to check
 * @return true if brainstem is enabled and initialized
 */
bool nimcp_brain_is_brainstem_enabled(const struct brain_struct* brain);

/**
 * @brief Connect brainstem to thalamus
 *
 * Establishes connection between brainstem and thalamus for
 * sensory relay and arousal modulation.
 *
 * @param brain Brain structure with initialized brainstem
 * @return true on success
 */
bool nimcp_brain_connect_brainstem_thalamus(struct brain_struct* brain);

/**
 * @brief Connect brainstem to cerebellum
 *
 * Establishes connection between brainstem (pons) and cerebellum
 * for motor coordination and timing.
 *
 * @param brain Brain structure with initialized brainstem
 * @return true on success
 */
bool nimcp_brain_connect_brainstem_cerebellum(struct brain_struct* brain);

/**
 * @brief Update brainstem subsystem
 *
 * Called during brain update to step the brainstem simulation.
 *
 * @param brain Brain structure
 * @param dt Time step in seconds
 * @return true on success
 */
bool nimcp_brain_update_brainstem(struct brain_struct* brain, float dt);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_BRAINSTEM_H */
