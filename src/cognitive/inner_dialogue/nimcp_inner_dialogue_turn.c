/**
 * @file nimcp_inner_dialogue_turn.c
 * @brief Turn History Circular Buffer and Analysis Helpers
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Implements turn recording, circular buffer management, and analysis utilities
 * WHY:  Dialogue engine needs bounded, efficient turn storage with running statistics
 * HOW:  Power-of-2 ring buffer with bitwise modular indexing; incremental stats update
 *
 * @author NIMCP Development Team
 */

#include "cognitive/inner_dialogue/nimcp_inner_dialogue_turn.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"

#include <string.h>
#include <math.h>
#include <ctype.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(turn, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Dialogue Act String Table
 * ============================================================================ */

static const char* s_dialogue_act_names[DIALOGUE_ACT_COUNT] = {
    "ASSERT",
    "QUESTION",
    "CHALLENGE",
    "ELABORATE",
    "SYNTHESIZE",
    "CONCLUDE",
    "DEFER",
    "INTROSPECT",
    "REFRAME",
    "WARN"
};

const char* dialogue_act_to_string(dialogue_act_t act) {
    if ((unsigned)act < DIALOGUE_ACT_COUNT) {
        return s_dialogue_act_names[(unsigned)act];
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

inner_dialogue_turn_history_t* inner_dialogue_turn_history_create(void) {
    NIMCP_LOGGING_DEBUG("inner_dialogue_turn: creating history (capacity=%u)",
                        (unsigned)INNER_DIALOGUE_MAX_HISTORY);

    inner_dialogue_turn_history_t* history = nimcp_malloc(sizeof(inner_dialogue_turn_history_t));
    if (!history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_TURN_ERROR_NO_MEMORY,
                              "inner_dialogue_turn: failed to allocate history struct");
        return NULL;
    }
    memset(history, 0, sizeof(inner_dialogue_turn_history_t));

    history->turns = nimcp_calloc(INNER_DIALOGUE_MAX_HISTORY, sizeof(inner_dialogue_turn_t));
    if (!history->turns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_TURN_ERROR_NO_MEMORY,
                              "inner_dialogue_turn: failed to allocate turn ring buffer");
        nimcp_free(history);
        history = NULL;
        return NULL;
    }
    /* calloc zero-initializes */

    history->capacity = INNER_DIALOGUE_MAX_HISTORY;
    history->head = 0;
    history->count = 0;
    history->next_turn_id = 1;

    turn_heartbeat("turn_history_create", 1.0f);
    NIMCP_LOGGING_INFO("inner_dialogue_turn: history created (capacity=%u)",
                       (unsigned)history->capacity);
    return history;
}

void inner_dialogue_turn_history_destroy(inner_dialogue_turn_history_t* history) {
    if (!history) {
        return;
    }
    NIMCP_LOGGING_DEBUG("inner_dialogue_turn: destroying history (total_recorded=%u)",
                        history->stats.total_turns_recorded);
    if (history->turns) {
        nimcp_free(history->turns);
        history->turns = NULL;
    }
    nimcp_free(history);
    history = NULL;
}

int inner_dialogue_turn_history_reset(inner_dialogue_turn_history_t* history) {
    if (!history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_TURN_ERROR_NULL,
                              "inner_dialogue_turn: reset called with NULL history");
        return NIMCP_INNER_DIALOGUE_TURN_ERROR_NULL;
    }

    if (history->turns) {
        memset(history->turns, 0, sizeof(inner_dialogue_turn_t) * history->capacity);
    }
    history->head = 0;
    history->count = 0;
    history->next_turn_id = 1;
    memset(&history->stats, 0, sizeof(inner_dialogue_turn_history_stats_t));

    NIMCP_LOGGING_DEBUG("inner_dialogue_turn: history reset");
    turn_heartbeat("turn_history_reset", 1.0f);
    return 0;
}

/* ============================================================================
 * Recording Implementation
 * ============================================================================ */

int inner_dialogue_turn_history_record(inner_dialogue_turn_history_t* history,
                                        const inner_dialogue_turn_t* turn) {
    if (!history || !history->turns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_TURN_ERROR_NULL,
                              "inner_dialogue_turn: record called with NULL history");
        return -1;
    }
    if (!turn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_INNER_DIALOGUE_TURN_ERROR_NULL,
                              "inner_dialogue_turn: record called with NULL turn");
        return -1;
    }

    /* Copy turn into ring buffer at head position */
    uint32_t idx = history->head & (history->capacity - 1);
    memcpy(&history->turns[idx], turn, sizeof(inner_dialogue_turn_t));

    /* Assign monotonic turn ID */
    uint32_t assigned_id = history->next_turn_id++;
    history->turns[idx].turn_id = assigned_id;

    /* Advance head */
    history->head++;
    if (history->count < history->capacity) {
        history->count++;
    }

    /* Update running statistics */
    inner_dialogue_turn_history_stats_t* st = &history->stats;
    st->total_turns_recorded++;

    /* Incremental averages */
    float n = (float)st->total_turns_recorded;
    st->avg_confidence += (turn->confidence - st->avg_confidence) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    st->avg_relevance  += (turn->relevance  - st->avg_relevance)  / (fabsf(n) > 1e-7f ? n : 1e-7f);
    st->avg_novelty    += (turn->novelty    - st->avg_novelty)    / (fabsf(n) > 1e-7f ? n : 1e-7f);

    /* Count by act type */
    if ((unsigned)turn->act < DIALOGUE_ACT_COUNT) {
        st->act_counts[(unsigned)turn->act]++;
    }

    /* Count by perspective */
    if (turn->perspective_idx < 16) {
        st->perspective_counts[turn->perspective_idx]++;
    }
    st->current_count = history->count;

    NIMCP_LOGGING_DEBUG("inner_dialogue_turn: recorded turn id=%u act=%s perspective=%u conf=%.2f",
                        assigned_id, dialogue_act_to_string(turn->act),
                        turn->perspective_idx, (double)turn->confidence);

    turn_heartbeat("turn_record", (float)history->count / (float)history->capacity);
    return (int)assigned_id;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

const inner_dialogue_turn_t* inner_dialogue_turn_history_get_latest(
    const inner_dialogue_turn_history_t* history) {
    if (!history || !history->turns || history->count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inner_dialogue_turn_history_reset: required parameter is NULL (history, history->turns)");
        return NULL;
    }
    uint32_t idx = (history->head - 1) & (history->capacity - 1);
    return &history->turns[idx];
}

const inner_dialogue_turn_t* inner_dialogue_turn_history_get_at(
    const inner_dialogue_turn_history_t* history, uint32_t index) {
    if (!history || !history->turns || index >= history->count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inner_dialogue_turn_history_reset: required parameter is NULL (history, history->turns)");
        return NULL;
    }
    /* index 0 = most recent, index 1 = second most recent, etc. */
    uint32_t idx = (history->head - 1 - index) & (history->capacity - 1);
    return &history->turns[idx];
}

const inner_dialogue_turn_t* inner_dialogue_turn_history_get_by_id(
    const inner_dialogue_turn_history_t* history, uint32_t turn_id) {
    if (!history || !history->turns || history->count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inner_dialogue_turn_history_reset: required parameter is NULL (history, history->turns)");
        return NULL;
    }
    /* Linear scan — history is small (max 128) so acceptable */
    for (uint32_t i = 0; i < history->count; i++) {
        uint32_t idx = (history->head - 1 - i) & (history->capacity - 1);
        if (history->turns[idx].turn_id == turn_id) {
            return &history->turns[idx];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inner_dialogue_turn_history_reset: validation failed");
    return NULL;
}

uint32_t inner_dialogue_turn_history_count(
    const inner_dialogue_turn_history_t* history) {
    return history ? history->count : 0;
}

int inner_dialogue_turn_history_get_stats(
    const inner_dialogue_turn_history_t* history,
    inner_dialogue_turn_history_stats_t* stats) {
    if (!history || !stats) {
        return NIMCP_INNER_DIALOGUE_TURN_ERROR_NULL;
    }
    memcpy(stats, &history->stats, sizeof(inner_dialogue_turn_history_stats_t));
    return 0;
}

/* ============================================================================
 * Analysis Helpers
 * ============================================================================ */

/**
 * @brief Compute Shannon entropy from frequency counts
 *
 * WHAT: H = -sum(p_i * log2(p_i)) for non-zero probabilities
 * WHY:  Reusable entropy computation for act and perspective analysis
 * HOW:  Convert counts to probabilities, delegate to nimcp_stats_entropy()
 *
 * Uses central statistics module for entropy calculation.
 * For future quantum extension, this could invoke quantum_shannon_entropy().
 */
static float compute_shannon_entropy(const uint32_t* counts, uint32_t num_bins,
                                      uint32_t total) {
    if (total == 0 || !counts || num_bins == 0) {
        return 0.0f;
    }

    /* Convert counts to probabilities for nimcp_stats_entropy() */
    float probs[DIALOGUE_ACT_COUNT > 16 ? DIALOGUE_ACT_COUNT : 16];
    if (num_bins > sizeof(probs) / sizeof(probs[0])) {
        /* Fallback for unexpected large num_bins */
        return 0.0f;
    }

    float inv_total = 1.0f / (float)total;
    for (uint32_t i = 0; i < num_bins; i++) {
        probs[i] = (float)counts[i] * inv_total;
    }

    return nimcp_stats_entropy(probs, num_bins);
}

float inner_dialogue_turn_history_act_entropy(
    const inner_dialogue_turn_history_t* history, uint32_t window) {
    if (!history || !history->turns || history->count == 0) {
        return -1.0f;
    }

    uint32_t n = (window > 0 && window < history->count) ? window : history->count;
    uint32_t act_counts[DIALOGUE_ACT_COUNT];
    memset(act_counts, 0, sizeof(act_counts));

    for (uint32_t i = 0; i < n; i++) {
        const inner_dialogue_turn_t* t = inner_dialogue_turn_history_get_at(history, i);
        if (t && (unsigned)t->act < DIALOGUE_ACT_COUNT) {
            act_counts[(unsigned)t->act]++;
        }
    }

    float entropy = compute_shannon_entropy(act_counts, DIALOGUE_ACT_COUNT, n);
    NIMCP_LOGGING_DEBUG("inner_dialogue_turn: act_entropy=%.3f (window=%u)", (double)entropy, n);
    return entropy;
}

/**
 * @brief Simple whitespace tokeniser for Jaccard similarity
 *
 * WHAT: Split content into word tokens for set comparison
 * WHY:  Jaccard index needs word-level set representation
 * HOW:  Walk string, emit tokens delimited by whitespace/punctuation
 */
static uint32_t tokenise_content(const char* content, uint32_t content_len,
                                  uint32_t* hashes, uint32_t max_tokens) {
    if (!content || content_len == 0 || !hashes) {
        return 0;
    }

    uint32_t count = 0;
    uint32_t i = 0;
    while (i < content_len && count < max_tokens) {
        /* Skip whitespace */
        while (i < content_len && (isspace((unsigned char)content[i]) ||
               ispunct((unsigned char)content[i]))) {
            i++;
        }
        if (i >= content_len) break;

        /* Hash the word (DJB2) */
        uint32_t hash = 5381;
        while (i < content_len && !isspace((unsigned char)content[i]) &&
               !ispunct((unsigned char)content[i])) {
            hash = ((hash << 5) + hash) + (unsigned char)tolower((unsigned char)content[i]);
            i++;
        }
        hashes[count++] = hash;
    }
    return count;
}

float inner_dialogue_turn_content_similarity(const inner_dialogue_turn_t* a,
                                              const inner_dialogue_turn_t* b) {
    if (!a || !b) {
        return -1.0f;
    }

    /* Tokenise both turns */
    uint32_t ha[128], hb[128];
    uint32_t na = tokenise_content(a->content, a->content_len, ha, 128);
    uint32_t nb = tokenise_content(b->content, b->content_len, hb, 128);

    if (na == 0 && nb == 0) {
        return 1.0f; /* Both empty = identical */
    }
    if (na == 0 || nb == 0) {
        return 0.0f;
    }

    /* Compute intersection and union via sorted merge */
    /* Sort both arrays — insertion sort is fine for N<=128 */
    for (uint32_t i = 1; i < na; i++) {
        uint32_t key = ha[i];
        uint32_t j = i;
        while (j > 0 && ha[j - 1] > key) {
            ha[j] = ha[j - 1];
            j--;
        }
        ha[j] = key;
    }
    for (uint32_t i = 1; i < nb; i++) {
        uint32_t key = hb[i];
        uint32_t j = i;
        while (j > 0 && hb[j - 1] > key) {
            hb[j] = hb[j - 1];
            j--;
        }
        hb[j] = key;
    }

    /* Merge to count intersection and union */
    uint32_t intersection = 0;
    uint32_t ia = 0, ib = 0;
    uint32_t union_count = 0;
    while (ia < na && ib < nb) {
        if (ha[ia] == hb[ib]) {
            intersection++;
            union_count++;
            ia++;
            ib++;
        } else if (ha[ia] < hb[ib]) {
            union_count++;
            ia++;
        } else {
            union_count++;
            ib++;
        }
    }
    union_count += (na - ia) + (nb - ib);

    float jaccard = (union_count > 0) ? (float)intersection / (float)union_count : 0.0f;
    NIMCP_LOGGING_TRACE("inner_dialogue_turn: content_similarity=%.3f (inter=%u, union=%u)",
                        (double)jaccard, intersection, union_count);
    return jaccard;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void turn_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_turn_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int turn_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "turn_training_begin: NULL argument");
        return -1;
    }
    turn_heartbeat_instance(NULL, "turn_training_begin", 0.0f);
    return 0;
}

int turn_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "turn_training_end: NULL argument");
        return -1;
    }
    turn_heartbeat_instance(NULL, "turn_training_end", 1.0f);
    return 0;
}

int turn_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "turn_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    turn_heartbeat_instance(NULL, "turn_training_step", progress);
    return 0;
}
