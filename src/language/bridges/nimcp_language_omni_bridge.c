#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_omni_bridge.c - Language-Omni Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_omni_bridge.c
 * @brief Implementation of Language-Omni bridge for predictive processing
 *
 * @version 1.0.0 - Phase L4: Advanced Language Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_omni_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LANG_OMNI_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_omni_bridge)

//=============================================================================
// Internal Helper Functions
//=============================================================================

static int init_phoneme_prediction_state(language_omni_bridge_t* bridge) {
    phoneme_prediction_state_t* state = &bridge->phoneme_pred;

    state->max_predictions = LANGUAGE_OMNI_MAX_PHONEME_PREDICTIONS;
    state->predictions = (language_prediction_t*)nimcp_calloc(
        state->max_predictions, sizeof(language_prediction_t));
    if (!state->predictions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_phoneme_prediction_state: state->predictions is NULL");
        return -1;
    }

    state->num_predictions = 0;
    state->horizon = LANGUAGE_OMNI_DEFAULT_PHONEME_HORIZON;
    state->precision_phoneme = LANGUAGE_OMNI_DEFAULT_PRECISION;
    state->hit_rate = 0.0f;
    state->total_predictions = 0;
    state->confirmed_predictions = 0;

    return 0;
}

static int init_word_prediction_state(language_omni_bridge_t* bridge) {
    word_prediction_state_t* state = &bridge->word_pred;

    state->max_predictions = LANGUAGE_OMNI_MAX_WORD_PREDICTIONS;
    state->predictions = (language_prediction_t*)nimcp_calloc(
        state->max_predictions, sizeof(language_prediction_t));
    if (!state->predictions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_word_prediction_state: state->predictions is NULL");
        return -1;
    }

    state->num_predictions = 0;
    state->horizon = LANGUAGE_OMNI_DEFAULT_WORD_HORIZON;
    state->precision_word = LANGUAGE_OMNI_DEFAULT_PRECISION;

    state->context_dim = 256;
    state->context_vector = (float*)nimcp_calloc(state->context_dim, sizeof(float));
    if (!state->context_vector) {
        nimcp_free(state->predictions);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_word_prediction_state: state->context_vector is NULL");
        return -1;
    }

    state->hit_rate = 0.0f;
    state->total_predictions = 0;
    state->confirmed_predictions = 0;

    return 0;
}

static int init_semantic_prediction_state(language_omni_bridge_t* bridge) {
    semantic_prediction_state_t* state = &bridge->semantic_pred;

    state->max_predictions = LANGUAGE_OMNI_MAX_SEMANTIC_PREDICTIONS;
    state->predictions = (language_prediction_t*)nimcp_calloc(
        state->max_predictions, sizeof(language_prediction_t));
    if (!state->predictions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_semantic_prediction_state: state->predictions is NULL");
        return -1;
    }

    state->num_predictions = 0;
    state->precision_semantic = LANGUAGE_OMNI_DEFAULT_PRECISION;

    state->semantic_dim = 128;
    state->predicted_semantic = (float*)nimcp_calloc(state->semantic_dim, sizeof(float));
    if (!state->predicted_semantic) {
        nimcp_free(state->predictions);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_semantic_prediction_state: state->predicted_semantic is NULL");
        return -1;
    }

    state->semantic_surprise = 0.0f;
    state->n400_amplitude = 0.0f;

    return 0;
}

static int init_error_queue(language_omni_bridge_t* bridge) {
    prediction_error_queue_t* queue = &bridge->error_queue;

    queue->max_errors = 64;
    queue->errors = (language_prediction_error_t*)nimcp_calloc(
        queue->max_errors, sizeof(language_prediction_error_t));
    if (!queue->errors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_error_queue: queue->errors is NULL");
        return -1;
    }

    queue->num_errors = 0;
    queue->avg_phoneme_error = 0.0f;
    queue->avg_word_error = 0.0f;
    queue->avg_semantic_error = 0.0f;
    queue->total_free_energy = 0.0f;

    return 0;
}

static int init_jepa_state(language_omni_bridge_t* bridge) {
    jepa_connection_state_t* state = &bridge->jepa_state;

    state->connected = false;
    state->jepa = NULL;

    state->embedding_dim = 256;
    state->current_embedding = (float*)nimcp_calloc(state->embedding_dim, sizeof(float));
    state->predicted_embedding = (float*)nimcp_calloc(state->embedding_dim, sizeof(float));
    if (!state->current_embedding || !state->predicted_embedding) {
        nimcp_free(state->current_embedding);
        nimcp_free(state->predicted_embedding);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_jepa_state: required parameter is NULL (state->current_embedding, state->predicted_embedding)");
        return -1;
    }

    state->jepa_prediction_error = 0.0f;

    return 0;
}

static void cleanup_all(language_omni_bridge_t* bridge) {
    if (!bridge) return;

    /* Phoneme predictions */
    if (bridge->phoneme_pred.predictions) {
        nimcp_free(bridge->phoneme_pred.predictions);
    }

    /* Word predictions */
    if (bridge->word_pred.predictions) {
        nimcp_free(bridge->word_pred.predictions);
    }
    if (bridge->word_pred.context_vector) {
        nimcp_free(bridge->word_pred.context_vector);
    }

    /* Semantic predictions */
    if (bridge->semantic_pred.predictions) {
        nimcp_free(bridge->semantic_pred.predictions);
    }
    if (bridge->semantic_pred.predicted_semantic) {
        nimcp_free(bridge->semantic_pred.predicted_semantic);
    }

    /* Error queue */
    if (bridge->error_queue.errors) {
        nimcp_free(bridge->error_queue.errors);
    }

    /* JEPA state */
    if (bridge->jepa_state.current_embedding) {
        nimcp_free(bridge->jepa_state.current_embedding);
    }
    if (bridge->jepa_state.predicted_embedding) {
        nimcp_free(bridge->jepa_state.predicted_embedding);
    }
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_omni_bridge_t* language_omni_bridge_create(const language_omni_config_t* config) {
    language_omni_bridge_t* bridge = (language_omni_bridge_t*)
        nimcp_calloc(1, sizeof(language_omni_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(language_omni_config_t));
    } else {
        language_omni_default_config(&bridge->config);
    }

    /* Initialize sub-states */
    if (init_phoneme_prediction_state(bridge) != 0 ||
        init_word_prediction_state(bridge) != 0 ||
        init_semantic_prediction_state(bridge) != 0 ||
        init_error_queue(bridge) != 0 ||
        init_jepa_state(bridge) != 0) {
        cleanup_all(bridge);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "language_omni_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize precision state */
    bridge->precision.precision_phoneme = LANGUAGE_OMNI_DEFAULT_PRECISION;
    bridge->precision.precision_word = LANGUAGE_OMNI_DEFAULT_PRECISION;
    bridge->precision.precision_semantic = LANGUAGE_OMNI_DEFAULT_PRECISION;
    bridge->precision.precision_syntactic = LANGUAGE_OMNI_DEFAULT_PRECISION;
    bridge->precision.precision_lr = LANGUAGE_OMNI_DEFAULT_PRECISION_LR;
    bridge->precision.precision_modulation_enabled = true;

    bridge->inference_mode = LANG_INFER_DIR_BIDIRECTIONAL;
    bridge->initialized = true;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "Omni bridge created");
    NIMCP_LOGGING_INFO("Created %s bridge", "language_omni");
    return bridge;
}

void language_omni_bridge_destroy(language_omni_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_omni");

    if (bridge->bio_async_registered) {
        language_omni_bridge_bio_async_unregister(bridge);
    }

    cleanup_all(bridge);
    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Omni bridge destroyed");
}

int language_omni_bridge_init(language_omni_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    memset(&bridge->stats, 0, sizeof(language_omni_stats_t));
    bridge->initialized = true;
    return 0;
}

int language_omni_bridge_start(language_omni_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_start: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }
    bridge->active = true;
    LOG_INFO(LOG_MODULE, "Omni bridge started");
    return 0;
}

int language_omni_bridge_stop(language_omni_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->active = false;
    LOG_INFO(LOG_MODULE, "Omni bridge stopped");
    return 0;
}

//=============================================================================
// Connection API Implementation
//=============================================================================

int language_omni_bridge_connect_orchestrator(
    language_omni_bridge_t* bridge, language_orchestrator_t* orchestrator) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->orchestrator = orchestrator;
    return 0;
}

int language_omni_bridge_connect_jepa(
    language_omni_bridge_t* bridge, jepa_bidirectional_t* jepa) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->jepa = jepa;
    bridge->jepa_state.connected = (jepa != NULL);
    bridge->jepa_state.jepa = jepa;
    return 0;
}

int language_omni_bridge_connect_predictive_hierarchy(
    language_omni_bridge_t* bridge, predictive_hierarchy_t* pred_hierarchy) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->pred_hierarchy = pred_hierarchy;
    return 0;
}

int language_omni_bridge_connect_hopfield(
    language_omni_bridge_t* bridge, hopfield_memory_t* hopfield) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->hopfield = hopfield;
    return 0;
}

int language_omni_bridge_connect_fep(
    language_omni_bridge_t* bridge, fep_orchestrator_t* fep) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->fep = fep;
    return 0;
}

//=============================================================================
// Prediction API Implementation
//=============================================================================

int language_omni_bridge_predict_phonemes(
    language_omni_bridge_t* bridge, const uint32_t* context_phonemes, uint32_t context_length) {
    if (!bridge || !context_phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_stop: required parameter is NULL (bridge, context_phonemes)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_stop: bridge->active is NULL");
        return -1;
    }

    phoneme_prediction_state_t* state = &bridge->phoneme_pred;
    state->num_predictions = 0;

    /* Generate predictions based on context */
    uint32_t num_preds = state->horizon < state->max_predictions ?
                         state->horizon : state->max_predictions;

    for (uint32_t i = 0; i < num_preds; i++) {
        language_prediction_t* pred = &state->predictions[i];
        pred->level = PREDICTION_LEVEL_PHONEME;
        /* Simple prediction: next likely phoneme based on last */
        pred->predicted_id = context_length > 0 ?
            (context_phonemes[context_length - 1] + i + 1) % 44 : i;
        pred->probability = 0.8f - (float)i * 0.1f;
        pred->precision = state->precision_phoneme;
        pred->horizon_position = i + 1;
        pred->generation_time_ms = bridge->stats.last_update_time_ms;
        pred->confirmed = false;
        pred->violated = false;
        state->num_predictions++;
    }

    state->total_predictions += num_preds;
    bridge->stats.phoneme_predictions += num_preds;

    return (int)state->num_predictions;
}

int language_omni_bridge_predict_words(
    language_omni_bridge_t* bridge, const uint32_t* context_words, uint32_t context_length) {
    if (!bridge || !context_words) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_stop: required parameter is NULL (bridge, context_words)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_stop: bridge->active is NULL");
        return -1;
    }

    word_prediction_state_t* state = &bridge->word_pred;
    state->num_predictions = 0;

    uint32_t num_preds = state->horizon < state->max_predictions ?
                         state->horizon : state->max_predictions;

    for (uint32_t i = 0; i < num_preds; i++) {
        language_prediction_t* pred = &state->predictions[i];
        pred->level = PREDICTION_LEVEL_WORD;
        pred->predicted_id = context_length > 0 ?
            (context_words[context_length - 1] + i + 1) % 10000 : i;
        pred->probability = 0.7f - (float)i * 0.15f;
        pred->precision = state->precision_word;
        pred->horizon_position = i + 1;
        pred->generation_time_ms = bridge->stats.last_update_time_ms;
        pred->confirmed = false;
        pred->violated = false;
        state->num_predictions++;
    }

    state->total_predictions += num_preds;
    bridge->stats.word_predictions += num_preds;

    return (int)state->num_predictions;
}

int language_omni_bridge_predict_semantic(
    language_omni_bridge_t* bridge, const float* context_vector, uint32_t context_dim) {
    if (!bridge || !context_vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_stop: required parameter is NULL (bridge, context_vector)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_stop: bridge->active is NULL");
        return -1;
    }

    semantic_prediction_state_t* state = &bridge->semantic_pred;

    /* Update semantic prediction based on context */
    uint32_t copy_dim = context_dim < state->semantic_dim ?
                        context_dim : state->semantic_dim;
    for (uint32_t i = 0; i < copy_dim; i++) {
        state->predicted_semantic[i] = tanhf(context_vector[i] * 0.9f);
    }

    bridge->stats.semantic_predictions++;
    return 0;
}

int language_omni_bridge_get_predictions(
    const language_omni_bridge_t* bridge, prediction_level_t level,
    language_prediction_t* predictions, uint32_t max_predictions) {
    if (!bridge || !predictions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_stop: required parameter is NULL (bridge, predictions)");
        return -1;
    }

    const language_prediction_t* src = NULL;
    uint32_t count = 0;

    switch (level) {
        case PREDICTION_LEVEL_PHONEME:
            src = bridge->phoneme_pred.predictions;
            count = bridge->phoneme_pred.num_predictions;
            break;
        case PREDICTION_LEVEL_WORD:
            src = bridge->word_pred.predictions;
            count = bridge->word_pred.num_predictions;
            break;
        case PREDICTION_LEVEL_SEMANTIC:
            src = bridge->semantic_pred.predictions;
            count = bridge->semantic_pred.num_predictions;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_omni_bridge_stop: operation failed");
            return -1;
    }

    uint32_t copy_count = count < max_predictions ? count : max_predictions;
    memcpy(predictions, src, copy_count * sizeof(language_prediction_t));

    return (int)copy_count;
}

//=============================================================================
// Prediction Error API Implementation
//=============================================================================

language_prediction_error_t language_omni_bridge_compute_error(
    language_omni_bridge_t* bridge, prediction_level_t level, uint32_t observed_id) {
    language_prediction_error_t error = {0};

    if (!bridge || !bridge->active) return error;

    error.level = level;
    error.actual_id = observed_id;
    error.timestamp_ms = bridge->stats.last_update_time_ms;

    /* Find best matching prediction */
    const language_prediction_t* preds = NULL;
    uint32_t num_preds = 0;

    switch (level) {
        case PREDICTION_LEVEL_PHONEME:
            preds = bridge->phoneme_pred.predictions;
            num_preds = bridge->phoneme_pred.num_predictions;
            error.type = PE_TYPE_MMN;
            break;
        case PREDICTION_LEVEL_WORD:
            preds = bridge->word_pred.predictions;
            num_preds = bridge->word_pred.num_predictions;
            error.type = PE_TYPE_N400;
            break;
        case PREDICTION_LEVEL_SEMANTIC:
            preds = bridge->semantic_pred.predictions;
            num_preds = bridge->semantic_pred.num_predictions;
            error.type = PE_TYPE_N400;
            break;
        default:
            return error;
    }

    /* Compute error magnitude */
    float min_error = 1.0f;
    for (uint32_t i = 0; i < num_preds; i++) {
        if (preds[i].predicted_id == observed_id) {
            min_error = 0.0f;
            error.predicted_id = preds[i].predicted_id;
            error.precision = preds[i].precision;
            break;
        }
        float err = 1.0f - preds[i].probability;
        if (err < min_error) {
            min_error = err;
            error.predicted_id = preds[i].predicted_id;
            error.precision = preds[i].precision;
        }
    }

    error.error_magnitude = min_error;

    return error;
}

int language_omni_bridge_report_error(
    language_omni_bridge_t* bridge, const language_prediction_error_t* error) {
    if (!bridge || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, error)");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->active is NULL");
        return -1;
    }

    prediction_error_queue_t* queue = &bridge->error_queue;

    if (queue->num_errors >= queue->max_errors) {
        /* Remove oldest error */
        memmove(&queue->errors[0], &queue->errors[1],
                (queue->max_errors - 1) * sizeof(language_prediction_error_t));
        queue->num_errors--;
    }

    memcpy(&queue->errors[queue->num_errors], error, sizeof(language_prediction_error_t));
    queue->num_errors++;

    /* Update stats */
    if (error->type == PE_TYPE_N400) bridge->stats.n400_events++;
    if (error->type == PE_TYPE_P600) bridge->stats.p600_events++;

    queue->total_free_energy += error->error_magnitude;

    return 0;
}

int language_omni_bridge_get_errors(
    language_omni_bridge_t* bridge, language_prediction_error_t* errors, uint32_t max_errors) {
    if (!bridge || !errors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, errors)");
        return -1;
    }

    prediction_error_queue_t* queue = &bridge->error_queue;
    uint32_t count = queue->num_errors < max_errors ? queue->num_errors : max_errors;

    memcpy(errors, queue->errors, count * sizeof(language_prediction_error_t));

    return (int)count;
}

float language_omni_bridge_get_free_energy(const language_omni_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->stats.current_free_energy;
}

//=============================================================================
// Precision API Implementation
//=============================================================================

int language_omni_bridge_set_precision(
    language_omni_bridge_t* bridge, prediction_level_t level, float precision) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    switch (level) {
        case PREDICTION_LEVEL_PHONEME:
            bridge->precision.precision_phoneme = precision;
            bridge->phoneme_pred.precision_phoneme = precision;
            break;
        case PREDICTION_LEVEL_WORD:
            bridge->precision.precision_word = precision;
            bridge->word_pred.precision_word = precision;
            break;
        case PREDICTION_LEVEL_SEMANTIC:
            bridge->precision.precision_semantic = precision;
            bridge->semantic_pred.precision_semantic = precision;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_omni_bridge_get_free_energy: operation failed");
            return -1;
    }

    return 0;
}

float language_omni_bridge_get_precision(
    const language_omni_bridge_t* bridge, prediction_level_t level) {
    if (!bridge) return 0.0f;

    switch (level) {
        case PREDICTION_LEVEL_PHONEME:
            return bridge->precision.precision_phoneme;
        case PREDICTION_LEVEL_WORD:
            return bridge->precision.precision_word;
        case PREDICTION_LEVEL_SEMANTIC:
            return bridge->precision.precision_semantic;
        default:
            return 0.0f;
    }
}

int language_omni_bridge_update_precision(language_omni_bridge_t* bridge) {
    if (!bridge || !bridge->precision.precision_modulation_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_update_precision: required parameter is NULL (bridge, bridge->precision)");
        return -1;
    }

    float lr = bridge->precision.precision_lr;
    prediction_error_queue_t* queue = &bridge->error_queue;

    /* Update precision based on prediction errors */
    if (queue->num_errors > 0) {
        /* Lower precision when errors are high */
        float avg_error = queue->total_free_energy / (float)queue->num_errors;
        bridge->precision.precision_phoneme -= lr * avg_error * 0.1f;
        bridge->precision.precision_word -= lr * avg_error * 0.1f;
        bridge->precision.precision_semantic -= lr * avg_error * 0.1f;

        /* Clamp */
        if (bridge->precision.precision_phoneme < 0.1f)
            bridge->precision.precision_phoneme = 0.1f;
        if (bridge->precision.precision_word < 0.1f)
            bridge->precision.precision_word = 0.1f;
        if (bridge->precision.precision_semantic < 0.1f)
            bridge->precision.precision_semantic = 0.1f;
    }

    return 0;
}

int language_omni_bridge_set_precision_lr(language_omni_bridge_t* bridge, float lr) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->precision.precision_lr = lr;
    return 0;
}

//=============================================================================
// Inference Mode API Implementation
//=============================================================================

int language_omni_bridge_set_inference_mode(
    language_omni_bridge_t* bridge, language_inference_direction_t direction) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->inference_mode = direction;
    return 0;
}

language_inference_direction_t language_omni_bridge_get_inference_mode(
    const language_omni_bridge_t* bridge) {
    if (!bridge) return LANG_INFER_DIR_BIDIRECTIONAL;
    return bridge->inference_mode;
}

//=============================================================================
// Update and Query API Implementation
//=============================================================================

int language_omni_bridge_update(language_omni_bridge_t* bridge, uint64_t current_time_ms) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->active) return 0;

    bridge->stats.last_update_time_ms = current_time_ms;

    /* Update free energy estimate */
    prediction_error_queue_t* queue = &bridge->error_queue;
    if (queue->num_errors > 0) {
        bridge->stats.current_free_energy = queue->total_free_energy / (float)queue->num_errors;
        bridge->stats.avg_free_energy =
            bridge->stats.avg_free_energy * 0.95f + bridge->stats.current_free_energy * 0.05f;
    }

    /* Update accuracies */
    if (bridge->phoneme_pred.total_predictions > 0) {
        bridge->stats.phoneme_accuracy =
            (float)bridge->phoneme_pred.confirmed_predictions /
            (float)bridge->phoneme_pred.total_predictions;
    }
    if (bridge->word_pred.total_predictions > 0) {
        bridge->stats.word_accuracy =
            (float)bridge->word_pred.confirmed_predictions /
            (float)bridge->word_pred.total_predictions;
    }

    /* Update precision */
    language_omni_bridge_update_precision(bridge);

    return 0;
}

int language_omni_bridge_get_stats(
    const language_omni_bridge_t* bridge, language_omni_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_update: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(language_omni_stats_t));
    return 0;
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

int language_omni_bridge_bio_async_register(
    language_omni_bridge_t* bridge, bio_router_t* router) {
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_omni_bridge_update: required parameter is NULL (bridge, router)");
        return -1;
    }
    bridge->bio_router = router;
    bridge->bio_async_registered = true;
    return 0;
}

int language_omni_bridge_bio_async_unregister(language_omni_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->bio_router = NULL;
    bridge->bio_async_registered = false;
    return 0;
}

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* prediction_level_to_string(prediction_level_t level) {
    switch (level) {
        case PREDICTION_LEVEL_PHONEME: return "phoneme";
        case PREDICTION_LEVEL_WORD: return "word";
        case PREDICTION_LEVEL_PHRASE: return "phrase";
        case PREDICTION_LEVEL_SEMANTIC: return "semantic";
        case PREDICTION_LEVEL_DISCOURSE: return "discourse";
        default: return "unknown";
    }
}

const char* prediction_error_type_to_string(prediction_error_type_t type) {
    switch (type) {
        case PE_TYPE_MISMATCH: return "mismatch";
        case PE_TYPE_N400: return "N400";
        case PE_TYPE_P600: return "P600";
        case PE_TYPE_MMN: return "MMN";
        default: return "unknown";
    }
}

const char* language_inference_direction_to_string(language_inference_direction_t direction) {
    switch (direction) {
        case LANG_INFER_DIR_BOTTOM_UP: return "bottom-up";
        case LANG_INFER_DIR_TOP_DOWN: return "top-down";
        case LANG_INFER_DIR_BIDIRECTIONAL: return "bidirectional";
        default: return "unknown";
    }
}
