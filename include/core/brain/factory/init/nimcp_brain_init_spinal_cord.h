/**
 * @file nimcp_brain_init_spinal_cord.h
 * @brief Brain factory initialization for Spinal Cord subsystem
 *
 * WHAT: Brain factory initialization for spinal cord / motor output
 * WHY:  Provides unified spinal cord initialization during brain_create()
 * HOW:  Creates spinal cord system, connects bridges to motor cortex,
 *       cerebellum, somatosensory, thalamus, training, immune, bio-async
 *
 * INITIALIZATION ORDER:
 * 1. Create spinal cord system (motor pools, CPGs, reflex arcs)
 * 2. Connect to motor cortex (corticospinal tract)
 * 3. Connect to cerebellum (motor coordination)
 * 4. Connect to somatosensory (proprioceptive feedback)
 * 5. Connect to brainstem (vestibulospinal, reticulospinal)
 * 6. Connect to thalamus (spinothalamic pain relay)
 * 7. Connect to training (motor learning)
 * 8. Connect to immune (inflammation effects)
 * 9. Connect to bio-async (event messaging)
 *
 * DEPENDENCIES:
 * - Motor cortex (should be initialized before spinal cord)
 * - Cerebellum (for coordination feedback)
 * - Brainstem (for vestibulospinal input)
 * - Bio-async router (for inter-module communication)
 *
 * @version 1.0.0
 * @date 2026-03-05
 */

#ifndef NIMCP_BRAIN_INIT_SPINAL_CORD_H
#define NIMCP_BRAIN_INIT_SPINAL_CORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Initialize spinal cord subsystem in brain
 *
 * Creates and configures the spinal cord motor output system:
 * - Motor pools for muscle group innervation
 * - Central pattern generators for rhythmic locomotion
 * - Reflex arcs for fast spinal-level responses
 * - Descending tract input buffers
 * - Proprioceptive feedback buffers
 *
 * IDEMPOTENT: If brain->spinal_cord is already non-NULL, returns true.
 *
 * @param brain Brain structure to initialize
 * @return true on success or non-fatal failure, false on critical error
 *
 * COMPLEXITY: O(num_motor_pools * neurons_per_pool)
 * THREAD-SAFE: No (called during brain creation)
 */
bool nimcp_brain_factory_init_spinal_cord_subsystem(brain_t brain);

/**
 * @brief Destroy spinal cord subsystem in brain
 *
 * Cleans up all spinal cord resources:
 * - Destroys spinal cord system and all motor pools
 * - NULL-safe (no-op if brain or spinal_cord is NULL)
 *
 * @param brain Brain structure to clean up
 *
 * COMPLEXITY: O(num_motor_pools)
 * THREAD-SAFE: No (called during brain destruction)
 */
void nimcp_brain_factory_destroy_spinal_cord_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_SPINAL_CORD_H */
