/**
 * @file nimcp_curiosity_thalamic_bridge.h
 * @brief Bridge between Curiosity system and thalamic router
 *
 * WHAT: Routes curiosity signals through attention-gated thalamic pathways
 * WHY: Novelty seeking requires attention allocation via thalamic gating
 * HOW: Packages exploration signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Curiosity involves dopaminergic novelty detection
 * - Thalamus gates novel stimuli for conscious processing
 * - Exploration-exploitation tradeoff modulated by attention
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_CURIOSITY_THALAMIC_BRIDGE_H
#define NIMCP_CURIOSITY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CURIOSITY_SIGNAL_NOVELTY      0x0901
#define CURIOSITY_SIGNAL_EXPLORATION  0x0902
#define CURIOSITY_SIGNAL_INFORMATION  0x0903
#define CURIOSITY_SIGNAL_INTEREST     0x0904

typedef struct {
    uint32_t signal_type;
    float novelty_value;
    float information_gain;
    float exploration_drive;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} curiosity_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_novelty_boost;
    float min_novelty_threshold;
    float exploration_threshold;
} curiosity_thalamic_config_t;

typedef struct curiosity_thalamic_bridge curiosity_thalamic_bridge_t;

curiosity_thalamic_config_t curiosity_thalamic_default_config(void);
curiosity_thalamic_bridge_t* curiosity_thalamic_bridge_create(void* curiosity, thalamic_router_t* router, const curiosity_thalamic_config_t* config);
void curiosity_thalamic_bridge_destroy(curiosity_thalamic_bridge_t* bridge);
int curiosity_thalamic_bridge_reset(curiosity_thalamic_bridge_t* bridge);
int curiosity_thalamic_route_novelty(curiosity_thalamic_bridge_t* bridge, const curiosity_thalamic_signal_t* signal);
int curiosity_thalamic_route_exploration(curiosity_thalamic_bridge_t* bridge, const void* target, float drive);
int curiosity_thalamic_set_attention(curiosity_thalamic_bridge_t* bridge, float attention);
int curiosity_thalamic_get_attention(const curiosity_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t novelties_detected;
    uint64_t explorations_initiated;
    uint64_t information_gains;
    float avg_novelty_value;
} curiosity_thalamic_stats_t;

int curiosity_thalamic_bridge_get_stats(const curiosity_thalamic_bridge_t* bridge, curiosity_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_THALAMIC_BRIDGE_H */
