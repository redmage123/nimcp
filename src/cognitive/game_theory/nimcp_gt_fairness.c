//=============================================================================
// nimcp_gt_fairness.c - Enhanced Fairness Metrics Implementation
//=============================================================================
/**
 * @file nimcp_gt_fairness.c
 * @brief Implementation of fairness metrics for game-theoretic resource allocation
 */

#include "cognitive/game_theory/nimcp_gt_fairness.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "fairness"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(gt_fairness, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Static Name Tables
//=============================================================================

static const char* s_fairness_measure_names[] = {
    "Jain's Index",
    "Gini Coefficient",
    "Theil Index",
    "Atkinson Index",
    "Coefficient of Variation"
};

static const char* s_allocation_property_names[] = {
    "Envy-Free",
    "EF1 (Envy-Free up to One)",
    "EFX (Envy-Free up to Any)",
    "Proportional",
    "Maximin Share Guarantee",
    "Pareto Optimal"
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute mean of values (wrapper for central statistics module)
 */
static float compute_mean(const float* values, uint32_t n) {
    if (!values || n == 0) return 0.0f;
    return nimcp_stats_mean(values, n);
}

/**
 * @brief Compute bundle value for a player given assignment
 */
static float compute_bundle_value(const float* player_valuations,
                                   const uint32_t* assignment,
                                   uint32_t player,
                                   uint32_t num_items) {
    float value = 0.0f;
    for (uint32_t item = 0; item < num_items; item++) {
        /* Phase 8: Loop progress heartbeat */
        if ((item & 0xFF) == 0 && num_items > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(item + 1) / (float)num_items);
        }

        if (assignment[item] == player) {
            value += player_valuations[item];
        }
    }
    return value;
}

/**
 * @brief Compute total value of all items for a player
 */
static float compute_total_value(const float* player_valuations, uint32_t num_items) {
    float total = 0.0f;
    for (uint32_t item = 0; item < num_items; item++) {
        /* Phase 8: Loop progress heartbeat */
        if ((item & 0xFF) == 0 && num_items > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(item + 1) / (float)num_items);
        }

        total += player_valuations[item];
    }
    return total;
}

/**
 * @brief Recursive helper for MMS partition enumeration
 */
static void mms_partition_helper(const float* valuations,
                                  uint32_t num_items,
                                  uint32_t num_players,
                                  uint32_t* current_assignment,
                                  uint32_t item_index,
                                  float* min_bundle_value,
                                  float* best_min) {
    if (item_index == num_items) {
        // Compute minimum bundle value for this partition
        float bundle_values[NIMCP_GT_MAX_PLAYERS] = {0};
        for (uint32_t i = 0; i < num_items; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_items > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(i + 1) / (float)num_items);
            }

            bundle_values[current_assignment[i]] += valuations[i];
        }

        float min_val = FLT_MAX;
        for (uint32_t p = 0; p < num_players; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && num_players > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(p + 1) / (float)num_players);
            }

            if (bundle_values[p] < min_val) {
                min_val = bundle_values[p];
            }
        }

        // Maximize the minimum
        if (min_val > *best_min) {
            *best_min = min_val;
        }
        return;
    }

    // Try assigning current item to each player
    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        current_assignment[item_index] = p;
        mms_partition_helper(valuations, num_items, num_players,
                            current_assignment, item_index + 1,
                            min_bundle_value, best_min);
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

nimcp_fairness_config_t nimcp_fairness_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_default_con", 0.0f);


    nimcp_fairness_config_t config = {
        .atkinson_epsilon = NIMCP_FAIRNESS_DEFAULT_ATKINSON_EPSILON,
        .compute_all_measures = true,
        .track_envy_pairs = true,
        .max_mms_items = NIMCP_FAIRNESS_MAX_MMS_ITEMS,
        .tolerance = 1e-6f
    };
    return config;
}

//=============================================================================
// Basic Fairness Index Functions
//=============================================================================

float nimcp_fairness_jain_index(const float* values, uint32_t num_values) {
    // Guard: validate inputs
    if (!values || num_values == 0) {
        return -1.0f;
    }

    // Compute Jain's fairness index: J = (sum x_i)^2 / (n * sum x_i^2)
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_jain_index", 0.0f);


    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < num_values; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_values > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_values);
        }

        sum += values[i];
        sum_sq += values[i] * values[i];
    }

    // Handle edge case: all zeros
    if (sum_sq < 1e-10f) {
        return 1.0f;  // All zero is perfectly fair
    }

    return (sum * sum) / ((float)num_values * sum_sq);
}

float nimcp_fairness_gini_coefficient(const float* values, uint32_t num_values) {
    // Guard: validate inputs
    if (!values || num_values == 0) {
        return -1.0f;
    }

    // Single value is perfectly equal
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_gini_coeffi", 0.0f);


    if (num_values == 1) {
        return 0.0f;
    }

    // Compute mean
    float mean = compute_mean(values, num_values);
    if (mean < 1e-10f) {
        return 0.0f;  // All zeros is perfectly equal
    }

    // Gini = sum|x_i - x_j| / (2 * n * sum x_i)
    // Simplified: G = sum|x_i - x_j| / (2 * n^2 * mean)
    float abs_diff_sum = 0.0f;
    for (uint32_t i = 0; i < num_values; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_values > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_values);
        }

        for (uint32_t j = 0; j < num_values; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_values > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(j + 1) / (float)num_values);
            }

            abs_diff_sum += fabsf(values[i] - values[j]);
        }
    }

    float n = (float)num_values;
    return abs_diff_sum / (2.0f * n * n * mean);
}

float nimcp_fairness_theil_index(const float* values, uint32_t num_values) {
    // Guard: validate inputs
    if (!values || num_values == 0) {
        return -1.0f;
    }

    // Compute mean
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_theil_index", 0.0f);


    float mean = compute_mean(values, num_values);
    if (mean < 1e-10f) {
        return 0.0f;  // All zeros
    }

    // Theil = (1/n) * sum (x_i/mu) * ln(x_i/mu)
    float theil = 0.0f;
    for (uint32_t i = 0; i < num_values; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_values > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_values);
        }

        if (values[i] > 1e-10f) {
            float ratio = values[i] / (fabsf(mean) > 1e-7f ? mean : 1e-7f);
            theil += ratio * logf(ratio);
        }
        // Zero values contribute 0 to Theil (limit of x*ln(x) as x->0)
    }

    return theil / (float)num_values;
}

float nimcp_fairness_atkinson_index(const float* values, uint32_t num_values,
                                     float epsilon) {
    // Guard: validate inputs
    if (!values || num_values == 0) {
        return -1.0f;
    }

    // Guard: epsilon must be in [0, 1)
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_atkinson_in", 0.0f);


    if (epsilon < 0.0f || epsilon >= 1.0f) {
        return -1.0f;
    }

    // Compute mean
    float mean = compute_mean(values, num_values);
    if (mean < 1e-10f) {
        return 0.0f;  // All zeros is perfectly equal
    }

    // Check for zero values (problematic with epsilon > 0)
    for (uint32_t i = 0; i < num_values; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_values > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_values);
        }

        if (values[i] < 1e-10f && epsilon > 0.0f) {
            // With epsilon > 0 and zero income, Atkinson = 1
            return 1.0f;
        }
    }

    // Atkinson = 1 - (1/mu) * (mean of x_i^(1-e))^(1/(1-e))
    float exponent = 1.0f - epsilon;
    float sum_powered = 0.0f;

    for (uint32_t i = 0; i < num_values; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_values > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_values);
        }

        sum_powered += powf(values[i], exponent);
    }

    float mean_powered = sum_powered / (float)num_values;
    float ede = powf(mean_powered, 1.0f / exponent);  // Equally Distributed Equivalent

    return 1.0f - (ede / (fabsf(mean) > 1e-7f ? mean : 1e-7f));
}

float nimcp_fairness_coefficient_variation(const float* values, uint32_t num_values) {
    // Guard: validate inputs
    if (!values || num_values == 0) {
        return -1.0f;
    }

    // Compute mean
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_coefficient", 0.0f);


    float mean = compute_mean(values, num_values);
    if (mean < 1e-10f) {
        return 0.0f;  // All zeros
    }

    // Compute variance
    float variance = 0.0f;
    for (uint32_t i = 0; i < num_values; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_values > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_values);
        }

        float diff = values[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)num_values;

    // CV = stddev / mean
    return sqrtf(variance) / (fabsf(mean) > 1e-7f ? mean : 1e-7f);
}

//=============================================================================
// Allocation Property Verification
//=============================================================================

bool nimcp_fairness_is_envy_free(const float* const* valuations,
                                  const uint32_t* assignment,
                                  uint32_t num_players,
                                  uint32_t num_items) {
    // Guard: validate inputs
    if (!valuations || !assignment || num_players == 0 || num_items == 0) {
        return false;
    }

    // Compute bundle values for each player
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_is_envy_fre", 0.0f);


    float bundle_values[NIMCP_GT_MAX_PLAYERS];
    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        bundle_values[p] = compute_bundle_value(valuations[p], assignment, p, num_items);
    }

    // Check if any player envies another
    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_players);
        }

        float my_value = bundle_values[i];

        for (uint32_t j = 0; j < num_players; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_players > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(j + 1) / (float)num_players);
            }

            if (i == j) continue;

            // Compute value of j's bundle from i's perspective
            float others_value = compute_bundle_value(valuations[i], assignment, j, num_items);

            if (others_value > my_value + 1e-6f) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_fairness_coefficient_variation: validation failed");
                return false;  // i envies j
            }
        }
    }

    return true;
}

bool nimcp_fairness_is_ef1(const float* const* valuations,
                            const uint32_t* assignment,
                            uint32_t num_players,
                            uint32_t num_items) {
    // Guard: validate inputs
    if (!valuations || !assignment || num_players == 0 || num_items == 0) {
        return false;
    }

    // For each pair (i, j), check if envy can be eliminated by removing one item from j
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_is_ef1", 0.0f);


    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_players);
        }

        float my_value = compute_bundle_value(valuations[i], assignment, i, num_items);

        for (uint32_t j = 0; j < num_players; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_players > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(j + 1) / (float)num_players);
            }

            if (i == j) continue;

            float others_value = compute_bundle_value(valuations[i], assignment, j, num_items);

            // If i doesn't envy j, no need to check further
            if (others_value <= my_value + 1e-6f) {
                continue;
            }

            // i envies j - check if removing any single item eliminates envy
            bool can_eliminate_envy = false;

            for (uint32_t item = 0; item < num_items; item++) {
                /* Phase 8: Loop progress heartbeat */
                if ((item & 0xFF) == 0 && num_items > 256) {
                    gt_fairness_heartbeat("gt_fairness_loop",
                                     (float)(item + 1) / (float)num_items);
                }

                if (assignment[item] != j) continue;

                // Try removing this item from j's bundle
                float reduced_value = others_value - valuations[i][item];
                if (reduced_value <= my_value + 1e-6f) {
                    can_eliminate_envy = true;
                    break;
                }
            }

            if (!can_eliminate_envy) {
                return false;  // i envies j even after removing any item — normal fairness result
            }
        }
    }

    return true;
}

bool nimcp_fairness_is_efx(const float* const* valuations,
                            const uint32_t* assignment,
                            uint32_t num_players,
                            uint32_t num_items) {
    // Guard: validate inputs
    if (!valuations || !assignment || num_players == 0 || num_items == 0) {
        return false;
    }

    // For each pair (i, j), check if removing ANY positively-valued item eliminates envy
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_is_efx", 0.0f);


    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_players);
        }

        float my_value = compute_bundle_value(valuations[i], assignment, i, num_items);

        for (uint32_t j = 0; j < num_players; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_players > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(j + 1) / (float)num_players);
            }

            if (i == j) continue;

            float others_value = compute_bundle_value(valuations[i], assignment, j, num_items);

            // If i doesn't envy j, no need to check further
            if (others_value <= my_value + 1e-6f) {
                continue;
            }

            // Check that removing ANY positively-valued item eliminates envy
            for (uint32_t item = 0; item < num_items; item++) {
                /* Phase 8: Loop progress heartbeat */
                if ((item & 0xFF) == 0 && num_items > 256) {
                    gt_fairness_heartbeat("gt_fairness_loop",
                                     (float)(item + 1) / (float)num_items);
                }

                if (assignment[item] != j) continue;
                if (valuations[i][item] <= 0.0f) continue;  // Only positive values

                float reduced_value = others_value - valuations[i][item];
                if (reduced_value > my_value + 1e-6f) {
                    return false;  // Removing this item doesn't eliminate envy
                }
            }
        }
    }

    return true;
}

bool nimcp_fairness_is_proportional(const float* const* valuations,
                                     const uint32_t* assignment,
                                     uint32_t num_players,
                                     uint32_t num_items) {
    // Guard: validate inputs
    if (!valuations || !assignment || num_players == 0 || num_items == 0) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_is_proporti", 0.0f);


    float proportion = 1.0f / (float)num_players;

    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        float total_value = compute_total_value(valuations[p], num_items);
        float bundle_value = compute_bundle_value(valuations[p], assignment, p, num_items);

        float proportional_share = proportion * total_value;

        if (bundle_value < proportional_share - 1e-6f) {
            return false;  // Player p doesn't get their proportional share
        }
    }

    return true;
}

//=============================================================================
// Maximin Share Functions
//=============================================================================

float nimcp_fairness_maximin_share(const float* const* valuations,
                                    uint32_t player,
                                    uint32_t num_players,
                                    uint32_t num_items) {
    // Guard: validate inputs
    if (!valuations || player >= num_players || num_players == 0 || num_items == 0) {
        return -1.0f;
    }

    // Guard: limit items due to exponential complexity
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_maximin_sha", 0.0f);


    if (num_items > NIMCP_FAIRNESS_MAX_MMS_ITEMS) {
        return -1.0f;
    }

    // Allocate temporary assignment array
    uint32_t* temp_assignment = nimcp_calloc(num_items, sizeof(uint32_t));
    if (!temp_assignment) {
        return -1.0f;
    }

    float best_min = 0.0f;
    float unused = 0.0f;

    // Enumerate all partitions and find best minimum
    mms_partition_helper(valuations[player], num_items, num_players,
                        temp_assignment, 0, &unused, &best_min);

    nimcp_free(temp_assignment);
    temp_assignment = NULL;

    return best_min;
}

bool nimcp_fairness_has_mms_guarantee(const float* const* valuations,
                                       const uint32_t* assignment,
                                       uint32_t num_players,
                                       uint32_t num_items) {
    // Guard: validate inputs
    if (!valuations || !assignment || num_players == 0 || num_items == 0) {
        return false;
    }

    // Guard: limit items due to exponential complexity
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_has_mms_gua", 0.0f);


    if (num_items > NIMCP_FAIRNESS_MAX_MMS_ITEMS) {
        return false;
    }

    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        float mms = nimcp_fairness_maximin_share(valuations, p, num_players, num_items);
        if (mms < 0.0f) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_fairness_has_mms_guarantee: MMS computation failed");
            return false;  // Error computing MMS
        }

        float bundle_value = compute_bundle_value(valuations[p], assignment, p, num_items);

        if (bundle_value < mms - 1e-6f) {
            return false;  // Player p doesn't get their MMS
        }
    }

    return true;
}

//=============================================================================
// Comprehensive Analysis Functions
//=============================================================================

nimcp_error_t nimcp_fairness_compute_all(const float* values,
                                          uint32_t num_values,
                                          const nimcp_fairness_config_t* config,
                                          nimcp_fairness_result_t* result) {
    // Guard: validate inputs
    if (!values || !result) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_compute_all", 0.0f);


    if (num_values == 0) {
        return NIMCP_GT_ERROR_FAIRNESS_EMPTY_INPUT;
    }

    // Use default config if not provided
    nimcp_fairness_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = nimcp_fairness_default_config();
    }

    // Initialize result structure
    memset(result, 0, sizeof(nimcp_fairness_result_t));
    result->worst_off_player = NIMCP_GT_INVALID_PLAYER;

    // Compute all fairness indices
    result->jain_index = nimcp_fairness_jain_index(values, num_values);
    result->gini_coefficient = nimcp_fairness_gini_coefficient(values, num_values);
    result->theil_index = nimcp_fairness_theil_index(values, num_values);
    result->atkinson_index = nimcp_fairness_atkinson_index(values, num_values, cfg.atkinson_epsilon);
    result->coefficient_of_variation = nimcp_fairness_coefficient_variation(values, num_values);

    // Find worst-off player
    float min_value = FLT_MAX;
    for (uint32_t i = 0; i < num_values; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_values > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_values);
        }

        if (values[i] < min_value) {
            min_value = values[i];
            result->worst_off_player = (nimcp_player_id_t)i;
        }
    }
    result->min_proportional_share = min_value;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_fairness_analyze_allocation(const nimcp_allocation_t* allocation,
                                                 const nimcp_fairness_config_t* config,
                                                 nimcp_fairness_result_t* result) {
    // Guard: validate inputs
    if (!allocation || !result) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_analyze_all", 0.0f);


    if (allocation->num_players == 0 || allocation->num_items == 0) {
        return NIMCP_GT_ERROR_FAIRNESS_EMPTY_INPUT;
    }
    if (!allocation->valuations || !allocation->assignment) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }

    // Use default config if not provided
    nimcp_fairness_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = nimcp_fairness_default_config();
    }

    // Initialize result structure
    memset(result, 0, sizeof(nimcp_fairness_result_t));
    result->worst_off_player = NIMCP_GT_INVALID_PLAYER;

    uint32_t num_players = allocation->num_players;
    uint32_t num_items = allocation->num_items;

    // Compute bundle values
    float* bundle_values = nimcp_calloc(num_players, sizeof(float));
    if (!bundle_values) {
        return NIMCP_GT_ERROR_FAIRNESS_NO_MEMORY;
    }

    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        bundle_values[p] = compute_bundle_value(allocation->valuations[p],
                                                 allocation->assignment,
                                                 p, num_items);
    }

    // Compute fairness indices on bundle values
    result->jain_index = nimcp_fairness_jain_index(bundle_values, num_players);
    result->gini_coefficient = nimcp_fairness_gini_coefficient(bundle_values, num_players);
    result->theil_index = nimcp_fairness_theil_index(bundle_values, num_players);
    result->atkinson_index = nimcp_fairness_atkinson_index(bundle_values, num_players, cfg.atkinson_epsilon);
    result->coefficient_of_variation = nimcp_fairness_coefficient_variation(bundle_values, num_players);

    // Check allocation properties
    result->is_envy_free = nimcp_fairness_is_envy_free(
        (const float* const*)allocation->valuations,
        allocation->assignment, num_players, num_items);

    result->is_ef1 = nimcp_fairness_is_ef1(
        (const float* const*)allocation->valuations,
        allocation->assignment, num_players, num_items);

    result->is_efx = nimcp_fairness_is_efx(
        (const float* const*)allocation->valuations,
        allocation->assignment, num_players, num_items);

    result->is_proportional = nimcp_fairness_is_proportional(
        (const float* const*)allocation->valuations,
        allocation->assignment, num_players, num_items);

    // Compute MMS if items within limit
    if (num_items <= cfg.max_mms_items) {
        result->has_mms_guarantee = nimcp_fairness_has_mms_guarantee(
            (const float* const*)allocation->valuations,
            allocation->assignment, num_players, num_items);

        // Compute MMS for each player
        result->maximin_shares = nimcp_calloc(num_players, sizeof(float));
        if (result->maximin_shares) {
            result->min_mms_ratio = FLT_MAX;
            for (uint32_t p = 0; p < num_players; p++) {
                /* Phase 8: Loop progress heartbeat */
                if ((p & 0xFF) == 0 && num_players > 256) {
                    gt_fairness_heartbeat("gt_fairness_loop",
                                     (float)(p + 1) / (float)num_players);
                }

                result->maximin_shares[p] = nimcp_fairness_maximin_share(
                    (const float* const*)allocation->valuations,
                    p, num_players, num_items);

                if (result->maximin_shares[p] > 0.0f) {
                    float ratio = bundle_values[p] / result->maximin_shares[p];
                    if (ratio < result->min_mms_ratio) {
                        result->min_mms_ratio = ratio;
                    }
                }
            }
        }
    }

    // Compute envy analysis
    result->num_envy_pairs = 0;
    result->max_envy = 0.0f;
    result->total_envy = 0.0f;

    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_players);
        }

        for (uint32_t j = 0; j < num_players; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_players > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(j + 1) / (float)num_players);
            }

            if (i == j) continue;

            float others_value = compute_bundle_value(allocation->valuations[i],
                                                       allocation->assignment,
                                                       j, num_items);
            float envy = others_value - bundle_values[i];

            if (envy > 0.0f) {
                result->num_envy_pairs++;
                result->total_envy += envy;
                if (envy > result->max_envy) {
                    result->max_envy = envy;
                }
            }
        }
    }

    // Find worst-off player
    float min_value = FLT_MAX;
    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        float total = compute_total_value(allocation->valuations[p], num_items);
        float share = (total > 0.0f) ? bundle_values[p] / total : 0.0f;

        if (share < min_value) {
            min_value = share;
            result->worst_off_player = (nimcp_player_id_t)p;
        }
    }
    result->min_proportional_share = min_value;

    nimcp_free(bundle_values);
    bundle_values = NULL;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_fairness_find_envious_pairs(const float* const* valuations,
                                                 const uint32_t* assignment,
                                                 uint32_t num_players,
                                                 uint32_t num_items,
                                                 nimcp_envy_pair_t* pairs,
                                                 uint32_t max_pairs,
                                                 uint32_t* num_found) {
    // Guard: validate inputs
    if (!valuations || !assignment || !pairs || !num_found) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_find_enviou", 0.0f);


    if (num_players == 0 || num_items == 0 || max_pairs == 0) {
        return NIMCP_GT_ERROR_FAIRNESS_INVALID_PARAM;
    }

    *num_found = 0;

    // Compute bundle values for each player
    float bundle_values[NIMCP_GT_MAX_PLAYERS];
    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        bundle_values[p] = compute_bundle_value(valuations[p], assignment, p, num_items);
    }

    // Find all envious pairs
    for (uint32_t i = 0; i < num_players && *num_found < max_pairs; i++) {
        for (uint32_t j = 0; j < num_players && *num_found < max_pairs; j++) {
            if (i == j) continue;

            float others_value = compute_bundle_value(valuations[i], assignment, j, num_items);
            float envy = others_value - bundle_values[i];

            if (envy > 1e-6f) {
                nimcp_envy_pair_t* pair = &pairs[*num_found];
                pair->envying_player = (nimcp_player_id_t)i;
                pair->envied_player = (nimcp_player_id_t)j;
                pair->envy_amount = envy;
                pair->blocking_item = 0xFFFFFFFF;  // No blocking item yet

                // Find blocking item (for EF1)
                for (uint32_t item = 0; item < num_items; item++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((item & 0xFF) == 0 && num_items > 256) {
                        gt_fairness_heartbeat("gt_fairness_loop",
                                         (float)(item + 1) / (float)num_items);
                    }

                    if (assignment[item] != j) continue;

                    float reduced_value = others_value - valuations[i][item];
                    if (reduced_value <= bundle_values[i] + 1e-6f) {
                        pair->blocking_item = item;
                        break;
                    }
                }

                (*num_found)++;
            }
        }
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Allocation Improvement Functions
//=============================================================================

nimcp_error_t nimcp_fairness_pareto_improve(const nimcp_allocation_t* allocation,
                                             const float* const* valuations,
                                             nimcp_allocation_t* improved_allocation) {
    // Guard: validate inputs
    if (!allocation || !valuations || !improved_allocation) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_pareto_impr", 0.0f);


    uint32_t num_players = allocation->num_players;
    uint32_t num_items = allocation->num_items;

    // Copy current allocation
    nimcp_error_t err = nimcp_allocation_copy(allocation, improved_allocation);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Compute current bundle values
    float* current_values = nimcp_calloc(num_players, sizeof(float));
    if (!current_values) {
        return NIMCP_GT_ERROR_FAIRNESS_NO_MEMORY;
    }

    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        current_values[p] = compute_bundle_value(valuations[p], allocation->assignment,
                                                  p, num_items);
    }

    bool improved = false;

    // Try all possible swaps
    for (uint32_t item1 = 0; item1 < num_items && !improved; item1++) {
        for (uint32_t item2 = item1 + 1; item2 < num_items && !improved; item2++) {
            uint32_t owner1 = allocation->assignment[item1];
            uint32_t owner2 = allocation->assignment[item2];

            if (owner1 == owner2) continue;

            // Compute values after swap
            float new_value1 = current_values[owner1]
                               - valuations[owner1][item1]
                               + valuations[owner1][item2];
            float new_value2 = current_values[owner2]
                               - valuations[owner2][item2]
                               + valuations[owner2][item1];

            // Check if this is a Pareto improvement
            bool at_least_one_better = (new_value1 > current_values[owner1] + 1e-6f) ||
                                       (new_value2 > current_values[owner2] + 1e-6f);
            bool none_worse = (new_value1 >= current_values[owner1] - 1e-6f) &&
                              (new_value2 >= current_values[owner2] - 1e-6f);

            if (at_least_one_better && none_worse) {
                // Apply swap
                improved_allocation->assignment[item1] = owner2;
                improved_allocation->assignment[item2] = owner1;
                improved = true;
            }
        }
    }

    nimcp_free(current_values);
    current_values = NULL;

    if (!improved) {
        return NIMCP_GT_ERROR_FAIRNESS_NO_IMPROVEMENT;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_fairness_reduce_envy(const nimcp_allocation_t* allocation,
                                          const float* const* valuations,
                                          nimcp_allocation_t* improved_allocation) {
    // Guard: validate inputs
    if (!allocation || !valuations || !improved_allocation) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_reduce_envy", 0.0f);


    uint32_t num_players = allocation->num_players;
    uint32_t num_items = allocation->num_items;

    // Copy current allocation
    nimcp_error_t err = nimcp_allocation_copy(allocation, improved_allocation);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Compute current total envy
    float current_total_envy = 0.0f;
    float* bundle_values = nimcp_calloc(num_players, sizeof(float));
    if (!bundle_values) {
        return NIMCP_GT_ERROR_FAIRNESS_NO_MEMORY;
    }

    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        bundle_values[p] = compute_bundle_value(valuations[p], allocation->assignment,
                                                 p, num_items);
    }

    for (uint32_t i = 0; i < num_players; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(i + 1) / (float)num_players);
        }

        for (uint32_t j = 0; j < num_players; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_players > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(j + 1) / (float)num_players);
            }

            if (i == j) continue;
            float others_value = compute_bundle_value(valuations[i], allocation->assignment,
                                                       j, num_items);
            float envy = others_value - bundle_values[i];
            if (envy > 0.0f) {
                current_total_envy += envy;
            }
        }
    }

    bool improved = false;
    float best_improvement = 0.0f;
    uint32_t best_item1 = 0, best_item2 = 0;

    // Try all possible swaps
    for (uint32_t item1 = 0; item1 < num_items; item1++) {
        /* Phase 8: Loop progress heartbeat */
        if ((item1 & 0xFF) == 0 && num_items > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(item1 + 1) / (float)num_items);
        }

        for (uint32_t item2 = item1 + 1; item2 < num_items; item2++) {
            uint32_t owner1 = allocation->assignment[item1];
            uint32_t owner2 = allocation->assignment[item2];

            if (owner1 == owner2) continue;

            // Temporarily swap
            uint32_t* temp_assignment = nimcp_calloc(num_items, sizeof(uint32_t));
            if (!temp_assignment) {
                nimcp_free(bundle_values);
                bundle_values = NULL;
                return NIMCP_GT_ERROR_FAIRNESS_NO_MEMORY;
            }
            memcpy(temp_assignment, allocation->assignment, num_items * sizeof(uint32_t));
            temp_assignment[item1] = owner2;
            temp_assignment[item2] = owner1;

            // Compute new total envy
            float new_total_envy = 0.0f;
            for (uint32_t i = 0; i < num_players; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_players > 256) {
                    gt_fairness_heartbeat("gt_fairness_loop",
                                     (float)(i + 1) / (float)num_players);
                }

                float my_value = compute_bundle_value(valuations[i], temp_assignment, i, num_items);
                for (uint32_t j = 0; j < num_players; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && num_players > 256) {
                        gt_fairness_heartbeat("gt_fairness_loop",
                                         (float)(j + 1) / (float)num_players);
                    }

                    if (i == j) continue;
                    float others_value = compute_bundle_value(valuations[i], temp_assignment, j, num_items);
                    float envy = others_value - my_value;
                    if (envy > 0.0f) {
                        new_total_envy += envy;
                    }
                }
            }

            float improvement = current_total_envy - new_total_envy;
            if (improvement > best_improvement) {
                best_improvement = improvement;
                best_item1 = item1;
                best_item2 = item2;
                improved = true;
            }

            nimcp_free(temp_assignment);
            temp_assignment = NULL;
        }
    }

    nimcp_free(bundle_values);
    bundle_values = NULL;

    if (!improved || best_improvement < 1e-6f) {
        return NIMCP_GT_ERROR_FAIRNESS_NO_IMPROVEMENT;
    }

    // Apply best swap
    uint32_t owner1 = allocation->assignment[best_item1];
    uint32_t owner2 = allocation->assignment[best_item2];
    improved_allocation->assignment[best_item1] = owner2;
    improved_allocation->assignment[best_item2] = owner1;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Allocation Structure Functions
//=============================================================================

nimcp_allocation_t* nimcp_allocation_create(uint32_t num_players, uint32_t num_items) {
    // Guard: validate inputs
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_allocation_create", 0.0f);


    if (num_players == 0 || num_items == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_allocation_create: num_players or num_items is zero");
        return NULL;
    }
    if (num_players > NIMCP_GT_MAX_PLAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_allocation_create: num_players exceeds maximum");
        return NULL;
    }

    nimcp_allocation_t* alloc = nimcp_calloc(1, sizeof(nimcp_allocation_t));
    if (!alloc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate alloc");

        return NULL;
    }

    alloc->num_players = num_players;
    alloc->num_items = num_items;

    // Allocate assignment array
    alloc->assignment = nimcp_calloc(num_items, sizeof(uint32_t));
    if (!alloc->assignment) {
        nimcp_free(alloc);
        alloc = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_allocation_create: alloc->assignment is NULL");
        return NULL;
    }

    // Allocate valuations array (2D)
    alloc->valuations = nimcp_calloc(num_players, sizeof(float*));
    if (!alloc->valuations) {
        nimcp_free(alloc->assignment);
        nimcp_free(alloc);
        alloc = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_allocation_create: alloc->valuations is NULL");
        return NULL;
    }

    for (uint32_t p = 0; p < num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)num_players);
        }

        alloc->valuations[p] = nimcp_calloc(num_items, sizeof(float));
        if (!alloc->valuations[p]) {
            // Cleanup on failure
            for (uint32_t q = 0; q < p; q++) {
                /* Phase 8: Loop progress heartbeat */
                if ((q & 0xFF) == 0 && p > 256) {
                    gt_fairness_heartbeat("gt_fairness_loop",
                                     (float)(q + 1) / (float)p);
                }

                nimcp_free(alloc->valuations[q]);
            }
            nimcp_free(alloc->valuations);
            nimcp_free(alloc->assignment);
            nimcp_free(alloc);
            alloc = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_allocation_create: failed to allocate valuation row");
            return NULL;
        }
    }

    // Allocate bundle values cache
    alloc->bundle_values = nimcp_calloc(num_players, sizeof(float));
    if (!alloc->bundle_values) {
        for (uint32_t p = 0; p < num_players; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && num_players > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(p + 1) / (float)num_players);
            }

            nimcp_free(alloc->valuations[p]);
        }
        nimcp_free(alloc->valuations);
        nimcp_free(alloc->assignment);
        nimcp_free(alloc);
        alloc = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_allocation_create: failed to allocate bundle_values");
        return NULL;
    }

    alloc->values_cached = false;

    return alloc;
}

void nimcp_allocation_destroy(nimcp_allocation_t* allocation) {
    if (!allocation) return;

    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_allocation_destroy", 0.0f);


    if (allocation->bundle_values) {
        nimcp_free(allocation->bundle_values);
    }

    if (allocation->valuations) {
        for (uint32_t p = 0; p < allocation->num_players; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && allocation->num_players > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(p + 1) / (float)allocation->num_players);
            }

            if (allocation->valuations[p]) {
                nimcp_free(allocation->valuations[p]);
            }
        }
        nimcp_free(allocation->valuations);
    }

    if (allocation->assignment) {
        nimcp_free(allocation->assignment);
    }

    nimcp_free(allocation);
    allocation = NULL;
}

nimcp_error_t nimcp_allocation_set_valuations(nimcp_allocation_t* allocation,
                                               uint32_t player,
                                               const float* valuations) {
    // Guard: validate inputs
    if (!allocation || !valuations) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_allocation_set_valua", 0.0f);


    if (player >= allocation->num_players) {
        return NIMCP_GT_ERROR_FAIRNESS_INVALID_PARAM;
    }

    memcpy(allocation->valuations[player], valuations,
           allocation->num_items * sizeof(float));
    allocation->values_cached = false;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_allocation_assign_item(nimcp_allocation_t* allocation,
                                            uint32_t item,
                                            uint32_t player) {
    // Guard: validate inputs
    if (!allocation) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_allocation_assign_it", 0.0f);


    if (item >= allocation->num_items || player >= allocation->num_players) {
        return NIMCP_GT_ERROR_FAIRNESS_INVALID_PARAM;
    }

    allocation->assignment[item] = player;
    allocation->values_cached = false;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_allocation_compute_bundle_values(nimcp_allocation_t* allocation) {
    // Guard: validate inputs
    if (!allocation) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_allocation_compute_b", 0.0f);


    for (uint32_t p = 0; p < allocation->num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && allocation->num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)allocation->num_players);
        }

        allocation->bundle_values[p] = compute_bundle_value(
            allocation->valuations[p],
            allocation->assignment,
            p,
            allocation->num_items);
    }

    allocation->values_cached = true;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_allocation_copy(const nimcp_allocation_t* src,
                                     nimcp_allocation_t* dst) {
    // Guard: validate inputs
    if (!src || !dst) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_allocation_copy", 0.0f);


    if (src->num_players != dst->num_players || src->num_items != dst->num_items) {
        return NIMCP_GT_ERROR_FAIRNESS_INVALID_PARAM;
    }

    // Copy assignment
    memcpy(dst->assignment, src->assignment, src->num_items * sizeof(uint32_t));

    // Copy valuations
    for (uint32_t p = 0; p < src->num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && src->num_players > 256) {
            gt_fairness_heartbeat("gt_fairness_loop",
                             (float)(p + 1) / (float)src->num_players);
        }

        memcpy(dst->valuations[p], src->valuations[p], src->num_items * sizeof(float));
    }

    // Copy bundle values
    memcpy(dst->bundle_values, src->bundle_values, src->num_players * sizeof(float));
    dst->values_cached = src->values_cached;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Result Structure Functions
//=============================================================================

nimcp_error_t nimcp_fairness_result_init(nimcp_fairness_result_t* result,
                                          uint32_t num_players) {
    // Guard: validate inputs
    if (!result) {
        return NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_result_init", 0.0f);


    memset(result, 0, sizeof(nimcp_fairness_result_t));
    result->worst_off_player = NIMCP_GT_INVALID_PLAYER;
    result->min_mms_ratio = 0.0f;

    if (num_players > 0) {
        result->maximin_shares = nimcp_calloc(num_players, sizeof(float));
        if (!result->maximin_shares) {
            return NIMCP_GT_ERROR_FAIRNESS_NO_MEMORY;
        }
    }

    return NIMCP_SUCCESS;
}

void nimcp_fairness_result_cleanup(nimcp_fairness_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_result_clea", 0.0f);


    if (result->maximin_shares) {
        nimcp_free(result->maximin_shares);
        result->maximin_shares = NULL;
    }
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_fairness_measure_name(nimcp_fairness_measure_t measure) {
    if (measure >= NIMCP_FAIRNESS_COUNT) {
        return "Unknown";
    }
    return s_fairness_measure_names[measure];
}

const char* nimcp_allocation_property_name(nimcp_allocation_property_t property) {
    if (property >= NIMCP_ALLOC_PROPERTY_COUNT) {
        return "Unknown";
    }
    return s_allocation_property_names[property];
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int fairness_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    gt_fairness_heartbeat("gt_fairness_fairness_query_self_", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Fairness_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_fairness_heartbeat("gt_fairness_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG(LOG_MODULE, "Fairness self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Fairness_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Fairness_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_fairness_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_fairness_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_fairness_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_fairness_training_begin: NULL argument");
        return -1;
    }
    gt_fairness_heartbeat_instance(NULL, "gt_fairness_training_begin", 0.0f);
    return 0;
}

int gt_fairness_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_fairness_training_end: NULL argument");
        return -1;
    }
    gt_fairness_heartbeat_instance(NULL, "gt_fairness_training_end", 1.0f);
    return 0;
}

int gt_fairness_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_fairness_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_fairness_heartbeat_instance(NULL, "gt_fairness_training_step", progress);
    return 0;
}
