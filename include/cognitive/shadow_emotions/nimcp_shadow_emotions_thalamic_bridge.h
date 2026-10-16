/**
 * @file nimcp_shadow_emotions_thalamic_bridge.h
 * @brief Bridge between Shadow Emotions system and thalamic router
 *
 * WHAT: Routes shadow emotion signals through thalamic attention pathways
 * WHY: Suppressed emotions require attention for integration/processing
 * HOW: Packages shadow emotion signals, routes via amygdala-thalamic pathways
 *
 * BIOLOGICAL BASIS:
 * - Repressed emotions involve amygdala-prefrontal dysregulation
 * - MD nucleus supports conscious integration of suppressed content
 * - Pulvinar routes shadow emotional salience
 * - Intralaminar nuclei support emotional arousal from shadow content
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SHADOW_EMOTIONS_THALAMIC_BRIDGE_H
#define NIMCP_SHADOW_EMOTIONS_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHADOW_EMO_SIGNAL_EMERGENCE    0x3401  /**< Shadow emergence */
#define SHADOW_EMO_SIGNAL_SUPPRESSION  0x3402  /**< Active suppression */
#define SHADOW_EMO_SIGNAL_INTEGRATION  0x3403  /**< Integration attempt */
#define SHADOW_EMO_SIGNAL_PROJECTION   0x3404  /**< Projection detected */

typedef struct {
    uint32_t signal_type;
    float shadow_urgency;
    float suppression_strength;
    float emergence_pressure;
    float integration_readiness;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} shadow_emotions_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_emergence_priority;
    float min_urgency_threshold;
    float emergence_boost;
} shadow_emotions_thalamic_config_t;

typedef struct {
    uint64_t emergences;
    uint64_t suppressions;
    uint64_t integrations;
    uint64_t projections;
    uint64_t signals_gated;
    float avg_suppression_strength;
    float avg_emergence_pressure;
} shadow_emotions_thalamic_stats_t;

typedef struct shadow_emotions_thalamic_bridge shadow_emotions_thalamic_bridge_t;

shadow_emotions_thalamic_config_t shadow_emotions_thalamic_default_config(void);
shadow_emotions_thalamic_bridge_t* shadow_emotions_thalamic_bridge_create(
    void* shadow_emotions, thalamic_router_t* router,
    const shadow_emotions_thalamic_config_t* config);
void shadow_emotions_thalamic_bridge_destroy(shadow_emotions_thalamic_bridge_t* bridge);
int shadow_emotions_thalamic_bridge_reset(shadow_emotions_thalamic_bridge_t* bridge);

int shadow_emotions_thalamic_route_signal(
    shadow_emotions_thalamic_bridge_t* bridge,
    const shadow_emotions_thalamic_signal_t* signal);
int shadow_emotions_thalamic_route_emergence(
    shadow_emotions_thalamic_bridge_t* bridge,
    float pressure, float urgency);

int shadow_emotions_thalamic_set_attention(shadow_emotions_thalamic_bridge_t* bridge, float attention);
int shadow_emotions_thalamic_get_attention(const shadow_emotions_thalamic_bridge_t* bridge, float* attention);
int shadow_emotions_thalamic_bridge_get_stats(
    const shadow_emotions_thalamic_bridge_t* bridge,
    shadow_emotions_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHADOW_EMOTIONS_THALAMIC_BRIDGE_H */
