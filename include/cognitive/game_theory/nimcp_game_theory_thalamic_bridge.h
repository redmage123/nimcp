/**
 * @file nimcp_game_theory_thalamic_bridge.h
 * @brief Bridge between Game Theory system and thalamic router
 *
 * WHAT: Routes game-theoretic decisions through attention-gated thalamic pathways
 * WHY: Strategic decisions require conscious deliberation via thalamic gating
 * HOW: Packages game theory signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Strategic decisions involve conscious deliberation via thalamo-cortical loops
 * - High-stakes game outcomes get enhanced thalamic routing priority
 * - Attention modulates which strategic options reach consciousness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_GAME_THEORY_THALAMIC_BRIDGE_H
#define NIMCP_GAME_THEORY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GT_SIGNAL_STRATEGY      0x0901
#define GT_SIGNAL_OUTCOME       0x0902
#define GT_SIGNAL_NEGOTIATION   0x0903
#define GT_SIGNAL_COALITION     0x0904

typedef struct {
    uint32_t signal_type;
    float stakes_level;
    float strategic_importance;
    float urgency;
    void* game_data;
    uint32_t data_size;
    uint64_t timestamp_us;
} game_theory_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_stakes_boost;
    float min_stakes_threshold;
    float coalition_boost;
} game_theory_thalamic_config_t;

typedef struct game_theory_thalamic_bridge game_theory_thalamic_bridge_t;

game_theory_thalamic_config_t game_theory_thalamic_default_config(void);
game_theory_thalamic_bridge_t* game_theory_thalamic_bridge_create(void* game_theory, thalamic_router_t* router, const game_theory_thalamic_config_t* config);
void game_theory_thalamic_bridge_destroy(game_theory_thalamic_bridge_t* bridge);
int game_theory_thalamic_bridge_reset(game_theory_thalamic_bridge_t* bridge);
int game_theory_thalamic_route_strategy(game_theory_thalamic_bridge_t* bridge, const game_theory_thalamic_signal_t* signal);
int game_theory_thalamic_route_outcome(game_theory_thalamic_bridge_t* bridge, const void* outcome, float payoff);
int game_theory_thalamic_set_attention(game_theory_thalamic_bridge_t* bridge, float attention);
int game_theory_thalamic_get_attention(const game_theory_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t strategies_routed;
    uint64_t outcomes_processed;
    uint64_t negotiations_initiated;
    float avg_stakes_level;
} game_theory_thalamic_stats_t;

int game_theory_thalamic_bridge_get_stats(const game_theory_thalamic_bridge_t* bridge, game_theory_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAME_THEORY_THALAMIC_BRIDGE_H */
