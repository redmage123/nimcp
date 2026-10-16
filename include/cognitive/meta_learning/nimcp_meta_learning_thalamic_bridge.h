/**
 * @file nimcp_meta_learning_thalamic_bridge.h
 * @brief Bridge between Meta-Learning system and thalamic router
 *
 * WHAT: Routes meta-learning signals through attention-gated thalamic pathways
 * WHY: Learning strategy selection requires conscious processing via thalamic gating
 * HOW: Packages learning signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Meta-learning involves prefrontal-striatal circuits
 * - Thalamus coordinates attention during strategy selection
 * - Learning rate adaptation requires conscious oversight
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_META_LEARNING_THALAMIC_BRIDGE_H
#define NIMCP_META_LEARNING_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define META_LEARNING_SIGNAL_STRATEGY    0x0E01
#define META_LEARNING_SIGNAL_RATE        0x0E02
#define META_LEARNING_SIGNAL_TRANSFER    0x0E03
#define META_LEARNING_SIGNAL_EVALUATE    0x0E04

typedef struct {
    uint32_t signal_type;
    float strategy_confidence;
    float learning_rate;
    float transfer_potential;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} meta_learning_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_strategy_broadcast;
    float min_strategy_confidence;
    float transfer_threshold;
} meta_learning_thalamic_config_t;

typedef struct meta_learning_thalamic_bridge meta_learning_thalamic_bridge_t;

meta_learning_thalamic_config_t meta_learning_thalamic_default_config(void);
meta_learning_thalamic_bridge_t* meta_learning_thalamic_bridge_create(void* meta_learning, thalamic_router_t* router, const meta_learning_thalamic_config_t* config);
void meta_learning_thalamic_bridge_destroy(meta_learning_thalamic_bridge_t* bridge);
int meta_learning_thalamic_bridge_reset(meta_learning_thalamic_bridge_t* bridge);
int meta_learning_thalamic_route_strategy(meta_learning_thalamic_bridge_t* bridge, const meta_learning_thalamic_signal_t* signal);
int meta_learning_thalamic_route_transfer(meta_learning_thalamic_bridge_t* bridge, const void* knowledge, float potential);
int meta_learning_thalamic_set_attention(meta_learning_thalamic_bridge_t* bridge, float attention);
int meta_learning_thalamic_get_attention(const meta_learning_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t strategies_routed;
    uint64_t rate_adjustments;
    uint64_t transfers_initiated;
    float avg_learning_rate;
} meta_learning_thalamic_stats_t;

int meta_learning_thalamic_bridge_get_stats(const meta_learning_thalamic_bridge_t* bridge, meta_learning_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_META_LEARNING_THALAMIC_BRIDGE_H */
