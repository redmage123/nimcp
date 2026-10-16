//=============================================================================
// nimcp_bg_temporal_credit.c - Temporal Credit Assignment
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_temporal_credit.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bg_temporal_credit, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct bg_temporal_credit {
    /* Configuration */
    bgtc_config_t config;

    /* Eligibility traces */
    bgtc_trace_entry_t* traces;
    uint32_t num_traces;
    uint32_t max_traces;

    /* N-step buffer */
    bgtc_nstep_buffer_t nstep_buffer;

    /* Timing cells */
    bgtc_timing_cell_t* timing_cells;
    uint32_t num_timing_cells;
    bool timing_active;
    float elapsed_time;

    /* Successor representation */
    bgtc_successor_rep_t sr;
    bool sr_enabled;

    /* Current state */
    uint64_t current_timestamp;
    float last_td_error;

    /* Statistics */
    bgtc_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

void bgtc_default_config(bgtc_config_t* config) {
    if (!config) return;

    config->method = BGTC_METHOD_TD_LAMBDA;
    config->trace_type = BGTC_TRACE_REPLACING;

    config->lambda = BGTC_DEFAULT_LAMBDA;
    config->gamma = BGTC_DEFAULT_GAMMA;
    config->n_steps = 5;

    config->trace_threshold = 0.01f;
    config->max_traces = 256;

    config->enable_timing_cells = true;
    config->num_timing_cells = BGTC_NUM_TIMING_CELLS;
    config->timing_precision = 0.1f;

    config->enable_successor_rep = false;
    config->sr_learning_rate = 0.1f;
}

bg_temporal_credit_t* bgtc_create(const bgtc_config_t* config) {
    bg_temporal_credit_t* tc = nimcp_calloc(1, sizeof(bg_temporal_credit_t));
    if (!tc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tc is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        tc->config = *config;
    } else {
        bgtc_default_config(&tc->config);
    }

    /* Allocate eligibility traces */
    tc->max_traces = tc->config.max_traces;
    tc->traces = nimcp_calloc(tc->max_traces, sizeof(bgtc_trace_entry_t));
    if (!tc->traces) goto cleanup;

    /* Allocate N-step buffer */
    tc->nstep_buffer.capacity = tc->config.n_steps * 2;
    tc->nstep_buffer.buffer = nimcp_calloc(tc->nstep_buffer.capacity, sizeof(bgtc_experience_t));
    if (!tc->nstep_buffer.buffer) goto cleanup;
    tc->nstep_buffer.n_steps = tc->config.n_steps;

    /* Allocate timing cells */
    if (tc->config.enable_timing_cells) {
        tc->num_timing_cells = tc->config.num_timing_cells;
        tc->timing_cells = nimcp_calloc(tc->num_timing_cells, sizeof(bgtc_timing_cell_t));
        if (!tc->timing_cells) goto cleanup;

        /* Initialize timing cells with different preferred intervals */
        for (uint32_t i = 0; i < tc->num_timing_cells; i++) {
            tc->timing_cells[i].type = i % BGTC_TIMING_COUNT;
            /* Logarithmically spaced preferred intervals */
            float t = (float)i / (float)(tc->num_timing_cells - 1);
            tc->timing_cells[i].preferred_interval =
                100.0f * powf(BGTC_MAX_INTERVAL_MS / 100.0f, t);
            tc->timing_cells[i].precision = tc->config.timing_precision;
            tc->timing_cells[i].current_activity = 0.0f;
        }
    }

    /* Allocate successor representation if enabled */
    if (tc->config.enable_successor_rep) {
        uint32_t ns = BGTC_MAX_STATES;
        tc->sr.num_states = ns;
        tc->sr.sr_learning_rate = tc->config.sr_learning_rate;
        tc->sr.sr_discount = tc->config.gamma;

        tc->sr.sr_matrix = nimcp_calloc(ns, sizeof(float*));
        if (!tc->sr.sr_matrix) goto cleanup;

        for (uint32_t s = 0; s < ns; s++) {
            tc->sr.sr_matrix[s] = nimcp_calloc(ns, sizeof(float));
            if (!tc->sr.sr_matrix[s]) goto cleanup;
            /* Initialize as identity (state predicts itself) */
            tc->sr.sr_matrix[s][s] = 1.0f;
        }

        tc->sr_enabled = true;
    }

    /* Create mutex */
    tc->mutex = nimcp_mutex_create(NULL);
    if (!tc->mutex) goto cleanup;

    return tc;

cleanup:
    bgtc_destroy(tc);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_create: operation failed");
    return NULL;
}

void bgtc_destroy(bg_temporal_credit_t* tc) {
    if (!tc) return;

    if (tc->mutex) {
        nimcp_mutex_free(tc->mutex);
    }

    nimcp_free(tc->traces);
    nimcp_free(tc->nstep_buffer.buffer);
    nimcp_free(tc->timing_cells);

    if (tc->sr.sr_matrix) {
        for (uint32_t s = 0; s < tc->sr.num_states; s++) {
            nimcp_free(tc->sr.sr_matrix[s]);
        }
        nimcp_free(tc->sr.sr_matrix);
    }

    nimcp_free(tc);
}

int bgtc_reset(bg_temporal_credit_t* tc) {
    if (!tc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_reset: tc is NULL");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);

    /* Clear traces */
    tc->num_traces = 0;
    memset(tc->traces, 0, tc->max_traces * sizeof(bgtc_trace_entry_t));

    /* Clear N-step buffer */
    tc->nstep_buffer.head = 0;
    tc->nstep_buffer.tail = 0;
    tc->nstep_buffer.count = 0;

    /* Reset timing cells */
    if (tc->timing_cells) {
        for (uint32_t i = 0; i < tc->num_timing_cells; i++) {
            tc->timing_cells[i].current_activity = 0.0f;
            tc->timing_cells[i].time_since_start = 0.0f;
        }
    }
    tc->timing_active = false;
    tc->elapsed_time = 0.0f;

    /* Reset successor representation */
    if (tc->sr_enabled) {
        for (uint32_t s = 0; s < tc->sr.num_states; s++) {
            memset(tc->sr.sr_matrix[s], 0, tc->sr.num_states * sizeof(float));
            tc->sr.sr_matrix[s][s] = 1.0f;
        }
    }

    /* Reset state */
    tc->current_timestamp = 0;
    tc->last_td_error = 0.0f;

    /* Reset statistics */
    memset(&tc->stats, 0, sizeof(bgtc_stats_t));

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

/* ============================================================================
 * TRACE MANAGEMENT IMPLEMENTATION
 * ============================================================================ */

int bgtc_update_trace(bg_temporal_credit_t* tc,
                       uint32_t state,
                       uint32_t action) {
    if (!tc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_reset: tc is NULL");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);

    /* Find existing trace */
    int found_idx = -1;
    for (uint32_t i = 0; i < tc->num_traces; i++) {
        if (tc->traces[i].state == state && tc->traces[i].action == action) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx >= 0) {
        /* Update existing trace based on type */
        switch (tc->config.trace_type) {
            case BGTC_TRACE_ACCUMULATING:
                tc->traces[found_idx].trace += 1.0f;
                break;
            case BGTC_TRACE_REPLACING:
                tc->traces[found_idx].trace = 1.0f;
                break;
            case BGTC_TRACE_DUTCH:
                tc->traces[found_idx].trace =
                    tc->config.lambda * tc->traces[found_idx].trace + 1.0f;
                break;
            default:
                break;
        }
        tc->traces[found_idx].timestamp = tc->current_timestamp;
    } else if (tc->num_traces < tc->max_traces) {
        /* Add new trace */
        tc->traces[tc->num_traces].state = state;
        tc->traces[tc->num_traces].action = action;
        tc->traces[tc->num_traces].trace = 1.0f;
        tc->traces[tc->num_traces].timestamp = tc->current_timestamp;
        tc->num_traces++;
    }

    tc->stats.active_traces = tc->num_traces;

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

/* Internal unlocked version - caller must hold mutex */
static void bgtc_decay_traces_unlocked(bg_temporal_credit_t* tc) {
    float decay = tc->config.gamma * tc->config.lambda;
    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < tc->num_traces; i++) {
        tc->traces[i].trace *= decay;

        /* Keep trace if above threshold */
        if (tc->traces[i].trace >= tc->config.trace_threshold) {
            if (write_idx != i) {
                tc->traces[write_idx] = tc->traces[i];
            }
            write_idx++;
        }
    }

    tc->num_traces = write_idx;
    tc->stats.active_traces = tc->num_traces;

    /* Compute average trace value */
    float sum = 0.0f;
    for (uint32_t i = 0; i < tc->num_traces; i++) {
        sum += tc->traces[i].trace;
    }
    tc->stats.avg_trace_value = (tc->num_traces > 0) ? sum / tc->num_traces : 0.0f;
}

int bgtc_decay_traces(bg_temporal_credit_t* tc) {
    if (!tc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_decay_traces: tc is NULL");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);
    bgtc_decay_traces_unlocked(tc);
    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

float bgtc_get_trace(const bg_temporal_credit_t* tc,
                      uint32_t state,
                      uint32_t action) {
    if (!tc) return 0.0f;

    for (uint32_t i = 0; i < tc->num_traces; i++) {
        if (tc->traces[i].state == state && tc->traces[i].action == action) {
            return tc->traces[i].trace;
        }
    }
    return 0.0f;
}

int bgtc_clear_traces(bg_temporal_credit_t* tc) {
    if (!tc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_clear_traces: tc is NULL");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);
    tc->num_traces = 0;
    tc->stats.active_traces = 0;
    tc->stats.avg_trace_value = 0.0f;
    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

int bgtc_get_active_traces(const bg_temporal_credit_t* tc,
                            bgtc_trace_entry_t* traces,
                            uint32_t* count) {
    if (!tc || !traces || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_clear_traces: required parameter is NULL (tc, traces, count)");
        return -1;
    }

    uint32_t n = (*count < tc->num_traces) ? *count : tc->num_traces;
    memcpy(traces, tc->traces, n * sizeof(bgtc_trace_entry_t));
    *count = n;
    return 0;
}

/* ============================================================================
 * CREDIT ASSIGNMENT IMPLEMENTATION
 * ============================================================================ */

float bgtc_compute_td_error(bg_temporal_credit_t* tc,
                             float reward,
                             float current_value,
                             float next_value,
                             bool terminal) {
    if (!tc) return 0.0f;

    float gamma = terminal ? 0.0f : tc->config.gamma;
    float td_error = reward + gamma * next_value - current_value;

    tc->last_td_error = td_error;
    tc->stats.avg_td_error = tc->stats.avg_td_error * 0.95f + fabsf(td_error) * 0.05f;

    return td_error;
}

int bgtc_apply_credit(bg_temporal_credit_t* tc,
                       float td_error,
                       float learning_rate,
                       float* value_updates,
                       uint32_t* num_updates) {
    if (!tc || !value_updates || !num_updates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_clear_traces: required parameter is NULL (tc, value_updates, num_updates)");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);

    uint32_t update_count = 0;

    for (uint32_t i = 0; i < tc->num_traces && update_count < *num_updates; i++) {
        float update = learning_rate * td_error * tc->traces[i].trace;
        value_updates[update_count] = update;
        update_count++;
    }

    *num_updates = update_count;
    tc->stats.updates += update_count;

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

int bgtc_store_experience(bg_temporal_credit_t* tc,
                           const bgtc_experience_t* exp) {
    if (!tc || !exp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_clear_traces: required parameter is NULL (tc, exp)");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);

    bgtc_nstep_buffer_t* buf = &tc->nstep_buffer;

    buf->buffer[buf->head] = *exp;
    buf->head = (buf->head + 1) % buf->capacity;

    if (buf->count < buf->capacity) {
        buf->count++;
    } else {
        buf->tail = (buf->tail + 1) % buf->capacity;
    }

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

float bgtc_get_nstep_return(const bg_temporal_credit_t* tc,
                             uint32_t steps_back) {
    if (!tc) return 0.0f;

    const bgtc_nstep_buffer_t* buf = &tc->nstep_buffer;
    if (steps_back >= buf->count) return 0.0f;

    float G = 0.0f;
    float gamma = tc->config.gamma;
    float discount = 1.0f;

    for (uint32_t i = 0; i < steps_back && i < buf->count; i++) {
        uint32_t idx = (buf->head + buf->capacity - 1 - i) % buf->capacity;
        G += discount * buf->buffer[idx].reward;
        discount *= gamma;
    }

    return G;
}

int bgtc_compute_gae(bg_temporal_credit_t* tc,
                      const float* values,
                      const float* rewards,
                      uint32_t length,
                      float* advantages) {
    if (!tc || !values || !rewards || !advantages) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_clear_traces: required parameter is NULL (tc, values, rewards, advantages)");
        return -1;
    }
    if (length == 0) return 0;

    float gamma = tc->config.gamma;
    float lambda = tc->config.lambda;
    float gae = 0.0f;

    /* Compute GAE backwards */
    for (int i = (int)length - 1; i >= 0; i--) {
        float next_value = (i < (int)length - 1) ? values[i + 1] : 0.0f;
        float delta = rewards[i] + gamma * next_value - values[i];
        gae = delta + gamma * lambda * gae;
        advantages[i] = gae;
    }

    return 0;
}

/* ============================================================================
 * TIMING IMPLEMENTATION
 * ============================================================================ */

int bgtc_start_timing(bg_temporal_credit_t* tc) {
    if (!tc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_start_timing: tc is NULL");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);

    tc->timing_active = true;
    tc->elapsed_time = 0.0f;

    /* Reset timing cells */
    for (uint32_t i = 0; i < tc->num_timing_cells; i++) {
        tc->timing_cells[i].time_since_start = 0.0f;
        tc->timing_cells[i].current_activity = 0.0f;
    }

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

/* Internal unlocked version - caller must hold mutex */
static void bgtc_update_timing_unlocked(bg_temporal_credit_t* tc, float dt_ms) {
    tc->elapsed_time += dt_ms;

    /* Update each timing cell */
    for (uint32_t i = 0; i < tc->num_timing_cells; i++) {
        bgtc_timing_cell_t* cell = &tc->timing_cells[i];
        cell->time_since_start = tc->elapsed_time;

        float pref = cell->preferred_interval;
        float t = tc->elapsed_time;

        switch (cell->type) {
            case BGTC_TIMING_RAMPING:
                /* Ramps up to preferred time */
                cell->current_activity = nimcp_clampf(t / pref, 0.0f, 1.0f);
                break;

            case BGTC_TIMING_PEAKING:
                /* Peaks at preferred time (Gaussian-like) */
                {
                    float diff = (t - pref) / (pref * cell->precision);
                    cell->current_activity = expf(-0.5f * diff * diff);
                }
                break;

            case BGTC_TIMING_DECAYING:
                /* Exponential decay from start */
                cell->current_activity = expf(-t / pref);
                break;

            default:
                break;
        }
    }
}

int bgtc_update_timing(bg_temporal_credit_t* tc, float dt_ms) {
    if (!tc || !tc->timing_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_update_timing: required parameter is NULL (tc, tc->timing_active)");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);
    bgtc_update_timing_unlocked(tc, dt_ms);
    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

int bgtc_get_timing_activities(const bg_temporal_credit_t* tc,
                                float* activities,
                                uint32_t* count) {
    if (!tc || !activities || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_update_timing: required parameter is NULL (tc, activities, count)");
        return -1;
    }

    uint32_t n = (*count < tc->num_timing_cells) ? *count : tc->num_timing_cells;
    for (uint32_t i = 0; i < n; i++) {
        activities[i] = tc->timing_cells[i].current_activity;
    }
    *count = n;
    return 0;
}

float bgtc_estimate_elapsed_time(const bg_temporal_credit_t* tc) {
    if (!tc || !tc->timing_active || tc->num_timing_cells == 0) return 0.0f;

    /* Population vector estimate */
    float weighted_sum = 0.0f;
    float weight_total = 0.0f;

    for (uint32_t i = 0; i < tc->num_timing_cells; i++) {
        float w = tc->timing_cells[i].current_activity;
        weighted_sum += w * tc->timing_cells[i].preferred_interval;
        weight_total += w;
    }

    return (weight_total > 0.001f) ? weighted_sum / weight_total : tc->elapsed_time;
}

int bgtc_learn_interval(bg_temporal_credit_t* tc,
                         float actual_interval_ms) {
    if (!tc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_estimate_elapsed_time: tc is NULL");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);

    /* Adjust timing cell preferences based on prediction error */
    float estimated = bgtc_estimate_elapsed_time(tc);
    float error = actual_interval_ms - estimated;
    float lr = NIMCP_LEARNING_RATE_DEFAULT;

    for (uint32_t i = 0; i < tc->num_timing_cells; i++) {
        if (tc->timing_cells[i].current_activity > 0.1f) {
            /* Shift preferred interval toward actual */
            tc->timing_cells[i].preferred_interval +=
                lr * error * tc->timing_cells[i].current_activity;
            tc->timing_cells[i].preferred_interval =
                nimcp_clampf(tc->timing_cells[i].preferred_interval, 10.0f, BGTC_MAX_INTERVAL_MS);
        }
    }

    /* Update timing accuracy statistic */
    float accuracy = 1.0f - fabsf(error) / (actual_interval_ms + 1.0f);
    tc->stats.timing_accuracy = tc->stats.timing_accuracy * 0.9f + accuracy * 0.1f;

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

/* ============================================================================
 * SUCCESSOR REPRESENTATION IMPLEMENTATION
 * ============================================================================ */

int bgtc_update_successor(bg_temporal_credit_t* tc,
                           uint32_t state,
                           uint32_t next_state) {
    if (!tc || !tc->sr_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_estimate_elapsed_time: required parameter is NULL (tc, tc->sr_enabled)");
        return -1;
    }
    if (state >= tc->sr.num_states || next_state >= tc->sr.num_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtc_estimate_elapsed_time: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);

    float lr = tc->sr.sr_learning_rate;
    float gamma = tc->sr.sr_discount;

    /* SR update: M(s) = e_s + gamma * M(s') */
    for (uint32_t sp = 0; sp < tc->sr.num_states; sp++) {
        float target = (sp == state ? 1.0f : 0.0f) + gamma * tc->sr.sr_matrix[next_state][sp];
        tc->sr.sr_matrix[state][sp] += lr * (target - tc->sr.sr_matrix[state][sp]);
    }

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

int bgtc_get_successor_features(const bg_temporal_credit_t* tc,
                                 uint32_t state,
                                 float* features) {
    if (!tc || !tc->sr_enabled || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_estimate_elapsed_time: required parameter is NULL (tc, tc->sr_enabled, features)");
        return -1;
    }
    if (state >= tc->sr.num_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtc_estimate_elapsed_time: capacity exceeded");
        return -1;
    }

    memcpy(features, tc->sr.sr_matrix[state], tc->sr.num_states * sizeof(float));
    return 0;
}

float bgtc_successor_value(const bg_temporal_credit_t* tc,
                            uint32_t state,
                            const float* reward_weights) {
    if (!tc || !tc->sr_enabled || !reward_weights) return 0.0f;
    if (state >= tc->sr.num_states) return 0.0f;

    /* V(s) = M(s) . w */
    float value = 0.0f;
    for (uint32_t i = 0; i < tc->sr.num_states; i++) {
        value += tc->sr.sr_matrix[state][i] * reward_weights[i];
    }
    return value;
}

/* ============================================================================
 * PROCESSING IMPLEMENTATION
 * ============================================================================ */

int bgtc_step(bg_temporal_credit_t* tc, float dt_ms) {
    if (!tc || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgtc_step: tc is NULL");
        return -1;
    }

    nimcp_mutex_lock(tc->mutex);

    tc->current_timestamp++;

    /* Decay traces (use unlocked version since we hold mutex) */
    bgtc_decay_traces_unlocked(tc);

    /* Update timing if active (use unlocked version since we hold mutex) */
    if (tc->timing_active) {
        bgtc_update_timing_unlocked(tc, dt_ms);
    }

    /* Compute effective horizon */
    float eff_horizon = 0.0f;
    if (tc->config.lambda > 0.0f && tc->config.gamma > 0.0f) {
        eff_horizon = 1.0f / (1.0f - tc->config.gamma * tc->config.lambda);
    }
    tc->stats.effective_horizon = eff_horizon;

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

int bgtc_process_reward(bg_temporal_credit_t* tc,
                         float reward,
                         uint32_t delay_steps) {
    if (!tc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_step: tc is NULL");
        return -1;
    }
    (void)delay_steps; /* Used for timing, simplified here */

    nimcp_mutex_lock(tc->mutex);

    /* Store as experience */
    bgtc_experience_t exp;
    memset(&exp, 0, sizeof(exp));
    exp.reward = reward;
    exp.timestamp = tc->current_timestamp;

    bgtc_store_experience(tc, &exp);

    nimcp_mutex_unlock(tc->mutex);
    return 0;
}

float bgtc_get_effective_discount(const bg_temporal_credit_t* tc,
                                   uint32_t steps) {
    if (!tc) return 0.0f;
    return powf(tc->config.gamma, (float)steps);
}

int bgtc_get_stats(const bg_temporal_credit_t* tc, bgtc_stats_t* stats) {
    if (!tc || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgtc_get_stats: required parameter is NULL (tc, stats)");
        return -1;
    }
    *stats = tc->stats;
    return 0;
}
