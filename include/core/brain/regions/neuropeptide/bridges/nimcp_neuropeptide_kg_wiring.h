//=============================================================================
// nimcp_neuropeptide_kg_wiring.h - Neuropeptide KG Wiring (W4)
//=============================================================================
/**
 * Neuropeptide (slow modulatory) system structural + runtime KG wiring.
 * Admin-token self-elevation pattern per kg-node-naming-registry.md §7.
 */

#ifndef NIMCP_NEUROPEPTIDE_KG_WIRING_H
#define NIMCP_NEUROPEPTIDE_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NEUROPEPTIDE_KG_MODULE_NAME  "neuropeptide_kg_wiring"
#define NEUROPEPTIDE_KG_ROOT_NAME    "neuropeptide"

NIMCP_EXPORT int  nimcp_neuropeptide_kg_wiring_init(brain_t brain);

/**
 * @brief Emit a runtime peptide-release event.
 *
 * Null-safe. Self-elevates. Peptide name is recorded in the event description
 * and the event is linked to the matching structural peptide node if present.
 *
 * @param brain         Brain handle.
 * @param peptide_name  e.g. "oxytocin", "crh", "orexin" (free string).
 * @param concentration Release magnitude.
 */
NIMCP_EXPORT void neuropeptide_emit_release(
    brain_t brain,
    const char* peptide_name,
    float concentration
);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_NEUROPEPTIDE_KG_WIRING_H */
