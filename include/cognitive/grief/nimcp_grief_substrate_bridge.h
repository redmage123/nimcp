/**
 * @file nimcp_grief_substrate_bridge.h
 * @brief Bridge between Grief processing system and neural substrate
 *
 * WHAT: Links grief processing to metabolic state
 * WHY: Grief processing requires sustained emotional and cognitive resources
 * HOW: Monitors ATP/fatigue; modulates grief intensity, processing, adaptation
 *
 * BIOLOGICAL BASIS:
 * - Grief involves anterior cingulate and limbic circuits
 * - ATP depletion intensifies grief experience
 * - Fatigue impairs grief processing and adaptation
 * - Metabolic stress prolongs grief resolution
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_GRIEF_SUBSTRATE_BRIDGE_H
#define NIMCP_GRIEF_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_GRIEF 0x1221

typedef struct {
    float processing_capacity;    /* Capacity for grief processing [0-1] */
    float emotion_regulation;     /* Ability to regulate grief [0-1] */
    float adaptation_rate;        /* Rate of grief adaptation [0-1] */
    float resilience_level;       /* Emotional resilience [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} grief_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} grief_substrate_config_t;

typedef struct grief_substrate_bridge grief_substrate_bridge_t;

grief_substrate_config_t grief_substrate_default_config(void);
grief_substrate_bridge_t* grief_substrate_bridge_create(void* grief, neural_substrate_t* substrate, const grief_substrate_config_t* config);
void grief_substrate_bridge_destroy(grief_substrate_bridge_t* bridge);
int grief_substrate_bridge_update(grief_substrate_bridge_t* bridge);
int grief_substrate_bridge_get_effects(const grief_substrate_bridge_t* bridge, grief_substrate_effects_t* effects);
int grief_substrate_bridge_apply_effects(grief_substrate_bridge_t* bridge);
int grief_substrate_bridge_register_bio_async(grief_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRIEF_SUBSTRATE_BRIDGE_H */
