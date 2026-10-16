/**
 * @file nimcp_pattern_db_sleep_bridge.h
 * @brief Sleep-Pattern Database Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between sleep/wake system and pattern database
 * WHY:  Sleep states affect pattern matching sensitivity and consolidation
 * HOW:  Sleep state modulates match confidence, priority threshold, and pattern updates
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full pattern matching, normal updates
 * - DROWSY: Reduced sensitivity, focus on high-priority
 * - LIGHT_NREM: Pattern consolidation active
 * - DEEP_NREM: Consolidation only, updates disabled
 * - REM: Consolidation continues, moderate matching
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PATTERN_DB_SLEEP_BRIDGE_H
#define NIMCP_PATTERN_DB_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Match confidence modulation */
#define PATTERN_DB_SLEEP_CONF_AWAKE           1.0f
#define PATTERN_DB_SLEEP_CONF_DROWSY          0.95f
#define PATTERN_DB_SLEEP_CONF_LIGHT_NREM      0.85f
#define PATTERN_DB_SLEEP_CONF_DEEP_NREM       0.7f
#define PATTERN_DB_SLEEP_CONF_REM             0.9f

/* Priority threshold modulation (higher = focus on high-priority) */
#define PATTERN_DB_SLEEP_PRIO_AWAKE           1.0f
#define PATTERN_DB_SLEEP_PRIO_DROWSY          1.1f
#define PATTERN_DB_SLEEP_PRIO_LIGHT_NREM      1.3f
#define PATTERN_DB_SLEEP_PRIO_DEEP_NREM       1.5f
#define PATTERN_DB_SLEEP_PRIO_REM             1.2f

typedef struct {
    bool enable_confidence_modulation;
    bool enable_priority_modulation;
    float modulation_strength;
} pattern_db_sleep_config_t;

typedef struct {
    float confidence_factor;
    float priority_threshold_factor;
    sleep_state_t current_state;
    float sleep_pressure;
    bool consolidating;
    bool updates_allowed;
} pattern_db_sleep_effects_t;

typedef struct pattern_db_sleep_bridge_struct* pattern_db_sleep_bridge_t;

int pattern_db_sleep_default_config(pattern_db_sleep_config_t* config);
pattern_db_sleep_bridge_t pattern_db_sleep_bridge_create(
    const pattern_db_sleep_config_t* config,
    sleep_system_t sleep_system);
void pattern_db_sleep_bridge_destroy(pattern_db_sleep_bridge_t bridge);
int pattern_db_sleep_update(pattern_db_sleep_bridge_t bridge);
int pattern_db_sleep_get_effects(const pattern_db_sleep_bridge_t bridge,
                                  pattern_db_sleep_effects_t* effects);
float pattern_db_sleep_get_confidence_threshold(const pattern_db_sleep_bridge_t bridge, float base);
float pattern_db_sleep_get_priority_threshold(const pattern_db_sleep_bridge_t bridge, float base);
bool pattern_db_sleep_is_consolidating(const pattern_db_sleep_bridge_t bridge);
bool pattern_db_sleep_allow_updates(const pattern_db_sleep_bridge_t bridge);

float pattern_db_sleep_get_conf_factor(sleep_state_t state);
float pattern_db_sleep_get_prio_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PATTERN_DB_SLEEP_BRIDGE_H */
