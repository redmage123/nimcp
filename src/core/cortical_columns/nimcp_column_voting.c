/**
 * @file nimcp_column_voting.c
 * @brief Column-Level Voting for Thousand Brains Consensus — Implementation
 *
 * WHAT: Each cortical column independently hypothesizes what object it's sensing.
 *       Columns vote through lateral connections. Consensus broadcasts to global
 *       workspace. Disagreement = uncertainty.
 * WHY:  Multiple independent models (columns) each recognize from different
 *       viewpoints. Voting merges these into a unified percept. This is the
 *       core of Hawkins' Thousand Brains theory.
 * HOW:  Each column submits hypotheses. Voting rounds broadcast to lateral
 *       neighbors, blend sensory evidence + received votes, check >70% consensus.
 *       On consensus, broadcast to global workspace.
 *
 * Based on Hawkins' Thousand Brains theory (Numenta, 2019).
 */

#include "core/cortical_columns/nimcp_column_voting.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <math.h>
#include <string.h>
#include <float.h>

/* =========================================================================
 * Configuration
 * ========================================================================= */

void column_voting_config_default(column_voting_config_t* config) {
    if (!config) return;
    config->max_columns = COLUMN_VOTING_MAX_COLUMNS;
    config->max_voting_rounds = COLUMN_VOTING_MAX_ROUNDS;
    config->consensus_threshold = COLUMN_VOTING_CONSENSUS_RATIO;
    config->sensory_weight = COLUMN_VOTING_SENSORY_WEIGHT;
    config->vote_weight = COLUMN_VOTING_VOTE_WEIGHT;
    config->min_confidence = 0.1f;
    config->enable_workspace_broadcast = true;
}

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

column_voting_manager_t* column_voting_create(const column_voting_config_t* config) {
    if (!config) return NULL;

    column_voting_manager_t* mgr = nimcp_calloc(1, sizeof(column_voting_manager_t));
    if (!mgr) return NULL;

    uint32_t max_cols = config->max_columns;
    if (max_cols == 0) max_cols = COLUMN_VOTING_MAX_COLUMNS;

    /* Allocate column states */
    mgr->states = nimcp_calloc(max_cols, sizeof(column_voting_state_t));
    if (!mgr->states) {
        nimcp_free(mgr);
        return NULL;
    }

    /* Allocate lateral connectivity */
    uint32_t max_neighbors = 8; /* Default: 8 lateral neighbors per column */
    mgr->lateral_neighbors = nimcp_calloc(max_cols, sizeof(uint32_t*));
    mgr->num_neighbors = nimcp_calloc(max_cols, sizeof(uint32_t));
    if (!mgr->lateral_neighbors || !mgr->num_neighbors) {
        column_voting_destroy(mgr);
        return NULL;
    }

    for (uint32_t i = 0; i < max_cols; i++) {
        mgr->lateral_neighbors[i] = nimcp_calloc(max_neighbors, sizeof(uint32_t));
        if (!mgr->lateral_neighbors[i]) {
            column_voting_destroy(mgr);
            return NULL;
        }
        mgr->states[i].column_id = i;
    }

    mgr->num_columns = 0;
    mgr->max_columns = max_cols;
    mgr->max_neighbors = max_neighbors;

    mgr->max_voting_rounds = config->max_voting_rounds;
    mgr->consensus_threshold = config->consensus_threshold;
    mgr->sensory_weight = config->sensory_weight;
    mgr->vote_weight = config->vote_weight;
    mgr->min_confidence = config->min_confidence;
    mgr->enable_workspace_broadcast = config->enable_workspace_broadcast;

    mgr->mutex = nimcp_mutex_create(NULL);

    NIMCP_LOGGING_INFO("column_voting: created manager (max_columns=%u, consensus=%.1f%%)",
                       max_cols, mgr->consensus_threshold * 100.0f);
    return mgr;
}

void column_voting_destroy(column_voting_manager_t* mgr) {
    if (!mgr) return;
    if (mgr->lateral_neighbors) {
        for (uint32_t i = 0; i < mgr->max_columns; i++) {
            nimcp_free(mgr->lateral_neighbors[i]);
        }
        nimcp_free(mgr->lateral_neighbors);
    }
    nimcp_free(mgr->num_neighbors);
    nimcp_free(mgr->states);
    if (mgr->mutex) nimcp_mutex_free(mgr->mutex);
    nimcp_free(mgr);
}

/* =========================================================================
 * Connectivity
 * ========================================================================= */

int column_voting_connect_lateral(column_voting_manager_t* mgr,
                                   uint32_t col_a, uint32_t col_b) {
    if (!mgr) return -1;
    if (col_a >= mgr->max_columns || col_b >= mgr->max_columns) return -1;
    if (col_a == col_b) return -1;

    /* Add B as neighbor of A */
    if (mgr->num_neighbors[col_a] < mgr->max_neighbors) {
        /* Check for duplicate */
        for (uint32_t i = 0; i < mgr->num_neighbors[col_a]; i++) {
            if (mgr->lateral_neighbors[col_a][i] == col_b) goto skip_a;
        }
        mgr->lateral_neighbors[col_a][mgr->num_neighbors[col_a]++] = col_b;
    }
skip_a:

    /* Add A as neighbor of B (bidirectional) */
    if (mgr->num_neighbors[col_b] < mgr->max_neighbors) {
        for (uint32_t i = 0; i < mgr->num_neighbors[col_b]; i++) {
            if (mgr->lateral_neighbors[col_b][i] == col_a) goto skip_b;
        }
        mgr->lateral_neighbors[col_b][mgr->num_neighbors[col_b]++] = col_a;
    }
skip_b:

    /* Track active column count */
    if (col_a >= mgr->num_columns) mgr->num_columns = col_a + 1;
    if (col_b >= mgr->num_columns) mgr->num_columns = col_b + 1;

    return 0;
}

int column_voting_connect_workspace(column_voting_manager_t* mgr, void* workspace) {
    if (!mgr) return -1;
    mgr->workspace = workspace;
    NIMCP_LOGGING_INFO("column_voting: connected to global workspace");
    return 0;
}

/* =========================================================================
 * Hypotheses
 * ========================================================================= */

int column_voting_submit_hypothesis(column_voting_manager_t* mgr,
                                     uint32_t column_idx,
                                     uint32_t object_id, float confidence,
                                     const float* evidence, uint32_t num_evidence) {
    if (!mgr) return -1;
    if (column_idx >= mgr->max_columns) return -1;
    if (confidence < mgr->min_confidence) return 0; /* Below threshold, skip */

    column_voting_state_t* state = &mgr->states[column_idx];
    if (state->num_hypotheses >= COLUMN_VOTING_MAX_HYPOTHESES) return -1;

    column_hypothesis_t* hyp = &state->hypotheses[state->num_hypotheses];
    memset(hyp, 0, sizeof(column_hypothesis_t));
    hyp->object_id = object_id;
    hyp->confidence = confidence;

    if (evidence && num_evidence > 0) {
        uint32_t copy = num_evidence < COLUMN_VOTING_MAX_EVIDENCE
                      ? num_evidence : COLUMN_VOTING_MAX_EVIDENCE;
        memcpy(hyp->feature_evidence, evidence, copy * sizeof(float));
        hyp->num_evidence = copy;
    }

    state->num_hypotheses++;

    /* Track active columns */
    if (column_idx >= mgr->num_columns) mgr->num_columns = column_idx + 1;

    return 0;
}

int column_voting_clear_hypotheses(column_voting_manager_t* mgr) {
    if (!mgr) return -1;
    for (uint32_t i = 0; i < mgr->num_columns; i++) {
        mgr->states[i].num_hypotheses = 0;
        mgr->states[i].num_votes = 0;
        mgr->states[i].has_consensus = false;
        mgr->states[i].best_confidence = 0.0f;
        mgr->states[i].best_object_id = 0;
    }
    mgr->global_consensus = false;
    mgr->consensus_confidence = 0.0f;
    mgr->agreement_ratio = 0.0f;
    return 0;
}

/* =========================================================================
 * Voting rounds
 * ========================================================================= */

/**
 * @brief One voting round: broadcast best hypothesis to neighbors,
 *        accumulate votes, blend sensory + votes, check consensus.
 */
int column_voting_run_round(column_voting_manager_t* mgr) {
    if (!mgr) return -1;
    if (mgr->num_columns == 0) return -1;

    /* Phase 1: Each column broadcasts its best hypothesis to lateral neighbors */
    for (uint32_t c = 0; c < mgr->num_columns; c++) {
        column_voting_state_t* sender = &mgr->states[c];
        if (sender->num_hypotheses == 0) continue;

        /* Find sender's best hypothesis */
        int best_h = 0;
        float best_conf = sender->hypotheses[0].confidence;
        for (uint32_t h = 1; h < sender->num_hypotheses; h++) {
            if (sender->hypotheses[h].confidence > best_conf) {
                best_conf = sender->hypotheses[h].confidence;
                best_h = (int)h;
            }
        }

        uint32_t vote_obj = sender->hypotheses[best_h].object_id;
        float vote_weight = sender->hypotheses[best_h].confidence;

        /* Send to all lateral neighbors */
        for (uint32_t n = 0; n < mgr->num_neighbors[c]; n++) {
            uint32_t neighbor_idx = mgr->lateral_neighbors[c][n];
            if (neighbor_idx >= mgr->num_columns) continue;

            column_voting_state_t* receiver = &mgr->states[neighbor_idx];
            if (receiver->num_votes >= COLUMN_VOTING_MAX_HYPOTHESES) continue;

            receiver->vote_object_ids[receiver->num_votes] = vote_obj;
            receiver->votes[receiver->num_votes] = vote_weight;
            receiver->num_votes++;
        }
    }

    /* Phase 2: Each column blends sensory evidence with received votes */
    for (uint32_t c = 0; c < mgr->num_columns; c++) {
        column_voting_state_t* state = &mgr->states[c];

        for (uint32_t h = 0; h < state->num_hypotheses; h++) {
            column_hypothesis_t* hyp = &state->hypotheses[h];

            /* Sensory component */
            float sensory = hyp->confidence * mgr->sensory_weight;

            /* Vote component: sum of votes matching this hypothesis */
            float vote_sum = 0.0f;
            uint32_t vote_count = 0;
            for (uint32_t v = 0; v < state->num_votes; v++) {
                if (state->vote_object_ids[v] == hyp->object_id) {
                    vote_sum += state->votes[v];
                    vote_count++;
                }
            }
            float vote_component = 0.0f;
            if (state->num_votes > 0) {
                vote_component = (vote_sum / (float)state->num_votes) * mgr->vote_weight;
            }

            /* Blend with diversity preservation:
             * Sensory evidence always gets a minimum floor (0.1) to prevent
             * lateral votes from completely overriding local evidence.
             * Without this, all columns converge to the same hypothesis
             * regardless of what they actually sense → mode collapse. */
            float sensory_floor = 0.1f * hyp->confidence; /* 10% of original */
            if (sensory < sensory_floor) sensory = sensory_floor;
            hyp->confidence = sensory + vote_component;
            if (hyp->confidence > 1.0f) hyp->confidence = 1.0f;
        }

        /* Find this column's best after blending */
        state->best_confidence = 0.0f;
        for (uint32_t h = 0; h < state->num_hypotheses; h++) {
            if (state->hypotheses[h].confidence > state->best_confidence) {
                state->best_confidence = state->hypotheses[h].confidence;
                state->best_object_id = state->hypotheses[h].object_id;
            }
        }

        /* Clear votes for next round */
        state->num_votes = 0;
    }

    /* Phase 3: Check global consensus */
    uint32_t columns_with_hyp = 0;
    uint32_t agreement_counts[COLUMN_VOTING_MAX_HYPOTHESES];
    uint32_t agreement_objects[COLUMN_VOTING_MAX_HYPOTHESES];
    uint32_t num_unique = 0;

    memset(agreement_counts, 0, sizeof(agreement_counts));
    memset(agreement_objects, 0, sizeof(agreement_objects));

    for (uint32_t c = 0; c < mgr->num_columns; c++) {
        if (mgr->states[c].num_hypotheses == 0) continue;
        columns_with_hyp++;

        uint32_t obj = mgr->states[c].best_object_id;

        /* Find or insert in unique list */
        bool found = false;
        for (uint32_t u = 0; u < num_unique; u++) {
            if (agreement_objects[u] == obj) {
                agreement_counts[u]++;
                found = true;
                break;
            }
        }
        if (!found && num_unique < COLUMN_VOTING_MAX_HYPOTHESES) {
            agreement_objects[num_unique] = obj;
            agreement_counts[num_unique] = 1;
            num_unique++;
        }
    }

    /* Find most-agreed object */
    uint32_t best_obj = 0;
    uint32_t best_count = 0;
    float best_conf = 0.0f;
    for (uint32_t u = 0; u < num_unique; u++) {
        if (agreement_counts[u] > best_count) {
            best_count = agreement_counts[u];
            best_obj = agreement_objects[u];
        }
    }

    /* Compute confidence as average of agreeing columns' confidence */
    if (best_count > 0) {
        float conf_sum = 0.0f;
        for (uint32_t c = 0; c < mgr->num_columns; c++) {
            if (mgr->states[c].num_hypotheses == 0) continue;
            if (mgr->states[c].best_object_id == best_obj) {
                conf_sum += mgr->states[c].best_confidence;
            }
        }
        best_conf = conf_sum / (float)best_count;
    }

    mgr->consensus_object_id = best_obj;
    mgr->consensus_confidence = best_conf;
    mgr->agreement_ratio = (columns_with_hyp > 0)
                          ? (float)best_count / (float)columns_with_hyp
                          : 0.0f;

    mgr->stats.total_rounds++;

    /* Check consensus threshold */
    if (mgr->agreement_ratio >= mgr->consensus_threshold) {
        mgr->global_consensus = true;
        mgr->stats.total_consensus_reached++;

        for (uint32_t c = 0; c < mgr->num_columns; c++) {
            if (mgr->states[c].best_object_id == best_obj) {
                mgr->states[c].has_consensus = true;
            }
        }

        mgr->stats.last_consensus_object_id = best_obj;
        mgr->stats.last_consensus_confidence = best_conf;

        /* Update mean rounds to consensus (running average) */
        float n = (float)mgr->stats.total_consensus_reached;
        mgr->stats.mean_rounds_to_consensus =
            mgr->stats.mean_rounds_to_consensus * ((n - 1.0f) / n) +
            (float)mgr->stats.total_rounds / n;
        mgr->stats.mean_agreement_ratio =
            mgr->stats.mean_agreement_ratio * ((n - 1.0f) / n) +
            mgr->agreement_ratio / n;

        NIMCP_LOGGING_DEBUG("column_voting: consensus reached — object %u (%.1f%% agreement, conf=%.3f)",
                            best_obj, mgr->agreement_ratio * 100.0f, best_conf);
        return 1; /* Consensus reached */
    }

    return 0; /* No consensus yet */
}

int column_voting_run_to_consensus(column_voting_manager_t* mgr,
                                    uint32_t* rounds_taken) {
    if (!mgr) return -1;

    uint32_t round = 0;
    for (round = 0; round < mgr->max_voting_rounds; round++) {
        int rc = column_voting_run_round(mgr);
        if (rc < 0) return -1;
        if (rc == 1) {
            /* Consensus reached */
            if (rounds_taken) *rounds_taken = round + 1;
            return 0;
        }
    }

    /* Timeout — no consensus */
    mgr->stats.total_timeouts++;
    if (rounds_taken) *rounds_taken = round;

    NIMCP_LOGGING_DEBUG("column_voting: timeout after %u rounds (agreement=%.1f%%)",
                        round, mgr->agreement_ratio * 100.0f);
    return 1;
}

/* =========================================================================
 * Query API
 * ========================================================================= */

bool column_voting_has_consensus(const column_voting_manager_t* mgr) {
    if (!mgr) return false;
    return mgr->global_consensus;
}

int column_voting_get_consensus(const column_voting_manager_t* mgr,
                                 uint32_t* object_id, float* confidence) {
    if (!mgr) return -1;
    if (!mgr->global_consensus) return 1;
    if (object_id) *object_id = mgr->consensus_object_id;
    if (confidence) *confidence = mgr->consensus_confidence;
    return 0;
}

float column_voting_get_agreement_ratio(const column_voting_manager_t* mgr) {
    if (!mgr) return 0.0f;
    return mgr->agreement_ratio;
}

int column_voting_get_column_belief(const column_voting_manager_t* mgr,
                                     uint32_t column_idx,
                                     uint32_t* object_id, float* confidence) {
    if (!mgr) return -1;
    if (column_idx >= mgr->num_columns) return -1;
    const column_voting_state_t* state = &mgr->states[column_idx];
    if (object_id) *object_id = state->best_object_id;
    if (confidence) *confidence = state->best_confidence;
    return 0;
}

int column_voting_get_stats(const column_voting_manager_t* mgr,
                             column_voting_stats_t* stats) {
    if (!mgr || !stats) return -1;
    *stats = mgr->stats;
    return 0;
}
