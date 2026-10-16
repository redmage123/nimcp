/**
 * @file nimcp_gw_substrate_bridge.h
 * @brief Bridge between Global Workspace and neural substrate
 *
 * WHAT: Bidirectional integration linking global workspace to metabolic state
 * WHY: Conscious broadcasting requires high ATP for widespread cortical activation
 * HOW: Monitors ATP/fatigue; modulates broadcast reach, ignition threshold, coherence
 *
 * BIOLOGICAL BASIS:
 * - Global workspace ignition requires prefrontal-parietal "ignition"
 * - Conscious broadcast consumes significant metabolic resources
 * - ATP depletion reduces broadcast reach and coherence
 * - Fatigue raises ignition threshold (harder to achieve consciousness)
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_GW_SUBSTRATE_BRIDGE_H
#define NIMCP_GW_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_GLOBAL_WORKSPACE 0x1211

typedef struct {
    float broadcast_reach;        /* Extent of cortical broadcast [0-1] */
    float ignition_threshold;     /* Threshold for conscious ignition [0-1] */
    float coherence;              /* Global coherence of workspace [0-1] */
    float processing_depth;       /* Depth of conscious processing [0-1] */
    float overall_capacity;       /* Combined modulation factor [0-1] */
} gw_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} gw_substrate_config_t;

typedef struct gw_substrate_bridge gw_substrate_bridge_t;

gw_substrate_config_t gw_substrate_default_config(void);
gw_substrate_bridge_t* gw_substrate_bridge_create(void* gw, neural_substrate_t* substrate, const gw_substrate_config_t* config);
void gw_substrate_bridge_destroy(gw_substrate_bridge_t* bridge);
int gw_substrate_bridge_update(gw_substrate_bridge_t* bridge);
int gw_substrate_bridge_get_effects(const gw_substrate_bridge_t* bridge, gw_substrate_effects_t* effects);
int gw_substrate_bridge_apply_effects(gw_substrate_bridge_t* bridge);
int gw_substrate_bridge_register_bio_async(gw_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GW_SUBSTRATE_BRIDGE_H */
