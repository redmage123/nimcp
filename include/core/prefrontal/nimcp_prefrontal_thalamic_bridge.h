/**
 * @file nimcp_prefrontal_thalamic_bridge.h
 * @brief Bridge between Prefrontal Cortex and thalamic router
 *
 * WHAT: Routes executive signals through thalamic relay (MD)
 * WHY: Mediodorsal thalamus is primary prefrontal thalamic relay
 * HOW: Packages executive signals, routes via MD nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - MD (mediodorsal) nucleus connects with prefrontal cortex
 * - Higher-order thalamic relay for executive function
 * - Working memory and attention involve MD
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PREFRONTAL_THALAMIC_BRIDGE_H
#define NIMCP_PREFRONTAL_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PREFRONTAL_SIGNAL_EXECUTIVE   0x2601
#define PREFRONTAL_SIGNAL_WORKING_MEM 0x2602
#define PREFRONTAL_SIGNAL_INHIBITION  0x2603
#define PREFRONTAL_SIGNAL_PLANNING    0x2604

typedef struct {
    uint32_t signal_type;
    float executive_load;
    float working_memory_demand;
    float inhibition_strength;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} prefrontal_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_working_memory_boost;
    float min_executive_load;
    float inhibition_threshold;
} prefrontal_thalamic_config_t;

typedef struct prefrontal_thalamic_bridge prefrontal_thalamic_bridge_t;

prefrontal_thalamic_config_t prefrontal_thalamic_default_config(void);
prefrontal_thalamic_bridge_t* prefrontal_thalamic_bridge_create(void* prefrontal, thalamic_router_t* router, const prefrontal_thalamic_config_t* config);
void prefrontal_thalamic_bridge_destroy(prefrontal_thalamic_bridge_t* bridge);
int prefrontal_thalamic_bridge_reset(prefrontal_thalamic_bridge_t* bridge);
int prefrontal_thalamic_route_signal(prefrontal_thalamic_bridge_t* bridge, const prefrontal_thalamic_signal_t* signal);
int prefrontal_thalamic_route_inhibition(prefrontal_thalamic_bridge_t* bridge, const void* target, float strength);
int prefrontal_thalamic_set_attention(prefrontal_thalamic_bridge_t* bridge, float attention);
int prefrontal_thalamic_get_attention(const prefrontal_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t executive_signals_routed;
    uint64_t working_memory_updates;
    uint64_t inhibitions_applied;
    float avg_executive_load;
} prefrontal_thalamic_stats_t;

int prefrontal_thalamic_bridge_get_stats(const prefrontal_thalamic_bridge_t* bridge, prefrontal_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREFRONTAL_THALAMIC_BRIDGE_H */
