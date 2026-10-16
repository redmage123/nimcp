//=============================================================================
// nimcp_broca_kg_wiring.h - Broca KG Wiring (W3)
//=============================================================================
#ifndef NIMCP_BROCA_KG_WIRING_H
#define NIMCP_BROCA_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BROCA_KG_MODULE_NAME  "broca_kg_wiring"
#define BROCA_KG_ROOT_NAME    "broca_area"

NIMCP_EXPORT int  nimcp_broca_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_broca_kg_emit_event(brain_t brain,
                                            const char* kind,
                                            float intensity,
                                            uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_BROCA_KG_WIRING_H */
