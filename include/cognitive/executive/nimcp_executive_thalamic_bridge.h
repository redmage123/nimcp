/**
 * @file nimcp_executive_thalamic_bridge.h
 * @brief Bridge between Executive system and thalamic router
 *
 * WHAT: Routes executive control signals through thalamic pathways
 * WHY: Executive function requires thalamic gating for conscious control
 * HOW: Packages executive signals, routes via mediodorsal nucleus
 *
 * BIOLOGICAL BASIS:
 * - Mediodorsal nucleus is key relay for PFC executive functions
 * - Ventral anterior nucleus supports motor planning
 * - Thalamic reticular nucleus gates inhibitory control
 * - Executive attention requires pulvinar coordination
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EXECUTIVE_THALAMIC_BRIDGE_H
#define NIMCP_EXECUTIVE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXECUTIVE_SIGNAL_INHIBITION     0x2D01  /**< Inhibitory control */
#define EXECUTIVE_SIGNAL_SWITCHING      0x2D02  /**< Task switching */
#define EXECUTIVE_SIGNAL_PLANNING       0x2D03  /**< Planning/sequencing */
#define EXECUTIVE_SIGNAL_MONITORING     0x2D04  /**< Performance monitoring */
#define EXECUTIVE_SIGNAL_DECISION       0x2D05  /**< Decision execution */

typedef struct {
    uint32_t signal_type;
    float control_urgency;
    float cognitive_load;
    float inhibition_strength;
    float switch_cost;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} executive_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_load_routing;
    bool enable_inhibition_priority;
    float min_urgency_threshold;
    float inhibition_boost;
    float switch_penalty;
} executive_thalamic_config_t;

typedef struct {
    uint64_t inhibitions_routed;
    uint64_t switches_executed;
    uint64_t plans_routed;
    uint64_t monitors_updated;
    uint64_t decisions_routed;
    uint64_t signals_gated;
    float avg_cognitive_load;
    float avg_switch_cost;
} executive_thalamic_stats_t;

typedef struct executive_thalamic_bridge executive_thalamic_bridge_t;

executive_thalamic_config_t executive_thalamic_default_config(void);
executive_thalamic_bridge_t* executive_thalamic_bridge_create(
    void* executive, thalamic_router_t* router,
    const executive_thalamic_config_t* config);
void executive_thalamic_bridge_destroy(executive_thalamic_bridge_t* bridge);
int executive_thalamic_bridge_reset(executive_thalamic_bridge_t* bridge);

int executive_thalamic_route_signal(
    executive_thalamic_bridge_t* bridge,
    const executive_thalamic_signal_t* signal);
int executive_thalamic_route_inhibition(
    executive_thalamic_bridge_t* bridge,
    float strength, float urgency);
int executive_thalamic_route_switch(
    executive_thalamic_bridge_t* bridge,
    float cost, float urgency);

int executive_thalamic_set_attention(executive_thalamic_bridge_t* bridge, float attention);
int executive_thalamic_get_attention(executive_thalamic_bridge_t* bridge, float* attention);
int executive_thalamic_bridge_get_stats(
    const executive_thalamic_bridge_t* bridge,
    executive_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_THALAMIC_BRIDGE_H */
