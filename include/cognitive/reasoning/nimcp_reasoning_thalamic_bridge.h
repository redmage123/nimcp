/**
 * @file nimcp_reasoning_thalamic_bridge.h
 * @brief Bridge between Reasoning system and thalamic router
 *
 * WHAT: Routes reasoning signals through thalamic attention pathways
 * WHY: Reasoning requires conscious attention for inference chains
 * HOW: Packages reasoning signals, routes via MD nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - MD nucleus is key relay for prefrontal reasoning
 * - Pulvinar coordinates attention during inference
 * - Anterior thalamus links premises to conclusions
 * - Intralaminar nuclei support sustained reasoning effort
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_REASONING_THALAMIC_BRIDGE_H
#define NIMCP_REASONING_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REASONING_SIGNAL_INFERENCE    0x3201  /**< Inference step */
#define REASONING_SIGNAL_DEDUCTION    0x3202  /**< Deductive reasoning */
#define REASONING_SIGNAL_INDUCTION    0x3203  /**< Inductive reasoning */
#define REASONING_SIGNAL_ANALOGY      0x3204  /**< Analogical reasoning */
#define REASONING_SIGNAL_CONCLUSION   0x3205  /**< Conclusion reached */

typedef struct {
    uint32_t signal_type;
    float reasoning_urgency;
    float inference_depth;
    float confidence;
    float complexity;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} reasoning_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_depth_routing;
    bool enable_confidence_boost;
    float min_urgency_threshold;
    float depth_boost;
} reasoning_thalamic_config_t;

typedef struct {
    uint64_t inferences_routed;
    uint64_t deductions;
    uint64_t inductions;
    uint64_t analogies;
    uint64_t conclusions;
    uint64_t signals_gated;
    float avg_inference_depth;
    float avg_confidence;
} reasoning_thalamic_stats_t;

typedef struct reasoning_thalamic_bridge reasoning_thalamic_bridge_t;

reasoning_thalamic_config_t reasoning_thalamic_default_config(void);
reasoning_thalamic_bridge_t* reasoning_thalamic_bridge_create(
    void* reasoning, thalamic_router_t* router,
    const reasoning_thalamic_config_t* config);
void reasoning_thalamic_bridge_destroy(reasoning_thalamic_bridge_t* bridge);
int reasoning_thalamic_bridge_reset(reasoning_thalamic_bridge_t* bridge);

int reasoning_thalamic_route_signal(
    reasoning_thalamic_bridge_t* bridge,
    const reasoning_thalamic_signal_t* signal);
int reasoning_thalamic_route_inference(
    reasoning_thalamic_bridge_t* bridge,
    float depth, float confidence);
int reasoning_thalamic_route_conclusion(
    reasoning_thalamic_bridge_t* bridge,
    float confidence, float urgency);

int reasoning_thalamic_set_attention(reasoning_thalamic_bridge_t* bridge, float attention);
int reasoning_thalamic_get_attention(reasoning_thalamic_bridge_t* bridge, float* attention);
int reasoning_thalamic_bridge_get_stats(
    const reasoning_thalamic_bridge_t* bridge,
    reasoning_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_THALAMIC_BRIDGE_H */
