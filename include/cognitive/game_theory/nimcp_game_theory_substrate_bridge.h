/**
 * @file nimcp_game_theory_substrate_bridge.h
 * @brief Bridge between Game Theory system and neural substrate
 *
 * WHAT: Links game-theoretic computations to metabolic/energy state
 * WHY: Strategic reasoning and equilibrium computation require resources
 * HOW: Monitors ATP/fatigue; modulates strategy depth, opponent modeling, fairness
 *
 * BIOLOGICAL BASIS:
 * - Strategic reasoning involves prefrontal cortex with high metabolic demand
 * - ATP depletion leads to simpler heuristic-based decisions
 * - Fatigue impairs opponent modeling and theory of mind in games
 * - Metabolic stress reduces consideration of fairness and long-term outcomes
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_GAME_THEORY_SUBSTRATE_BRIDGE_H
#define NIMCP_GAME_THEORY_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_GAME_THEORY 0x1307

typedef struct {
    float strategy_depth;          /* Strategic planning depth [0-1] */
    float opponent_modeling;       /* Opponent modeling capacity [0-1] */
    float fairness_consideration;  /* Fairness evaluation capacity [0-1] */
    float equilibrium_search;      /* Equilibrium search capacity [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} game_theory_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} game_theory_substrate_config_t;

typedef struct game_theory_substrate_bridge game_theory_substrate_bridge_t;

game_theory_substrate_config_t game_theory_substrate_default_config(void);
game_theory_substrate_bridge_t* game_theory_substrate_bridge_create(void* game_theory, neural_substrate_t* substrate, const game_theory_substrate_config_t* config);
void game_theory_substrate_bridge_destroy(game_theory_substrate_bridge_t* bridge);
int game_theory_substrate_bridge_update(game_theory_substrate_bridge_t* bridge);
int game_theory_substrate_bridge_get_effects(const game_theory_substrate_bridge_t* bridge, game_theory_substrate_effects_t* effects);
int game_theory_substrate_bridge_apply_effects(game_theory_substrate_bridge_t* bridge);
int game_theory_substrate_bridge_register_bio_async(game_theory_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAME_THEORY_SUBSTRATE_BRIDGE_H */
