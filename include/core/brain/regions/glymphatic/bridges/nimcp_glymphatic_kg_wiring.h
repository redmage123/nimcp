//=============================================================================
// nimcp_glymphatic_kg_wiring.h - Glymphatic KG Wiring (W4)
//=============================================================================
/**
 * @file nimcp_glymphatic_kg_wiring.h
 * @brief Glymphatic (brain waste-clearance) structural + runtime KG wiring.
 *
 * Admin-token self-elevation pattern per kg-node-naming-registry.md §7.
 * Relevant for sleep-KG crosstalk: clearance rate ~10x higher during SWS.
 */

#ifndef NIMCP_GLYMPHATIC_KG_WIRING_H
#define NIMCP_GLYMPHATIC_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLYMPHATIC_KG_MODULE_NAME  "glymphatic_kg_wiring"
#define GLYMPHATIC_KG_ROOT_NAME    "glymphatic"

NIMCP_EXPORT int  nimcp_glymphatic_kg_wiring_init(brain_t brain);

/** Emit a glymphatic clearance event. Null-safe. Self-elevates. */
NIMCP_EXPORT void glymphatic_emit_clearance(brain_t brain, float volume);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_GLYMPHATIC_KG_WIRING_H */
