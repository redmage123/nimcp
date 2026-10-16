//=============================================================================
// nimcp_sensory_integration_kg_wiring.h - Sensory Integration KG Wiring (W4)
//=============================================================================
/**
 * Cross-modal sensory-integration structural + runtime KG wiring.
 * Admin-token self-elevation pattern per kg-node-naming-registry.md §7.
 */

#ifndef NIMCP_SENSORY_INTEGRATION_KG_WIRING_H
#define NIMCP_SENSORY_INTEGRATION_KG_WIRING_H

#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSORY_INTEGRATION_KG_MODULE_NAME  "sensory_integration_kg_wiring"
#define SENSORY_INTEGRATION_KG_ROOT_NAME    "sensory_integration"

NIMCP_EXPORT int  nimcp_sensory_integration_kg_wiring_init(brain_t brain);

/**
 * @brief Emit a crossmodal binding event. Null-safe. Self-elevates.
 * @param modality_a First modality ("visual", "audio", "touch", ...).
 * @param modality_b Second modality.
 * @param strength   Binding strength [0..1].
 */
NIMCP_EXPORT void sensory_integration_emit_crossmodal_bind(
    brain_t brain,
    const char* modality_a,
    const char* modality_b,
    float strength
);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_SENSORY_INTEGRATION_KG_WIRING_H */
