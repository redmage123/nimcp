/**
 * @file nimcp_dendritic_sequence.c
 * @brief HTM-inspired Dendritic Sequence Prediction — Implementation
 *
 * WHAT: Distal dendritic segments learn temporal sequences via permanence-based
 *       Hebbian learning. Predicted cells burst (BAC), unpredicted cells spike.
 * WHY:  Temporal prediction gives sequences processing advantage and enables
 *       the brain to anticipate upcoming inputs.
 * HOW:  Each cell has distal segments connecting to other cells. When enough
 *       presynaptic cells are active, the segment activates → cell is predicted.
 *       Learning strengthens correct predictions and creates new segments for
 *       surprising (unpredicted) activations.
 */

#include "core/cortical_columns/nimcp_dendritic_sequence.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

/* =========================================================================
 * Configuration
 * ========================================================================= */

void dendritic_seq_config_default(dendritic_seq_config_t* config) {
    if (!config) return;
    config->num_cells = DENDRITE_SEQ_DEFAULT_CELLS;
    config->cells_per_column = DENDRITE_SEQ_DEFAULT_CPC;
    config->permanence_increment = 0.1f;
    config->permanence_decrement = 0.02f;
    config->initial_permanence = 0.21f;
    config->activation_threshold = 0.5f;
    config->permanence_threshold = 0.5f;
    config->predicted_cell_boost = 0.8f;
    config->latency_advantage_ms = 5.0f;
    config->max_segments_per_cell = DENDRITE_SEQ_MAX_SEGMENTS;
    config->max_synapses_per_segment = DENDRITE_SEQ_SEGMENT_SIZE;
}

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

dendritic_sequence_mgr_t* dendritic_seq_create(const dendritic_seq_config_t* config) {
    if (!config) return NULL;
    if (config->num_cells == 0 || config->cells_per_column == 0) return NULL;

    dendritic_sequence_mgr_t* mgr = nimcp_calloc(1, sizeof(dendritic_sequence_mgr_t));
    if (!mgr) return NULL;

    mgr->num_cells = config->num_cells;
    mgr->cells_per_column = config->cells_per_column;
    mgr->num_columns = config->num_cells / config->cells_per_column;
    if (mgr->num_columns == 0) mgr->num_columns = 1;

    /* Allocate cells */
    mgr->cells = nimcp_calloc(mgr->num_cells, sizeof(cell_predictive_state_t));
    if (!mgr->cells) { nimcp_free(mgr); return NULL; }

    for (uint32_t i = 0; i < mgr->num_cells; i++) {
        mgr->cells[i].cell_id = i;
        /* Set default thresholds on all potential segments */
        for (uint32_t s = 0; s < DENDRITE_SEQ_MAX_SEGMENTS; s++) {
            mgr->cells[i].segments[s].activation_threshold = config->activation_threshold;
            mgr->cells[i].segments[s].permanence_threshold = config->permanence_threshold;
        }
    }

    /* Allocate active/winner buffers */
    uint32_t buf_cap = mgr->num_cells;
    mgr->prev_active_cells = nimcp_calloc(buf_cap, sizeof(uint32_t));
    mgr->prev_winner_cells = nimcp_calloc(buf_cap, sizeof(uint32_t));
    mgr->cur_active_cells = nimcp_calloc(buf_cap, sizeof(uint32_t));
    mgr->cur_winner_cells = nimcp_calloc(buf_cap, sizeof(uint32_t));
    mgr->prev_active_capacity = buf_cap;

    if (!mgr->prev_active_cells || !mgr->prev_winner_cells ||
        !mgr->cur_active_cells || !mgr->cur_winner_cells) {
        dendritic_seq_destroy(mgr);
        return NULL;
    }

    /* Learning params */
    mgr->permanence_increment = config->permanence_increment;
    mgr->permanence_decrement = config->permanence_decrement;
    mgr->initial_permanence = config->initial_permanence;
    mgr->predicted_cell_boost = config->predicted_cell_boost;
    mgr->latency_advantage_ms = config->latency_advantage_ms;

    /* Mutex */
    mgr->mutex = nimcp_mutex_create(NULL);

    NIMCP_LOGGING_INFO("dendritic_seq: created %u cells, %u columns (%u cells/col)",
                       mgr->num_cells, mgr->num_columns, mgr->cells_per_column);
    return mgr;
}

void dendritic_seq_destroy(dendritic_sequence_mgr_t* mgr) {
    if (!mgr) return;
    nimcp_free(mgr->cells);
    nimcp_free(mgr->prev_active_cells);
    nimcp_free(mgr->prev_winner_cells);
    nimcp_free(mgr->cur_active_cells);
    nimcp_free(mgr->cur_winner_cells);
    if (mgr->mutex) nimcp_mutex_free(mgr->mutex);
    nimcp_free(mgr);
}

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/**
 * @brief Check if a segment is active (enough connected presynaptic cells are active).
 */
static float segment_compute_activation(const predictive_segment_t* seg,
                                         const uint32_t* active_cells,
                                         uint32_t num_active) {
    if (!seg || seg->num_synapses == 0) return 0.0f;

    uint32_t connected_active = 0;
    for (uint32_t s = 0; s < seg->num_synapses; s++) {
        if (seg->permanences[s] < seg->permanence_threshold) continue;
        /* Check if presynaptic cell is in active set */
        for (uint32_t a = 0; a < num_active; a++) {
            if (seg->presynaptic_cells[s] == active_cells[a]) {
                connected_active++;
                break;
            }
        }
    }

    return (float)connected_active / (float)seg->num_synapses;
}

/**
 * @brief Find the best matching segment for a cell given active cells.
 * Returns segment index or -1 if none above threshold.
 */
static int find_best_segment(const cell_predictive_state_t* cell,
                              const uint32_t* active_cells,
                              uint32_t num_active) {
    int best = -1;
    float best_activation = 0.0f;

    for (uint32_t s = 0; s < cell->num_segments; s++) {
        float act = segment_compute_activation(&cell->segments[s],
                                                active_cells, num_active);
        if (act > best_activation && act >= cell->segments[s].activation_threshold) {
            best_activation = act;
            best = (int)s;
        }
    }
    return best;
}

/**
 * @brief Create a new segment on a cell, connecting to previous winner cells.
 */
static void create_new_segment(dendritic_sequence_mgr_t* mgr,
                                cell_predictive_state_t* cell) {
    if (cell->num_segments >= DENDRITE_SEQ_MAX_SEGMENTS) return;
    if (mgr->num_prev_winners == 0) return;

    predictive_segment_t* seg = &cell->segments[cell->num_segments];
    memset(seg, 0, sizeof(predictive_segment_t));
    seg->activation_threshold = 0.5f;
    seg->permanence_threshold = 0.5f;

    /* Sample from previous winner cells */
    uint32_t max_syn = (mgr->num_prev_winners < DENDRITE_SEQ_SEGMENT_SIZE)
                     ? mgr->num_prev_winners : DENDRITE_SEQ_SEGMENT_SIZE;
    for (uint32_t i = 0; i < max_syn; i++) {
        uint32_t idx = (mgr->num_prev_winners <= DENDRITE_SEQ_SEGMENT_SIZE)
                     ? i : (uint32_t)(rand() % mgr->num_prev_winners);
        seg->presynaptic_cells[i] = mgr->prev_winner_cells[idx];
        seg->permanences[i] = mgr->initial_permanence;
    }
    seg->num_synapses = max_syn;

    cell->num_segments++;
    mgr->stats.total_segments_created++;
}

/* =========================================================================
 * Core API
 * ========================================================================= */

int dendritic_seq_activate_columns(dendritic_sequence_mgr_t* mgr,
                                   const uint32_t* active_columns,
                                   uint32_t num_active) {
    if (!mgr || !active_columns) return -1;

    mgr->num_cur_active = 0;
    mgr->num_cur_winners = 0;

    for (uint32_t c = 0; c < num_active; c++) {
        uint32_t col_id = active_columns[c];
        if (col_id >= mgr->num_columns) continue;

        uint32_t cell_start = col_id * mgr->cells_per_column;
        uint32_t cell_end = cell_start + mgr->cells_per_column;
        if (cell_end > mgr->num_cells) cell_end = mgr->num_cells;

        /* Find predicted cell in this column */
        int predicted_idx = -1;
        float best_strength = 0.0f;
        for (uint32_t i = cell_start; i < cell_end; i++) {
            if (mgr->cells[i].is_predicted &&
                mgr->cells[i].prediction_strength > best_strength) {
                predicted_idx = (int)i;
                best_strength = mgr->cells[i].prediction_strength;
            }
        }

        if (predicted_idx >= 0) {
            /* Predicted cell wins — this is a correct prediction */
            if (mgr->num_cur_active < mgr->prev_active_capacity) {
                mgr->cur_active_cells[mgr->num_cur_active++] = (uint32_t)predicted_idx;
                mgr->cur_winner_cells[mgr->num_cur_winners++] = (uint32_t)predicted_idx;
            }
            mgr->cells[predicted_idx].last_active_time_us = 0; /* TODO: real time */
            mgr->stats.correct_predictions++;
        } else {
            /* No prediction — BURST: all cells in column activate (surprise) */
            for (uint32_t i = cell_start; i < cell_end; i++) {
                if (mgr->num_cur_active < mgr->prev_active_capacity) {
                    mgr->cur_active_cells[mgr->num_cur_active++] = i;
                }
            }
            /* Pick one winner for learning (least used = fewest segments) */
            uint32_t winner = cell_start;
            uint32_t min_segs = mgr->cells[cell_start].num_segments;
            for (uint32_t i = cell_start + 1; i < cell_end; i++) {
                if (mgr->cells[i].num_segments < min_segs) {
                    min_segs = mgr->cells[i].num_segments;
                    winner = i;
                }
            }
            if (mgr->num_cur_winners < mgr->prev_active_capacity) {
                mgr->cur_winner_cells[mgr->num_cur_winners++] = winner;
            }
            mgr->stats.total_bursts++;
        }
    }

    mgr->stats.total_predictions += num_active;
    return 0;
}

int dendritic_seq_compute_predictions(dendritic_sequence_mgr_t* mgr) {
    if (!mgr) return -1;

    /* Clear all predictions */
    for (uint32_t i = 0; i < mgr->num_cells; i++) {
        mgr->cells[i].is_predicted = false;
        mgr->cells[i].prediction_strength = 0.0f;
        for (uint32_t s = 0; s < mgr->cells[i].num_segments; s++) {
            mgr->cells[i].segments[s].is_active = false;
        }
    }

    /* Check each cell's segments against current active cells */
    for (uint32_t i = 0; i < mgr->num_cells; i++) {
        cell_predictive_state_t* cell = &mgr->cells[i];
        for (uint32_t s = 0; s < cell->num_segments; s++) {
            float act = segment_compute_activation(&cell->segments[s],
                                                    mgr->cur_active_cells,
                                                    mgr->num_cur_active);
            if (act >= cell->segments[s].activation_threshold) {
                cell->segments[s].is_active = true;
                cell->is_predicted = true;
                if (act > cell->prediction_strength) {
                    cell->prediction_strength = act;
                }
            }
        }
    }

    return 0;
}

int dendritic_seq_learn(dendritic_sequence_mgr_t* mgr) {
    if (!mgr) return -1;
    if (mgr->num_prev_active == 0) return 0; /* No previous state to learn from */

    /* For each current winner cell */
    for (uint32_t w = 0; w < mgr->num_cur_winners; w++) {
        uint32_t cell_id = mgr->cur_winner_cells[w];
        if (cell_id >= mgr->num_cells) continue;
        cell_predictive_state_t* cell = &mgr->cells[cell_id];

        /* Find segment that predicted this activation */
        int seg_idx = find_best_segment(cell, mgr->prev_active_cells,
                                         mgr->num_prev_active);

        if (seg_idx >= 0) {
            /* Strengthen correct prediction */
            predictive_segment_t* seg = &cell->segments[seg_idx];
            for (uint32_t s = 0; s < seg->num_synapses; s++) {
                /* Check if presynaptic cell was active */
                bool was_active = false;
                for (uint32_t a = 0; a < mgr->num_prev_active; a++) {
                    if (seg->presynaptic_cells[s] == mgr->prev_active_cells[a]) {
                        was_active = true;
                        break;
                    }
                }
                if (was_active) {
                    seg->permanences[s] += mgr->permanence_increment;
                    if (seg->permanences[s] > 1.0f) seg->permanences[s] = 1.0f;
                } else {
                    seg->permanences[s] -= mgr->permanence_decrement;
                    if (seg->permanences[s] < 0.0f) seg->permanences[s] = 0.0f;
                }
            }
        } else {
            /* No matching segment — create new one (surprise learning) */
            create_new_segment(mgr, cell);
        }
    }

    /* Punish segments that predicted incorrectly (predicted but cell not activated) */
    for (uint32_t i = 0; i < mgr->num_cells; i++) {
        if (!mgr->cells[i].is_predicted) continue;
        /* Check if this cell was actually activated */
        bool was_activated = false;
        for (uint32_t a = 0; a < mgr->num_cur_active; a++) {
            if (mgr->cur_active_cells[a] == i) { was_activated = true; break; }
        }
        if (!was_activated) {
            /* Wrong prediction — decrement permanences on active segments */
            for (uint32_t s = 0; s < mgr->cells[i].num_segments; s++) {
                if (!mgr->cells[i].segments[s].is_active) continue;
                predictive_segment_t* seg = &mgr->cells[i].segments[s];
                for (uint32_t p = 0; p < seg->num_synapses; p++) {
                    seg->permanences[p] -= mgr->permanence_decrement;
                    if (seg->permanences[p] < 0.0f) seg->permanences[p] = 0.0f;
                }
            }
        }
    }

    return 0;
}

int dendritic_seq_advance_timestep(dendritic_sequence_mgr_t* mgr) {
    if (!mgr) return -1;

    /* Swap current → previous */
    uint32_t* tmp;
    tmp = mgr->prev_active_cells;
    mgr->prev_active_cells = mgr->cur_active_cells;
    mgr->cur_active_cells = tmp;
    mgr->num_prev_active = mgr->num_cur_active;
    mgr->num_cur_active = 0;

    tmp = mgr->prev_winner_cells;
    mgr->prev_winner_cells = mgr->cur_winner_cells;
    mgr->cur_winner_cells = tmp;
    mgr->num_prev_winners = mgr->num_cur_winners;
    mgr->num_cur_winners = 0;

    /* Slow permanence decay on ALL segments — prevents old segments from
     * dominating predictions indefinitely. Segments that are not reinforced
     * by correct predictions gradually weaken, allowing new sequences to
     * compete. Decay rate 0.001 per timestep = ~1000 steps to halve.
     * This is the HTM equivalent of synaptic weight decay / forgetting. */
    {
        float decay = 0.001f;
        for (uint32_t c = 0; c < mgr->num_cells; c++) {
            cell_predictive_state_t* cell = &mgr->cells[c];
            for (uint32_t s = 0; s < cell->num_segments; s++) {
                predictive_segment_t* seg = &cell->segments[s];
                bool any_alive = false;
                for (uint32_t p = 0; p < seg->num_synapses; p++) {
                    seg->permanences[p] -= decay;
                    if (seg->permanences[p] < 0.0f) seg->permanences[p] = 0.0f;
                    if (seg->permanences[p] > 0.01f) any_alive = true;
                }
                /* Prune dead segments (all permanences near zero) */
                if (!any_alive && cell->num_segments > 1) {
                    cell->segments[s] = cell->segments[cell->num_segments - 1];
                    cell->num_segments--;
                    mgr->stats.total_segments_destroyed++;
                    s--; /* Re-check this slot */
                }
            }
        }
    }

    /* Update accuracy EMA */
    uint64_t total = mgr->stats.correct_predictions + mgr->stats.total_bursts;
    if (total > 0) {
        float raw_accuracy = (float)mgr->stats.correct_predictions / (float)total;
        if (mgr->stats.prediction_accuracy <= 0.0f) {
            mgr->stats.prediction_accuracy = raw_accuracy;
        } else {
            mgr->stats.prediction_accuracy =
                0.99f * mgr->stats.prediction_accuracy + 0.01f * raw_accuracy;
        }
        mgr->stats.surprise_rate =
            (float)mgr->stats.total_bursts / (float)total;
    }

    return 0;
}

int dendritic_seq_step(dendritic_sequence_mgr_t* mgr,
                       const uint32_t* active_columns, uint32_t num_active) {
    if (!mgr || !active_columns) return -1;

    int rc;
    rc = dendritic_seq_activate_columns(mgr, active_columns, num_active);
    if (rc != 0) return rc;

    rc = dendritic_seq_compute_predictions(mgr);
    if (rc != 0) return rc;

    rc = dendritic_seq_learn(mgr);
    if (rc != 0) return rc;

    rc = dendritic_seq_advance_timestep(mgr);
    return rc;
}

/* =========================================================================
 * Query API
 * ========================================================================= */

int dendritic_seq_get_predicted_cells(const dendritic_sequence_mgr_t* mgr,
                                      uint32_t* predicted_cells, uint32_t max_cells,
                                      uint32_t* num_predicted) {
    if (!mgr || !num_predicted) return -1;
    *num_predicted = 0;

    for (uint32_t i = 0; i < mgr->num_cells && *num_predicted < max_cells; i++) {
        if (mgr->cells[i].is_predicted) {
            if (predicted_cells) predicted_cells[*num_predicted] = i;
            (*num_predicted)++;
        }
    }
    return 0;
}

bool dendritic_seq_is_cell_predicted(const dendritic_sequence_mgr_t* mgr,
                                     uint32_t cell_id) {
    if (!mgr || cell_id >= mgr->num_cells) return false;
    return mgr->cells[cell_id].is_predicted;
}

float dendritic_seq_get_prediction_accuracy(const dendritic_sequence_mgr_t* mgr) {
    if (!mgr) return 0.0f;
    return mgr->stats.prediction_accuracy;
}

float dendritic_seq_get_surprise_rate(const dendritic_sequence_mgr_t* mgr) {
    if (!mgr) return 1.0f;
    return mgr->stats.surprise_rate;
}

int dendritic_seq_get_stats(const dendritic_sequence_mgr_t* mgr,
                            dendritic_seq_stats_t* stats) {
    if (!mgr || !stats) return -1;
    *stats = mgr->stats;
    return 0;
}
