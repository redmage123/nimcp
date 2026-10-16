//=============================================================================
// nimcp_olfactory_kg_wiring.h - Olfactory KG Wiring (W3)
//=============================================================================
#ifndef NIMCP_OLFACTORY_KG_WIRING_H
#define NIMCP_OLFACTORY_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLFACTORY_KG_MODULE_NAME  "olfactory_kg_wiring"
#define OLFACTORY_KG_ROOT_NAME    "olfactory_system"

NIMCP_EXPORT int  nimcp_olfactory_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_olfactory_kg_emit_event(brain_t brain,
                                                const char* kind,
                                                float intensity,
                                                uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_OLFACTORY_KG_WIRING_H */
