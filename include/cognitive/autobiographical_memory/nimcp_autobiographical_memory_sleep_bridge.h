/**
 * @file nimcp_autobiographical_memory_sleep_bridge.h
 * @brief Sleep-Autobiographical Memory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and autobiographical memory
 * WHY:  Episodic memory consolidation critically depends on sleep
 * HOW:  Sleep state modulates encoding, consolidation, and retrieval of personal memories
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active encoding of new episodic memories
 * - DROWSY: Reduced encoding efficiency, memory lapses
 * - LIGHT NREM: Hippocampal-cortical dialogue (memory sorting)
 * - DEEP NREM: Strong consolidation via slow oscillations (hippocampus → cortex)
 * - REM: Integration of episodic memories into narrative, emotional processing
 *
 * Sleep deprivation effects:
 * - Impaired memory encoding (can't form new autobiographical memories)
 * - Reduced consolidation of recent experiences
 * - Fragmented memory retrieval
 * - Loss of temporal context
 * - Difficulty integrating memories into life narrative
 *
 * Sleep benefits autobiographical memory:
 * - NREM consolidates hippocampal traces into cortical long-term storage
 * - REM integrates memories into coherent life story
 * - Sleep protects memories from interference
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AUTOBIOGRAPHICAL_MEMORY_SLEEP_BRIDGE_H
#define NIMCP_AUTOBIOGRAPHICAL_MEMORY_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define AUTOBIO_SLEEP_ENCODING_AWAKE       1.0f
#define AUTOBIO_SLEEP_ENCODING_DROWSY      0.5f
#define AUTOBIO_SLEEP_ENCODING_LIGHT_NREM  0.0f
#define AUTOBIO_SLEEP_ENCODING_DEEP_NREM   0.0f
#define AUTOBIO_SLEEP_ENCODING_REM         0.0f

#define AUTOBIO_SLEEP_CONSOLIDATION_AWAKE       0.1f  /* Minimal during wake */
#define AUTOBIO_SLEEP_CONSOLIDATION_DROWSY      0.2f
#define AUTOBIO_SLEEP_CONSOLIDATION_LIGHT_NREM  0.7f  /* Active consolidation */
#define AUTOBIO_SLEEP_CONSOLIDATION_DEEP_NREM   1.0f  /* Peak consolidation */
#define AUTOBIO_SLEEP_CONSOLIDATION_REM         0.8f  /* Integration */

typedef struct {
    bool enable_encoding_modulation;
    bool enable_consolidation_modulation;
    float modulation_strength;
} autobio_sleep_config_t;

typedef struct {
    float encoding_efficiency_factor;
    float consolidation_rate_factor;
    float retrieval_accuracy_factor;
    float narrative_integration_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool consolidation_active;
} autobio_sleep_effects_t;

typedef struct autobio_sleep_bridge_struct* autobio_sleep_bridge_t;

int autobio_sleep_default_config(autobio_sleep_config_t* config);
autobio_sleep_bridge_t autobio_sleep_bridge_create(const autobio_sleep_config_t* config, sleep_system_t sleep);
void autobio_sleep_bridge_destroy(autobio_sleep_bridge_t bridge);
int autobio_sleep_update(autobio_sleep_bridge_t bridge);
int autobio_sleep_get_effects(const autobio_sleep_bridge_t bridge, autobio_sleep_effects_t* effects);
float autobio_sleep_get_encoding_efficiency(const autobio_sleep_bridge_t bridge);
bool autobio_sleep_is_consolidation_active(const autobio_sleep_bridge_t bridge);

float autobio_sleep_encoding_for_state(sleep_state_t state);
float autobio_sleep_consolidation_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUTOBIOGRAPHICAL_MEMORY_SLEEP_BRIDGE_H */
