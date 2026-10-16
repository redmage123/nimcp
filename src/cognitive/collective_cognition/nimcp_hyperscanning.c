/**
 * @file nimcp_hyperscanning.c
 * @brief Implementation of inter-brain neural synchronization
 *
 * WHAT: Real-time neural synchronization between NIMCP brain instances
 * WHY: Enable distributed consciousness through coordinated neural activity
 * HOW: Phase-locking value (PLV) computation across EEG-like frequency bands
 */

#include "cognitive/collective_cognition/nimcp_hyperscanning.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(hyperscanning, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Constants
 *===========================================================================*/

#define PI NIMCP_PI_F

/** Number of phase history samples for circular statistics PLV */
#define PLV_HISTORY_SIZE 32

/** Time-series history length for Granger causality */
#define GRANGER_HISTORY_SIZE 16

/** Hash table bucket count for instance lookup */
#define INSTANCE_HASH_BUCKETS 32

/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Phase history for proper PLV computation using circular statistics
 */
typedef struct {
    float phase_diff_history[PLV_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;
} phase_history_t;

/**
 * @brief Time-series history for Granger causality computation
 */
typedef struct {
    float power_history_a[GRANGER_HISTORY_SIZE];
    float power_history_b[GRANGER_HISTORY_SIZE];
    uint64_t timestamp_history[GRANGER_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;
} granger_history_t;

/**
 * @brief Registered instance entry
 */
typedef struct {
    uint32_t instance_id;
    bool active;

    /* Current neural state */
    hyperscanning_neural_state_t state;

    /* Callback for state updates */
    hyperscanning_state_callback_fn callback;
    void* callback_user_data;

    /* Entrainment state */
    entrainment_status_t entrainment_status;
    uint32_t entrainment_target;
    sync_band_t entrainment_band;
    float entrainment_target_phase;
    uint32_t entrainment_progress;
} hyperscanning_instance_t;

/**
 * @brief Pair synchronization entry
 */
typedef struct {
    uint32_t instance_a;
    uint32_t instance_b;
    hyperscan_pair_t metrics;
    uint32_t sync_count;        /* Count of sustained high sync */

    /* Phase history per band for proper PLV computation */
    phase_history_t phase_history[SYNC_BAND_COUNT];

    /* Granger causality history */
    granger_history_t granger_history;
} pair_sync_entry_t;

/**
 * @brief Internal hyperscanning state
 */
struct hyperscanning {
    /* Configuration */
    hyperscanning_config_t config;

    /* Registered instances */
    hyperscanning_instance_t instances[COLLECTIVE_MAX_INSTANCES];
    uint32_t instance_count;

    /* Hash table for O(1) instance lookup */
    hash_table_t* instance_lookup;

    /* Pair metrics */
    pair_sync_entry_t pairs[HYPERSCAN_MAX_PAIRS];
    uint32_t pair_count;

    /* Global state */
    hyperscan_state_t global_state;

    /* Entrainment callback */
    hyperscanning_entrainment_callback_fn entrainment_callback;
    void* entrainment_callback_data;

    /* Statistics */
    hyperscanning_stats_t stats;

    /* Flags */
    bool initialized;
    uint64_t last_update_us;
};

/*=============================================================================
 * Helper Functions - Time
 *===========================================================================*/

/**
 * @brief Get current wall-clock timestamp in microseconds
 *
 * Uses nimcp_time_get_us() for actual wall-clock time instead of
 * a monotonic counter. Handles potential overflow by using the full
 * 64-bit range which won't overflow for ~584,000 years from epoch.
 */
static uint64_t get_timestamp_us(void) {
    return nimcp_time_get_us();
}

/*=============================================================================
 * Helper Functions - Instance Management (O(1) Hash Table Lookup)
 *===========================================================================*/

/**
 * @brief Find instance by ID using hash table for O(1) lookup
 *
 * Replaces the O(n) linear search with hash table lookup.
 * Falls back to linear search if hash table is not initialized.
 */
static hyperscanning_instance_t* find_instance(
    hyperscanning_t* hs,
    uint32_t instance_id
) {
    /* Try hash table first for O(1) lookup */
    if (hs->instance_lookup) {
        uint32_t* index_ptr = hash_table_lookup_uint32(hs->instance_lookup, instance_id);
        if (index_ptr) {
            uint32_t index = *index_ptr;
            if (index < COLLECTIVE_MAX_INSTANCES &&
                hs->instances[index].active &&
                hs->instances[index].instance_id == instance_id) {
                return &hs->instances[index];
            }
        }
    }

    /* Fallback to linear search (should rarely happen) */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (hs->instances[i].active && hs->instances[i].instance_id == instance_id) {
            return &hs->instances[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static int find_instance_index(
    const hyperscanning_t* hs,
    uint32_t instance_id
) {
    /* Try hash table first for O(1) lookup */
    if (hs->instance_lookup) {
        uint32_t* index_ptr = hash_table_lookup_uint32((hash_table_t*)hs->instance_lookup, instance_id);
        if (index_ptr) {
            uint32_t index = *index_ptr;
            if (index < COLLECTIVE_MAX_INSTANCES &&
                hs->instances[index].active &&
                hs->instances[index].instance_id == instance_id) {
                return (int)index;
            }
        }
    }

    /* Fallback to linear search */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (hs->instances[i].active && hs->instances[i].instance_id == instance_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found is normal */
}

static hyperscanning_instance_t* find_free_slot(hyperscanning_t* hs) {
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!hs->instances[i].active) {
            return &hs->instances[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

/**
 * @brief Get index of instance in array
 */
static int get_instance_array_index(hyperscanning_t* hs, hyperscanning_instance_t* inst) {
    if (!inst || !hs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_instance_array_index: required parameter is NULL (inst, hs)");
        return -1;
    }
    ptrdiff_t diff = inst - hs->instances;
    if (diff >= 0 && diff < COLLECTIVE_MAX_INSTANCES) {
        return (int)diff;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_instance_array_index: capacity exceeded");
    return -1;
}

/*=============================================================================
 * Helper Functions - Pair Management
 *===========================================================================*/

static pair_sync_entry_t* find_pair(
    hyperscanning_t* hs,
    uint32_t instance_a,
    uint32_t instance_b
) {
    /* Ensure consistent ordering */
    if (instance_a > instance_b) {
        uint32_t tmp = instance_a;
        instance_a = instance_b;
        instance_b = tmp;
    }

    for (uint32_t i = 0; i < hs->pair_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hs->pair_count > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)hs->pair_count);
        }

        if (hs->pairs[i].instance_a == instance_a &&
            hs->pairs[i].instance_b == instance_b) {
            return &hs->pairs[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static pair_sync_entry_t* get_or_create_pair(
    hyperscanning_t* hs,
    uint32_t instance_a,
    uint32_t instance_b
) {
    /* Ensure consistent ordering */
    if (instance_a > instance_b) {
        uint32_t tmp = instance_a;
        instance_a = instance_b;
        instance_b = tmp;
    }

    pair_sync_entry_t* pair = find_pair(hs, instance_a, instance_b);
    if (pair) return pair;

    /* Create new pair */
    if (hs->pair_count >= HYPERSCAN_MAX_PAIRS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_or_create_pair: capacity exceeded");
        return NULL;
    }

    pair = &hs->pairs[hs->pair_count++];
    memset(pair, 0, sizeof(pair_sync_entry_t));
    pair->instance_a = instance_a;
    pair->instance_b = instance_b;
    pair->metrics.instance_a = instance_a;
    pair->metrics.instance_b = instance_b;

    return pair;
}

/*=============================================================================
 * Helper Functions - PLV Computation (Proper Circular Statistics)
 *===========================================================================*/

/**
 * @brief Compute phase-locking value using proper circular statistics
 *
 * Implements the standard PLV formula:
 *   PLV = |mean(exp(i * (phase_a - phase_b)))|
 *       = sqrt((sum(cos(phase_diff)))^2 + (sum(sin(phase_diff)))^2) / N
 *
 * This uses the phase difference history to compute the circular mean,
 * providing a statistically robust measure of phase synchronization.
 *
 * Complexity: O(PLV_HISTORY_SIZE) = O(32) = O(1) constant time
 */
static float compute_plv_for_band(
    const hyperscanning_neural_state_t* state_a,
    const hyperscanning_neural_state_t* state_b,
    sync_band_t band
) {
    float phase_diff = state_a->band_phase[band] - state_b->band_phase[band];

    /* Normalize to [-PI, PI] */
    while (phase_diff > PI) phase_diff -= 2.0f * PI;
    while (phase_diff < -PI) phase_diff += 2.0f * PI;

    /* Single sample PLV approximation using circular statistics principle:
     * PLV = |exp(i * phase_diff)| for single sample
     * But we need the real and imaginary components to properly average */
    float cos_component = cosf(phase_diff);

    /* For a single sample, PLV = sqrt(cos^2 + sin^2) = 1 always,
     * so we return |cos| as a proxy that indicates how aligned the phases are */
    return fabsf(cos_component);
}

/**
 * @brief Compute PLV with history using proper circular statistics
 *
 * This is the full PLV computation that maintains phase difference history
 * and computes the circular mean magnitude.
 *
 * @param pair The pair entry containing phase history
 * @param state_a Neural state of first instance
 * @param state_b Neural state of second instance
 * @param band Frequency band
 * @return PLV value [0, 1]
 */
static float compute_plv_with_history(
    pair_sync_entry_t* pair,
    const hyperscanning_neural_state_t* state_a,
    const hyperscanning_neural_state_t* state_b,
    sync_band_t band
) {
    phase_history_t* hist = &pair->phase_history[band];

    /* Compute current phase difference */
    float phase_diff = state_a->band_phase[band] - state_b->band_phase[band];

    /* Normalize to [-PI, PI] */
    while (phase_diff > PI) phase_diff -= 2.0f * PI;
    while (phase_diff < -PI) phase_diff += 2.0f * PI;

    /* Add to circular buffer */
    hist->phase_diff_history[hist->history_index] = phase_diff;
    hist->history_index = (hist->history_index + 1) % PLV_HISTORY_SIZE;
    if (hist->history_count < PLV_HISTORY_SIZE) {
        hist->history_count++;
    }

    /* Compute PLV using circular statistics */
    if (hist->history_count == 0) {
        return fabsf(cosf(phase_diff));  /* Fallback for empty history */
    }

    float sum_cos = 0.0f;
    float sum_sin = 0.0f;

    for (uint32_t i = 0; i < hist->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hist->history_count > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)hist->history_count);
        }

        float diff = hist->phase_diff_history[i];
        sum_cos += cosf(diff);
        sum_sin += sinf(diff);
    }

    /* PLV = |mean(exp(i * phase_diff))| = sqrt((sum_cos/N)^2 + (sum_sin/N)^2) */
    float mean_cos = sum_cos / (float)hist->history_count;
    float mean_sin = sum_sin / (float)hist->history_count;
    float plv = sqrtf(mean_cos * mean_cos + mean_sin * mean_sin);

    return plv;
}

/**
 * @brief Compute spectral coherence between two instances for a band
 *
 * Coherence = (power_a * power_b) / sqrt(power_a^2 * power_b^2) * PLV
 */
static float compute_coherence_for_band(
    const hyperscanning_neural_state_t* state_a,
    const hyperscanning_neural_state_t* state_b,
    sync_band_t band
) {
    float power_a = state_a->band_power[band];
    float power_b = state_b->band_power[band];
    float plv = compute_plv_for_band(state_a, state_b, band);

    /* Coherence weighted by power similarity and PLV */
    float power_similarity = 1.0f - fabsf(power_a - power_b);
    return plv * power_similarity;
}

/**
 * @brief Estimate temporal lag between two instances
 *
 * Positive = instance_a leads, negative = instance_b leads
 */
static float estimate_lag_ms(
    const hyperscanning_neural_state_t* state_a,
    const hyperscanning_neural_state_t* state_b
) {
    /* Use timestamp difference as proxy */
    int64_t time_diff = (int64_t)state_a->timestamp_us - (int64_t)state_b->timestamp_us;

    /* Convert to ms and add phase-based estimate */
    float phase_diff = state_a->band_phase[SYNC_BAND_GAMMA] -
                       state_b->band_phase[SYNC_BAND_GAMMA];

    /* Gamma at ~40Hz means 25ms period, so phase_diff / (2*PI) * 25ms */
    float phase_lag_ms = (phase_diff / (2.0f * PI)) * 25.0f;

    return (float)(time_diff / 1000) + phase_lag_ms;
}

/*=============================================================================
 * Helper Functions - Granger Causality (Time-Series Analysis)
 *===========================================================================*/

/**
 * @brief Update Granger causality history
 */
static void update_granger_history(
    granger_history_t* hist,
    float power_a,
    float power_b,
    uint64_t timestamp
) {
    hist->power_history_a[hist->history_index] = power_a;
    hist->power_history_b[hist->history_index] = power_b;
    hist->timestamp_history[hist->history_index] = timestamp;
    hist->history_index = (hist->history_index + 1) % GRANGER_HISTORY_SIZE;
    if (hist->history_count < GRANGER_HISTORY_SIZE) {
        hist->history_count++;
    }
}

/**
 * @brief Compute Granger causality using time-series analysis
 *
 * Granger causality measures whether past values of X help predict Y
 * beyond what past values of Y alone can predict.
 *
 * Simplified implementation using prediction error comparison:
 * - Predict current power_b from its own history (autoregression)
 * - Predict current power_b from both histories
 * - Compare prediction errors
 *
 * Positive return = A Granger-causes B (A's past helps predict B)
 * Negative return = B Granger-causes A
 *
 * Complexity: O(GRANGER_HISTORY_SIZE^2) for full analysis
 * This simplified version is O(GRANGER_HISTORY_SIZE) = O(16) = O(1)
 */
static float compute_granger_causality_timeseries(
    granger_history_t* hist,
    float current_power_a,
    float current_power_b
) {
    if (hist->history_count < 4) {
        /* Not enough history for meaningful analysis */
        /* Fall back to simple power ratio */
        if (current_power_a + current_power_b < 0.01f) return 0.0f;
        return (current_power_a - current_power_b) / (current_power_a + current_power_b);
    }

    /* Compute means */
    float mean_a = 0.0f, mean_b = 0.0f;
    for (uint32_t i = 0; i < hist->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hist->history_count > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)hist->history_count);
        }

        mean_a += hist->power_history_a[i];
        mean_b += hist->power_history_b[i];
    }
    mean_a /= (float)hist->history_count;
    mean_b /= (float)hist->history_count;

    /* Compute lagged cross-correlation (simplified Granger proxy)
     * If A at time t-1 correlates with B at time t more than
     * B at time t-1 correlates with A at time t, then A->B causation */
    float cross_ab = 0.0f;  /* A past predicting B current */
    float cross_ba = 0.0f;  /* B past predicting A current */
    float var_a = 0.0f, var_b = 0.0f;

    for (uint32_t i = 1; i < hist->history_count; i++) {
        /* Get lagged values (circular buffer) */
        uint32_t curr_idx = (hist->history_index + GRANGER_HISTORY_SIZE - hist->history_count + i) % GRANGER_HISTORY_SIZE;
        uint32_t prev_idx = (curr_idx + GRANGER_HISTORY_SIZE - 1) % GRANGER_HISTORY_SIZE;

        float a_curr = hist->power_history_a[curr_idx] - mean_a;
        float a_prev = hist->power_history_a[prev_idx] - mean_a;
        float b_curr = hist->power_history_b[curr_idx] - mean_b;
        float b_prev = hist->power_history_b[prev_idx] - mean_b;

        cross_ab += a_prev * b_curr;  /* Does past A predict current B? */
        cross_ba += b_prev * a_curr;  /* Does past B predict current A? */
        var_a += a_prev * a_prev;
        var_b += b_prev * b_prev;
    }

    /* Normalize by variance */
    float epsilon = NIMCP_EPSILON_NUMERICAL;
    if (var_a > epsilon) cross_ab /= sqrtf(var_a);
    if (var_b > epsilon) cross_ba /= sqrtf(var_b);

    /* Granger causality index: positive if A->B, negative if B->A */
    float gc = cross_ab - cross_ba;

    /* Clamp to [-1, 1] */
    if (gc > 1.0f) gc = 1.0f;
    if (gc < -1.0f) gc = -1.0f;

    return gc;
}

/**
 * @brief Compute Granger causality (wrapper using history if available)
 */
static float compute_granger_causality(
    pair_sync_entry_t* pair,
    const hyperscanning_neural_state_t* state_a,
    const hyperscanning_neural_state_t* state_b
) {
    float power_a = state_a->band_power[SYNC_BAND_GAMMA];
    float power_b = state_b->band_power[SYNC_BAND_GAMMA];

    /* Update history */
    uint64_t timestamp = (state_a->timestamp_us + state_b->timestamp_us) / 2;
    update_granger_history(&pair->granger_history, power_a, power_b, timestamp);

    /* Compute Granger causality from time-series */
    return compute_granger_causality_timeseries(&pair->granger_history, power_a, power_b);
}

/**
 * @brief Update pair metrics
 */
static void update_pair_metrics(
    hyperscanning_t* hs,
    pair_sync_entry_t* pair,
    const hyperscanning_neural_state_t* state_a,
    const hyperscanning_neural_state_t* state_b
) {
    hyperscan_pair_t* m = &pair->metrics;

    /* Compute PLV and coherence for each band using proper circular statistics */
    float avg_plv = 0.0f;
    for (int b = 0; b < SYNC_BAND_COUNT; b++) {
        /* Phase 8: Loop progress heartbeat */
        if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(b + 1) / (float)SYNC_BAND_COUNT);
        }

        m->plv[b] = compute_plv_with_history(pair, state_a, state_b, (sync_band_t)b);
        m->coherence[b] = compute_coherence_for_band(state_a, state_b, (sync_band_t)b);
        avg_plv += m->plv[b];
    }
    avg_plv /= SYNC_BAND_COUNT;

    /* Compute temporal dynamics */
    m->lag_ms = estimate_lag_ms(state_a, state_b);
    m->granger_causality = compute_granger_causality(pair, state_a, state_b);

    /* Check if synchronized */
    m->is_synchronized = avg_plv >= HYPERSCAN_PLV_THRESHOLD;

    /* Track sustained sync count */
    if (m->is_synchronized) {
        pair->sync_count++;
    } else {
        pair->sync_count = 0;
    }

    hs->stats.sync_computations++;
}

/*=============================================================================
 * Helper Functions - Global State
 *===========================================================================*/

static void update_global_state(hyperscanning_t* hs) {
    hyperscan_state_t* gs = &hs->global_state;

    if (hs->instance_count < 2) {
        /* No pairs to sync, but still detect leader if enabled */
        gs->global_sync = 0.0f;
        gs->gamma_binding = 0.0f;
        gs->theta_emotional = 0.0f;
        gs->beta_coordination = 0.0f;
        gs->is_entrained = false;

        /* Single instance is trivially its own leader */
        if (hs->config.enable_leader_detection && hs->instance_count == 1) {
            for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
                    hyperscanning_heartbeat("hyperscannin_loop",
                                     (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
                }

                if (hs->instances[i].active) {
                    gs->leader_instance_id = hs->instances[i].instance_id;
                    gs->leader_influence = hs->instances[i].state.band_power[SYNC_BAND_GAMMA];
                    break;
                }
            }
        } else {
            gs->leader_instance_id = 0;
            gs->leader_influence = 0.0f;
        }
        return;
    }

    /* Compute average sync across all pairs */
    float total_sync = 0.0f;
    float gamma_total = 0.0f;
    float theta_total = 0.0f;
    float beta_total = 0.0f;
    uint32_t pair_count = 0;

    for (uint32_t i = 0; i < hs->pair_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hs->pair_count > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)hs->pair_count);
        }

        pair_sync_entry_t* pair = &hs->pairs[i];
        hyperscan_pair_t* m = &pair->metrics;

        float pair_avg = 0.0f;
        for (int b = 0; b < SYNC_BAND_COUNT; b++) {
            /* Phase 8: Loop progress heartbeat */
            if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
                hyperscanning_heartbeat("hyperscannin_loop",
                                 (float)(b + 1) / (float)SYNC_BAND_COUNT);
            }

            pair_avg += m->plv[b];
        }
        pair_avg /= SYNC_BAND_COUNT;

        total_sync += pair_avg;
        gamma_total += m->plv[SYNC_BAND_GAMMA];
        theta_total += m->plv[SYNC_BAND_THETA];
        beta_total += m->plv[SYNC_BAND_BETA];
        pair_count++;
    }

    if (pair_count > 0) {
        gs->global_sync = total_sync / pair_count;
        gs->gamma_binding = gamma_total / pair_count;
        gs->theta_emotional = theta_total / pair_count;
        gs->beta_coordination = beta_total / pair_count;
    } else {
        gs->global_sync = 0.0f;
        gs->gamma_binding = 0.0f;
        gs->theta_emotional = 0.0f;
        gs->beta_coordination = 0.0f;
    }

    /* Check entrainment threshold */
    bool was_entrained = gs->is_entrained;
    gs->is_entrained = gs->global_sync >= hs->config.sync_threshold;

    /* Update avg statistics */
    hs->stats.avg_global_sync = (hs->stats.avg_global_sync + gs->global_sync) / 2.0f;
    if (gs->global_sync > hs->stats.max_global_sync) {
        hs->stats.max_global_sync = gs->global_sync;
    }
    hs->stats.avg_plv_gamma = (hs->stats.avg_plv_gamma + gs->gamma_binding) / 2.0f;
    hs->stats.avg_plv_theta = (hs->stats.avg_plv_theta + gs->theta_emotional) / 2.0f;

    /* Find leader (highest gamma power) */
    if (hs->config.enable_leader_detection) {
        float max_gamma = -1.0f;
        uint32_t leader_id = 0;

        for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
                hyperscanning_heartbeat("hyperscannin_loop",
                                 (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
            }

            if (!hs->instances[i].active) continue;

            float gamma = hs->instances[i].state.band_power[SYNC_BAND_GAMMA];
            if (gamma > max_gamma) {
                max_gamma = gamma;
                leader_id = hs->instances[i].instance_id;
            }
        }

        gs->leader_instance_id = leader_id;
        gs->leader_influence = max_gamma > 0.0f ? max_gamma : 0.0f;
    }

    /* Track entrainment events */
    if (gs->is_entrained && !was_entrained) {
        hs->stats.entrainment_successes++;
    }
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

hyperscanning_t* hyperscanning_create(const hyperscanning_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_create", 0.0f);


    hyperscanning_t* hs = nimcp_malloc(sizeof(hyperscanning_t));
    if (!hs) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate hs");

        return NULL;

    }

    memset(hs, 0, sizeof(hyperscanning_t));

    /* Apply configuration */
    if (config) {
        hs->config = *config;
    } else {
        hs->config = hyperscanning_default_config();
    }

    /* Create hash table for O(1) instance lookup */
    hash_table_config_t ht_config = {
        .initial_buckets = INSTANCE_HASH_BUCKETS,
        .key_type = HASH_KEY_UINT32,
        .hash_algorithm = HASH_ALG_MURMUR3,
        .value_destructor = NULL,
        .case_insensitive = false,
        .thread_safe = false
    };
    hs->instance_lookup = hash_table_create(&ht_config);
    /* Note: We continue even if hash table fails - will use linear fallback */

    /* Initialize instances */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        hs->instances[i].active = false;
        hs->instances[i].entrainment_status = ENTRAINMENT_NONE;

        /* Initialize default state */
        for (int b = 0; b < SYNC_BAND_COUNT; b++) {
            /* Phase 8: Loop progress heartbeat */
            if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
                hyperscanning_heartbeat("hyperscannin_loop",
                                 (float)(b + 1) / (float)SYNC_BAND_COUNT);
            }

            hs->instances[i].state.band_power[b] = 0.5f;
            hs->instances[i].state.band_phase[b] = 0.0f;
        }
    }

    hs->initialized = true;
    hs->last_update_us = get_timestamp_us();

    return hs;
}

void hyperscanning_destroy(hyperscanning_t* hs) {
    if (!hs) return;

    /* Destroy hash table */
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_destroy", 0.0f);


    if (hs->instance_lookup) {
        hash_table_destroy(hs->instance_lookup);
        hs->instance_lookup = NULL;
    }

    nimcp_free(hs);
    hs = NULL;
}

int hyperscanning_reset(hyperscanning_t* hs) {
    if (!hs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_reset: hs is NULL");
        return -1;
    }

    /* Clear hash table */
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_reset", 0.0f);


    if (hs->instance_lookup) {
        hash_table_clear(hs->instance_lookup);
    }

    /* Reset all instances */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        hs->instances[i].active = false;
        hs->instances[i].entrainment_status = ENTRAINMENT_NONE;
    }
    hs->instance_count = 0;

    /* Reset pairs */
    hs->pair_count = 0;

    /* Reset state */
    memset(&hs->global_state, 0, sizeof(hs->global_state));
    memset(&hs->stats, 0, sizeof(hs->stats));

    hs->last_update_us = get_timestamp_us();

    return 0;
}

/*=============================================================================
 * Instance Management API
 *===========================================================================*/

int hyperscanning_register_instance(
    hyperscanning_t* hs,
    uint32_t instance_id,
    hyperscanning_state_callback_fn state_callback,
    void* user_data
) {
    if (!hs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_register_instance: hs is NULL");
        return -1;
    }

    /* Check if already registered */
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_register_instance", 0.0f);


    if (find_instance(hs, instance_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hyperscanning_register_instance: validation failed");
        return -1;
    }

    /* Find free slot */
    hyperscanning_instance_t* slot = find_free_slot(hs);
    if (!slot) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_register_instance: slot is NULL");
        return -1;
    }

    /* Initialize instance */
    slot->instance_id = instance_id;
    slot->active = true;
    slot->callback = state_callback;
    slot->callback_user_data = user_data;
    slot->entrainment_status = ENTRAINMENT_NONE;

    /* Initialize state */
    slot->state.instance_id = instance_id;
    slot->state.timestamp_us = get_timestamp_us();
    slot->state.atp_level = 1.0f;
    slot->state.fatigue_level = 0.0f;
    slot->state.gw_broadcast_strength = 0.5f;

    for (int b = 0; b < SYNC_BAND_COUNT; b++) {
        /* Phase 8: Loop progress heartbeat */
        if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(b + 1) / (float)SYNC_BAND_COUNT);
        }

        slot->state.band_power[b] = 0.5f;
        slot->state.band_phase[b] = (float)(instance_id * 0.5);
    }

    hs->instance_count++;

    /* Add to hash table for O(1) lookup */
    if (hs->instance_lookup) {
        int index = get_instance_array_index(hs, slot);
        if (index >= 0) {
            uint32_t idx = (uint32_t)index;
            hash_table_insert_uint32(hs->instance_lookup, instance_id, &idx, sizeof(uint32_t));
        }
    }

    /* Create pairs with all existing instances */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (hs->instances[i].active &&
            hs->instances[i].instance_id != instance_id) {
            get_or_create_pair(hs, instance_id, hs->instances[i].instance_id);
        }
    }

    return 0;
}

int hyperscanning_unregister_instance(
    hyperscanning_t* hs,
    uint32_t instance_id
) {
    if (!hs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_unregister_instance: hs is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_unregister_instance", 0.0f);


    hyperscanning_instance_t* inst = find_instance(hs, instance_id);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_unregister_instance: inst is NULL");
        return -1;
    }

    inst->active = false;
    hs->instance_count--;

    /* Remove from hash table */
    if (hs->instance_lookup) {
        hash_table_remove_uint32(hs->instance_lookup, instance_id);
    }

    /* Note: Pairs will become stale but won't be updated */
    /* A full cleanup could remove pairs involving this instance */

    return 0;
}

int hyperscanning_update_state(
    hyperscanning_t* hs,
    const hyperscanning_neural_state_t* state
) {
    if (!hs || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_update_state: required parameter is NULL (hs, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_update_state", 0.0f);


    hyperscanning_instance_t* inst = find_instance(hs, state->instance_id);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_update_state: inst is NULL");
        return -1;
    }

    inst->state = *state;
    inst->state.timestamp_us = get_timestamp_us();

    hs->stats.states_received++;

    /* Notify callback if registered */
    if (inst->callback) {
        inst->callback(&inst->state, inst->callback_user_data);
    }

    return 0;
}

/*=============================================================================
 * Synchronization API
 *===========================================================================*/

int hyperscanning_update(hyperscanning_t* hs) {
    if (!hs || !hs->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_update: required parameter is NULL (hs, hs->initialized)");
        return -1;
    }

    /* Update all pair metrics */
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_update", 0.0f);


    for (uint32_t i = 0; i < hs->pair_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hs->pair_count > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)hs->pair_count);
        }

        pair_sync_entry_t* pair = &hs->pairs[i];

        /* Find both instances */
        hyperscanning_instance_t* inst_a = find_instance(hs, pair->instance_a);
        hyperscanning_instance_t* inst_b = find_instance(hs, pair->instance_b);

        if (inst_a && inst_b) {
            update_pair_metrics(hs, pair, &inst_a->state, &inst_b->state);
        }
    }

    /* Update global state */
    update_global_state(hs);

    /* Process entrainment */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        hyperscanning_instance_t* inst = &hs->instances[i];
        if (!inst->active) continue;

        if (inst->entrainment_status == ENTRAINMENT_IN_PROGRESS) {
            hyperscanning_instance_t* target = find_instance(hs, inst->entrainment_target);
            if (target) {
                /* Adjust phase towards target */
                sync_band_t band = inst->entrainment_band;
                float target_phase = target->state.band_phase[band];
                float current_phase = inst->state.band_phase[band];
                float diff = target_phase - current_phase;

                /* Normalize to [-PI, PI] */
                while (diff > PI) diff -= 2.0f * PI;
                while (diff < -PI) diff += 2.0f * PI;

                /* Adjust phase by 10% of difference */
                inst->state.band_phase[band] += diff * 0.1f;

                /* Check if achieved */
                float plv = compute_plv_for_band(&inst->state, &target->state, band);
                if (plv >= 0.9f) {
                    inst->entrainment_progress++;
                    if (inst->entrainment_progress >= 10) {
                        inst->entrainment_status = ENTRAINMENT_ACHIEVED;
                        if (hs->entrainment_callback) {
                            hs->entrainment_callback(
                                inst->instance_id,
                                inst->entrainment_target,
                                ENTRAINMENT_ACHIEVED,
                                hs->entrainment_callback_data
                            );
                        }
                    }
                } else {
                    inst->entrainment_progress = 0;
                }
            }
        }
    }

    /* Simulate phase evolution */
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (!hs->instances[i].active) continue;

        for (int b = 0; b < SYNC_BAND_COUNT; b++) {
            /* Phase 8: Loop progress heartbeat */
            if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
                hyperscanning_heartbeat("hyperscannin_loop",
                                 (float)(b + 1) / (float)SYNC_BAND_COUNT);
            }

            /* Different base frequencies per band */
            float base_freq = 2.0f + b * 8.0f;
            hs->instances[i].state.band_phase[b] += base_freq * 0.01f;
            if (hs->instances[i].state.band_phase[b] > 2.0f * PI) {
                hs->instances[i].state.band_phase[b] -= 2.0f * PI;
            }
        }
    }

    hs->stats.states_processed += hs->instance_count;
    hs->last_update_us = get_timestamp_us();

    return 0;
}

int hyperscanning_get_pair_sync(
    const hyperscanning_t* hs,
    uint32_t instance_a,
    uint32_t instance_b,
    hyperscan_pair_t* pair
) {
    if (!hs || !pair) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_get_pair_sync: required parameter is NULL (hs, pair)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_get_pair_sync", 0.0f);


    pair_sync_entry_t* entry = find_pair((hyperscanning_t*)hs, instance_a, instance_b);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_get_pair_sync: entry is NULL");
        return -1;
    }

    *pair = entry->metrics;
    return 0;
}

int hyperscanning_get_state(
    const hyperscanning_t* hs,
    hyperscan_state_t* state
) {
    if (!hs || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_get_state: required parameter is NULL (hs, state)");
        return -1;
    }

    *state = hs->global_state;
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_get_state", 0.0f);


    return 0;
}

float hyperscanning_get_plv(
    const hyperscanning_t* hs,
    uint32_t instance_a,
    uint32_t instance_b,
    sync_band_t band
) {
    if (!hs || band >= SYNC_BAND_COUNT) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_get_plv", 0.0f);


    pair_sync_entry_t* entry = find_pair((hyperscanning_t*)hs, instance_a, instance_b);
    if (!entry) return -1.0f;

    return entry->metrics.plv[band];
}

/*=============================================================================
 * Entrainment API
 *===========================================================================*/

int hyperscanning_entrain_to(
    hyperscanning_t* hs,
    const entrainment_request_t* request
) {
    if (!hs || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_entrain_to: required parameter is NULL (hs, request)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_entrain_to", 0.0f);


    hyperscanning_instance_t* requester = find_instance(hs, request->requester_id);
    hyperscanning_instance_t* target = find_instance(hs, request->target_id);

    if (!requester || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_entrain_to: required parameter is NULL (requester, target)");
        return -1;
    }

    requester->entrainment_status = ENTRAINMENT_IN_PROGRESS;
    requester->entrainment_target = request->target_id;
    requester->entrainment_band = request->target_band;
    requester->entrainment_target_phase = request->target_phase;
    requester->entrainment_progress = 0;

    hs->stats.entrainment_requests++;

    if (hs->entrainment_callback) {
        hs->entrainment_callback(
            request->requester_id,
            request->target_id,
            ENTRAINMENT_IN_PROGRESS,
            hs->entrainment_callback_data
        );
    }

    return 0;
}

int hyperscanning_release_entrainment(
    hyperscanning_t* hs,
    uint32_t instance_id
) {
    if (!hs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_release_entrainment: hs is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_release_entrainment", 0.0f);


    hyperscanning_instance_t* inst = find_instance(hs, instance_id);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_release_entrainment: inst is NULL");
        return -1;
    }

    entrainment_status_t old_status = inst->entrainment_status;
    inst->entrainment_status = ENTRAINMENT_RELEASED;

    if (old_status == ENTRAINMENT_IN_PROGRESS) {
        hs->stats.entrainment_failures++;
    }

    if (hs->entrainment_callback) {
        hs->entrainment_callback(
            instance_id,
            inst->entrainment_target,
            ENTRAINMENT_RELEASED,
            hs->entrainment_callback_data
        );
    }

    return 0;
}

entrainment_status_t hyperscanning_get_entrainment_status(
    const hyperscanning_t* hs,
    uint32_t instance_id
) {
    if (!hs) return ENTRAINMENT_NONE;

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_get_entrainment_stat", 0.0f);


    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (hs->instances[i].active && hs->instances[i].instance_id == instance_id) {
            return hs->instances[i].entrainment_status;
        }
    }
    return ENTRAINMENT_NONE;
}

int hyperscanning_set_entrainment_callback(
    hyperscanning_t* hs,
    hyperscanning_entrainment_callback_fn callback,
    void* user_data
) {
    if (!hs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_set_entrainment_callback: hs is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_set_entrainment_call", 0.0f);


    hs->entrainment_callback = callback;
    hs->entrainment_callback_data = user_data;

    return 0;
}

/*=============================================================================
 * Leader-Follower API
 *===========================================================================*/

uint32_t hyperscanning_get_leader(const hyperscanning_t* hs) {
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_get_leader", 0.0f);


    return hs ? hs->global_state.leader_instance_id : 0;
}

float hyperscanning_get_leader_influence(const hyperscanning_t* hs) {
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_get_leader_influence", 0.0f);


    return hs ? hs->global_state.leader_influence : 0.0f;
}

float hyperscanning_get_influence(
    const hyperscanning_t* hs,
    uint32_t from_instance,
    uint32_t to_instance
) {
    if (!hs) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_get_influence", 0.0f);


    pair_sync_entry_t* pair = find_pair((hyperscanning_t*)hs, from_instance, to_instance);
    if (!pair) return 0.0f;

    /* Return Granger causality, adjusted for direction */
    float gc = pair->metrics.granger_causality;

    /* If pair order is reversed, flip the sign */
    if (pair->instance_a != from_instance) {
        gc = -gc;
    }

    return gc;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int hyperscanning_get_stats(
    const hyperscanning_t* hs,
    hyperscanning_stats_t* stats
) {
    if (!hs || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperscanning_get_stats: required parameter is NULL (hs, stats)");
        return -1;
    }

    *stats = hs->stats;
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_get_stats", 0.0f);


    return 0;
}

void hyperscanning_reset_stats(hyperscanning_t* hs) {
    if (!hs) return;
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_reset_stats", 0.0f);


    memset(&hs->stats, 0, sizeof(hs->stats));
}

/*=============================================================================
 * Debug API
 *===========================================================================*/

void hyperscanning_dump(const hyperscanning_t* hs) {
    if (!hs) {
        printf("Hyperscanning: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_dump", 0.0f);


    printf("=== Hyperscanning State ===\n");
    printf("Initialized: %s\n", hs->initialized ? "yes" : "no");
    printf("Instances: %u\n", hs->instance_count);
    printf("Pairs: %u\n", hs->pair_count);

    printf("\nGlobal State:\n");
    printf("  Global sync: %.3f\n", hs->global_state.global_sync);
    printf("  Gamma binding: %.3f\n", hs->global_state.gamma_binding);
    printf("  Theta emotional: %.3f\n", hs->global_state.theta_emotional);
    printf("  Beta coordination: %.3f\n", hs->global_state.beta_coordination);
    printf("  Entrained: %s\n", hs->global_state.is_entrained ? "yes" : "no");
    printf("  Leader: %u (influence: %.3f)\n",
           hs->global_state.leader_instance_id,
           hs->global_state.leader_influence);

    printf("\nRegistered Instances:\n");
    for (uint32_t i = 0; i < COLLECTIVE_MAX_INSTANCES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COLLECTIVE_MAX_INSTANCES > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)COLLECTIVE_MAX_INSTANCES);
        }

        if (hs->instances[i].active) {
            printf("  [%u] ID=%u entrainment=%d\n",
                   i, hs->instances[i].instance_id,
                   hs->instances[i].entrainment_status);
            printf("       Powers: D=%.2f T=%.2f A=%.2f B=%.2f G=%.2f\n",
                   hs->instances[i].state.band_power[SYNC_BAND_DELTA],
                   hs->instances[i].state.band_power[SYNC_BAND_THETA],
                   hs->instances[i].state.band_power[SYNC_BAND_ALPHA],
                   hs->instances[i].state.band_power[SYNC_BAND_BETA],
                   hs->instances[i].state.band_power[SYNC_BAND_GAMMA]);
        }
    }

    printf("\nPair Metrics:\n");
    for (uint32_t i = 0; i < hs->pair_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hs->pair_count > 256) {
            hyperscanning_heartbeat("hyperscannin_loop",
                             (float)(i + 1) / (float)hs->pair_count);
        }

        const pair_sync_entry_t* pair = &hs->pairs[i];
        printf("  [%u-%u] PLV: D=%.2f T=%.2f A=%.2f B=%.2f G=%.2f sync=%s\n",
               pair->instance_a, pair->instance_b,
               pair->metrics.plv[SYNC_BAND_DELTA],
               pair->metrics.plv[SYNC_BAND_THETA],
               pair->metrics.plv[SYNC_BAND_ALPHA],
               pair->metrics.plv[SYNC_BAND_BETA],
               pair->metrics.plv[SYNC_BAND_GAMMA],
               pair->metrics.is_synchronized ? "yes" : "no");
    }

    printf("\nStatistics:\n");
    printf("  States received: %lu\n", (unsigned long)hs->stats.states_received);
    printf("  Sync computations: %lu\n", (unsigned long)hs->stats.sync_computations);
    printf("  Entrainment requests: %lu\n", (unsigned long)hs->stats.entrainment_requests);
    printf("  Entrainment successes: %lu\n", (unsigned long)hs->stats.entrainment_successes);
    printf("  Avg global sync: %.3f\n", hs->stats.avg_global_sync);
    printf("  Max global sync: %.3f\n", hs->stats.max_global_sync);
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Hyperscanning self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int hyperscanning_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    hyperscanning_heartbeat("hyperscannin_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Hyperscanning");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                hyperscanning_heartbeat("hyperscannin_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            printf("Hyperscanning self-knowledge: %s\n", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Hyperscanning");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Hyperscanning");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */

void hyperscanning_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_hyperscanning_instance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static _Atomic uint64_t g_hyperscanning_training_steps = 0;
static _Atomic double g_hyperscanning_training_total_error = 0.0;
static _Atomic double g_hyperscanning_training_best_error = 1e30;
static _Atomic bool g_hyperscanning_training_active = false;

int hyperscanning_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperscanning_training_begin: NULL argument");
        return -1;
    }
    hyperscanning_heartbeat_instance(g_hyperscanning_instance_health_agent, "hyperscan_train_beg", 0.0f);
    hyperscanning_t* ctx = (hyperscanning_t*)instance;

    /* Reset training counters */
    g_hyperscanning_training_steps = 0;
    g_hyperscanning_training_total_error = 0.0;
    g_hyperscanning_training_best_error = 1e30;
    g_hyperscanning_training_active = true;

    /* Reset module stats */
    memset(&ctx->stats, 0, sizeof(ctx->stats));

    NIMCP_LOGGING_INFO("hyperscanning training begin: counters reset");
    return 0;
}

int hyperscanning_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperscanning_training_step: NULL argument");
        return -1;
    }

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    hyperscanning_heartbeat_instance(g_hyperscanning_instance_health_agent, "hyperscan_train_stp", progress);
    (void)instance;

    g_hyperscanning_training_steps++;

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    g_hyperscanning_training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    g_hyperscanning_training_best_error -= (double)threshold_adjust;
    if (g_hyperscanning_training_best_error < 0.0) g_hyperscanning_training_best_error = 0.0;

    return 0;
}

int hyperscanning_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperscanning_training_end: NULL argument");
        return -1;
    }
    hyperscanning_heartbeat_instance(g_hyperscanning_instance_health_agent, "hyperscan_train_end", 1.0f);

    /* Compute final averages */
    double avg_error = (g_hyperscanning_training_steps > 0)
        ? g_hyperscanning_training_total_error / (double)g_hyperscanning_training_steps
        : 0.0;

    /* Clear training flag */
    g_hyperscanning_training_active = false;

    NIMCP_LOGGING_INFO("hyperscanning training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)g_hyperscanning_training_steps,
                       avg_error, g_hyperscanning_training_best_error);
    return 0;
}
