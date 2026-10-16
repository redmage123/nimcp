#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_training_bridge.c - Language-Training Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_training_bridge.c
 * @brief Implementation of Language-Training bridge for language learning
 *
 * WHAT: Bridge connecting language processing with training systems
 * WHY:  Enable STDP, vocabulary expansion, grammar learning, semantic binding
 * HOW:  Connects orchestrator to training pipeline for adaptive learning
 *
 * BIOLOGICAL BASIS:
 * - Vocabulary Learning: New word-concept associations (hippocampus → cortex)
 * - Grammar Learning: Statistical learning of grammatical patterns
 * - Phoneme Learning: Perceptual learning via STDP in auditory cortex
 * - Semantic Binding: Strengthening word-concept associations
 * - Error-Driven Learning: N400-like prediction errors drive learning
 *
 * @version 1.0.0 - Phase L2: Language Layer Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_training_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LANG_TRAINING_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_training_bridge)

//=============================================================================
// Internal Helper Functions
//=============================================================================

static int init_vocab_state(language_training_bridge_t* bridge) {
    vocabulary_learning_state_t* state = &bridge->vocab_state;

    state->vocabulary_lr = LANGUAGE_TRAINING_DEFAULT_VOCAB_LR;
    state->vocabulary_decay_rate = 0.001f;
    state->new_word_capacity = LANGUAGE_TRAINING_MAX_NEW_WORDS_PER_BATCH;
    state->new_word_count = 0;

    state->new_word_ids = (uint32_t*)nimcp_calloc(
        state->new_word_capacity, sizeof(uint32_t));
    state->new_word_novelty = (float*)nimcp_calloc(
        state->new_word_capacity, sizeof(float));

    if (!state->new_word_ids || !state->new_word_novelty) {
        nimcp_free(state->new_word_ids);
        nimcp_free(state->new_word_novelty);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_vocab_state: required parameter is NULL (state->new_word_ids, state->new_word_novelty)");
        return -1;
    }

    state->words_acquired = 0;
    state->words_reinforced = 0;
    state->words_forgotten = 0;
    state->avg_word_strength = 0.5f;
    state->expansion_enabled = true;
    state->vocab_size = 0;
    state->max_vocab_size = 100000;

    return 0;
}

static void cleanup_vocab_state(vocabulary_learning_state_t* state) {
    nimcp_free(state->new_word_ids);
    nimcp_free(state->new_word_novelty);
    state->new_word_ids = NULL;
    state->new_word_novelty = NULL;
}

static int init_grammar_state(language_training_bridge_t* bridge) {
    grammar_learning_state_t* state = &bridge->grammar_state;

    state->grammar_lr = LANGUAGE_TRAINING_DEFAULT_GRAMMAR_LR;
    state->rule_capacity = LANGUAGE_TRAINING_MAX_GRAMMAR_RULES;
    state->num_rules = 0;
    state->transition_dim = 64;  /* POS tag space */
    state->exposure_count = 0;
    state->induction_enabled = true;

    state->rule_strengths = (float*)nimcp_calloc(
        state->rule_capacity, sizeof(float));
    state->transition_probabilities = (float*)nimcp_calloc(
        state->transition_dim * state->transition_dim, sizeof(float));

    if (!state->rule_strengths || !state->transition_probabilities) {
        nimcp_free(state->rule_strengths);
        nimcp_free(state->transition_probabilities);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_grammar_state: required parameter is NULL (state->rule_strengths, state->transition_probabilities)");
        return -1;
    }

    return 0;
}

static void cleanup_grammar_state(grammar_learning_state_t* state) {
    nimcp_free(state->rule_strengths);
    nimcp_free(state->transition_probabilities);
    state->rule_strengths = NULL;
    state->transition_probabilities = NULL;
}

static int init_phoneme_state(language_training_bridge_t* bridge) {
    phoneme_learning_state_t* state = &bridge->phoneme_state;

    state->tau_plus = LANGUAGE_TRAINING_STDP_TAU_PLUS;
    state->tau_minus = LANGUAGE_TRAINING_STDP_TAU_MINUS;
    state->a_plus = LANGUAGE_TRAINING_STDP_A_PLUS;
    state->a_minus = LANGUAGE_TRAINING_STDP_A_MINUS;
    state->time_window_ms = 50.0f;
    state->phoneme_lr = LANGUAGE_TRAINING_DEFAULT_PHONEME_LR;
    state->plasticity_scale = 1.0f;
    state->trace_dim = 64;  /* Phoneme inventory size */
    state->potentiation_events = 0;
    state->depression_events = 0;
    state->stdp_enabled = true;
    state->type = PLASTICITY_STDP;

    state->eligibility_trace = (float*)nimcp_calloc(
        state->trace_dim, sizeof(float));
    if (!state->eligibility_trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_phoneme_state: state->eligibility_trace is NULL");
        return -1;
    }

    return 0;
}

static void cleanup_phoneme_state(phoneme_learning_state_t* state) {
    nimcp_free(state->eligibility_trace);
    state->eligibility_trace = NULL;
}

static int init_semantic_state(language_training_bridge_t* bridge) {
    semantic_learning_state_t* state = &bridge->semantic_state;

    state->binding_lr = LANGUAGE_TRAINING_DEFAULT_SEMANTIC_LR;
    state->update_capacity = 64;
    state->num_updates = 0;
    state->bindings_strengthened = 0;
    state->bindings_weakened = 0;
    state->semantic_learning_enabled = true;

    state->word_ids = (uint32_t*)nimcp_calloc(state->update_capacity, sizeof(uint32_t));
    state->concept_ids = (uint32_t*)nimcp_calloc(state->update_capacity, sizeof(uint32_t));
    state->binding_deltas = (float*)nimcp_calloc(state->update_capacity, sizeof(float));

    if (!state->word_ids || !state->concept_ids || !state->binding_deltas) {
        nimcp_free(state->word_ids);
        nimcp_free(state->concept_ids);
        nimcp_free(state->binding_deltas);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_semantic_state: required parameter is NULL (state->word_ids, state->concept_ids, state->binding_deltas)");
        return -1;
    }

    return 0;
}

static void cleanup_semantic_state(semantic_learning_state_t* state) {
    nimcp_free(state->word_ids);
    nimcp_free(state->concept_ids);
    nimcp_free(state->binding_deltas);
    state->word_ids = NULL;
    state->concept_ids = NULL;
    state->binding_deltas = NULL;
}

static int init_error_state(language_training_bridge_t* bridge) {
    error_feedback_state_t* state = &bridge->error_state;

    state->error_capacity = 32;
    state->error_count = 0;
    state->n400_threshold = LANGUAGE_TRAINING_N400_THRESHOLD;
    state->p600_threshold = LANGUAGE_TRAINING_P600_THRESHOLD;
    state->error_scale = LANGUAGE_TRAINING_ERROR_SCALE_DEFAULT;
    state->n400_events = 0;
    state->p600_events = 0;
    state->avg_error_magnitude = 0.0f;
    state->comprehension_feedback_enabled = true;
    state->production_feedback_enabled = true;

    state->error_queue = (language_error_t*)nimcp_calloc(
        state->error_capacity, sizeof(language_error_t));
    if (!state->error_queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_error_state: state->error_queue is NULL");
        return -1;
    }

    return 0;
}

static void cleanup_error_state(error_feedback_state_t* state) {
    nimcp_free(state->error_queue);
    state->error_queue = NULL;
}

static void log_learning_event(language_training_bridge_t* bridge,
                                language_learning_event_t event,
                                language_learning_type_t type,
                                uint32_t target_id,
                                float delta,
                                uint64_t timestamp_ms) {
    if (!bridge->event_log || bridge->event_log_size == 0) return;

    learning_event_record_t* record = &bridge->event_log[bridge->event_log_idx];
    record->event = event;
    record->learning_type = type;
    record->target_id = target_id;
    record->delta = delta;
    record->timestamp_ms = timestamp_ms;

    bridge->event_log_idx = (bridge->event_log_idx + 1) % bridge->event_log_size;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_training_bridge_t* language_training_bridge_create(
    const language_training_config_t* config)
{
    language_training_bridge_t* bridge = (language_training_bridge_t*)
        nimcp_calloc(1, sizeof(language_training_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_training_bridge_create: allocation failed");
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        return NULL;
    }

    /* Copy configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(language_training_config_t));
    } else {
        language_training_default_config(&bridge->config);
    }

    /* Initialize learning states */
    if (init_vocab_state(bridge) != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_training_bridge_create: validation failed");
        return NULL;
    }

    if (init_grammar_state(bridge) != 0) {
        cleanup_vocab_state(&bridge->vocab_state);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_training_bridge_create: validation failed");
        return NULL;
    }

    if (init_phoneme_state(bridge) != 0) {
        cleanup_grammar_state(&bridge->grammar_state);
        cleanup_vocab_state(&bridge->vocab_state);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_training_bridge_create: validation failed");
        return NULL;
    }

    if (init_semantic_state(bridge) != 0) {
        cleanup_phoneme_state(&bridge->phoneme_state);
        cleanup_grammar_state(&bridge->grammar_state);
        cleanup_vocab_state(&bridge->vocab_state);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_training_bridge_create: validation failed");
        return NULL;
    }

    if (init_error_state(bridge) != 0) {
        cleanup_semantic_state(&bridge->semantic_state);
        cleanup_phoneme_state(&bridge->phoneme_state);
        cleanup_grammar_state(&bridge->grammar_state);
        cleanup_vocab_state(&bridge->vocab_state);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_training_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize event log */
    bridge->event_log_size = 128;
    bridge->event_log_idx = 0;
    bridge->event_log = (learning_event_record_t*)nimcp_calloc(
        bridge->event_log_size, sizeof(learning_event_record_t));
    if (!bridge->event_log) {
        cleanup_error_state(&bridge->error_state);
        cleanup_semantic_state(&bridge->semantic_state);
        cleanup_phoneme_state(&bridge->phoneme_state);
        cleanup_grammar_state(&bridge->grammar_state);
        cleanup_vocab_state(&bridge->vocab_state);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_create: bridge->event_log is NULL");
        return NULL;
    }

    bridge->initialized = true;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "Training bridge created");
    return bridge;
}

void language_training_bridge_destroy(language_training_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_training");

    if (bridge->bio_async_registered) {
        language_training_bridge_bio_async_unregister(bridge);
    }

    nimcp_free(bridge->event_log);
    cleanup_error_state(&bridge->error_state);
    cleanup_semantic_state(&bridge->semantic_state);
    cleanup_phoneme_state(&bridge->phoneme_state);
    cleanup_grammar_state(&bridge->grammar_state);
    cleanup_vocab_state(&bridge->vocab_state);

    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Training bridge destroyed");
}

int language_training_bridge_init(language_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_init: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(language_training_stats_t));
    bridge->initialized = true;

    LOG_DEBUG(LOG_MODULE, "Training bridge initialized");
    return 0;
}

int language_training_bridge_start(language_training_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_start: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    bridge->active = true;
    LOG_INFO(LOG_MODULE, "Training bridge started");
    return 0;
}

int language_training_bridge_stop(language_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_stop: bridge is NULL");
        return -1;
    }

    bridge->active = false;
    LOG_INFO(LOG_MODULE, "Training bridge stopped");
    return 0;
}

//=============================================================================
// Connection API Implementation
//=============================================================================

int language_training_bridge_connect_orchestrator(
    language_training_bridge_t* bridge,
    language_orchestrator_t* orchestrator)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_connect_orchestrator: bridge is NULL");
        return -1;
    }
    bridge->orchestrator = orchestrator;
    return 0;
}

int language_training_bridge_connect_training_context(
    language_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_connect_training_context: bridge is NULL");
        return -1;
    }
    bridge->training_ctx = training_ctx;
    return 0;
}

int language_training_bridge_connect_cognitive_training(
    language_training_bridge_t* bridge,
    cognitive_training_bridge_t* cognitive_bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_connect_cognitive_training: bridge is NULL");
        return -1;
    }
    bridge->cognitive_bridge = cognitive_bridge;
    return 0;
}

int language_training_bridge_connect_perception_training(
    language_training_bridge_t* bridge,
    perception_training_bridge_t* perception_bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_connect_perception_training: bridge is NULL");
        return -1;
    }
    bridge->perception_bridge = perception_bridge;
    return 0;
}

int language_training_bridge_connect_plasticity(
    language_training_bridge_t* bridge,
    training_plasticity_bridge_t* plasticity_bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_connect_plasticity: bridge is NULL");
        return -1;
    }
    bridge->plasticity_bridge = plasticity_bridge;
    return 0;
}

//=============================================================================
// Vocabulary Learning API Implementation
//=============================================================================

int language_training_bridge_learn_word(
    language_training_bridge_t* bridge,
    const language_word_t* word,
    float novelty)
{
    if (!bridge || !word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_learn_word: required parameter is NULL (bridge, word)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_learn_word: bridge->active is NULL");
        return -1;
    }

    vocabulary_learning_state_t* state = &bridge->vocab_state;

    if (!state->expansion_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_learn_word: state->expansion_enabled is NULL");
        return -1;
    }
    if (state->vocab_size >= state->max_vocab_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_training_bridge_learn_word: capacity exceeded");
        return -1;
    }
    if (state->new_word_count >= state->new_word_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_training_bridge_learn_word: capacity exceeded");
        return -1;
    }

    /* Add to new word queue */
    state->new_word_ids[state->new_word_count] = word->id;
    state->new_word_novelty[state->new_word_count] = novelty;
    state->new_word_count++;

    state->vocab_size++;
    state->words_acquired++;
    bridge->stats.vocabulary_updates++;

    log_learning_event(bridge, LEARNING_EVENT_WORD_ACQUIRED,
                       LEARNING_TYPE_VOCABULARY, word->id, novelty,
                       bridge->stats.last_update_time_ms);

    return 0;
}

int language_training_bridge_reinforce_word(
    language_training_bridge_t* bridge,
    uint32_t word_id,
    float reinforcement)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_reinforce_word: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_reinforce_word: bridge->active is NULL");
        return -1;
    }

    vocabulary_learning_state_t* state = &bridge->vocab_state;
    state->words_reinforced++;
    bridge->stats.vocabulary_updates++;

    log_learning_event(bridge, LEARNING_EVENT_WORD_REINFORCED,
                       LEARNING_TYPE_VOCABULARY, word_id, reinforcement,
                       bridge->stats.last_update_time_ms);

    return 0;
}

int language_training_bridge_set_vocab_lr(
    language_training_bridge_t* bridge,
    float lr)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_set_vocab_lr: bridge is NULL");
        return -1;
    }
    bridge->vocab_state.vocabulary_lr = lr;
    return 0;
}

//=============================================================================
// Grammar Learning API Implementation
//=============================================================================

int language_training_bridge_learn_grammar(
    language_training_bridge_t* bridge,
    const language_word_t* words,
    uint32_t count)
{
    if (!bridge || !words || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_learn_grammar: required parameter is NULL (bridge, words)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_learn_grammar: bridge->active is NULL");
        return -1;
    }

    grammar_learning_state_t* state = &bridge->grammar_state;
    if (!state->induction_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_learn_grammar: state->induction_enabled is NULL");
        return -1;
    }

    /* Update transition probabilities for adjacent words */
    for (uint32_t i = 0; i < count - 1; i++) {
        /* Use POS as index into transition matrix */
        uint32_t from_idx = (uint32_t)words[i].pos % state->transition_dim;
        uint32_t to_idx = (uint32_t)words[i + 1].pos % state->transition_dim;
        uint32_t idx = from_idx * state->transition_dim + to_idx;

        state->transition_probabilities[idx] += state->grammar_lr;
        if (state->transition_probabilities[idx] > 1.0f) {
            state->transition_probabilities[idx] = 1.0f;
        }
    }

    state->exposure_count++;
    bridge->stats.grammar_updates++;

    return 0;
}

int language_training_bridge_get_grammar_rules(
    const language_training_bridge_t* bridge,
    float* rule_strengths,
    uint32_t max_rules)
{
    if (!bridge || !rule_strengths) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_get_grammar_rules: required parameter is NULL (bridge, rule_strengths)");
        return -1;
    }

    const grammar_learning_state_t* state = &bridge->grammar_state;
    uint32_t copy_count = max_rules < state->num_rules ? max_rules : state->num_rules;

    memcpy(rule_strengths, state->rule_strengths, copy_count * sizeof(float));
    return (int)copy_count;
}

//=============================================================================
// Phoneme Learning API (STDP) Implementation
//=============================================================================

float language_training_bridge_stdp_update(
    language_training_bridge_t* bridge,
    uint32_t pre_phoneme,
    uint32_t post_phoneme,
    float dt)
{
    if (!bridge) return 0.0f;
    if (!bridge->active) return 0.0f;

    phoneme_learning_state_t* state = &bridge->phoneme_state;
    if (!state->stdp_enabled) return 0.0f;

    float delta_w = 0.0f;

    if (dt > 0.0f && dt < state->time_window_ms) {
        /* Pre before post: LTP (potentiation) */
        delta_w = state->a_plus * expf(-dt / state->tau_plus);
        state->potentiation_events++;
    } else if (dt < 0.0f && -dt < state->time_window_ms) {
        /* Post before pre: LTD (depression) */
        delta_w = -state->a_minus * expf(dt / state->tau_minus);
        state->depression_events++;
    }

    delta_w *= state->phoneme_lr * state->plasticity_scale;

    /* Update eligibility traces */
    if (pre_phoneme < state->trace_dim) {
        state->eligibility_trace[pre_phoneme] += delta_w;
    }
    if (post_phoneme < state->trace_dim) {
        state->eligibility_trace[post_phoneme] += delta_w;
    }

    bridge->stats.phoneme_updates++;

    log_learning_event(bridge, LEARNING_EVENT_PHONEME_TUNED,
                       LEARNING_TYPE_PHONEME, pre_phoneme, delta_w,
                       bridge->stats.last_update_time_ms);

    return delta_w;
}

int language_training_bridge_set_stdp_params(
    language_training_bridge_t* bridge,
    float tau_plus,
    float tau_minus,
    float a_plus,
    float a_minus)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_set_stdp_params: bridge is NULL");
        return -1;
    }

    phoneme_learning_state_t* state = &bridge->phoneme_state;
    state->tau_plus = tau_plus;
    state->tau_minus = tau_minus;
    state->a_plus = a_plus;
    state->a_minus = a_minus;

    return 0;
}

//=============================================================================
// Semantic Binding API Implementation
//=============================================================================

int language_training_bridge_bind_word_concept(
    language_training_bridge_t* bridge,
    uint32_t word_id,
    uint32_t concept_id,
    float strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_bind_word_concept: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_bind_word_concept: bridge->active is NULL");
        return -1;
    }

    semantic_learning_state_t* state = &bridge->semantic_state;
    if (!state->semantic_learning_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_bind_word_concept: state->semantic_learning_enabled is NULL");
        return -1;
    }
    if (state->num_updates >= state->update_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_training_bridge_bind_word_concept: capacity exceeded");
        return -1;
    }

    /* Add to update queue */
    state->word_ids[state->num_updates] = word_id;
    state->concept_ids[state->num_updates] = concept_id;
    state->binding_deltas[state->num_updates] = strength * state->binding_lr;
    state->num_updates++;

    if (strength > 0.0f) {
        state->bindings_strengthened++;
    } else {
        state->bindings_weakened++;
    }

    bridge->stats.semantic_updates++;

    log_learning_event(bridge, LEARNING_EVENT_SEMANTIC_BOUND,
                       LEARNING_TYPE_SEMANTIC, word_id, strength,
                       bridge->stats.last_update_time_ms);

    return 0;
}

//=============================================================================
// Error Feedback API Implementation
//=============================================================================

int language_training_bridge_report_error(
    language_training_bridge_t* bridge,
    const language_error_t* error)
{
    if (!bridge || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_report_error: required parameter is NULL (bridge, error)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_report_error: bridge->active is NULL");
        return -1;
    }

    error_feedback_state_t* state = &bridge->error_state;
    if (state->error_count >= state->error_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_training_bridge_report_error: capacity exceeded");
        return -1;
    }

    /* Check if feedback enabled for this error type */
    if (error->type == ERROR_SIGNAL_N400 && !state->comprehension_feedback_enabled) {
        return 0;  /* Silently ignore */
    }
    if (error->type == ERROR_SIGNAL_P600 && !state->production_feedback_enabled) {
        return 0;
    }

    /* Add to error queue */
    memcpy(&state->error_queue[state->error_count], error, sizeof(language_error_t));
    state->error_count++;

    /* Update statistics */
    if (error->type == ERROR_SIGNAL_N400) {
        state->n400_events++;
    } else if (error->type == ERROR_SIGNAL_P600) {
        state->p600_events++;
    }

    /* Update average error magnitude */
    float n = (float)(state->n400_events + state->p600_events);
    state->avg_error_magnitude = (state->avg_error_magnitude * (n - 1.0f) + error->magnitude) / n;

    bridge->stats.total_errors++;

    return 0;
}

int language_training_bridge_get_errors(
    language_training_bridge_t* bridge,
    language_error_t* errors,
    uint32_t max_errors)
{
    if (!bridge || !errors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_get_errors: required parameter is NULL (bridge, errors)");
        return -1;
    }

    error_feedback_state_t* state = &bridge->error_state;
    uint32_t copy_count = max_errors < state->error_count ? max_errors : state->error_count;

    memcpy(errors, state->error_queue, copy_count * sizeof(language_error_t));

    /* Clear returned errors from queue */
    if (copy_count < state->error_count) {
        memmove(state->error_queue, &state->error_queue[copy_count],
                (state->error_count - copy_count) * sizeof(language_error_t));
    }
    state->error_count -= copy_count;

    return (int)copy_count;
}

int language_training_bridge_set_error_scale(
    language_training_bridge_t* bridge,
    float scale)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_set_error_scale: bridge is NULL");
        return -1;
    }
    bridge->error_state.error_scale = scale;
    return 0;
}

//=============================================================================
// Training Parameter API Implementation
//=============================================================================

int language_training_bridge_get_learning_rates(
    const language_training_bridge_t* bridge,
    float* vocab_lr,
    float* grammar_lr,
    float* phoneme_lr,
    float* semantic_lr)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_get_learning_rates: bridge is NULL");
        return -1;
    }

    if (vocab_lr) *vocab_lr = bridge->vocab_state.vocabulary_lr;
    if (grammar_lr) *grammar_lr = bridge->grammar_state.grammar_lr;
    if (phoneme_lr) *phoneme_lr = bridge->phoneme_state.phoneme_lr;
    if (semantic_lr) *semantic_lr = bridge->semantic_state.binding_lr;

    return 0;
}

int language_training_bridge_apply_training_update(
    language_training_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_apply_training_update: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_apply_training_update: bridge->active is NULL");
        return -1;
    }

    /* Clear processed items from queues */
    bridge->vocab_state.new_word_count = 0;
    bridge->semantic_state.num_updates = 0;

    return 0;
}

//=============================================================================
// Update and Query API Implementation
//=============================================================================

int language_training_bridge_update(
    language_training_bridge_t* bridge,
    uint64_t current_time_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->active) return 0;

    bridge->stats.last_update_time_ms = current_time_ms;

    /* Decay eligibility traces */
    for (uint32_t i = 0; i < bridge->phoneme_state.trace_dim; i++) {
        bridge->phoneme_state.eligibility_trace[i] *= 0.99f;
    }

    /* Update effective learning rates */
    bridge->stats.effective_vocab_lr = bridge->vocab_state.vocabulary_lr;
    bridge->stats.effective_grammar_lr = bridge->grammar_state.grammar_lr;
    bridge->stats.effective_phoneme_lr = bridge->phoneme_state.phoneme_lr *
                                          bridge->phoneme_state.plasticity_scale;

    /* Compute error rate */
    if (bridge->stats.vocabulary_updates > 0) {
        bridge->stats.error_rate = (float)bridge->stats.total_errors /
                                   (float)bridge->stats.vocabulary_updates;
    }

    return 0;
}

int language_training_bridge_get_events(
    const language_training_bridge_t* bridge,
    learning_event_record_t* events,
    uint32_t max_events)
{
    if (!bridge || !events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_get_events: required parameter is NULL (bridge, events)");
        return -1;
    }
    if (!bridge->event_log) return 0;

    uint32_t copy_count = max_events < bridge->event_log_size ?
                          max_events : bridge->event_log_size;

    memcpy(events, bridge->event_log, copy_count * sizeof(learning_event_record_t));
    return (int)copy_count;
}

int language_training_bridge_get_stats(
    const language_training_bridge_t* bridge,
    language_training_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(language_training_stats_t));
    return 0;
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

int language_training_bridge_bio_async_register(
    language_training_bridge_t* bridge,
    bio_router_t* router)
{
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_bio_async_register: required parameter is NULL (bridge, router)");
        return -1;
    }

    bridge->bio_router = router;
    bridge->bio_async_registered = true;

    LOG_DEBUG(LOG_MODULE, "Registered with bio-async router");
    return 0;
}

int language_training_bridge_bio_async_unregister(language_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_training_bridge_bio_async_unregister: bridge is NULL");
        return -1;
    }

    bridge->bio_router = NULL;
    bridge->bio_async_registered = false;

    LOG_DEBUG(LOG_MODULE, "Unregistered from bio-async router");
    return 0;
}
