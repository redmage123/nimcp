//=============================================================================
// nimcp_brain_init_inferior_colliculus.h - IC Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_inferior_colliculus.h
 * @brief Inferior colliculus subsystem initialization for brain factory
 *
 * WHAT: Initialization of the inferior colliculus as the auditory midbrain relay
 * WHY:  All ascending auditory information passes through IC before thalamus/cortex
 * HOW:  Creates IC with tonotopic channels, configures binaural processing
 *
 * BIOLOGICAL RATIONALE:
 * The inferior colliculus is the principal midbrain auditory nucleus. It
 * receives convergent input from multiple lower brainstem auditory nuclei
 * (cochlear nucleus, superior olivary complex, lateral lemniscus) and
 * projects primarily to the thalamic MGN. The IC is the first site where
 * complete spectral, temporal, and spatial representations of sound are
 * integrated.
 *
 * INTEGRATION POINTS:
 * - Thalamus (MGN): IC output relayed to MGN for cortical forwarding
 * - Superior colliculus: Sound localization drives orienting responses
 * - Auditory cortex: Top-down modulation via ICD (dorsal cortex)
 *
 * @version 1.0.0
 * @date 2026-03-05
 */

#ifndef NIMCP_BRAIN_INIT_INFERIOR_COLLICULUS_H
#define NIMCP_BRAIN_INIT_INFERIOR_COLLICULUS_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/subcortical/nimcp_inferior_colliculus.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize inferior colliculus subsystem for brain
 *
 * WHAT: Creates and integrates IC with the brain
 * WHY:  Enables auditory midbrain processing and sound localization
 * HOW:  Creates IC with default config, connects to brain
 *
 * INITIALIZATION ORDER:
 * 1. Check idempotency (skip if already initialized)
 * 2. Create IC with default configuration
 * 3. Set brain->inferior_colliculus pointer
 * 4. Set brain->inferior_colliculus_enabled = true
 *
 * @param brain The brain to initialize IC for
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_inferior_colliculus_subsystem(brain_t brain);

/**
 * @brief Destroy inferior colliculus subsystem
 *
 * @param brain The brain containing the IC
 */
void nimcp_brain_ic_destroy(brain_t brain);

/**
 * @brief Step IC subsystem
 *
 * @param brain The brain containing the IC
 * @param dt_s Timestep in seconds
 * @return 0 on success, -1 on error
 */
int nimcp_brain_ic_step(brain_t brain, float dt_s);

/**
 * @brief Process audio through brain's IC
 *
 * @param brain The brain containing the IC
 * @param left Left ear samples
 * @param right Right ear samples
 * @param num_samples Number of samples per channel
 * @return 0 on success, -1 on error
 */
int nimcp_brain_ic_process_audio(brain_t brain,
                                  const float* left,
                                  const float* right,
                                  uint32_t num_samples);

/**
 * @brief Check if IC is enabled
 *
 * @param brain The brain to check
 * @return true if IC is enabled and initialized
 */
bool nimcp_brain_ic_is_enabled(brain_t brain);

/**
 * @brief Get direct pointer to IC (advanced use only)
 *
 * @param brain The brain containing the IC
 * @return Pointer to IC, or NULL if not enabled
 */
inferior_colliculus_t* nimcp_brain_ic_get_handle(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_INFERIOR_COLLICULUS_H */
