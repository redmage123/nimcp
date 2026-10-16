//=============================================================================
// nimcp_motor_kg_wiring.h - Motor Region KG Wiring (W3)
//=============================================================================
#ifndef NIMCP_MOTOR_KG_WIRING_H
#define NIMCP_MOTOR_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_KG_MODULE_NAME  "motor_kg_wiring"
#define MOTOR_KG_ROOT_NAME    "motor_cortex"

NIMCP_EXPORT int  nimcp_motor_kg_wiring_init(brain_t brain);
NIMCP_EXPORT void nimcp_motor_kg_emit_event(brain_t brain,
                                            const char* kind,
                                            float intensity,
                                            uint64_t ts_us);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_MOTOR_KG_WIRING_H */
