//=============================================================================
// nimcp_raphe_kg_wiring.h - Raphe KG Wiring (W3)
//=============================================================================
#ifndef NIMCP_RAPHE_KG_WIRING_H
#define NIMCP_RAPHE_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAPHE_KG_MODULE_NAME  "raphe_kg_wiring"
#define RAPHE_KG_ROOT_NAME    "raphe_nuclei"

NIMCP_EXPORT int  nimcp_raphe_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_raphe_kg_emit_event(brain_t brain,
                                            const char* kind,
                                            float intensity,
                                            uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_RAPHE_KG_WIRING_H */
