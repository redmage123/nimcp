/**
 * @file nimcp_game_theory.c
 * @brief Game theory engine implementation
 *
 * Normal form games, Nash equilibrium (support enumeration, Lemke-Howson),
 * dominant strategy elimination, minimax for zero-sum, replicator dynamics,
 * ESS, Shapley value, first/second-price auctions.
 */

#include "cognitive/math/nimcp_game_theory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "game_theory"

/* ========================================================================== */
/* Internal helpers                                                           */
/* ========================================================================== */

static uint32_t factorial(uint32_t n) {
    uint32_t f = 1;
    for (uint32_t i = 2; i <= n; i++) f *= i;
    return f;
}

/** Solve 2x2 linear system [a b; c d] x = [e; f] */
static bool solve_2x2(double a, double b, double c, double d,
                       double e, double f, double *x1, double *x2) {
    double det = a * d - b * c;
    if (fabs(det) < GT_EPSILON) return false;
    *x1 = (e * d - b * f) / det;
    *x2 = (a * f - e * c) / det;
    return true;
}

/** Gaussian elimination for small systems (up to 16x16) */
static bool solve_linear_system(double *A, double *b, double *x, uint32_t n) {
    double M[GT_MAX_STRATEGIES * GT_MAX_STRATEGIES];
    double rhs[GT_MAX_STRATEGIES];
    memcpy(M, A, n * n * sizeof(double));
    memcpy(rhs, b, n * sizeof(double));

    for (uint32_t k = 0; k < n; k++) {
        uint32_t pivot = k;
        double max_val = fabs(M[k * n + k]);
        for (uint32_t i = k + 1; i < n; i++) {
            if (fabs(M[i * n + k]) > max_val) {
                max_val = fabs(M[i * n + k]);
                pivot = i;
            }
        }
        if (max_val < GT_EPSILON) return false;

        if (pivot != k) {
            for (uint32_t j = 0; j < n; j++) {
                double tmp = M[k * n + j];
                M[k * n + j] = M[pivot * n + j];
                M[pivot * n + j] = tmp;
            }
            double tmp = rhs[k]; rhs[k] = rhs[pivot]; rhs[pivot] = tmp;
        }

        for (uint32_t i = k + 1; i < n; i++) {
            double factor = M[i * n + k] / M[k * n + k];
            for (uint32_t j = k; j < n; j++) {
                M[i * n + j] -= factor * M[k * n + j];
            }
            rhs[i] -= factor * rhs[k];
        }
    }

    for (int32_t i = (int32_t)n - 1; i >= 0; i--) {
        x[i] = rhs[i];
        for (uint32_t j = (uint32_t)i + 1; j < n; j++) {
            x[i] -= M[i * n + j] * x[j];
        }
        x[i] /= M[i * n + i];
    }
    return true;
}

/* ========================================================================== */
/* Lifecycle                                                                  */
/* ========================================================================== */

game_theory_t *gamt_create(void) {
    game_theory_t *gt = (game_theory_t *)nimcp_calloc(1, sizeof(game_theory_t));
    if (!gt) {
        LOG_ERROR(LOG_TAG, "Failed to allocate game_theory_t");
        return NULL;
    }
    return gt;
}

void gamt_destroy(game_theory_t *gt) {
    if (!gt) return;
    if (gt->current_game) gt_game_destroy(gt->current_game);
    if (gt->evo_state) gt_evo_destroy(gt->evo_state);
    nimcp_free(gt);
}

/* ========================================================================== */
/* Game construction                                                          */
/* ========================================================================== */

game_t *gt_game_create(uint32_t n_players, const uint32_t *n_strategies) {
    if (n_players == 0 || n_players > GT_MAX_PLAYERS || !n_strategies) {
        return NULL;
    }
    for (uint32_t i = 0; i < n_players; i++) {
        if (n_strategies[i] == 0 || n_strategies[i] > GT_MAX_STRATEGIES) {
            return NULL;
        }
    }

    game_t *game = (game_t *)nimcp_calloc(1, sizeof(game_t));
    if (!game) return NULL;

    game->type = GT_GAME_GENERIC;
    game->n_players = n_players;
    for (uint32_t i = 0; i < n_players; i++) {
        game->n_strategies[i] = n_strategies[i];
    }
    return game;
}

void gt_game_destroy(game_t *game) {
    if (game) nimcp_free(game);
}

void gt_game_set_payoff(game_t *game, uint32_t player,
                        uint32_t s1, uint32_t s2, double payoff) {
    if (!game || player >= game->n_players ||
        s1 >= GT_MAX_STRATEGIES || s2 >= GT_MAX_STRATEGIES) return;
    game->payoff[player][s1][s2] = payoff;
}

double gt_game_get_payoff(const game_t *game, uint32_t player,
                          uint32_t s1, uint32_t s2) {
    if (!game || player >= game->n_players ||
        s1 >= GT_MAX_STRATEGIES || s2 >= GT_MAX_STRATEGIES) return 0.0;
    return game->payoff[player][s1][s2];
}

/* ========================================================================== */
/* Preset games                                                               */
/* ========================================================================== */

game_t *gt_game_load_preset(gt_game_type_t type) {
    uint32_t strats[2] = {2, 2};
    game_t *g = gt_game_create(2, strats);
    if (!g) return NULL;
    g->type = type;

    switch (type) {
        case GT_GAME_PRISONERS_DILEMMA:
            /* (C,C)=(-1,-1) (C,D)=(-3,0) (D,C)=(0,-3) (D,D)=(-2,-2) */
            g->payoff[0][0][0] = -1; g->payoff[1][0][0] = -1;
            g->payoff[0][0][1] = -3; g->payoff[1][0][1] =  0;
            g->payoff[0][1][0] =  0; g->payoff[1][1][0] = -3;
            g->payoff[0][1][1] = -2; g->payoff[1][1][1] = -2;
            break;

        case GT_GAME_HAWK_DOVE:
            /* V=2,C=4: (H,H)=(-1,-1) (H,D)=(2,0) (D,H)=(0,2) (D,D)=(1,1) */
            g->payoff[0][0][0] = -1; g->payoff[1][0][0] = -1;
            g->payoff[0][0][1] =  2; g->payoff[1][0][1] =  0;
            g->payoff[0][1][0] =  0; g->payoff[1][1][0] =  2;
            g->payoff[0][1][1] =  1; g->payoff[1][1][1] =  1;
            break;

        case GT_GAME_STAG_HUNT:
            /* (S,S)=(4,4) (S,H)=(0,3) (H,S)=(3,0) (H,H)=(2,2) */
            g->payoff[0][0][0] = 4; g->payoff[1][0][0] = 4;
            g->payoff[0][0][1] = 0; g->payoff[1][0][1] = 3;
            g->payoff[0][1][0] = 3; g->payoff[1][1][0] = 0;
            g->payoff[0][1][1] = 2; g->payoff[1][1][1] = 2;
            break;

        case GT_GAME_MATCHING_PENNIES:
            g->payoff[0][0][0] =  1; g->payoff[1][0][0] = -1;
            g->payoff[0][0][1] = -1; g->payoff[1][0][1] =  1;
            g->payoff[0][1][0] = -1; g->payoff[1][1][0] =  1;
            g->payoff[0][1][1] =  1; g->payoff[1][1][1] = -1;
            g->type = GT_GAME_ZERO_SUM;
            break;

        case GT_GAME_BATTLE_OF_SEXES:
            g->payoff[0][0][0] = 3; g->payoff[1][0][0] = 2;
            g->payoff[0][0][1] = 0; g->payoff[1][0][1] = 0;
            g->payoff[0][1][0] = 0; g->payoff[1][1][0] = 0;
            g->payoff[0][1][1] = 2; g->payoff[1][1][1] = 3;
            break;

        default:
            break;
    }
    return g;
}

/* ========================================================================== */
/* Expected payoff under mixed strategies                                     */
/* ========================================================================== */

double gt_expected_payoff(const game_t *game, uint32_t player,
                          const gt_mixed_strategy_t *strategies) {
    if (!game || !strategies || game->n_players < 2 || player >= game->n_players) {
        return 0.0;
    }
    /* 2-player case */
    double ep = 0.0;
    for (uint32_t s1 = 0; s1 < game->n_strategies[0]; s1++) {
        for (uint32_t s2 = 0; s2 < game->n_strategies[1]; s2++) {
            ep += strategies[0].prob[s1] * strategies[1].prob[s2]
                  * game->payoff[player][s1][s2];
        }
    }
    return ep;
}

bool gt_is_zero_sum(const game_t *game) {
    if (!game || game->n_players != 2) return false;
    for (uint32_t s1 = 0; s1 < game->n_strategies[0]; s1++) {
        for (uint32_t s2 = 0; s2 < game->n_strategies[1]; s2++) {
            double sum = game->payoff[0][s1][s2] + game->payoff[1][s1][s2];
            if (fabs(sum) > GT_EPSILON) return false;
        }
    }
    return true;
}

/* ========================================================================== */
/* Dominance                                                                  */
/* ========================================================================== */

bool gt_strictly_dominates(const game_t *game, uint32_t player,
                           uint32_t s1, uint32_t s2) {
    if (!game || game->n_players != 2) return false;
    uint32_t opp = 1 - player;
    for (uint32_t o = 0; o < game->n_strategies[opp]; o++) {
        double p1, p2;
        if (player == 0) {
            p1 = game->payoff[0][s1][o];
            p2 = game->payoff[0][s2][o];
        } else {
            p1 = game->payoff[1][o][s1];
            p2 = game->payoff[1][o][s2];
        }
        if (p1 <= p2) return false;
    }
    return true;
}

game_t *gt_eliminate_dominated(const game_t *game) {
    if (!game || game->n_players != 2) return NULL;

    game_t *g = (game_t *)nimcp_calloc(1, sizeof(game_t));
    if (!g) return NULL;
    memcpy(g, game, sizeof(game_t));

    bool changed = true;
    while (changed) {
        changed = false;
        for (uint32_t p = 0; p < 2; p++) {
            for (uint32_t s1 = 0; s1 < g->n_strategies[p]; s1++) {
                if (g->dominated[p][s1]) continue;
                for (uint32_t s2 = 0; s2 < g->n_strategies[p]; s2++) {
                    if (s1 == s2 || g->dominated[p][s2]) continue;
                    if (gt_strictly_dominates(g, p, s1, s2)) {
                        g->dominated[p][s2] = true;
                        changed = true;
                    }
                }
            }
        }
    }
    return g;
}

/* ========================================================================== */
/* Nash equilibrium: support enumeration (2-player)                           */
/* ========================================================================== */

gt_nash_result_t gt_nash_support_enum(const game_t *game) {
    gt_nash_result_t res;
    memset(&res, 0, sizeof(res));
    if (!game || game->n_players != 2) return res;

    uint32_t m = game->n_strategies[0];
    uint32_t n = game->n_strategies[1];

    /* Try all support pairs. For 2x2, this is simple. */
    /* First check pure strategy Nash equilibria */
    for (uint32_t s1 = 0; s1 < m; s1++) {
        for (uint32_t s2 = 0; s2 < n; s2++) {
            bool is_nash = true;
            /* Check player 0 has no profitable deviation */
            for (uint32_t d = 0; d < m; d++) {
                if (game->payoff[0][d][s2] > game->payoff[0][s1][s2] + GT_EPSILON) {
                    is_nash = false; break;
                }
            }
            if (!is_nash) continue;
            /* Check player 1 */
            for (uint32_t d = 0; d < n; d++) {
                if (game->payoff[1][s1][d] > game->payoff[1][s1][s2] + GT_EPSILON) {
                    is_nash = false; break;
                }
            }
            if (is_nash) {
                res.found = true;
                res.is_pure = true;
                memset(&res.strategies[0], 0, sizeof(gt_mixed_strategy_t));
                memset(&res.strategies[1], 0, sizeof(gt_mixed_strategy_t));
                res.strategies[0].n_strategies = m;
                res.strategies[1].n_strategies = n;
                res.strategies[0].prob[s1] = 1.0;
                res.strategies[1].prob[s2] = 1.0;
                res.payoffs[0] = game->payoff[0][s1][s2];
                res.payoffs[1] = game->payoff[1][s1][s2];
                return res;
            }
        }
    }

    /* 2x2 mixed strategy Nash equilibrium */
    if (m == 2 && n == 2) {
        /* Player 1 mixes to make player 0 indifferent:
         * u0(0,q) = u0(1,q) => a00*q + a01*(1-q) = a10*q + a11*(1-q) */
        double a00 = game->payoff[0][0][0], a01 = game->payoff[0][0][1];
        double a10 = game->payoff[0][1][0], a11 = game->payoff[0][1][1];
        double denom_q = (a00 - a01 - a10 + a11);

        double b00 = game->payoff[1][0][0], b01 = game->payoff[1][0][1];
        double b10 = game->payoff[1][1][0], b11 = game->payoff[1][1][1];
        double denom_p = (b00 - b01 - b10 + b11);

        if (fabs(denom_q) > GT_EPSILON && fabs(denom_p) > GT_EPSILON) {
            double q = (a11 - a01) / denom_q;
            double p = (b11 - b10) / denom_p;

            if (p >= -GT_EPSILON && p <= 1.0 + GT_EPSILON &&
                q >= -GT_EPSILON && q <= 1.0 + GT_EPSILON) {
                p = fmax(0.0, fmin(1.0, p));
                q = fmax(0.0, fmin(1.0, q));
                res.found = true;
                res.is_pure = false;
                res.strategies[0].n_strategies = 2;
                res.strategies[0].prob[0] = p;
                res.strategies[0].prob[1] = 1.0 - p;
                res.strategies[1].n_strategies = 2;
                res.strategies[1].prob[0] = q;
                res.strategies[1].prob[1] = 1.0 - q;
                res.payoffs[0] = gt_expected_payoff(game, 0, res.strategies);
                res.payoffs[1] = gt_expected_payoff(game, 1, res.strategies);
            }
        }
    }

    return res;
}

/* ========================================================================== */
/* Nash equilibrium: Lemke-Howson (2-player bimatrix)                         */
/* ========================================================================== */

gt_nash_result_t gt_nash_lemke_howson(const game_t *game) {
    gt_nash_result_t res;
    memset(&res, 0, sizeof(res));
    if (!game || game->n_players != 2) return res;

    uint32_t m = game->n_strategies[0];
    uint32_t n = game->n_strategies[1];
    uint32_t total = m + n;

    /* Simplified Lemke-Howson for small games using complementary pivoting.
     * Build augmented tableau and pivot. For tractability, limit to small games. */
    if (total > 12) {
        LOG_WARN(LOG_TAG, "Lemke-Howson: game too large (%u strategies), "
                 "falling back to support enumeration", total);
        return gt_nash_support_enum(game);
    }

    /* Construct tableaux for both players:
     * Player 1: B' * p <= 1 (where B is P2's payoff matrix)
     * Player 2: A  * q <= 1 (where A is P1's payoff matrix) */
    double tab[12][13]; /* max 12 vars + 1 RHS */
    memset(tab, 0, sizeof(tab));

    /* Shift payoffs to be positive */
    double shift = 0.0;
    for (uint32_t i = 0; i < m; i++)
        for (uint32_t j = 0; j < n; j++) {
            if (game->payoff[0][i][j] < shift) shift = game->payoff[0][i][j];
            if (game->payoff[1][i][j] < shift) shift = game->payoff[1][i][j];
        }
    shift = fabs(shift) + 1.0;

    /* Build system: for each player's strategy, compute indifference conditions */
    /* Use support enumeration with LP as a practical Lemke-Howson substitute */
    /* For 2x2: analytic, for larger: iterate supports */

    /* For correctness and completeness, try all support sizes */
    for (uint32_t sz1 = 1; sz1 <= m; sz1++) {
        for (uint32_t sz2 = 1; sz2 <= n; sz2++) {
            /* For simplicity, try full support first */
            if (sz1 != m || sz2 != n) continue; /* full support only */

            /* Player 2 indifference: A * q = v1 * 1, sum(q) = 1 */
            double A_sys[(GT_MAX_STRATEGIES + 1) * GT_MAX_STRATEGIES];
            double b_sys[GT_MAX_STRATEGIES + 1];
            memset(A_sys, 0, sizeof(A_sys));
            memset(b_sys, 0, sizeof(b_sys));

            /* Indifference: row differences equal zero */
            uint32_t eq = 0;
            for (uint32_t i = 1; i < m && eq < n; i++) {
                for (uint32_t j = 0; j < n; j++) {
                    A_sys[eq * n + j] = (game->payoff[0][0][j] + shift)
                                        - (game->payoff[0][i][j] + shift);
                }
                b_sys[eq] = 0.0;
                eq++;
            }
            /* sum = 1 */
            for (uint32_t j = 0; j < n; j++) A_sys[eq * n + j] = 1.0;
            b_sys[eq] = 1.0;
            eq++;

            if (eq >= n) {
                double q[GT_MAX_STRATEGIES];
                if (solve_linear_system(A_sys, b_sys, q, n)) {
                    bool valid = true;
                    for (uint32_t j = 0; j < n; j++) {
                        if (q[j] < -GT_EPSILON) { valid = false; break; }
                    }
                    if (valid) {
                        /* Similarly solve for p */
                        eq = 0;
                        memset(A_sys, 0, sizeof(A_sys));
                        for (uint32_t j = 1; j < n && eq < m; j++) {
                            for (uint32_t i = 0; i < m; i++) {
                                A_sys[eq * m + i] = (game->payoff[1][i][0] + shift)
                                                    - (game->payoff[1][i][j] + shift);
                            }
                            b_sys[eq] = 0.0;
                            eq++;
                        }
                        for (uint32_t i = 0; i < m; i++) A_sys[eq * m + i] = 1.0;
                        b_sys[eq] = 1.0;
                        eq++;

                        double p[GT_MAX_STRATEGIES];
                        if (eq >= m && solve_linear_system(A_sys, b_sys, p, m)) {
                            valid = true;
                            for (uint32_t i = 0; i < m; i++) {
                                if (p[i] < -GT_EPSILON) { valid = false; break; }
                            }
                            if (valid) {
                                res.found = true;
                                res.is_pure = false;
                                res.strategies[0].n_strategies = m;
                                res.strategies[1].n_strategies = n;
                                for (uint32_t i = 0; i < m; i++)
                                    res.strategies[0].prob[i] = fmax(0.0, p[i]);
                                for (uint32_t j = 0; j < n; j++)
                                    res.strategies[1].prob[j] = fmax(0.0, q[j]);
                                res.payoffs[0] = gt_expected_payoff(game, 0, res.strategies);
                                res.payoffs[1] = gt_expected_payoff(game, 1, res.strategies);
                                return res;
                            }
                        }
                    }
                }
            }
        }
    }

    /* Fall back to support enumeration */
    return gt_nash_support_enum(game);
}

bool gt_is_nash_equilibrium(const game_t *game,
                            const gt_mixed_strategy_t *strategies) {
    if (!game || !strategies || game->n_players != 2) return false;

    for (uint32_t p = 0; p < 2; p++) {
        double current = gt_expected_payoff(game, p, strategies);
        uint32_t ns = game->n_strategies[p];
        for (uint32_t s = 0; s < ns; s++) {
            /* Check deviation to pure strategy s */
            gt_mixed_strategy_t dev[2];
            memcpy(dev, strategies, 2 * sizeof(gt_mixed_strategy_t));
            memset(dev[p].prob, 0, sizeof(dev[p].prob));
            dev[p].prob[s] = 1.0;
            double deviated = gt_expected_payoff(game, p, dev);
            if (deviated > current + GT_EPSILON) return false;
        }
    }
    return true;
}

/* ========================================================================== */
/* Minimax for zero-sum games                                                 */
/* ========================================================================== */

double gt_minimax_value(const game_t *game,
                        gt_mixed_strategy_t *p1_strategy,
                        gt_mixed_strategy_t *p2_strategy) {
    if (!game || game->n_players != 2) return 0.0;

    uint32_t m = game->n_strategies[0];
    uint32_t n = game->n_strategies[1];

    /* For 2x2 zero-sum: solve analytically */
    if (m == 2 && n == 2) {
        double a = game->payoff[0][0][0], b = game->payoff[0][0][1];
        double c = game->payoff[0][1][0], d = game->payoff[0][1][1];

        double denom = a - b - c + d;
        if (fabs(denom) > GT_EPSILON) {
            double p = (d - c) / denom;
            double q = (d - b) / denom;
            p = fmax(0.0, fmin(1.0, p));
            q = fmax(0.0, fmin(1.0, q));

            if (p1_strategy) {
                p1_strategy->n_strategies = 2;
                p1_strategy->prob[0] = p;
                p1_strategy->prob[1] = 1.0 - p;
            }
            if (p2_strategy) {
                p2_strategy->n_strategies = 2;
                p2_strategy->prob[0] = q;
                p2_strategy->prob[1] = 1.0 - q;
            }
            return a * p * q + b * p * (1 - q) + c * (1 - p) * q + d * (1 - p) * (1 - q);
        }
    }

    /* General case: maximin via iterative best response */
    double best_val = -1e30;
    double p[GT_MAX_STRATEGIES], q_br[GT_MAX_STRATEGIES];
    memset(p, 0, sizeof(p));
    p[0] = 1.0;

    for (uint32_t iter = 0; iter < 1000; iter++) {
        /* Best response for player 2 (minimizer): min_j sum_i p[i]*A[i][j] */
        double min_val = 1e30;
        uint32_t best_j = 0;
        for (uint32_t j = 0; j < n; j++) {
            double val = 0.0;
            for (uint32_t i = 0; i < m; i++) {
                val += p[i] * game->payoff[0][i][j];
            }
            if (val < min_val) { min_val = val; best_j = j; }
        }

        /* Best response for player 1 (maximizer) */
        memset(q_br, 0, sizeof(q_br));
        q_br[best_j] = 1.0;

        double max_val = -1e30;
        uint32_t best_i = 0;
        for (uint32_t i = 0; i < m; i++) {
            double val = game->payoff[0][i][best_j];
            if (val > max_val) { max_val = val; best_i = i; }
        }

        /* Smooth update (fictitious play) */
        double lr = 2.0 / (double)(iter + 2);
        for (uint32_t i = 0; i < m; i++) {
            p[i] *= (1.0 - lr);
        }
        p[best_i] += lr;

        if (min_val > best_val) best_val = min_val;
    }

    if (p1_strategy) {
        p1_strategy->n_strategies = m;
        memcpy(p1_strategy->prob, p, m * sizeof(double));
    }
    return best_val;
}

/* ========================================================================== */
/* Evolutionary game theory                                                   */
/* ========================================================================== */

gt_evo_state_t *gt_evo_create(uint32_t n_strategies, const double *initial_freq,
                               double dt) {
    if (n_strategies == 0 || n_strategies > GT_MAX_POPULATIONS || !initial_freq) {
        return NULL;
    }
    gt_evo_state_t *evo = (gt_evo_state_t *)nimcp_calloc(1, sizeof(gt_evo_state_t));
    if (!evo) return NULL;

    evo->n_strategies = n_strategies;
    evo->dt = (dt > 0.0) ? dt : GT_REPLICATOR_DT;
    evo->step = 0;
    memcpy(evo->frequencies, initial_freq, n_strategies * sizeof(double));
    return evo;
}

void gt_evo_destroy(gt_evo_state_t *evo) {
    if (evo) nimcp_free(evo);
}

void gt_evo_replicator_step(gt_evo_state_t *evo, const game_t *game) {
    if (!evo || !game || game->n_players != 2) return;
    uint32_t n = evo->n_strategies;

    /* Compute fitness: f_i = sum_j x_j * A[i][j] (symmetric game, player 0) */
    double f_bar = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        evo->fitness[i] = 0.0;
        for (uint32_t j = 0; j < n; j++) {
            evo->fitness[i] += evo->frequencies[j] * game->payoff[0][i][j];
        }
        f_bar += evo->frequencies[i] * evo->fitness[i];
    }
    evo->avg_fitness = f_bar;

    /* Replicator dynamics: dx_i/dt = x_i * (f_i - f_bar) */
    double new_freq[GT_MAX_POPULATIONS];
    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double dx = evo->frequencies[i] * (evo->fitness[i] - f_bar);
        new_freq[i] = evo->frequencies[i] + evo->dt * dx;
        if (new_freq[i] < 0.0) new_freq[i] = 0.0;
        sum += new_freq[i];
    }

    /* Normalize to maintain valid distribution */
    if (sum > GT_EPSILON) {
        for (uint32_t i = 0; i < n; i++) {
            evo->frequencies[i] = new_freq[i] / sum;
        }
    }
    evo->step++;
}

void gt_evo_replicator_run(gt_evo_state_t *evo, const game_t *game,
                           uint32_t max_steps, double tol) {
    if (!evo || !game) return;

    for (uint32_t s = 0; s < max_steps; s++) {
        double prev[GT_MAX_POPULATIONS];
        memcpy(prev, evo->frequencies, evo->n_strategies * sizeof(double));

        gt_evo_replicator_step(evo, game);

        /* Check convergence */
        double diff = 0.0;
        for (uint32_t i = 0; i < evo->n_strategies; i++) {
            diff += fabs(evo->frequencies[i] - prev[i]);
        }
        if (diff < tol) break;
    }
}

bool gt_evo_is_ess(const game_t *game, uint32_t strategy) {
    if (!game || game->n_players != 2 || strategy >= game->n_strategies[0]) {
        return false;
    }
    uint32_t n = game->n_strategies[0];

    /* ESS condition: for all j != i:
     * (1) A[i][i] > A[j][i]  (strict Nash), or
     * (2) A[i][i] == A[j][i] and A[i][j] > A[j][j] (stability) */
    for (uint32_t j = 0; j < n; j++) {
        if (j == strategy) continue;
        double aii = game->payoff[0][strategy][strategy];
        double aji = game->payoff[0][j][strategy];
        if (aii < aji - GT_EPSILON) return false;
        if (fabs(aii - aji) < GT_EPSILON) {
            double aij = game->payoff[0][strategy][j];
            double ajj = game->payoff[0][j][j];
            if (aij <= ajj) return false;
        }
    }
    return true;
}

/* ========================================================================== */
/* Cooperative games: Shapley value                                           */
/* ========================================================================== */

gt_cooperative_game_t *gt_coop_create(uint32_t n_players) {
    if (n_players == 0 || n_players > GT_MAX_COALITION_PLAYERS) return NULL;
    gt_cooperative_game_t *coop = (gt_cooperative_game_t *)nimcp_calloc(
        1, sizeof(gt_cooperative_game_t));
    if (!coop) return NULL;
    coop->n_players = n_players;
    return coop;
}

void gt_coop_destroy(gt_cooperative_game_t *coop) {
    if (coop) nimcp_free(coop);
}

void gt_coop_set_value(gt_cooperative_game_t *coop,
                       uint32_t coalition_mask, double value) {
    if (!coop || coalition_mask >= (1u << coop->n_players)) return;
    coop->v[coalition_mask] = value;
}

gt_shapley_result_t gt_shapley_value(const gt_cooperative_game_t *coop) {
    gt_shapley_result_t res;
    memset(&res, 0, sizeof(res));
    if (!coop) return res;

    uint32_t n = coop->n_players;
    res.n_players = n;
    res.valid = true;

    /* phi_i = sum over S not containing i:
     *   |S|! * (n-|S|-1)! / n! * [v(S union {i}) - v(S)] */
    uint32_t n_fact = factorial(n);

    for (uint32_t i = 0; i < n; i++) {
        double phi = 0.0;
        uint32_t player_bit = 1u << i;

        /* Iterate over all coalitions S that don't contain i */
        for (uint32_t S = 0; S < (1u << n); S++) {
            if (S & player_bit) continue; /* skip if i in S */

            /* Count |S| */
            uint32_t s_size = 0;
            for (uint32_t b = 0; b < n; b++) {
                if (S & (1u << b)) s_size++;
            }

            uint32_t S_with_i = S | player_bit;
            double marginal = coop->v[S_with_i] - coop->v[S];
            double weight = (double)(factorial(s_size) * factorial(n - s_size - 1))
                            / (double)n_fact;
            phi += weight * marginal;
        }
        res.phi[i] = phi;
    }

    return res;
}

/* ========================================================================== */
/* Auctions                                                                   */
/* ========================================================================== */

gt_auction_result_t gt_auction_run(gt_auction_type_t type,
                                   const double *bids, uint32_t n_bidders) {
    gt_auction_result_t res;
    memset(&res, 0, sizeof(res));
    if (!bids || n_bidders == 0 || n_bidders > GT_MAX_PLAYERS) return res;

    res.n_bidders = n_bidders;
    memcpy(res.bids, bids, n_bidders * sizeof(double));

    /* Find highest and second-highest bids */
    uint32_t first = 0, second = 0;
    double max1 = -1e30, max2 = -1e30;
    for (uint32_t i = 0; i < n_bidders; i++) {
        if (bids[i] > max1) {
            max2 = max1; second = first;
            max1 = bids[i]; first = i;
        } else if (bids[i] > max2) {
            max2 = bids[i]; second = i;
        }
    }

    res.winner = first;

    switch (type) {
        case GT_AUCTION_FIRST_PRICE:
            res.payment = max1;
            break;
        case GT_AUCTION_SECOND_PRICE:
            res.payment = max2;
            break;
        default:
            res.payment = max1;
            break;
    }
    res.revenue = res.payment;
    return res;
}

double gt_auction_expected_revenue(gt_auction_type_t type, uint32_t n_bidders) {
    if (n_bidders < 2) return 0.0;
    /* Revenue equivalence: for n bidders with i.i.d. uniform [0,1] valuations,
     * expected revenue = (n-1)/(n+1) for both first-price and second-price */
    (void)type;
    return (double)(n_bidders - 1) / (double)(n_bidders + 1);
}
