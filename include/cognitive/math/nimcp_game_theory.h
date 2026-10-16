/**
 * @file nimcp_game_theory.h
 * @brief Game theory engine for NIMCP cognitive mathematics
 *
 * Normal form games with payoff matrices, Nash equilibrium (support
 * enumeration, Lemke-Howson), dominant strategy elimination, minimax
 * for zero-sum, mixed strategy equilibria, evolutionary game theory
 * (replicator dynamics, ESS), cooperative games (Shapley value),
 * auction theory (first-price, Vickrey), preloaded games.
 */

#ifndef NIMCP_GAME_THEORY_H
#define NIMCP_GAME_THEORY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define GT_MAX_PLAYERS          8       /* max players in a game              */
#define GT_MAX_STRATEGIES       16      /* max strategies per player          */
#define GT_MAX_COALITION_PLAYERS 12     /* max players for Shapley value      */
#define GT_MAX_POPULATIONS      16      /* max populations for replicator     */
#define GT_REPLICATOR_DT        0.001   /* default time step                  */
#define GT_REPLICATOR_MAX_STEPS 100000  /* max replicator dynamics steps      */
#define GT_EPSILON              1e-10   /* numerical tolerance                */

/* --------------------------------------------------------------------------
 * Enums
 * -------------------------------------------------------------------------- */

typedef enum {
    GT_GAME_GENERIC = 0,
    GT_GAME_ZERO_SUM,
    GT_GAME_SYMMETRIC,
    GT_GAME_PRISONERS_DILEMMA,
    GT_GAME_HAWK_DOVE,
    GT_GAME_STAG_HUNT,
    GT_GAME_MATCHING_PENNIES,
    GT_GAME_BATTLE_OF_SEXES,
    GT_GAME_TYPE_COUNT
} gt_game_type_t;

typedef enum {
    GT_AUCTION_FIRST_PRICE = 0,
    GT_AUCTION_SECOND_PRICE,    /* Vickrey */
    GT_AUCTION_TYPE_COUNT
} gt_auction_type_t;

typedef enum {
    GT_NASH_SUPPORT_ENUM = 0,
    GT_NASH_LEMKE_HOWSON,
    GT_NASH_METHOD_COUNT
} gt_nash_method_t;

/* --------------------------------------------------------------------------
 * Structures
 * -------------------------------------------------------------------------- */

/** Strategy profile: one strategy index per player */
typedef struct {
    uint32_t strategy[GT_MAX_PLAYERS];
} gt_strategy_profile_t;

/** Mixed strategy: probability over each pure strategy */
typedef struct {
    double   prob[GT_MAX_STRATEGIES];
    uint32_t n_strategies;
} gt_mixed_strategy_t;

/** Nash equilibrium result */
typedef struct {
    gt_mixed_strategy_t strategies[GT_MAX_PLAYERS];
    double              payoffs[GT_MAX_PLAYERS];
    bool                is_pure;
    bool                found;
} gt_nash_result_t;

/** Normal form game */
typedef struct {
    gt_game_type_t type;
    uint32_t       n_players;
    uint32_t       n_strategies[GT_MAX_PLAYERS];

    /* payoff tensor — for 2 players: payoff[player][s1][s2]
     * For n>2 players, flattened: payoff[player][flat_index] */
    double         payoff[GT_MAX_PLAYERS][GT_MAX_STRATEGIES][GT_MAX_STRATEGIES];

    /* dominated strategy tracking */
    bool           dominated[GT_MAX_PLAYERS][GT_MAX_STRATEGIES];
} game_t;

/** Cooperative game: characteristic function v(S) for coalitions */
typedef struct {
    uint32_t n_players;
    /* v[mask] where mask is a bitmask of player membership */
    double   v[1 << GT_MAX_COALITION_PLAYERS];
} gt_cooperative_game_t;

/** Shapley value result */
typedef struct {
    double   phi[GT_MAX_COALITION_PLAYERS];
    uint32_t n_players;
    bool     valid;
} gt_shapley_result_t;

/** Evolutionary game state */
typedef struct {
    double   frequencies[GT_MAX_POPULATIONS];  /* population fractions x_i */
    double   fitness[GT_MAX_POPULATIONS];      /* per-strategy fitness     */
    double   avg_fitness;                      /* population mean fitness  */
    uint32_t n_strategies;
    double   dt;
    uint32_t step;
} gt_evo_state_t;

/** Auction result */
typedef struct {
    uint32_t winner;            /* index of winning bidder      */
    double   payment;           /* amount paid                  */
    double   bids[GT_MAX_PLAYERS];
    uint32_t n_bidders;
    double   revenue;           /* seller revenue               */
} gt_auction_result_t;

/** Top-level game theory engine */
typedef struct {
    game_t             *current_game;
    gt_evo_state_t     *evo_state;
} game_theory_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

game_theory_t *gamt_create(void);
void           gamt_destroy(game_theory_t *gt);

/* --------------------------------------------------------------------------
 * Game construction
 * -------------------------------------------------------------------------- */

game_t *gt_game_create(uint32_t n_players, const uint32_t *n_strategies);
void    gt_game_destroy(game_t *game);
void    gt_game_set_payoff(game_t *game, uint32_t player,
                           uint32_t s1, uint32_t s2, double payoff);
double  gt_game_get_payoff(const game_t *game, uint32_t player,
                           uint32_t s1, uint32_t s2);

/** Load a prebuilt game (prisoners dilemma, hawk-dove, stag hunt, etc.) */
game_t *gt_game_load_preset(gt_game_type_t type);

/* --------------------------------------------------------------------------
 * Nash equilibrium
 * -------------------------------------------------------------------------- */

/** Find Nash equilibrium via support enumeration (2-player) */
gt_nash_result_t gt_nash_support_enum(const game_t *game);

/** Find Nash equilibrium via Lemke-Howson (2-player) */
gt_nash_result_t gt_nash_lemke_howson(const game_t *game);

/** Check if a strategy profile is a Nash equilibrium */
bool gt_is_nash_equilibrium(const game_t *game,
                            const gt_mixed_strategy_t *strategies);

/* --------------------------------------------------------------------------
 * Dominance
 * -------------------------------------------------------------------------- */

/** Iterated elimination of strictly dominated strategies */
game_t *gt_eliminate_dominated(const game_t *game);

/** Check if strategy s1 strictly dominates s2 for player */
bool gt_strictly_dominates(const game_t *game, uint32_t player,
                           uint32_t s1, uint32_t s2);

/* --------------------------------------------------------------------------
 * Minimax (zero-sum games)
 * -------------------------------------------------------------------------- */

/** Compute minimax value and optimal strategies for 2-player zero-sum */
double gt_minimax_value(const game_t *game,
                        gt_mixed_strategy_t *p1_strategy,
                        gt_mixed_strategy_t *p2_strategy);

/* --------------------------------------------------------------------------
 * Evolutionary game theory
 * -------------------------------------------------------------------------- */

/** Initialize replicator dynamics */
gt_evo_state_t *gt_evo_create(uint32_t n_strategies, const double *initial_freq,
                               double dt);
void             gt_evo_destroy(gt_evo_state_t *evo);

/** Step replicator dynamics: dx_i/dt = x_i(f_i - f_bar) */
void gt_evo_replicator_step(gt_evo_state_t *evo, const game_t *game);

/** Run replicator dynamics to convergence */
void gt_evo_replicator_run(gt_evo_state_t *evo, const game_t *game,
                           uint32_t max_steps, double tol);

/** Check if strategy is evolutionarily stable (ESS) */
bool gt_evo_is_ess(const game_t *game, uint32_t strategy);

/* --------------------------------------------------------------------------
 * Cooperative games
 * -------------------------------------------------------------------------- */

gt_cooperative_game_t *gt_coop_create(uint32_t n_players);
void                   gt_coop_destroy(gt_cooperative_game_t *coop);
void                   gt_coop_set_value(gt_cooperative_game_t *coop,
                                         uint32_t coalition_mask, double value);

/** Compute Shapley value for all players */
gt_shapley_result_t gt_shapley_value(const gt_cooperative_game_t *coop);

/* --------------------------------------------------------------------------
 * Auctions
 * -------------------------------------------------------------------------- */

/** Run sealed-bid auction (first-price or Vickrey) */
gt_auction_result_t gt_auction_run(gt_auction_type_t type,
                                   const double *bids, uint32_t n_bidders);

/** Check revenue equivalence: expected revenue first-price == second-price
 *  for n bidders with uniform [0,1] valuations */
double gt_auction_expected_revenue(gt_auction_type_t type, uint32_t n_bidders);

/* --------------------------------------------------------------------------
 * Utility
 * -------------------------------------------------------------------------- */

/** Compute expected payoff under mixed strategies */
double gt_expected_payoff(const game_t *game, uint32_t player,
                          const gt_mixed_strategy_t *strategies);

/** Check if game is zero-sum */
bool gt_is_zero_sum(const game_t *game);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAME_THEORY_H */
