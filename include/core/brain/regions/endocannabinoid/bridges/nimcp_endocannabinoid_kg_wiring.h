//=============================================================================
// nimcp_endocannabinoid_kg_wiring.h - Endocannabinoid KG Wiring (W4)
//=============================================================================
/**
 * @file nimcp_endocannabinoid_kg_wiring.h
 * @brief Endocannabinoid (eCB) structural + runtime KG registration.
 *
 * Adds endocannabinoid/anandamide/2-AG/CB1/CB2 structural nodes at init.
 * Provides endocannabinoid_emit_retrograde_signal() for runtime retrograde
 * signalling events. Admin-token self-elevation pattern — see
 * docs/claude/kg-node-naming-registry.md §7.
 */

#ifndef NIMCP_ENDOCANNABINOID_KG_WIRING_H
#define NIMCP_ENDOCANNABINOID_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENDOCANNABINOID_KG_MODULE_NAME  "endocannabinoid_kg_wiring"
#define ENDOCANNABINOID_KG_ROOT_NAME    "endocannabinoid"

NIMCP_EXPORT int  nimcp_endocannabinoid_kg_wiring_init(brain_t brain);

/**
 * @brief Emit a runtime eCB retrograde-signalling event.
 *
 * Null-safe. Self-elevates KG access. Event node name:
 *   ecb_event_retrograde_signal_<ts_us>
 *
 * @param brain     Brain handle.
 * @param ligand    "anandamide", "2_ag", or similar ligand identifier.
 * @param strength  Signal strength [0..1].
 */
NIMCP_EXPORT void endocannabinoid_emit_retrograde_signal(
    brain_t brain,
    const char* ligand,
    float strength
);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_ENDOCANNABINOID_KG_WIRING_H */
