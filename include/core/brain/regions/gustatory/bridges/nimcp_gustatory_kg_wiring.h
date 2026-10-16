//=============================================================================
// nimcp_gustatory_kg_wiring.h - Gustatory KG Wiring (W3)
//=============================================================================
#ifndef NIMCP_GUSTATORY_KG_WIRING_H
#define NIMCP_GUSTATORY_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUSTATORY_KG_MODULE_NAME  "gustatory_kg_wiring"
#define GUSTATORY_KG_ROOT_NAME    "gustatory_cortex"

NIMCP_EXPORT int  nimcp_gustatory_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_gustatory_kg_emit_event(brain_t brain,
                                                const char* kind,
                                                float intensity,
                                                uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_GUSTATORY_KG_WIRING_H */
