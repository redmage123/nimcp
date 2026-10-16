/**
 * @file nimcp_knowledge_substrate_bridge.h
 * @brief Bridge between Knowledge systems and neural substrate
 *
 * WHAT: Links knowledge retrieval/storage to metabolic state
 * WHY: Knowledge access requires temporal lobe and hippocampal activation
 * HOW: Monitors ATP/fatigue; modulates retrieval speed, accuracy, consolidation
 *
 * BIOLOGICAL BASIS:
 * - Knowledge retrieval involves hippocampus and temporal cortex
 * - ATP depletion slows retrieval and increases errors
 * - Fatigue impairs memory consolidation
 * - Metabolic stress causes memory blocking
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_KNOWLEDGE_SUBSTRATE_BRIDGE_H
#define NIMCP_KNOWLEDGE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_KNOWLEDGE 0x1213

typedef struct {
    float retrieval_speed;        /* Speed of knowledge retrieval [0-1] */
    float retrieval_accuracy;     /* Accuracy of retrieval [0-1] */
    float consolidation_rate;     /* Rate of memory consolidation [0-1] */
    float association_strength;   /* Strength of associations [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} knowledge_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} knowledge_substrate_config_t;

typedef struct knowledge_substrate_bridge knowledge_substrate_bridge_t;

knowledge_substrate_config_t knowledge_substrate_default_config(void);
knowledge_substrate_bridge_t* knowledge_substrate_bridge_create(void* kg, neural_substrate_t* substrate, const knowledge_substrate_config_t* config);
void knowledge_substrate_bridge_destroy(knowledge_substrate_bridge_t* bridge);
int knowledge_substrate_bridge_update(knowledge_substrate_bridge_t* bridge);
int knowledge_substrate_bridge_get_effects(const knowledge_substrate_bridge_t* bridge, knowledge_substrate_effects_t* effects);
int knowledge_substrate_bridge_apply_effects(knowledge_substrate_bridge_t* bridge);
int knowledge_substrate_bridge_register_bio_async(knowledge_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOWLEDGE_SUBSTRATE_BRIDGE_H */
