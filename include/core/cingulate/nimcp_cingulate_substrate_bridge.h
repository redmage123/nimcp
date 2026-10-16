/**
 * @file nimcp_cingulate_substrate_bridge.h
 * @brief Bridge between Cingulate Cortex and neural substrate
 *
 * WHAT: Links cingulate function to metabolic state
 * WHY: ACC is critical for error monitoring and conflict resolution
 * HOW: Monitors ATP/fatigue; modulates error detection, conflict monitoring
 *
 * BIOLOGICAL BASIS:
 * - ACC monitors cognitive conflict and errors
 * - ATP depletion reduces error detection accuracy
 * - Fatigue impairs conflict resolution
 * - Metabolic stress affects emotional regulation
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_CINGULATE_SUBSTRATE_BRIDGE_H
#define NIMCP_CINGULATE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_CINGULATE 0x1238

typedef struct {
    float error_detection;        /* Error monitoring accuracy [0-1] */
    float conflict_resolution;    /* Conflict handling capacity [0-1] */
    float emotional_regulation;   /* Emotion regulation [0-1] */
    float pain_processing;        /* Pain signal integration [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} cingulate_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} cingulate_substrate_config_t;

typedef struct cingulate_substrate_bridge cingulate_substrate_bridge_t;

cingulate_substrate_config_t cingulate_substrate_default_config(void);
cingulate_substrate_bridge_t* cingulate_substrate_bridge_create(void* cingulate, neural_substrate_t* substrate, const cingulate_substrate_config_t* config);
void cingulate_substrate_bridge_destroy(cingulate_substrate_bridge_t* bridge);
int cingulate_substrate_bridge_update(cingulate_substrate_bridge_t* bridge);
int cingulate_substrate_bridge_get_effects(const cingulate_substrate_bridge_t* bridge, cingulate_substrate_effects_t* effects);
int cingulate_substrate_bridge_apply_effects(cingulate_substrate_bridge_t* bridge);
int cingulate_substrate_bridge_register_bio_async(cingulate_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CINGULATE_SUBSTRATE_BRIDGE_H */
