/**
 * @file nimcp_ethics_thalamic_bridge.h
 * @brief Bridge between Ethics system and thalamic router
 *
 * WHAT: Routes ethical judgments through attention-gated thalamic pathways
 * WHY: Moral decisions require conscious deliberation via thalamic gating
 * HOW: Packages moral signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Moral cognition involves conscious deliberation (medial PFC)
 * - Thalamus gates moral content to prefrontal regions
 * - Emotional moral signals get enhanced attention
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_ETHICS_THALAMIC_BRIDGE_H
#define NIMCP_ETHICS_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ETHICS_SIGNAL_JUDGMENT      0x0201
#define ETHICS_SIGNAL_VIOLATION     0x0202
#define ETHICS_SIGNAL_CONFLICT      0x0203
#define ETHICS_SIGNAL_RESOLUTION    0x0204

typedef struct {
    uint32_t signal_type;
    float moral_salience;
    float emotional_weight;
    float urgency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} ethics_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_emotional_boost;
    float min_moral_salience;
    float violation_boost;
} ethics_thalamic_config_t;

typedef struct ethics_thalamic_bridge ethics_thalamic_bridge_t;

ethics_thalamic_config_t ethics_thalamic_default_config(void);
ethics_thalamic_bridge_t* ethics_thalamic_bridge_create(void* ethics, thalamic_router_t* router, const ethics_thalamic_config_t* config);
void ethics_thalamic_bridge_destroy(ethics_thalamic_bridge_t* bridge);
int ethics_thalamic_bridge_reset(ethics_thalamic_bridge_t* bridge);
int ethics_thalamic_route_judgment(ethics_thalamic_bridge_t* bridge, const ethics_thalamic_signal_t* signal);
int ethics_thalamic_route_violation(ethics_thalamic_bridge_t* bridge, const void* violation, float severity);
int ethics_thalamic_set_attention(ethics_thalamic_bridge_t* bridge, float attention);
int ethics_thalamic_get_attention(const ethics_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t judgments_routed;
    uint64_t violations_flagged;
    uint64_t conflicts_detected;
    float avg_moral_salience;
} ethics_thalamic_stats_t;

int ethics_thalamic_bridge_get_stats(const ethics_thalamic_bridge_t* bridge, ethics_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ETHICS_THALAMIC_BRIDGE_H */
