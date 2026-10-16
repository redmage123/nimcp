//=============================================================================
// nimcp_brainstem_kg_wiring.h - Brainstem KG Wiring (W3)
//=============================================================================
#ifndef NIMCP_BRAINSTEM_KG_WIRING_H
#define NIMCP_BRAINSTEM_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BRAINSTEM_KG_MODULE_NAME  "brainstem_kg_wiring"
#define BRAINSTEM_KG_ROOT_NAME    "brainstem"

NIMCP_EXPORT int  nimcp_brainstem_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_brainstem_kg_emit_event(brain_t brain,
                                                const char* kind,
                                                float intensity,
                                                uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_BRAINSTEM_KG_WIRING_H */
