//=============================================================================
// nimcp_occipital_kg_wiring.h - Occipital KG Wiring (W3)
//=============================================================================
/**
 * @file nimcp_occipital_kg_wiring.h
 * @brief Occipital cortex structural + runtime KG registration.
 *
 * Adds occipital/V1/V2/V4/IT nodes at init. Provides
 * nimcp_occipital_kg_emit_event for runtime feature-detection events.
 * Admin-token self-elevation pattern (see kg-node-naming-registry.md §7).
 */

#ifndef NIMCP_OCCIPITAL_KG_WIRING_H
#define NIMCP_OCCIPITAL_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OCCIPITAL_KG_MODULE_NAME  "occipital_kg_wiring"
#define OCCIPITAL_KG_ROOT_NAME    "occipital_cortex"

NIMCP_EXPORT int  nimcp_occipital_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_occipital_kg_emit_event(brain_t brain,
                                                const char* kind,
                                                float intensity,
                                                uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_OCCIPITAL_KG_WIRING_H */
