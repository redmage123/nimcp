/**
 * @file nimcp_curiosity_substrate_bridge.h
 * @brief Bridge between Curiosity system and neural substrate
 *
 * WHAT: Links curiosity/exploration drive to metabolic state
 * WHY: Curiosity requires dopaminergic and noradrenergic resources
 * HOW: Monitors ATP/fatigue; modulates exploration drive, novelty seeking, information gain
 *
 * BIOLOGICAL BASIS:
 * - Curiosity involves dopaminergic reward prediction circuits
 * - ATP depletion reduces exploratory behavior
 * - Fatigue decreases novelty seeking
 * - Metabolic stress favors exploitation over exploration
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_CURIOSITY_SUBSTRATE_BRIDGE_H
#define NIMCP_CURIOSITY_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_CURIOSITY 0x1219

typedef struct {
    float exploration_drive;      /* Motivation to explore [0-1] */
    float novelty_seeking;        /* Attraction to novel stimuli [0-1] */
    float information_gain;       /* Value of information seeking [0-1] */
    float uncertainty_tolerance;  /* Tolerance for unknown [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} curiosity_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} curiosity_substrate_config_t;

typedef struct curiosity_substrate_bridge curiosity_substrate_bridge_t;

curiosity_substrate_config_t curiosity_substrate_default_config(void);
curiosity_substrate_bridge_t* curiosity_substrate_bridge_create(void* curiosity, neural_substrate_t* substrate, const curiosity_substrate_config_t* config);
void curiosity_substrate_bridge_destroy(curiosity_substrate_bridge_t* bridge);
int curiosity_substrate_bridge_update(curiosity_substrate_bridge_t* bridge);
int curiosity_substrate_bridge_get_effects(const curiosity_substrate_bridge_t* bridge, curiosity_substrate_effects_t* effects);
int curiosity_substrate_bridge_apply_effects(curiosity_substrate_bridge_t* bridge);
int curiosity_substrate_bridge_register_bio_async(curiosity_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_SUBSTRATE_BRIDGE_H */
