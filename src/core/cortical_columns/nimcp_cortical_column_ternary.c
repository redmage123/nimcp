/**
 * @file nimcp_cortical_column_ternary.c
 * @brief Ternary Inter-Column Connectivity Implementation
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Ternary inter-column weights with winner-take-all ternary dynamics
 * WHY:  Memory-efficient column connectivity with discrete competition
 * HOW:  Ternary adjacency matrices with iterative WTA convergence
 *
 * @author NIMCP Development Team
 */

#include "core/cortical_columns/nimcp_cortical_column_ternary.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_column_ternary)

/*=============================================================================
 * Connectivity Lifecycle
 *===========================================================================*/

cc_ternary_connectivity_t* cc_ternary_connectivity_create(
    uint32_t n_source,
    uint32_t n_target,
    ternary_pack_mode_t pack_mode
) {
    /* Guard: validate dimensions */
    if (n_source == 0 || n_target == 0) {
        NIMCP_LOGGING_ERROR("Invalid dimensions: n_source=%u, n_target=%u",
                           n_source, n_target);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cc_ternary_connectivity_create: n_source is zero");
        return NULL;
    }

    /* Allocate structure */
    cc_ternary_connectivity_t* conn = nimcp_malloc(sizeof(cc_ternary_connectivity_t));
    if (!conn) {
        NIMCP_LOGGING_ERROR("Failed to allocate connectivity");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cc_ternary_connectivity_create: conn is NULL");
        return NULL;
    }
    memset(conn, 0, sizeof(cc_ternary_connectivity_t));

    conn->magic = CC_TERNARY_MAGIC;
    conn->n_source = n_source;
    conn->n_target = n_target;
    conn->excitatory_scale = 1.0f;
    conn->inhibitory_scale = 1.0f;

    /* Create ternary weight matrix */
    conn->weights = trit_matrix_create(n_source, n_target, pack_mode);
    if (!conn->weights) {
        nimcp_free(conn);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cc_ternary_connectivity_create: conn->weights is NULL");
        return NULL;
    }

    /* Initialize to all zeros (no connections) */
    trit_matrix_fill(conn->weights, TRIT_UNKNOWN);
    conn->n_absent = n_source * n_target;

    return conn;
}

cc_ternary_connectivity_t* cc_ternary_connectivity_create_lateral(
    uint32_t n_columns,
    int pattern,
    uint32_t neighborhood_size
) {
    cc_ternary_connectivity_t* conn = cc_ternary_connectivity_create(
        n_columns, n_columns, TERNARY_PACK_NONE
    );
    if (!conn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "conn is NULL");

        return NULL;

    }

    conn->n_absent = 0;

    for (uint32_t i = 0; i < n_columns; i++) {
        for (uint32_t j = 0; j < n_columns; j++) {
            trit_t weight;

            if (i == j) {
                /* Self-connection: excitatory */
                weight = TRIT_POSITIVE;
                conn->n_excitatory++;
            } else {
                switch (pattern) {
                    case 0:  /* FULL_INHIBITION */
                        weight = TRIT_NEGATIVE;
                        conn->n_inhibitory++;
                        break;

                    case 1:  /* NEIGHBOR_INHIBITION */
                        {
                            uint32_t dist = (i > j) ? (i - j) : (j - i);
                            /* Handle circular distance */
                            if (dist > n_columns / 2) {
                                dist = n_columns - dist;
                            }
                            if (dist <= neighborhood_size) {
                                weight = TRIT_NEGATIVE;
                                conn->n_inhibitory++;
                            } else {
                                weight = TRIT_UNKNOWN;
                                conn->n_absent++;
                            }
                        }
                        break;

                    case 2:  /* MEXICAN_HAT */
                        {
                            uint32_t dist = (i > j) ? (i - j) : (j - i);
                            if (dist > n_columns / 2) {
                                dist = n_columns - dist;
                            }
                            if (dist <= neighborhood_size / 2) {
                                /* Near neighbors: inhibitory */
                                weight = TRIT_NEGATIVE;
                                conn->n_inhibitory++;
                            } else if (dist <= neighborhood_size) {
                                /* Distant: no connection */
                                weight = TRIT_UNKNOWN;
                                conn->n_absent++;
                            } else {
                                weight = TRIT_UNKNOWN;
                                conn->n_absent++;
                            }
                        }
                        break;

                    default:
                        weight = TRIT_UNKNOWN;
                        conn->n_absent++;
                        break;
                }
            }

            trit_matrix_set(conn->weights, i, j, weight);
        }
    }

    /* Update E/I ratio */
    if (conn->n_inhibitory > 0) {
        conn->ei_ratio = (float)conn->n_excitatory / (float)conn->n_inhibitory;
    } else {
        conn->ei_ratio = (conn->n_excitatory > 0) ? INFINITY : 1.0f;
    }

    return conn;
}

void cc_ternary_connectivity_destroy(cc_ternary_connectivity_t* conn) {
    if (!conn) return;
    if (conn->magic != CC_TERNARY_MAGIC) return;

    if (conn->weights) {
        trit_matrix_destroy(conn->weights);
    }

    conn->magic = 0;
    nimcp_free(conn);
}

cc_ternary_connectivity_t* cc_ternary_connectivity_clone(
    const cc_ternary_connectivity_t* src
) {
    if (!src || src->magic != CC_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc_ternary_connectivity_clone: src is NULL");
        return NULL;
    }

    cc_ternary_connectivity_t* dst = nimcp_malloc(sizeof(cc_ternary_connectivity_t));
    if (!dst) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dst is NULL");

        return NULL;

    }

    memcpy(dst, src, sizeof(cc_ternary_connectivity_t));

    dst->weights = trit_matrix_clone(src->weights);
    if (!dst->weights) {
        nimcp_free(dst);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cc_ternary_connectivity_clone: dst->weights is NULL");
        return NULL;
    }

    return dst;
}

/*=============================================================================
 * Connectivity Access
 *===========================================================================*/

trit_t cc_ternary_connectivity_get(
    const cc_ternary_connectivity_t* conn,
    uint32_t source,
    uint32_t target
) {
    if (!conn || conn->magic != CC_TERNARY_MAGIC) return TRIT_UNKNOWN;
    if (source >= conn->n_source || target >= conn->n_target) return TRIT_UNKNOWN;

    return trit_matrix_get(conn->weights, source, target);
}

int cc_ternary_connectivity_set(
    cc_ternary_connectivity_t* conn,
    uint32_t source,
    uint32_t target,
    trit_t weight
) {
    if (!conn || conn->magic != CC_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cc_ternary_connectivity_set: conn is NULL");
        return -1;
    }
    if (source >= conn->n_source || target >= conn->n_target) return -2;
    if (!TRIT_IS_VALID(weight)) return -3;

    /* Update counts */
    trit_t old = trit_matrix_get(conn->weights, source, target);
    if (old == TRIT_POSITIVE) conn->n_excitatory--;
    else if (old == TRIT_NEGATIVE) conn->n_inhibitory--;
    else conn->n_absent--;

    if (weight == TRIT_POSITIVE) conn->n_excitatory++;
    else if (weight == TRIT_NEGATIVE) conn->n_inhibitory++;
    else conn->n_absent++;

    /* Update E/I ratio */
    if (conn->n_inhibitory > 0) {
        conn->ei_ratio = (float)conn->n_excitatory / (float)conn->n_inhibitory;
    } else {
        conn->ei_ratio = (conn->n_excitatory > 0) ? INFINITY : 1.0f;
    }

    return trit_matrix_set(conn->weights, source, target, weight);
}

int cc_ternary_connectivity_apply(
    const cc_ternary_connectivity_t* conn,
    const float* input,
    float* output
) {
    if (!conn || conn->magic != CC_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cc_ternary_connectivity_apply: conn is NULL");
        return -1;
    }
    if (!input || !output) return -2;

    for (uint32_t t = 0; t < conn->n_target; t++) {
        float sum = 0.0f;
        for (uint32_t s = 0; s < conn->n_source; s++) {
            trit_t w = trit_matrix_get(conn->weights, s, t);
            if (w == TRIT_POSITIVE) {
                sum += conn->excitatory_scale * input[s];
            } else if (w == TRIT_NEGATIVE) {
                sum -= conn->inhibitory_scale * input[s];
            }
            /* TRIT_UNKNOWN contributes nothing */
        }
        output[t] = sum;
    }

    return 0;
}

/*=============================================================================
 * Ternary Hypercolumn Lifecycle
 *===========================================================================*/

cc_ternary_hypercolumn_t* cc_ternary_hypercolumn_create(
    hypercolumn_t* base,
    const cc_ternary_wta_config_t* wta_config
) {
    if (!base) {
        NIMCP_LOGGING_ERROR("NULL base hypercolumn");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc_ternary_hypercolumn_create: base is NULL");
        return NULL;
    }

    /* Get number of minicolumns from base hypercolumn stats */
    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(base, &stats);
    uint32_t num_mini = stats.num_minicolumns;

    if (num_mini == 0) {
        NIMCP_LOGGING_ERROR("Base hypercolumn has 0 minicolumns");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cc_ternary_hypercolumn_create: num_mini is zero");
        return NULL;
    }

    /* Allocate structure */
    cc_ternary_hypercolumn_t* thcol = nimcp_malloc(sizeof(cc_ternary_hypercolumn_t));
    if (!thcol) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "thcol is NULL");

        return NULL;

    }
    memset(thcol, 0, sizeof(cc_ternary_hypercolumn_t));

    thcol->magic = CC_TERNARY_MAGIC;
    thcol->base = base;
    thcol->num_minicolumns = num_mini;

    /* Initialize WTA config */
    if (wta_config) {
        thcol->wta_config = *wta_config;
    } else {
        cc_ternary_wta_config_default(&thcol->wta_config);
    }

    /* Allocate ternary states */
    thcol->states = nimcp_malloc(num_mini * sizeof(cc_ternary_state_t));
    if (!thcol->states) {
        nimcp_free(thcol);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cc_ternary_hypercolumn_create: thcol->states is NULL");
        return NULL;
    }
    memset(thcol->states, 0, num_mini * sizeof(cc_ternary_state_t));

    /* Initialize states to neutral */
    for (uint32_t i = 0; i < num_mini; i++) {
        thcol->states[i].state = TRIT_UNKNOWN;
        thcol->states[i].raw_activation = 0.0f;
        thcol->states[i].confidence = 0.0f;
        thcol->states[i].win_count = 0;
    }

    /* Create lateral inhibition connectivity */
    thcol->lateral = cc_ternary_connectivity_create_lateral(
        num_mini, 0, 0  /* Full inhibition by default */
    );
    if (!thcol->lateral) {
        nimcp_free(thcol->states);
        nimcp_free(thcol);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc_ternary_hypercolumn_create: thcol->lateral is NULL");
        return NULL;
    }

    return thcol;
}

void cc_ternary_hypercolumn_destroy(cc_ternary_hypercolumn_t* thcol) {
    if (!thcol) return;
    if (thcol->magic != CC_TERNARY_MAGIC) return;

    if (thcol->states) nimcp_free(thcol->states);
    if (thcol->lateral) cc_ternary_connectivity_destroy(thcol->lateral);

    /* Note: base is not owned, don't destroy */

    thcol->magic = 0;
    nimcp_free(thcol);
}

/*=============================================================================
 * Ternary WTA Competition
 *===========================================================================*/

uint32_t cc_ternary_hypercolumn_wta(cc_ternary_hypercolumn_t* thcol) {
    if (!thcol || thcol->magic != CC_TERNARY_MAGIC) return UINT32_MAX;
    if (thcol->num_minicolumns == 0) return UINT32_MAX;

    /* Get current activations from base hypercolumn */
    float* activations = nimcp_malloc(thcol->num_minicolumns * sizeof(float));
    if (!activations) return UINT32_MAX;

    hypercolumn_get_distribution(thcol->base, activations, thcol->num_minicolumns);

    /* Update raw activations and convert to initial ternary states */
    for (uint32_t i = 0; i < thcol->num_minicolumns; i++) {
        thcol->states[i].raw_activation = activations[i];
        thcol->states[i].state = cc_activation_to_ternary(
            activations[i],
            thcol->wta_config.active_threshold,
            thcol->wta_config.suppress_threshold
        );
    }

    /* Allocate working arrays */
    float* new_activations = nimcp_malloc(thcol->num_minicolumns * sizeof(float));
    trit_t* new_states = nimcp_malloc(thcol->num_minicolumns * sizeof(trit_t));
    if (!new_activations || !new_states) {
        nimcp_free(activations);
        if (new_activations) nimcp_free(new_activations);
        if (new_states) nimcp_free(new_states);
        return UINT32_MAX;
    }

    /* Iterative WTA dynamics */
    uint32_t iter;
    bool converged = false;
    float inhibition = thcol->wta_config.inhibition_strength;

    for (iter = 0; iter < thcol->wta_config.max_iterations && !converged; iter++) {
        /* Apply lateral inhibition */
        for (uint32_t i = 0; i < thcol->num_minicolumns; i++) {
            float sum = activations[i];

            /* Apply ternary lateral weights */
            for (uint32_t j = 0; j < thcol->num_minicolumns; j++) {
                if (i == j) continue;

                trit_t w = cc_ternary_connectivity_get(thcol->lateral, j, i);
                if (w == TRIT_NEGATIVE) {
                    /* Inhibitory connection */
                    sum -= inhibition * activations[j];
                } else if (w == TRIT_POSITIVE) {
                    /* Excitatory connection (rare in lateral) */
                    sum += inhibition * activations[j];
                }
            }

            /* Clamp to [0, 1] */
            new_activations[i] = (sum < 0.0f) ? 0.0f : ((sum > 1.0f) ? 1.0f : sum);

            /* Convert to ternary */
            new_states[i] = cc_activation_to_ternary(
                new_activations[i],
                thcol->wta_config.active_threshold,
                thcol->wta_config.suppress_threshold
            );
        }

        /* Check for convergence */
        converged = true;
        for (uint32_t i = 0; i < thcol->num_minicolumns; i++) {
            if (new_states[i] != thcol->states[i].state) {
                converged = false;
                break;
            }
        }

        /* Update states */
        for (uint32_t i = 0; i < thcol->num_minicolumns; i++) {
            activations[i] = new_activations[i];
            thcol->states[i].state = new_states[i];
            thcol->states[i].raw_activation = new_activations[i];

            /* Compute confidence based on distance from thresholds */
            float dist_active = fabsf(new_activations[i] - thcol->wta_config.active_threshold);
            float dist_suppress = fabsf(new_activations[i] - thcol->wta_config.suppress_threshold);
            float min_dist = (dist_active < dist_suppress) ? dist_active : dist_suppress;
            thcol->states[i].confidence = 1.0f - 2.0f * min_dist;
            if (thcol->states[i].confidence < 0.0f) {
                thcol->states[i].confidence = 0.0f;
            }
        }
    }

    /* Update statistics */
    thcol->total_competitions++;
    if (!converged) {
        thcol->convergence_failures++;
    }
    thcol->avg_iterations = (thcol->avg_iterations * (thcol->total_competitions - 1) + iter) /
                            thcol->total_competitions;

    /* Find winner (highest final activation among +1 states) */
    uint32_t winner = UINT32_MAX;
    float max_activation = -1.0f;

    for (uint32_t i = 0; i < thcol->num_minicolumns; i++) {
        if (thcol->states[i].state == TRIT_POSITIVE &&
            thcol->states[i].raw_activation > max_activation) {
            max_activation = thcol->states[i].raw_activation;
            winner = i;
        }
    }

    /* Update win count */
    if (winner != UINT32_MAX) {
        thcol->states[winner].win_count++;
    }

    /* Cleanup */
    nimcp_free(activations);
    nimcp_free(new_activations);
    nimcp_free(new_states);

    return winner;
}

int cc_ternary_hypercolumn_update_states(cc_ternary_hypercolumn_t* thcol) {
    if (!thcol || thcol->magic != CC_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cc_ternary_hypercolumn_update_states: thcol is NULL");
        return -1;
    }

    /* Get activations from base hypercolumn */
    float* activations = nimcp_malloc(thcol->num_minicolumns * sizeof(float));
    if (!activations) return -2;

    hypercolumn_get_distribution(thcol->base, activations, thcol->num_minicolumns);

    /* Update states */
    for (uint32_t i = 0; i < thcol->num_minicolumns; i++) {
        thcol->states[i].raw_activation = activations[i];
        thcol->states[i].state = cc_activation_to_ternary(
            activations[i],
            thcol->wta_config.active_threshold,
            thcol->wta_config.suppress_threshold
        );
    }

    nimcp_free(activations);
    return 0;
}

trit_t cc_ternary_hypercolumn_get_state(
    const cc_ternary_hypercolumn_t* thcol,
    uint32_t minicolumn
) {
    if (!thcol || thcol->magic != CC_TERNARY_MAGIC) return TRIT_UNKNOWN;
    if (minicolumn >= thcol->num_minicolumns) return TRIT_UNKNOWN;

    return thcol->states[minicolumn].state;
}

int cc_ternary_hypercolumn_get_all_states(
    const cc_ternary_hypercolumn_t* thcol,
    trit_t* out_states
) {
    if (!thcol || thcol->magic != CC_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cc_ternary_hypercolumn_get_all_states: thcol is NULL");
        return -1;
    }
    if (!out_states) return -2;

    for (uint32_t i = 0; i < thcol->num_minicolumns; i++) {
        out_states[i] = thcol->states[i].state;
    }

    return 0;
}

void cc_ternary_hypercolumn_distribution(
    const cc_ternary_hypercolumn_t* thcol,
    uint32_t* n_active,
    uint32_t* n_neutral,
    uint32_t* n_suppressed
) {
    uint32_t active = 0, neutral = 0, suppressed = 0;

    if (thcol && thcol->magic == CC_TERNARY_MAGIC) {
        for (uint32_t i = 0; i < thcol->num_minicolumns; i++) {
            switch (thcol->states[i].state) {
                case TRIT_POSITIVE:  active++;     break;
                case TRIT_UNKNOWN:   neutral++;    break;
                case TRIT_NEGATIVE:  suppressed++; break;
                default: break;
            }
        }
    }

    if (n_active)     *n_active = active;
    if (n_neutral)    *n_neutral = neutral;
    if (n_suppressed) *n_suppressed = suppressed;
}

/*=============================================================================
 * Configuration Helpers
 *===========================================================================*/

void cc_ternary_wta_config_default(cc_ternary_wta_config_t* config) {
    if (!config) return;

    config->active_threshold = CC_TERNARY_ACTIVE_THRESHOLD;
    config->suppress_threshold = CC_TERNARY_SUPPRESS_THRESHOLD;
    config->use_soft_ternary = false;
    config->max_iterations = 100;
    config->convergence_epsilon = 1e-5f;
    config->inhibition_strength = 0.5f;
}

int cc_ternary_wta_config_validate(const cc_ternary_wta_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cc_ternary_wta_config_validate: config is NULL");
        return -1;
    }

    if (config->active_threshold <= config->suppress_threshold) {
        NIMCP_LOGGING_ERROR("active_threshold (%.3f) must be > suppress_threshold (%.3f)",
                           config->active_threshold, config->suppress_threshold);
        return -2;
    }

    if (config->active_threshold < 0.0f || config->active_threshold > 1.0f) {
        NIMCP_LOGGING_ERROR("active_threshold must be in [0, 1]");
        return -3;
    }

    if (config->suppress_threshold < 0.0f || config->suppress_threshold > 1.0f) {
        NIMCP_LOGGING_ERROR("suppress_threshold must be in [0, 1]");
        return -4;
    }

    if (config->max_iterations == 0) {
        NIMCP_LOGGING_ERROR("max_iterations must be > 0");
        return -5;
    }

    if (config->inhibition_strength < 0.0f || config->inhibition_strength > 1.0f) {
        NIMCP_LOGGING_ERROR("inhibition_strength must be in [0, 1]");
        return -6;
    }

    return 0;
}

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

trit_t cc_activation_to_ternary(
    float activation,
    float active_threshold,
    float suppress_threshold
) {
    if (activation >= active_threshold) {
        return TRIT_POSITIVE;
    } else if (activation <= suppress_threshold) {
        return TRIT_NEGATIVE;
    } else {
        return TRIT_UNKNOWN;
    }
}

float cc_ternary_to_activation(trit_t state) {
    switch (state) {
        case TRIT_POSITIVE: return 1.0f;
        case TRIT_NEGATIVE: return 0.0f;
        case TRIT_UNKNOWN:
        default:            return 0.5f;
    }
}
