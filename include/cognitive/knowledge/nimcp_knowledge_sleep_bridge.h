/**
 * @file nimcp_knowledge_sleep_bridge.h
 * @brief Sleep-Knowledge Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and semantic knowledge
 * WHY:  Semantic memory access and knowledge consolidation depend on sleep
 * HOW:  Sleep state modulates knowledge retrieval, consolidation, and integration
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active knowledge retrieval and learning
 * - DROWSY: Slower semantic access, retrieval deficits
 * - LIGHT NREM: Knowledge sorting and reorganization
 * - DEEP NREM: Semantic memory consolidation (hippocampus → neocortex)
 * - REM: Knowledge integration and creative connections
 *
 * Sleep deprivation effects:
 * - Impaired semantic memory retrieval (word-finding difficulties)
 * - Reduced knowledge integration
 * - Difficulty forming new conceptual links
 * - Slower knowledge access
 * - Reduced learning of new facts
 *
 * Sleep benefits knowledge:
 * - NREM consolidates semantic facts into long-term storage
 * - REM creates novel associations between concepts
 * - Sleep reorganizes knowledge networks for efficiency
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KNOWLEDGE_SLEEP_BRIDGE_H
#define NIMCP_KNOWLEDGE_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define KNOWLEDGE_SLEEP_RETRIEVAL_AWAKE       1.0f
#define KNOWLEDGE_SLEEP_RETRIEVAL_DROWSY      0.7f
#define KNOWLEDGE_SLEEP_RETRIEVAL_LIGHT_NREM  0.2f
#define KNOWLEDGE_SLEEP_RETRIEVAL_DEEP_NREM   0.0f
#define KNOWLEDGE_SLEEP_RETRIEVAL_REM         0.4f

#define KNOWLEDGE_SLEEP_CONSOLIDATION_AWAKE       0.1f
#define KNOWLEDGE_SLEEP_CONSOLIDATION_DROWSY      0.2f
#define KNOWLEDGE_SLEEP_CONSOLIDATION_LIGHT_NREM  0.6f
#define KNOWLEDGE_SLEEP_CONSOLIDATION_DEEP_NREM   1.0f
#define KNOWLEDGE_SLEEP_CONSOLIDATION_REM         0.7f

typedef struct {
    bool enable_retrieval_modulation;
    bool enable_consolidation_modulation;
    float modulation_strength;
} knowledge_sleep_config_t;

typedef struct {
    float retrieval_speed_factor;
    float consolidation_rate_factor;
    float integration_factor;
    float association_strength_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool consolidation_active;
} knowledge_sleep_effects_t;

typedef struct knowledge_sleep_bridge_struct* knowledge_sleep_bridge_t;

int knowledge_sleep_default_config(knowledge_sleep_config_t* config);
knowledge_sleep_bridge_t knowledge_sleep_bridge_create(const knowledge_sleep_config_t* config, sleep_system_t sleep);
void knowledge_sleep_bridge_destroy(knowledge_sleep_bridge_t bridge);
int knowledge_sleep_update(knowledge_sleep_bridge_t bridge);
int knowledge_sleep_get_effects(const knowledge_sleep_bridge_t bridge, knowledge_sleep_effects_t* effects);
float knowledge_sleep_get_retrieval_speed(const knowledge_sleep_bridge_t bridge);
bool knowledge_sleep_is_consolidation_active(const knowledge_sleep_bridge_t bridge);

float knowledge_sleep_retrieval_for_state(sleep_state_t state);
float knowledge_sleep_consolidation_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOWLEDGE_SLEEP_BRIDGE_H */
