/**
 * @file nimcp_insula_thalamic_bridge.h
 * @brief Bridge between Insula and thalamic router
 *
 * WHAT: Routes interoceptive signals through thalamic relay
 * WHY: Interoceptive information relayed via VMpo and other nuclei
 * HOW: Packages bodily signals, routes via appropriate thalamic pathway
 *
 * BIOLOGICAL BASIS:
 * - VMpo (posterior ventromedial) relays interoception
 * - Insula receives bodily state information via thalamus
 * - Emotional awareness involves insular-thalamic circuits
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_INSULA_THALAMIC_BRIDGE_H
#define NIMCP_INSULA_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INSULA_SIGNAL_INTEROCEPTION  0x2801
#define INSULA_SIGNAL_DISGUST        0x2802
#define INSULA_SIGNAL_EMPATHY        0x2803
#define INSULA_SIGNAL_AWARENESS      0x2804

typedef struct {
    uint32_t signal_type;
    float interoceptive_strength;
    float emotional_salience;
    float bodily_urgency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} insula_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_disgust_priority;
    float min_interoceptive_threshold;
    float urgency_threshold;
} insula_thalamic_config_t;

typedef struct insula_thalamic_bridge insula_thalamic_bridge_t;

insula_thalamic_config_t insula_thalamic_default_config(void);
insula_thalamic_bridge_t* insula_thalamic_bridge_create(void* insula, thalamic_router_t* router, const insula_thalamic_config_t* config);
void insula_thalamic_bridge_destroy(insula_thalamic_bridge_t* bridge);
int insula_thalamic_bridge_reset(insula_thalamic_bridge_t* bridge);
int insula_thalamic_route_signal(insula_thalamic_bridge_t* bridge, const insula_thalamic_signal_t* signal);
int insula_thalamic_route_interoception(insula_thalamic_bridge_t* bridge, const void* state, float strength);
int insula_thalamic_set_attention(insula_thalamic_bridge_t* bridge, float attention);
int insula_thalamic_get_attention(const insula_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t interoceptions_routed;
    uint64_t disgust_responses;
    uint64_t empathic_signals;
    float avg_interoceptive_strength;
} insula_thalamic_stats_t;

int insula_thalamic_bridge_get_stats(const insula_thalamic_bridge_t* bridge, insula_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INSULA_THALAMIC_BRIDGE_H */
