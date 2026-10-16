/**
 * @file nimcp_brain_init_cortical_interneurons.h
 * @brief Cortical Interneuron Subsystem Factory Initialization
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Factory initialization functions for cortical interneuron subsystem
 * WHY:  SRP refactoring - separate interneuron initialization from core brain creation
 * HOW:  Creates interneuron system with default config, connects bridge integrations
 *
 * BIOLOGICAL BASIS:
 * - GABAergic interneurons comprise ~20% of cortical neurons
 * - Five major types (PV basket, PV chandelier, SST Martinotti, VIP, NGF)
 *   implement distinct inhibitory circuit motifs
 * - E/I balance (~4:1) is maintained homeostatically
 * - PV basket cells generate gamma oscillations (30-80 Hz)
 * - VIP cells provide attention-gated disinhibition
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_CORTICAL_INTERNEURONS_H
#define NIMCP_BRAIN_INIT_CORTICAL_INTERNEURONS_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize cortical interneuron subsystem
 *
 * WHAT: Creates the cortical interneuron system with default configuration
 *       and connects all integration bridges
 * WHY:  Enable inhibitory circuit modeling for E/I balance, gamma oscillations,
 *       attention gating, and prediction error computation
 * HOW:  Creates system with cint_default_config, stores on brain struct,
 *       then connects cortical columns, plasticity, training, inference,
 *       thalamic TRN, bio-async, immune, and substrate GPU bridges
 *
 * IDEMPOTENCY: Returns true immediately if brain->cortical_interneurons is
 *              already non-NULL (safe to call multiple times)
 *
 * @param brain Brain instance to initialize (internal struct pointer)
 * @return true on success or non-fatal skip, false on critical error
 */
bool nimcp_brain_factory_init_cortical_interneurons_subsystem(brain_t brain);

/**
 * @brief Destroy cortical interneuron subsystem
 *
 * WHAT: Clean up interneuron system resources from brain
 * WHY:  Proper cleanup during brain destruction
 * HOW:  Calls cint_destroy and NULLs the brain field
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_cortical_interneurons_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_CORTICAL_INTERNEURONS_H */
