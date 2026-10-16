//=============================================================================
// nimcp_perirhinal_kg_wiring.h - Perirhinal KG Wiring (W3)
//=============================================================================
#ifndef NIMCP_PERIRHINAL_KG_WIRING_H
#define NIMCP_PERIRHINAL_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PERIRHINAL_KG_MODULE_NAME  "perirhinal_kg_wiring"
#define PERIRHINAL_KG_ROOT_NAME    "perirhinal_cortex"

NIMCP_EXPORT int  nimcp_perirhinal_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_perirhinal_kg_emit_event(brain_t brain,
                                                 const char* kind,
                                                 float intensity,
                                                 uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_PERIRHINAL_KG_WIRING_H */
