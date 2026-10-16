/**
 * @file nimcp_auditory_substrate_bridge.h
 * @brief Bridge between Auditory processing and neural substrate
 *
 * WHAT: Links auditory processing to metabolic state
 * WHY: Auditory cortex requires sustained energy for temporal processing
 * HOW: Monitors ATP/fatigue; modulates frequency resolution, speech processing
 *
 * BIOLOGICAL BASIS:
 * - Auditory cortex requires precise temporal processing
 * - ATP depletion reduces frequency discrimination
 * - Fatigue impairs speech comprehension in noise
 * - Metabolic stress affects auditory attention
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_AUDITORY_SUBSTRATE_BRIDGE_H
#define NIMCP_AUDITORY_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_AUDITORY 0x1232

typedef struct {
    float frequency_resolution;   /* Frequency discrimination [0-1] */
    float temporal_precision;     /* Temporal processing precision [0-1] */
    float speech_processing;      /* Speech comprehension quality [0-1] */
    float noise_filtering;        /* Signal-noise separation [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} auditory_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} auditory_substrate_config_t;

typedef struct auditory_substrate_bridge auditory_substrate_bridge_t;

auditory_substrate_config_t auditory_substrate_default_config(void);
auditory_substrate_bridge_t* auditory_substrate_bridge_create(void* auditory, neural_substrate_t* substrate, const auditory_substrate_config_t* config);
void auditory_substrate_bridge_destroy(auditory_substrate_bridge_t* bridge);
int auditory_substrate_bridge_update(auditory_substrate_bridge_t* bridge);
int auditory_substrate_bridge_get_effects(const auditory_substrate_bridge_t* bridge, auditory_substrate_effects_t* effects);
int auditory_substrate_bridge_apply_effects(auditory_substrate_bridge_t* bridge);
int auditory_substrate_bridge_register_bio_async(auditory_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDITORY_SUBSTRATE_BRIDGE_H */
