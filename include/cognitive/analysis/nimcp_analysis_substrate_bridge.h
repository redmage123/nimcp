/**
 * @file nimcp_analysis_substrate_bridge.h
 * @brief Bridge between Analysis system and neural substrate
 *
 * WHAT: Links analytical processing to metabolic state
 * WHY: Analysis requires sustained prefrontal-parietal processing
 * HOW: Monitors ATP/fatigue; modulates analysis depth, precision, speed
 *
 * BIOLOGICAL BASIS:
 * - Analysis involves dorsolateral PFC and parietal cortex
 * - ATP depletion reduces analytical depth
 * - Fatigue impairs systematic analysis
 * - Metabolic stress favors quick heuristics over deep analysis
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_ANALYSIS_SUBSTRATE_BRIDGE_H
#define NIMCP_ANALYSIS_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_ANALYSIS 0x1229

typedef struct {
    float analysis_depth;         /* Depth of analysis [0-1] */
    float precision_level;        /* Precision of analysis [0-1] */
    float processing_speed;       /* Speed of analytical processing [0-1] */
    float decomposition_ability;  /* Ability to break down problems [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} analysis_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} analysis_substrate_config_t;

typedef struct analysis_substrate_bridge analysis_substrate_bridge_t;

analysis_substrate_config_t analysis_substrate_default_config(void);
analysis_substrate_bridge_t* analysis_substrate_bridge_create(void* analysis, neural_substrate_t* substrate, const analysis_substrate_config_t* config);
void analysis_substrate_bridge_destroy(analysis_substrate_bridge_t* bridge);
int analysis_substrate_bridge_update(analysis_substrate_bridge_t* bridge);
int analysis_substrate_bridge_get_effects(const analysis_substrate_bridge_t* bridge, analysis_substrate_effects_t* effects);
int analysis_substrate_bridge_apply_effects(analysis_substrate_bridge_t* bridge);
int analysis_substrate_bridge_register_bio_async(analysis_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANALYSIS_SUBSTRATE_BRIDGE_H */
