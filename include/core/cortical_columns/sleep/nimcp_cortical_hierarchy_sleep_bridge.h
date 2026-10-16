/**
 * @file nimcp_cortical_hierarchy_sleep_bridge.h
 * @brief Sleep-Cortical Hierarchy Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and cortical hierarchy
 * WHY:  Sleep alters balance between feedforward and feedback processing across cortical areas
 * HOW:  Sleep state modulates FF/FB connectivity strength and hierarchical information flow
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Balanced FF/FB, bottom-up sensory processing dominates
 * - NREM: Enhanced feedback for memory consolidation, reduced feedforward
 * - REM: Altered hierarchy with internally-generated activity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_HIERARCHY_SLEEP_BRIDGE_H
#define NIMCP_CORTICAL_HIERARCHY_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "core/cortical_columns/nimcp_cortical_hierarchy.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FF/FB balance modulation */
#define HIERARCHY_SLEEP_FF_AWAKE       1.0f
#define HIERARCHY_SLEEP_FF_NREM        0.3f
#define HIERARCHY_SLEEP_FF_REM         0.7f

#define HIERARCHY_SLEEP_FB_AWAKE       1.0f
#define HIERARCHY_SLEEP_FB_NREM        1.5f  /* Enhanced for consolidation */
#define HIERARCHY_SLEEP_FB_REM         1.2f

typedef struct {
    bool enable_ff_modulation;
    bool enable_fb_modulation;
    float modulation_strength;
} cortical_hierarchy_sleep_config_t;

typedef struct {
    sleep_state_t current_state;
    float feedforward_strength;
    float feedback_strength;
    bool hierarchy_offline;
} cortical_hierarchy_sleep_effects_t;

typedef struct cortical_hierarchy_sleep_bridge_struct* cortical_hierarchy_sleep_bridge_t;

int cortical_hierarchy_sleep_default_config(cortical_hierarchy_sleep_config_t* config);
cortical_hierarchy_sleep_bridge_t cortical_hierarchy_sleep_bridge_create(
    const cortical_hierarchy_sleep_config_t* config,
    cortical_hierarchy_t* hierarchy,
    sleep_system_t sleep);
void cortical_hierarchy_sleep_bridge_destroy(cortical_hierarchy_sleep_bridge_t bridge);
int cortical_hierarchy_sleep_update(cortical_hierarchy_sleep_bridge_t bridge);
int cortical_hierarchy_sleep_apply_modulation(cortical_hierarchy_sleep_bridge_t bridge);
int cortical_hierarchy_sleep_get_effects(const cortical_hierarchy_sleep_bridge_t bridge,
                                         cortical_hierarchy_sleep_effects_t* effects);
float cortical_hierarchy_sleep_get_ff_strength(const cortical_hierarchy_sleep_bridge_t bridge);
float cortical_hierarchy_sleep_get_fb_strength(const cortical_hierarchy_sleep_bridge_t bridge);
bool cortical_hierarchy_sleep_is_offline(const cortical_hierarchy_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_HIERARCHY_SLEEP_BRIDGE_H */
