/**
 * @file nimcp_bias_thalamic_bridge.h
 * @brief Bridge between Cognitive Bias system and thalamic router
 *
 * WHAT: Routes bias detection signals through attention-gated thalamic pathways
 * WHY: Bias awareness requires conscious metacognitive access via thalamic gating
 * HOW: Packages bias signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Bias detection involves anterior cingulate metacognitive monitoring
 * - Thalamus gates bias awareness to prefrontal regions
 * - Conscious override of biases requires attention
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_BIAS_THALAMIC_BRIDGE_H
#define NIMCP_BIAS_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIAS_SIGNAL_DETECTION      0x1001
#define BIAS_SIGNAL_CORRECTION     0x1002
#define BIAS_SIGNAL_OVERRIDE       0x1003
#define BIAS_SIGNAL_AWARENESS      0x1004

typedef struct {
    uint32_t signal_type;
    float bias_strength;
    float detection_confidence;
    uint32_t bias_type;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} bias_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_automatic_correction;
    float min_detection_confidence;
    float correction_threshold;
} bias_thalamic_config_t;

typedef struct bias_thalamic_bridge bias_thalamic_bridge_t;

bias_thalamic_config_t bias_thalamic_default_config(void);
bias_thalamic_bridge_t* bias_thalamic_bridge_create(void* bias, thalamic_router_t* router, const bias_thalamic_config_t* config);
void bias_thalamic_bridge_destroy(bias_thalamic_bridge_t* bridge);
int bias_thalamic_bridge_reset(bias_thalamic_bridge_t* bridge);
int bias_thalamic_route_detection(bias_thalamic_bridge_t* bridge, const bias_thalamic_signal_t* signal);
int bias_thalamic_route_correction(bias_thalamic_bridge_t* bridge, const void* correction, float strength);
int bias_thalamic_set_attention(bias_thalamic_bridge_t* bridge, float attention);
int bias_thalamic_get_attention(const bias_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t biases_detected;
    uint64_t corrections_applied;
    uint64_t overrides_successful;
    float avg_detection_confidence;
} bias_thalamic_stats_t;

int bias_thalamic_bridge_get_stats(const bias_thalamic_bridge_t* bridge, bias_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIAS_THALAMIC_BRIDGE_H */
