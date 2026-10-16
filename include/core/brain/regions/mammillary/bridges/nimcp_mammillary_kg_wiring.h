//=============================================================================
// nimcp_mammillary_kg_wiring.h - Mammillary Bodies KG Wiring (W4)
//=============================================================================
/**
 * Mammillary bodies (Papez circuit relay) structural + runtime KG wiring.
 * Admin-token self-elevation pattern per kg-node-naming-registry.md §7.
 */

#ifndef NIMCP_MAMMILLARY_KG_WIRING_H
#define NIMCP_MAMMILLARY_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAMMILLARY_KG_MODULE_NAME  "mammillary_kg_wiring"
#define MAMMILLARY_KG_ROOT_NAME    "mammillary"

NIMCP_EXPORT int  nimcp_mammillary_kg_wiring_init(brain_t brain);

/** Emit a Papez-circuit relay event. Null-safe. Self-elevates. */
NIMCP_EXPORT void mammillary_emit_relay(brain_t brain, float signal_magnitude);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_MAMMILLARY_KG_WIRING_H */
