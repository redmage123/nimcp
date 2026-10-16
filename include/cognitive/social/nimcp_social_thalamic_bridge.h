/**
 * @file nimcp_social_thalamic_bridge.h
 * @brief Bridge between Social Cognition system and thalamic router
 *
 * WHAT: Routes social signals through attention-gated thalamic pathways
 * WHY: Social interactions require conscious awareness via thalamic gating
 * HOW: Packages social signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Social processing involves thalamo-cortical loops for conscious awareness
 * - High-salience social signals (e.g., betrayal, bonding) get enhanced priority
 * - Attention modulates which social cues reach consciousness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SOCIAL_THALAMIC_BRIDGE_H
#define NIMCP_SOCIAL_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOCIAL_SIGNAL_BOND          0x0B01
#define SOCIAL_SIGNAL_TRUST         0x0B02
#define SOCIAL_SIGNAL_BETRAYAL      0x0B03
#define SOCIAL_SIGNAL_ALLIANCE      0x0B04

typedef struct {
    uint32_t signal_type;
    float social_salience;
    float emotional_weight;
    float urgency;
    void* social_data;
    uint32_t data_size;
    uint64_t timestamp_us;
} social_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_salience_boost;
    float min_salience_threshold;
    float betrayal_boost;
} social_thalamic_config_t;

typedef struct social_thalamic_bridge social_thalamic_bridge_t;

social_thalamic_config_t social_thalamic_default_config(void);
social_thalamic_bridge_t* social_thalamic_bridge_create(void* social, thalamic_router_t* router, const social_thalamic_config_t* config);
void social_thalamic_bridge_destroy(social_thalamic_bridge_t* bridge);
int social_thalamic_bridge_reset(social_thalamic_bridge_t* bridge);
int social_thalamic_route_bond(social_thalamic_bridge_t* bridge, const social_thalamic_signal_t* signal);
int social_thalamic_route_trust(social_thalamic_bridge_t* bridge, const void* trust_event, float significance);
int social_thalamic_set_attention(social_thalamic_bridge_t* bridge, float attention);
int social_thalamic_get_attention(const social_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t bonds_routed;
    uint64_t trust_events;
    uint64_t betrayals_detected;
    float avg_social_salience;
} social_thalamic_stats_t;

int social_thalamic_bridge_get_stats(const social_thalamic_bridge_t* bridge, social_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOCIAL_THALAMIC_BRIDGE_H */
