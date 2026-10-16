//=============================================================================
// nimcp_brain_init_white_matter.h - White Matter Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_white_matter.h
 * @brief White matter tract subsystem initialization for brain factory
 *
 * WHAT: Initialization of the white matter tract modeling subsystem
 * WHY:  Provides biologically-realistic inter-region signal conduction with
 *       myelination-dependent velocity, integrity tracking, and pink noise jitter
 * HOW:  Creates wmt_system_t with default config, stores on brain struct
 *
 * BIOLOGICAL RATIONALE:
 * White matter comprises ~45% of the human brain volume and consists of
 * myelinated axon bundles (tracts) that connect cortical and subcortical
 * regions. Conduction velocity ranges from 1 m/s (unmyelinated) to 120 m/s
 * (heavily myelinated), directly affecting cognitive processing speed.
 *
 * Major tracts modeled:
 * - Corpus callosum: Interhemispheric transfer (200M+ axons)
 * - Arcuate fasciculus: Language (Broca <-> Wernicke)
 * - Uncinate fasciculus: Emotion-cognition integration
 * - Cingulum: Attention and memory
 * - IFOF: Visual-semantic processing
 * - Corticospinal: Motor output
 * - Spinothalamic: Sensory relay (pain, temperature)
 * - Optic radiation: Visual input (LGN -> V1)
 *
 * INTEGRATION POINTS:
 * - Sleep system: Myelination repair during SWS
 * - Immune system: Demyelination from neuroinflammation
 * - Training: Use-dependent myelination
 * - Thalamus: Sensory relay timing
 * - Inference: Inter-region signal delay modeling
 *
 * @version 1.0.0
 * @date 2026-03-05
 */

#ifndef NIMCP_BRAIN_INIT_WHITE_MATTER_H
#define NIMCP_BRAIN_INIT_WHITE_MATTER_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Initialize white matter tract subsystem for brain
 *
 * WHAT: Creates and integrates white matter tract system with the brain
 * WHY:  Enables biologically-realistic inter-region conduction modeling
 * HOW:  Creates wmt_system_t with default config, sets brain->white_matter
 *
 * IDEMPOTENCY: Returns true immediately if brain->white_matter is already set.
 *
 * INITIALIZATION ORDER:
 * 1. Check idempotency (return true if already initialized)
 * 2. Create wmt_system_t with wmt_default_config()
 * 3. Store on brain->white_matter
 * 4. Set brain->white_matter_enabled = true
 *
 * @param brain The brain to initialize white matter for
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_white_matter_subsystem(brain_t brain);

/**
 * @brief Destroy white matter tract subsystem
 *
 * WHAT: Cleans up white matter resources
 * WHY:  Proper resource management during brain teardown
 * HOW:  Calls wmt_destroy(), sets brain->white_matter to NULL
 *
 * @param brain The brain containing the white matter subsystem
 */
void nimcp_brain_factory_destroy_white_matter_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_WHITE_MATTER_H */
