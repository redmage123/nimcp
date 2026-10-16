//=============================================================================
// nimcp_parahippocampal_kg_wiring.h - Parahippocampal KG Wiring (W3)
//=============================================================================
#ifndef NIMCP_PARAHIPPOCAMPAL_KG_WIRING_H
#define NIMCP_PARAHIPPOCAMPAL_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PARAHIPPOCAMPAL_KG_MODULE_NAME  "parahippocampal_kg_wiring"
#define PARAHIPPOCAMPAL_KG_ROOT_NAME    "parahippocampal_cortex"

NIMCP_EXPORT int  nimcp_parahippocampal_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_parahippocampal_kg_emit_event(brain_t brain,
                                                      const char* kind,
                                                      float intensity,
                                                      uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_PARAHIPPOCAMPAL_KG_WIRING_H */
