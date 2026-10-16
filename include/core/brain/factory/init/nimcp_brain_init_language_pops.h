//=============================================================================
// nimcp_brain_init_language_pops.h - Language + Sensorymotor SNN Populations
//=============================================================================
/**
 * @file nimcp_brain_init_language_pops.h
 * @brief Cold-init of biological substrate pops for language + sensorymotor.
 *
 * Adds four lightweight (CSR) populations on top of the hierarchical SNN
 * created by snn_create_hierarchical_network:
 *
 *   - wernicke_pop      32K neurons   (BA22 STG analog, comprehension)
 *   - broca_pop         32K neurons   (BA44/45 IFG analog, production)
 *   - arcuate_pop       16K neurons   (gray-matter relay paired with the
 *                                      arcuate fasciculus white-matter tract)
 *   - sensorymotor_ring 20K neurons   (synfire-chain delay line: ~200 stages
 *                                      × 100 neurons → 2 s history horizon)
 *
 * Stage 1 (this file): pops are created cold and unconnected. Broca/Wernicke
 * adapter attach + sensor_hub/cerebellum/basal_ganglia wiring is Stage 2.
 *
 * Resume safety: SNN checkpoint loader fresh-creates extra pops on resume,
 * so this can land mid-training without invalidating an existing checkpoint.
 */

#ifndef NIMCP_BRAIN_INIT_LANGUAGE_POPS_H
#define NIMCP_BRAIN_INIT_LANGUAGE_POPS_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Add the four language + sensorymotor pops to brain->snn_network.
 *
 * Idempotent: bails out cleanly if the pops already exist (looked up by name)
 * or if SNN_MAX_POPULATIONS is reached.
 *
 * @param brain Brain instance with snn_network already created.
 * @return true on success (or if SNN unavailable — non-fatal),
 *         false only on hard allocation/registration failure.
 */
bool nimcp_brain_factory_init_language_pops(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_LANGUAGE_POPS_H */
