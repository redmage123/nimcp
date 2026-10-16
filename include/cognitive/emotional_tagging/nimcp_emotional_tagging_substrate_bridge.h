/**
 * @file nimcp_emotional_tagging_substrate_bridge.h
 * @brief Bridge between Emotional Tagging system and neural substrate
 *
 * WHAT: Links emotional tagging of memories to metabolic state
 * WHY: Emotional tagging requires amygdala-hippocampus coordination
 * HOW: Monitors ATP/fatigue; modulates tagging strength, specificity, retrieval
 *
 * BIOLOGICAL BASIS:
 * - Emotional tagging involves amygdala-hippocampal interaction
 * - ATP depletion affects emotional memory consolidation
 * - Fatigue may increase negative emotional tagging
 * - Metabolic state influences emotional salience assignment
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMOTIONAL_TAGGING_SUBSTRATE_BRIDGE_H
#define NIMCP_EMOTIONAL_TAGGING_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_EMOTIONAL_TAGGING 0x122A

typedef struct {
    float tagging_strength;       /* Strength of emotional tags [0-1] */
    float tag_specificity;        /* Specificity of emotional labels [0-1] */
    float consolidation_quality;  /* Quality of emotional memory consolidation [0-1] */
    float retrieval_accuracy;     /* Accuracy of emotional memory retrieval [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} emotional_tagging_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} emotional_tagging_substrate_config_t;

typedef struct emotional_tagging_substrate_bridge emotional_tagging_substrate_bridge_t;

emotional_tagging_substrate_config_t emotional_tagging_substrate_default_config(void);
emotional_tagging_substrate_bridge_t* emotional_tagging_substrate_bridge_create(void* emotional_tagging, neural_substrate_t* substrate, const emotional_tagging_substrate_config_t* config);
void emotional_tagging_substrate_bridge_destroy(emotional_tagging_substrate_bridge_t* bridge);
int emotional_tagging_substrate_bridge_update(emotional_tagging_substrate_bridge_t* bridge);
int emotional_tagging_substrate_bridge_get_effects(const emotional_tagging_substrate_bridge_t* bridge, emotional_tagging_substrate_effects_t* effects);
int emotional_tagging_substrate_bridge_apply_effects(emotional_tagging_substrate_bridge_t* bridge);
int emotional_tagging_substrate_bridge_register_bio_async(emotional_tagging_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_TAGGING_SUBSTRATE_BRIDGE_H */
