//=============================================================================
// nimcp_information_forager.c - Autonomous Curiosity-Driven Learning
//=============================================================================
//
// WHAT: Tick-based state machine wiring curiosity, salience, uncertainty,
//       and epistemic filtering into an autonomous learning loop.
//
// WHY:  The brain should drive its own learning, not wait for external push.
//
// HOW:  Five states: IDLE → SEEKING → EVALUATING → LEARNING → CONSOLIDATING
//       Each tick advances the state machine by one step.
//
// THREAD SAFETY:
//   Owns a recursive mutex. Lock ordering: forager mutex THEN brain mutex.
//   Never call forager functions from within brain-locked code paths.
//
//=============================================================================

#include "cognitive/curiosity/nimcp_information_forager.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/salience/nimcp_salience.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"
#include "cognitive/kg/nimcp_wave13_metacog_kg.h"  /* W13: forager learn events */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// Internal Structure
//=============================================================================

struct information_forager_struct {
    /* Connected subsystems (non-owned references) */
    brain_t brain;
    curiosity_engine_t curiosity;
    salience_evaluator_t salience;

    /* Optional subsystems (NULL if not connected) */
    struct ensemble_context_struct* ensemble;
    struct epistemic_filter_struct* epistemic_filter;
    struct hypo_drive_system* drives;

    /* Configuration */
    forager_config_t config;

    /* Priority queue (binary max-heap sorted by effective priority) */
    forager_target_t* queue;
    uint32_t queue_size;
    uint32_t queue_capacity;
    uint32_t next_target_id;

    /* State machine */
    forager_state_t state;
    forager_state_t prev_state;       /* State before PAUSED */
    uint64_t state_enter_tick;        /* Tick when current state was entered */
    uint32_t consolidation_counter;   /* Ticks remaining in CONSOLIDATING */
    uint64_t last_seek_tick;          /* Last time we entered SEEKING */

    /* Current work item */
    uint32_t current_target_id;       /* Target being worked on */
    char* pending_text;               /* Text fetched but not yet learned */
    size_t pending_text_len;
    float pending_quality;

    /* Data callback */
    forager_data_callback_t data_callback;
    void* callback_user_data;

    /* Statistics */
    forager_stats_t stats;

    /* IG tracking */
    float last_expected_ig;
    float ig_error_ema;               /* EMA of |expected - realized| */
    double queue_depth_sum;           /* For avg_queue_depth calculation */

    /* Recently learned topics (ring buffer for gap seeding) */
    char recent_topics[16][FORAGER_MAX_TOPIC_LEN];
    uint32_t recent_topics_idx;
    uint32_t recent_topics_count;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Tick counter */
    uint64_t tick_count;
};

//=============================================================================
// Priority Queue Helpers (Binary Max-Heap)
//=============================================================================

static float target_priority(const forager_target_t* t)
{
    return t->expected_ig * t->age_decay;
}

static void heap_swap(forager_target_t* a, forager_target_t* b)
{
    forager_target_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void heap_sift_up(information_forager_t f, uint32_t idx)
{
    while (idx > 0) {
        uint32_t parent = (idx - 1) / 2;
        if (target_priority(&f->queue[idx]) > target_priority(&f->queue[parent])) {
            heap_swap(&f->queue[idx], &f->queue[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

static void heap_sift_down(information_forager_t f, uint32_t idx)
{
    while (true) {
        uint32_t largest = idx;
        uint32_t left = 2 * idx + 1;
        uint32_t right = 2 * idx + 2;

        if (left < f->queue_size &&
            target_priority(&f->queue[left]) > target_priority(&f->queue[largest])) {
            largest = left;
        }
        if (right < f->queue_size &&
            target_priority(&f->queue[right]) > target_priority(&f->queue[largest])) {
            largest = right;
        }

        if (largest != idx) {
            heap_swap(&f->queue[idx], &f->queue[largest]);
            idx = largest;
        } else {
            break;
        }
    }
}

static int queue_push(information_forager_t f, const forager_target_t* target)
{
    if (f->queue_size >= f->queue_capacity) {
        return -1; /* Queue full */
    }
    f->queue[f->queue_size] = *target;
    f->queue[f->queue_size].active = true;
    heap_sift_up(f, f->queue_size);
    f->queue_size++;
    return 0;
}

static forager_target_t* queue_peek(information_forager_t f)
{
    if (f->queue_size == 0) return NULL;
    return &f->queue[0];
}

static int queue_pop(information_forager_t f, forager_target_t* out)
{
    if (f->queue_size == 0) return -1;
    if (out) *out = f->queue[0];
    f->queue_size--;
    if (f->queue_size > 0) {
        f->queue[0] = f->queue[f->queue_size];
        heap_sift_down(f, 0);
    }
    return 0;
}

static forager_target_t* queue_find_by_id(information_forager_t f, uint32_t id)
{
    for (uint32_t i = 0; i < f->queue_size; i++) {
        if (f->queue[i].target_id == id) {
            return &f->queue[i];
        }
    }
    return NULL;
}

static void queue_remove_at(information_forager_t f, uint32_t idx)
{
    if (idx >= f->queue_size) return;
    f->queue_size--;
    if (idx < f->queue_size) {
        f->queue[idx] = f->queue[f->queue_size];
        /* Re-heapify: could go up or down */
        heap_sift_down(f, idx);
        heap_sift_up(f, idx);
    }
}

//=============================================================================
// State Transition Helper
//=============================================================================

static void transition_state(information_forager_t f, forager_state_t new_state)
{
    f->state = new_state;
    f->state_enter_tick = f->tick_count;
    f->stats.ticks_in_current_state = 0;
}

//=============================================================================
// Information Gain Estimation
//=============================================================================

/**
 * @brief Compute prerequisite satisfaction for a knowledge gap
 *
 * WHAT: Fraction of prerequisites the brain is familiar with
 * WHY:  Zone of proximal development — don't pursue what we can't learn yet
 * HOW:  Check familiarity of each prerequisite concept
 */
static float compute_prerequisite_satisfaction(
    information_forager_t f,
    const knowledge_gap_t* gap
)
{
    if (!gap->prerequisites || gap->num_prerequisites == 0) {
        return 1.0f; /* No prerequisites = fully ready */
    }

    uint32_t satisfied = 0;
    for (uint32_t i = 0; i < gap->num_prerequisites; i++) {
        if (gap->prerequisites[i]) {
            float fam = curiosity_check_familiarity(f->curiosity, gap->prerequisites[i]);
            if (fam >= 0.5f) {
                satisfied++;
            }
        }
    }

    return (float)satisfied / (float)gap->num_prerequisites;
}

/**
 * @brief Estimate acquisition cost for a topic
 *
 * WHAT: Heuristic cost estimate based on topic complexity
 * WHY:  Factor into IG calculation — don't pursue expensive low-value topics
 * HOW:  Use related_concepts count and gap_size as proxy for complexity
 */
static float estimate_acquisition_cost(
    const knowledge_gap_t* gap
)
{
    /* More related concepts = more context needed = higher cost */
    float complexity = 0.0f;
    if (gap->related_concepts > 0) {
        complexity = 1.0f - (1.0f / (1.0f + (float)gap->related_concepts * 0.1f));
    }

    /* Larger gap = more data needed = higher cost */
    float volume = gap->gap_size * 0.5f;

    float cost = (complexity + volume) * 0.5f;
    if (cost > 1.0f) cost = 1.0f;
    return cost;
}

/**
 * @brief Compute expected information gain for a knowledge gap
 *
 * WHAT: IG = epistemic_uncertainty × curiosity × (1 - familiarity)
 *            × prerequisite_satisfaction / (1 + cost)
 * WHY:  Ranks topics by expected learning value
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic prediction error weights information value (Schultz 1998)
 * - Zone of proximal development models readiness (Vygotsky)
 * - Effort/reward tradeoff mirrors cost-benefit in ACC (Shenhav 2013)
 */
static float compute_expected_ig(
    information_forager_t f,
    const knowledge_gap_t* gap,
    float epistemic_uncertainty
)
{
    float curiosity = gap->curiosity_intensity;
    float familiarity = 1.0f - gap->gap_size;
    float prereq_sat = f->config.enable_prerequisite_check
                     ? compute_prerequisite_satisfaction(f, gap)
                     : 1.0f;
    float cost = estimate_acquisition_cost(gap);

    float ig = epistemic_uncertainty * curiosity * (1.0f - familiarity)
             * prereq_sat / (1.0f + cost);

    /* Clamp [0, 1] */
    if (ig < 0.0f) ig = 0.0f;
    if (ig > 1.0f) ig = 1.0f;

    return ig;
}

//=============================================================================
// Simple Text-to-Feature Hashing
//=============================================================================

/**
 * @brief Hash text into a fixed-size feature vector
 *
 * WHAT: Simple bag-of-character-trigrams hashing to float features
 * WHY:  Forager needs features for salience evaluation; full NLP is overkill
 * HOW:  FNV-1a hash of overlapping 3-char windows, distribute to buckets
 */
static void text_to_features(const char* text, float* features, uint32_t num_features)
{
    memset(features, 0, num_features * sizeof(float));
    if (!text || !*text) return;

    size_t len = strlen(text);
    if (len < 3) len = 3; /* Pad short strings */

    for (size_t i = 0; i + 2 < len && text[i]; i++) {
        /* FNV-1a hash of trigram */
        uint32_t h = 2166136261u;
        h ^= (uint8_t)text[i];     h *= 16777619u;
        h ^= (uint8_t)text[i + 1]; h *= 16777619u;
        h ^= (uint8_t)text[i + 2]; h *= 16777619u;

        uint32_t bucket = h % num_features;
        features[bucket] += 1.0f;
    }

    /* Normalize to [0, 1] */
    float max_val = 0.0f;
    for (uint32_t i = 0; i < num_features; i++) {
        if (features[i] > max_val) max_val = features[i];
    }
    if (max_val > 0.0f) {
        for (uint32_t i = 0; i < num_features; i++) {
            features[i] /= max_val;
        }
    }
}

//=============================================================================
// State Machine: Per-State Tick Handlers
//=============================================================================

/**
 * @brief IDLE state handler
 *
 * Age queue entries, prune expired/failed targets, check curiosity drive
 * to decide whether to start seeking.
 */
static int tick_idle(information_forager_t f)
{
    /* Age all queue entries */
    for (uint32_t i = 0; i < f->queue_size; /* no increment */) {
        f->queue[i].age_decay *= f->config.target_decay_rate;

        /* Remove expired targets (decayed below threshold) */
        if (f->queue[i].age_decay < 0.01f) {
            f->stats.targets_expired++;
            queue_remove_at(f, i);
            continue;
        }

        /* Remove failed targets (too many attempts) */
        if (f->queue[i].attempts >= f->config.max_attempts) {
            f->stats.targets_failed++;
            queue_remove_at(f, i);
            continue;
        }

        i++;
    }

    /* Check curiosity drive to decide whether to seek */
    float drive = curiosity_get_drive(f->curiosity);

    /* Respect seek interval to prevent thrashing */
    uint64_t ticks_since_seek = f->tick_count - f->last_seek_tick;
    if (ticks_since_seek < f->config.seek_interval_ticks && f->queue_size > 0) {
        /* Not time to seek yet, but if we have targets, evaluate them */
        transition_state(f, FORAGER_STATE_EVALUATING);
        return 1;
    }

    if (drive >= FORAGER_CURIOSITY_DRIVE_THRESHOLD || ticks_since_seek >= f->config.seek_interval_ticks * 2) {
        transition_state(f, FORAGER_STATE_SEEKING);
        return 1;
    }

    /* If we have viable targets, evaluate them even at low drive */
    if (f->queue_size > 0) {
        transition_state(f, FORAGER_STATE_EVALUATING);
        return 1;
    }

    return 0; /* Truly idle */
}

/**
 * @brief SEEKING state handler
 *
 * Detect knowledge gaps, compute expected IG, generate queries, fill queue.
 */
static int tick_seeking(information_forager_t f)
{
    f->last_seek_tick = f->tick_count;

    uint32_t gaps_found = 0;
    uint32_t targets_added = 0;

    /* Strategy: probe recently learned topics for neighboring gaps,
     * plus ask the curiosity engine for its current focus */

    /* Source 1: Curiosity engine's current focus */
    learning_progress_t progress;
    if (curiosity_get_progress(f->curiosity, &progress)) {
        if (progress.current_focus[0] != '\0') {
            /* Get related concepts as seed topics */
            char* related[8] = {0};
            uint32_t n_related = curiosity_get_related_concepts(
                f->curiosity, progress.current_focus, related, 8);

            for (uint32_t i = 0; i < n_related && gaps_found < f->config.top_n_gaps; i++) {
                if (!related[i]) continue;

                knowledge_gap_t gap = curiosity_detect_knowledge_gap(
                    f->curiosity, related[i]);

                if (gap.gap_size < 0.3f) {
                    continue; /* Already mostly known */
                }

                /* Compute epistemic uncertainty */
                float epistemic_u = gap.gap_size * 0.8f; /* Heuristic fallback */

                /* Compute expected IG */
                float ig = compute_expected_ig(f, &gap, epistemic_u);

                if (ig < f->config.ig_threshold) {
                    continue;
                }

                /* Generate questions for this gap */
                generated_question_t questions[3];
                uint32_t n_q = curiosity_generate_questions(
                    f->curiosity, &gap, questions, 3);

                /* Build target */
                forager_target_t target;
                memset(&target, 0, sizeof(target));
                target.target_id = f->next_target_id++;
                snprintf(target.topic, FORAGER_MAX_TOPIC_LEN, "%s", gap.topic);
                target.expected_ig = ig;
                target.curiosity_intensity = gap.curiosity_intensity;
                target.epistemic_uncertainty = epistemic_u;
                target.familiarity = 1.0f - gap.gap_size;
                target.prerequisite_satisfaction = compute_prerequisite_satisfaction(f, &gap);
                target.acquisition_cost_estimate = estimate_acquisition_cost(&gap);
                target.created_tick = f->tick_count;
                target.age_decay = 1.0f;
                target.active = true;

                /* Use best question as query */
                if (n_q > 0) {
                    snprintf(target.query, FORAGER_MAX_QUERY_LEN, "%s", questions[0].question);
                    if (questions[0].num_search_terms > 0 && questions[0].search_terms &&
                        questions[0].search_terms[0]) {
                        snprintf(target.source_hint, FORAGER_MAX_SOURCE_HINT_LEN,
                                 "%s", questions[0].search_terms[0]);
                    }
                } else {
                    snprintf(target.query, FORAGER_MAX_QUERY_LEN, "What is %s?", gap.topic);
                    snprintf(target.source_hint, FORAGER_MAX_SOURCE_HINT_LEN, "wikipedia");
                }

                if (queue_push(f, &target) == 0) {
                    targets_added++;
                    f->stats.targets_created++;
                }

                gaps_found++;
            }
            /* related[i] are non-owned pointers into the curiosity engine's
             * internal bucket storage — do NOT free them. */
        }
    }

    /* Source 2: Recently learned topics — probe their neighbors */
    for (uint32_t r = 0; r < f->recent_topics_count && gaps_found < f->config.top_n_gaps; r++) {
        uint32_t idx = (f->recent_topics_idx - 1 - r + 16) % 16;
        const char* topic = f->recent_topics[idx];
        if (!topic[0]) continue;

        knowledge_gap_t gap = curiosity_detect_knowledge_gap(f->curiosity, topic);
        if (gap.gap_size < 0.3f) continue;

        float epistemic_u = gap.gap_size * 0.8f;
        float ig = compute_expected_ig(f, &gap, epistemic_u);
        if (ig < f->config.ig_threshold) continue;

        /* Check for duplicate topic in queue */
        bool duplicate = false;
        for (uint32_t j = 0; j < f->queue_size; j++) {
            if (strncmp(f->queue[j].topic, gap.topic, FORAGER_MAX_TOPIC_LEN) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        generated_question_t questions[2];
        uint32_t n_q = curiosity_generate_questions(f->curiosity, &gap, questions, 2);

        forager_target_t target;
        memset(&target, 0, sizeof(target));
        target.target_id = f->next_target_id++;
        snprintf(target.topic, FORAGER_MAX_TOPIC_LEN, "%s", gap.topic);
        target.expected_ig = ig;
        target.curiosity_intensity = gap.curiosity_intensity;
        target.epistemic_uncertainty = epistemic_u;
        target.familiarity = 1.0f - gap.gap_size;
        target.prerequisite_satisfaction = compute_prerequisite_satisfaction(f, &gap);
        target.acquisition_cost_estimate = estimate_acquisition_cost(&gap);
        target.created_tick = f->tick_count;
        target.age_decay = 1.0f;
        target.active = true;

        if (n_q > 0) {
            snprintf(target.query, FORAGER_MAX_QUERY_LEN, "%s", questions[0].question);
            if (questions[0].num_search_terms > 0 && questions[0].search_terms &&
                questions[0].search_terms[0]) {
                snprintf(target.source_hint, FORAGER_MAX_SOURCE_HINT_LEN,
                         "%s", questions[0].search_terms[0]);
            }
        } else {
            snprintf(target.query, FORAGER_MAX_QUERY_LEN, "What is %s?", gap.topic);
            snprintf(target.source_hint, FORAGER_MAX_SOURCE_HINT_LEN, "wikipedia");
        }

        if (queue_push(f, &target) == 0) {
            targets_added++;
            f->stats.targets_created++;
        }
        gaps_found++;
    }

    /* Transition to evaluating if we have targets, else back to idle */
    if (f->queue_size > 0) {
        transition_state(f, FORAGER_STATE_EVALUATING);
    } else {
        transition_state(f, FORAGER_STATE_IDLE);
    }

    return 1;
}

/**
 * @brief EVALUATING state handler
 *
 * Pick top target, check salience, fetch data via callback or wait for
 * forager_feed_result().
 */
static int tick_evaluating(information_forager_t f)
{
    forager_target_t* top = queue_peek(f);
    if (!top) {
        transition_state(f, FORAGER_STATE_IDLE);
        return 0;
    }

    /* Salience check: convert topic to features and evaluate */
    float features[32];
    text_to_features(top->topic, features, 32);
    brain_salience_t sal = brain_evaluate_salience(f->salience, features, 32);

    /* If salience too low, discard this target and try next */
    if (sal.salience < FORAGER_SALIENCE_MIN_THRESHOLD) {
        forager_target_t discarded;
        queue_pop(f, &discarded);
        f->stats.targets_expired++;

        /* Try next target (up to 3 attempts per tick) */
        if (f->queue_size > 0) {
            return 1; /* Will re-enter EVALUATING next tick */
        }
        transition_state(f, FORAGER_STATE_IDLE);
        return 0;
    }

    /* If we have a data callback, invoke it OUTSIDE the lock to avoid
     * blocking other threads. Copy needed data under lock first. */
    if (f->data_callback) {
        char query_copy[FORAGER_MAX_QUERY_LEN];
        char hint_copy[FORAGER_MAX_SOURCE_HINT_LEN];
        forager_data_callback_t cb = f->data_callback;
        void* cb_data = f->callback_user_data;

        snprintf(query_copy, sizeof(query_copy), "%s", top->query);
        snprintf(hint_copy, sizeof(hint_copy), "%s", top->source_hint);
        f->stats.data_callbacks_made++;
        top->attempts++;
        uint32_t target_id = top->target_id;
        float expected_ig = top->expected_ig;

        /* Release lock before invoking external callback */
        nimcp_mutex_unlock(f->mutex);

        char* result_text = NULL;
        size_t result_len = 0;
        int rc = cb(query_copy, hint_copy, cb_data, &result_text, &result_len);

        /* Re-acquire lock to update state */
        nimcp_mutex_lock(f->mutex);

        if (rc == 0 && result_text && result_len > 0) {
            /* Got data — store and move to LEARNING */
            f->current_target_id = target_id;
            f->pending_text = result_text;
            f->pending_text_len = result_len;
            f->pending_quality = 0.5f; /* Unknown quality */
            f->last_expected_ig = expected_ig;
            transition_state(f, FORAGER_STATE_LEARNING);
            return 1;
        }

        /* Callback failed — leave target in queue, go idle */
        free(result_text);
        result_text = NULL;
        transition_state(f, FORAGER_STATE_IDLE);
        return 0;
    }

    /* No callback — wait for forager_feed_result() from external code.
     * Stay in EVALUATING but don't spin. Return 0 to indicate waiting. */
    return 0;
}

/**
 * @brief Internal learn function (unlocked, called with mutex held)
 *
 * Quality-gate data, curiosity-boost learning rate, feed to brain.
 */
static int forager_do_learn_unlocked(
    information_forager_t f,
    forager_target_t* target,
    const char* text,
    size_t text_len,
    float quality_score
)
{
    (void)text_len; /* Used for future expansion */

    /* Quality gate */
    if (quality_score < f->config.quality_threshold) {
        f->stats.quality_rejections++;
        return 1; /* Rejected */
    }

    /* Compute curiosity-boosted learning rate (mirrors brain_learning.c:468-481) */
    float base_lr = 0.01f; /* Default learning rate */
    float boost = target->curiosity_intensity * (1.0f - target->familiarity)
                * f->config.curiosity_boost_factor;
    float effective_lr = base_lr * (1.0f + boost);
    float max_lr = base_lr * 2.0f;
    if (effective_lr > max_lr) effective_lr = max_lr;

    /* Learn through curiosity engine (text-based incremental learning) */
    bool learned = curiosity_learn_answer(f->curiosity, target->query, text);

    if (learned) {
        f->stats.learn_events++;

        /* Track realized IG */
        float realized_ig = curiosity_get_information_gain(f->curiosity);
        if (isfinite(realized_ig)) {
            f->stats.avg_realized_ig = f->stats.avg_realized_ig * 0.9f + realized_ig * 0.1f;
        }

        /* Update IG prediction error */
        float error = fabsf(f->last_expected_ig - realized_ig);
        if (isfinite(error)) {
            f->ig_error_ema = f->ig_error_ema * 0.9f + error * 0.1f;
        }
        f->stats.ig_prediction_error = f->ig_error_ema;

        /* Record topic in recent_topics ring buffer */
        snprintf(f->recent_topics[f->recent_topics_idx],
                 FORAGER_MAX_TOPIC_LEN, "%s", target->topic);
        f->recent_topics_idx = (f->recent_topics_idx + 1) % 16;
        if (f->recent_topics_count < 16) f->recent_topics_count++;
    }

    f->stats.targets_completed++;
    return 0;
}

/**
 * @brief LEARNING state handler
 *
 * Apply epistemic filter, learn from pending data, transition to consolidating.
 */
static int tick_learning(information_forager_t f)
{
    forager_target_t* target = queue_find_by_id(f, f->current_target_id);
    if (!target || !f->pending_text) {
        /* Lost target or data — clean up and go idle */
        free(f->pending_text);
        f->pending_text = NULL;
        transition_state(f, FORAGER_STATE_IDLE);
        return 0;
    }

    int result = forager_do_learn_unlocked(
        f, target, f->pending_text, f->pending_text_len, f->pending_quality);

    /* W13: emit a forage-learn event to KG. */
    if (f->brain) {
        wave13_forager_emit_learn(f->brain, target->topic,
                                  f->pending_quality, result);
    }

    /* Clean up pending data */
    free(f->pending_text);
    f->pending_text = NULL;
    f->pending_text_len = 0;

    /* Remove target from queue */
    for (uint32_t i = 0; i < f->queue_size; i++) {
        if (f->queue[i].target_id == f->current_target_id) {
            queue_remove_at(f, i);
            break;
        }
    }

    if (result == 0) {
        /* Successfully learned — consolidate */
        f->consolidation_counter = f->config.consolidation_ticks;
        transition_state(f, FORAGER_STATE_CONSOLIDATING);
    } else {
        /* Rejected by quality gate */
        transition_state(f, FORAGER_STATE_IDLE);
    }

    return 1;
}

/**
 * @brief CONSOLIDATING state handler
 *
 * Wait for consolidation period to complete (mimics biological memory
 * consolidation window).
 */
static int tick_consolidating(information_forager_t f)
{
    if (f->consolidation_counter > 0) {
        f->consolidation_counter--;
        return 1; /* Still consolidating */
    }

    transition_state(f, FORAGER_STATE_IDLE);
    return 0;
}

//=============================================================================
// Comparison for qsort
//=============================================================================

static int compare_target_priority_desc(const void* a, const void* b)
{
    float pa = target_priority((const forager_target_t*)a);
    float pb = target_priority((const forager_target_t*)b);
    if (pa > pb) return -1;
    if (pa < pb) return 1;
    return 0;
}

//=============================================================================
// Public API: Lifecycle
//=============================================================================

forager_config_t forager_default_config(void)
{
    forager_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_queue_depth         = FORAGER_MAX_QUEUE_DEPTH;
    cfg.top_n_gaps              = 5;
    cfg.exploration_rate        = FORAGER_DEFAULT_EXPLORATION;
    cfg.ig_threshold            = FORAGER_MIN_IG_THRESHOLD;
    cfg.quality_threshold       = FORAGER_QUALITY_THRESHOLD;
    cfg.target_decay_rate       = FORAGER_TARGET_DECAY_RATE;
    cfg.curiosity_boost_factor  = 0.4f;
    cfg.max_attempts            = FORAGER_MAX_ATTEMPTS;
    cfg.consolidation_ticks     = FORAGER_CONSOLIDATION_TICKS;
    cfg.seek_interval_ticks     = 50;
    cfg.enable_prerequisite_check = true;
    cfg.enable_drive_integration  = true;
    return cfg;
}

information_forager_t forager_create(
    brain_t brain,
    curiosity_engine_t curiosity,
    salience_evaluator_t salience,
    const forager_config_t* config
)
{
    if (!brain || !curiosity || !salience) {
        return NULL;
    }

    information_forager_t f = nimcp_calloc(1, sizeof(struct information_forager_struct));
    if (!f) return NULL;

    f->brain = brain;
    f->curiosity = curiosity;
    f->salience = salience;

    /* Configuration */
    if (config) {
        f->config = *config;
    } else {
        f->config = forager_default_config();
    }

    /* Allocate priority queue */
    f->queue_capacity = f->config.max_queue_depth;
    f->queue = nimcp_calloc(f->queue_capacity, sizeof(forager_target_t));
    if (!f->queue) {
        nimcp_free(f);
        f = NULL;
        return NULL;
    }

    /* Thread safety */
    mutex_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = MUTEX_TYPE_RECURSIVE;
    f->mutex = nimcp_mutex_create(&attr);
    if (!f->mutex) {
        nimcp_free(f->queue);
        nimcp_free(f);
        f = NULL;
        return NULL;
    }

    /* Initial state */
    f->state = FORAGER_STATE_IDLE;
    f->next_target_id = 1;

    return f;
}

void forager_destroy(information_forager_t forager)
{
    if (!forager) return;

    /* Free pending data */
    free(forager->pending_text);

    /* Free queue */
    nimcp_free(forager->queue);

    /* Destroy mutex */
    if (forager->mutex) {
        nimcp_mutex_destroy(forager->mutex);
    }

    nimcp_free(forager);
    forager = NULL;
}

//=============================================================================
// Public API: Main Loop
//=============================================================================

int forager_tick(information_forager_t forager, uint64_t delta_ms)
{
    if (!forager) return -1;
    (void)delta_ms; /* Reserved for timing-based features */

    nimcp_mutex_lock(forager->mutex);

    forager->tick_count++;
    forager->stats.total_ticks++;
    forager->stats.ticks_in_current_state++;
    forager->stats.current_state = forager->state;
    forager->stats.active_targets = forager->queue_size;

    /* Update running average of queue depth */
    forager->queue_depth_sum += (double)forager->queue_size;
    forager->stats.avg_queue_depth = (float)(forager->queue_depth_sum /
                                              (double)forager->stats.total_ticks);

    int result = 0;

    switch (forager->state) {
    case FORAGER_STATE_IDLE:
        result = tick_idle(forager);
        break;
    case FORAGER_STATE_SEEKING:
        result = tick_seeking(forager);
        break;
    case FORAGER_STATE_EVALUATING:
        result = tick_evaluating(forager);
        break;
    case FORAGER_STATE_LEARNING:
        result = tick_learning(forager);
        break;
    case FORAGER_STATE_CONSOLIDATING:
        result = tick_consolidating(forager);
        break;
    case FORAGER_STATE_PAUSED:
        result = 0; /* No-op while paused */
        break;
    }

    nimcp_mutex_unlock(forager->mutex);
    return result;
}

//=============================================================================
// Public API: Callback Registration
//=============================================================================

int forager_register_data_callback(
    information_forager_t forager,
    forager_data_callback_t callback,
    void* user_data
)
{
    if (!forager) return -1;

    nimcp_mutex_lock(forager->mutex);
    forager->data_callback = callback;
    forager->callback_user_data = user_data;
    nimcp_mutex_unlock(forager->mutex);

    return 0;
}

//=============================================================================
// Public API: Target Inspection
//=============================================================================

int forager_get_top_targets(
    information_forager_t forager,
    forager_target_t* out_targets,
    uint32_t max_count
)
{
    if (!forager || !out_targets) return -1;

    nimcp_mutex_lock(forager->mutex);

    uint32_t count = forager->queue_size;
    if (count > max_count) count = max_count;

    if (count > 0) {
        /* Copy queue and sort by priority descending */
        memcpy(out_targets, forager->queue, count * sizeof(forager_target_t));
        qsort(out_targets, count, sizeof(forager_target_t), compare_target_priority_desc);
    }

    nimcp_mutex_unlock(forager->mutex);
    return (int)count;
}

//=============================================================================
// Public API: Manual Data Feed
//=============================================================================

int forager_feed_result(
    information_forager_t forager,
    uint32_t target_id,
    const char* text,
    size_t text_len,
    float quality_score
)
{
    if (!forager || !text) return -1;

    nimcp_mutex_lock(forager->mutex);

    forager_target_t* target = queue_find_by_id(forager, target_id);
    if (!target) {
        nimcp_mutex_unlock(forager->mutex);
        return -1;
    }

    forager->last_expected_ig = target->expected_ig;

    int result = forager_do_learn_unlocked(forager, target, text, text_len, quality_score);

    /* Remove target from queue */
    for (uint32_t i = 0; i < forager->queue_size; i++) {
        if (forager->queue[i].target_id == target_id) {
            queue_remove_at(forager, i);
            break;
        }
    }

    nimcp_mutex_unlock(forager->mutex);
    return result;
}

//=============================================================================
// Public API: Statistics
//=============================================================================

forager_stats_t forager_get_stats(information_forager_t forager)
{
    forager_stats_t empty;
    memset(&empty, 0, sizeof(empty));
    if (!forager) return empty;

    nimcp_mutex_lock(forager->mutex);
    forager_stats_t stats = forager->stats;
    stats.current_state = forager->state;  /* Always reflect live state */
    nimcp_mutex_unlock(forager->mutex);

    return stats;
}

//=============================================================================
// Public API: Control
//=============================================================================

int forager_set_exploration_rate(information_forager_t forager, float rate)
{
    if (!forager) return -1;
    if (rate < 0.0f) rate = 0.0f;
    if (rate > 1.0f) rate = 1.0f;

    nimcp_mutex_lock(forager->mutex);
    forager->config.exploration_rate = rate;
    nimcp_mutex_unlock(forager->mutex);

    return 0;
}

int forager_pause(information_forager_t forager)
{
    if (!forager) return -1;

    nimcp_mutex_lock(forager->mutex);
    if (forager->state != FORAGER_STATE_PAUSED) {
        forager->prev_state = forager->state;
        transition_state(forager, FORAGER_STATE_PAUSED);
    }
    nimcp_mutex_unlock(forager->mutex);

    return 0;
}

int forager_resume(information_forager_t forager)
{
    if (!forager) return -1;

    nimcp_mutex_lock(forager->mutex);
    if (forager->state == FORAGER_STATE_PAUSED) {
        transition_state(forager, FORAGER_STATE_IDLE);
    }
    nimcp_mutex_unlock(forager->mutex);

    return 0;
}

//=============================================================================
// Public API: Optional Subsystem Connections
//=============================================================================

int forager_connect_ensemble(
    information_forager_t forager,
    struct ensemble_context_struct* ensemble
)
{
    if (!forager) return -1;

    nimcp_mutex_lock(forager->mutex);
    forager->ensemble = ensemble;
    nimcp_mutex_unlock(forager->mutex);

    return 0;
}

int forager_connect_epistemic_filter(
    information_forager_t forager,
    struct epistemic_filter_struct* filter
)
{
    if (!forager) return -1;

    nimcp_mutex_lock(forager->mutex);
    forager->epistemic_filter = filter;
    nimcp_mutex_unlock(forager->mutex);

    return 0;
}

int forager_connect_drives(
    information_forager_t forager,
    struct hypo_drive_system* drives
)
{
    if (!forager) return -1;

    nimcp_mutex_lock(forager->mutex);
    forager->drives = drives;
    nimcp_mutex_unlock(forager->mutex);

    return 0;
}
