/**
 * @file nimcp_omni_wernicke_bridge.c
 * @brief Omnidirectional Inference to Wernicke's Area Bridge Implementation
 *
 * Implements predictive language comprehension with:
 * - Forward prediction: phoneme/word/semantic expectations
 * - Backward inference: meaning from word sequences
 * - Cross-modal integration: audiovisual speech fusion
 * - N400-like semantic surprise detection
 * - Knowledge graph synchronization
 *
 * @version Phase W4: Omnidirectional Integration
 * @date 2026-01-04
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/wernicke/nimcp_omni_wernicke_bridge.h"
#include "core/brain/nimcp_brain_kg.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_neural_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(omni_wernicke_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OMNI_WERNICKE_BRIDGE"


/*=============================================================================
 * INTERNAL CONSTANTS
 *=============================================================================*/

/* Default configuration values */
#define DEFAULT_PHONEME_HORIZON        3
#define DEFAULT_WORD_CANDIDATES        5
#define DEFAULT_SEMANTIC_DEPTH         3
#define DEFAULT_PHONEME_PRECISION      0.8f
#define DEFAULT_WORD_PRECISION         0.7f
#define DEFAULT_SEMANTIC_PRECISION     0.6f
#define DEFAULT_PE_THRESHOLD           0.3f
#define DEFAULT_N400_THRESHOLD         0.5f
#define DEFAULT_RECOGNITION_THRESHOLD  0.9f
#define DEFAULT_AV_COHERENCE_THRESHOLD NIMCP_PHASE_COHERENCE_THRESHOLD
#define DEFAULT_BROCA_FEEDBACK_WEIGHT  0.3f

/* Buffer sizes */
#define PHONEME_HISTORY_SIZE           16
#define CONTEXT_EMBEDDING_DIM          NIMCP_SMALL_EMBEDDING_DIM
#define MAX_WORD_LEN                   32

/* Precision decay rate */
#define PRECISION_DECAY_RATE           0.95f

/*=============================================================================
 * INTERNAL HELPERS
 *=============================================================================*/

/**
 * @brief Compute softmax for probability distribution
 */
static void softmax(float* values, uint32_t count) {
    if (!values || count == 0) return;

    /* Find max for numerical stability */
    float max_val = values[0];
    for (uint32_t i = 1; i < count; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    /* Normalize */
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < count; i++) {
            values[i] /= sum;
        }
    }
}

/**
 * @brief Compute prediction error between expected and observed
 */
static float compute_pe(const float* expected, const float* observed,
                        uint32_t dim, float precision) {
    if (!expected || !observed || dim == 0) return 0.0f;

    float mse = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = expected[i] - observed[i];
        mse += diff * diff;
    }
    mse /= (float)dim;

    /* Weight by precision */
    return sqrtf(mse) * precision;
}

/**
 * @brief Update running average
 */
static float update_avg(float current_avg, float new_value, uint64_t count) {
    if (count == 0) return isfinite(new_value) ? new_value : 0.0f;
    float n = (float)count;
    float result = ((n - 1.0f) * current_avg + new_value) / n;
    return isfinite(result) ? result : current_avg;
}

/**
 * @brief Initialize prediction structures
 */
static void init_predictions(omni_wernicke_bridge_t* bridge) {
    /* Phoneme prediction */
    bridge->omni_effects.phoneme_pred.phoneme_probs = NULL;
    bridge->omni_effects.phoneme_pred.num_phonemes = 0;
    bridge->omni_effects.phoneme_pred.predicted_phoneme = 0;
    bridge->omni_effects.phoneme_pred.confidence = 0.5f;
    bridge->omni_effects.phoneme_pred.coarticulation_bias = 0.0f;

    /* Word prediction */
    bridge->omni_effects.word_pred.word_candidates = NULL;
    bridge->omni_effects.word_pred.word_probs = NULL;
    bridge->omni_effects.word_pred.num_candidates = 0;
    bridge->omni_effects.word_pred.cohort_size = 0;
    bridge->omni_effects.word_pred.uniqueness_point = 0.0f;
    bridge->omni_effects.word_pred.recognition_complete = false;

    /* Semantic prediction */
    bridge->omni_effects.semantic_pred.predicted_concepts = NULL;
    bridge->omni_effects.semantic_pred.concept_activations = NULL;
    bridge->omni_effects.semantic_pred.num_concepts = 0;
    bridge->omni_effects.semantic_pred.semantic_coherence = 1.0f;
    bridge->omni_effects.semantic_pred.n400_magnitude = 0.0f;
    bridge->omni_effects.semantic_pred.semantic_violation = false;

    /* Cross-modal */
    bridge->omni_effects.crossmodal.audio_prediction = NULL;
    bridge->omni_effects.crossmodal.audio_dim = 0;
    bridge->omni_effects.crossmodal.visual_prediction = NULL;
    bridge->omni_effects.crossmodal.visual_dim = 0;
    bridge->omni_effects.crossmodal.audiovisual_coherence = 1.0f;
    bridge->omni_effects.crossmodal.mcgurk_conflict = false;
    bridge->omni_effects.crossmodal.fusion_weight_audio = 0.6f;
    bridge->omni_effects.crossmodal.fusion_weight_visual = 0.4f;

    /* Precision weights */
    bridge->omni_effects.phoneme_precision = bridge->config.default_phoneme_precision;
    bridge->omni_effects.word_precision = bridge->config.default_word_precision;
    bridge->omni_effects.semantic_precision = bridge->config.default_semantic_precision;

    /* Mode and level */
    bridge->omni_effects.mode = OMNI_WERNICKE_MODE_LISTENING;
    bridge->omni_effects.processing_level = OMNI_WERNICKE_LEVEL_PHONEME;
}

/**
 * @brief Initialize Wernicke effects
 */
static void init_wernicke_effects(omni_wernicke_bridge_t* bridge) {
    memset(&bridge->wernicke_effects, 0, sizeof(wernicke_to_omni_effects_t));
    bridge->wernicke_effects.comprehension_confidence = 1.0f;
    bridge->wernicke_effects.recognized_word = NULL;
}

/**
 * @brief Initialize world update state
 */
static void init_world_update(omni_wernicke_bridge_t* bridge) {
    memset(&bridge->world_update, 0, sizeof(omni_wernicke_world_update_t));
}

/**
 * @brief Initialize KG sync state
 */
static void init_kg_sync(omni_wernicke_bridge_t* bridge) {
    memset(&bridge->kg_sync, 0, sizeof(omni_wernicke_kg_sync_t));
}

/*=============================================================================
 * CONFIGURATION API
 *=============================================================================*/

int omni_wernicke_default_config(omni_wernicke_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    *config = (omni_wernicke_config_t){
        /* Prediction horizons */
        .phoneme_horizon = DEFAULT_PHONEME_HORIZON,
        .word_candidates = DEFAULT_WORD_CANDIDATES,
        .semantic_depth = DEFAULT_SEMANTIC_DEPTH,

        /* Precision defaults */
        .default_phoneme_precision = DEFAULT_PHONEME_PRECISION,
        .default_word_precision = DEFAULT_WORD_PRECISION,
        .default_semantic_precision = DEFAULT_SEMANTIC_PRECISION,

        /* Thresholds */
        .pe_threshold = DEFAULT_PE_THRESHOLD,
        .n400_threshold = DEFAULT_N400_THRESHOLD,
        .recognition_threshold = DEFAULT_RECOGNITION_THRESHOLD,

        /* Cross-modal */
        .enable_audiovisual = true,
        .av_coherence_threshold = DEFAULT_AV_COHERENCE_THRESHOLD,
        .disambig_strategy = OMNI_WERNICKE_DISAMBIG_BAYESIAN,

        /* Broca integration */
        .enable_broca_feedback = true,
        .broca_feedback_weight = DEFAULT_BROCA_FEEDBACK_WEIGHT,

        /* KG integration */
        .enable_kg_sync = true,
        .enable_world_model = true,

        /* Bio-async */
        .enable_bio_async = false,
        .enable_logging = false
    };

    return 0;
}

/*=============================================================================
 * LIFECYCLE API
 *=============================================================================*/

omni_wernicke_bridge_t* omni_wernicke_bridge_create(
    const omni_wernicke_config_t* config
) {
    omni_wernicke_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wernicke_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wernicke_default_config(&bridge->config);
    }

    /* Allocate phoneme history */
    bridge->phoneme_history = nimcp_calloc(PHONEME_HISTORY_SIZE, sizeof(float));
    if (!bridge->phoneme_history) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wernicke_bridge_create: bridge->phoneme_history is NULL");
        return NULL;
    }
    bridge->phoneme_history_len = 0;

    /* Allocate context embedding */
    bridge->context_dim = CONTEXT_EMBEDDING_DIM;
    bridge->word_context = nimcp_calloc(CONTEXT_EMBEDDING_DIM, sizeof(float));
    if (!bridge->word_context) {
        nimcp_free(bridge->phoneme_history);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wernicke_bridge_create: bridge->word_context is NULL");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "omni_wernicke") != 0) {
        nimcp_free(bridge->word_context);
        nimcp_free(bridge->phoneme_history);
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize structures */
    init_predictions(bridge);
    init_wernicke_effects(bridge);
    init_world_update(bridge);
    init_kg_sync(bridge);

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(omni_wernicke_stats_t));

    return bridge;
}

void omni_wernicke_bridge_destroy(omni_wernicke_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_wernicke");

    /* Cleanup bridge base */
    bridge_base_cleanup(&bridge->base);

    /* Free phoneme prediction */
    nimcp_free(bridge->omni_effects.phoneme_pred.phoneme_probs);

    /* Free word prediction */
    if (bridge->omni_effects.word_pred.word_candidates) {
        for (uint32_t i = 0; i < bridge->omni_effects.word_pred.num_candidates; i++) {
            nimcp_free(bridge->omni_effects.word_pred.word_candidates[i]);
        }
        nimcp_free(bridge->omni_effects.word_pred.word_candidates);
    }
    nimcp_free(bridge->omni_effects.word_pred.word_probs);

    /* Free semantic prediction */
    nimcp_free(bridge->omni_effects.semantic_pred.predicted_concepts);
    nimcp_free(bridge->omni_effects.semantic_pred.concept_activations);

    /* Free cross-modal */
    nimcp_free(bridge->omni_effects.crossmodal.audio_prediction);
    nimcp_free(bridge->omni_effects.crossmodal.visual_prediction);

    /* Free Wernicke effects */
    nimcp_free(bridge->wernicke_effects.recognized_word);

    /* Free world update */
    nimcp_free(bridge->world_update.updated_concepts);
    nimcp_free(bridge->world_update.concept_deltas);

    /* Free internal state */
    nimcp_free(bridge->phoneme_history);
    nimcp_free(bridge->word_context);

    nimcp_free(bridge);
}

/*=============================================================================
 * CONNECTION API
 *=============================================================================*/

int omni_wernicke_connect_jepa(omni_wernicke_bridge_t* bridge, void* jepa) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->jepa = jepa;
    return 0;
}

int omni_wernicke_connect_pred_hier(omni_wernicke_bridge_t* bridge, void* pred_hier) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->pred_hier = pred_hier;
    return 0;
}

int omni_wernicke_connect_hopfield(omni_wernicke_bridge_t* bridge, void* hopfield) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->hopfield = hopfield;
    return 0;
}

int omni_wernicke_connect_wernicke(omni_wernicke_bridge_t* bridge, void* wernicke) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->wernicke = wernicke;
    return 0;
}

int omni_wernicke_connect_broca_bridge(omni_wernicke_bridge_t* bridge, void* broca_bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->broca_bridge = broca_bridge;
    return 0;
}

int omni_wernicke_connect_audiovisual(omni_wernicke_bridge_t* bridge, void* audiovisual) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->audiovisual = audiovisual;
    return 0;
}

int omni_wernicke_connect_semantic(omni_wernicke_bridge_t* bridge, void* semantic_memory) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->semantic_memory = semantic_memory;
    return 0;
}

int omni_wernicke_connect_kg(omni_wernicke_bridge_t* bridge, void* brain_kg) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->brain_kg = brain_kg;
    return 0;
}

/*=============================================================================
 * UPDATE API
 *=============================================================================*/

int omni_wernicke_update(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Update statistics */
    bridge->stats.total_updates++;

    /* Generate predictions based on current mode */
    switch (bridge->omni_effects.mode) {
        case OMNI_WERNICKE_MODE_LISTENING:
            /* Passive: generate mild expectations */
            bridge->omni_effects.phoneme_precision *= PRECISION_DECAY_RATE;
            break;

        case OMNI_WERNICKE_MODE_COMPREHENDING:
            /* Active: full prediction cycle */
            omni_wernicke_predict_phoneme(bridge, &bridge->omni_effects.phoneme_pred);
            omni_wernicke_predict_word(bridge, &bridge->omni_effects.word_pred);
            omni_wernicke_predict_semantic(bridge, &bridge->omni_effects.semantic_pred);
            break;

        case OMNI_WERNICKE_MODE_PREDICTING:
            /* Strong predictions */
            bridge->omni_effects.phoneme_precision = bridge->config.default_phoneme_precision;
            bridge->omni_effects.word_precision = bridge->config.default_word_precision;
            omni_wernicke_predict_phoneme(bridge, &bridge->omni_effects.phoneme_pred);
            omni_wernicke_predict_word(bridge, &bridge->omni_effects.word_pred);
            break;

        case OMNI_WERNICKE_MODE_MONITORING:
            /* Self-monitoring via Broca feedback */
            if (bridge->broca_bridge) {
                /* Would receive efference copy here */
            }
            break;

        case OMNI_WERNICKE_MODE_REHEARSING:
            /* Working memory rehearsal - maintain context */
            break;
    }

    /* Cross-modal prediction if enabled */
    if (bridge->config.enable_audiovisual && bridge->audiovisual) {
        omni_wernicke_predict_crossmodal(bridge, &bridge->omni_effects.crossmodal);
    }

    /* Compute combined free energy */
    bridge->wernicke_effects.free_energy =
        bridge->omni_effects.phoneme_precision * bridge->wernicke_effects.phoneme_pe +
        bridge->omni_effects.word_precision * bridge->wernicke_effects.word_pe +
        bridge->omni_effects.semantic_precision * bridge->wernicke_effects.semantic_pe;

    /* Update running averages */
    bridge->stats.avg_free_energy = update_avg(
        bridge->stats.avg_free_energy,
        bridge->wernicke_effects.free_energy,
        bridge->stats.total_updates
    );

    /* KG sync if enabled */
    if (bridge->config.enable_kg_sync && bridge->brain_kg) {
        omni_wernicke_kg_sync(bridge);
    }

    return 0;
}

int omni_wernicke_apply_to_wernicke(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Apply top-down predictions to Wernicke adapter */
    /* This would call Wernicke API to set expectations */

    return 0;
}

int omni_wernicke_apply_to_omni(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Apply bottom-up prediction errors to omni system */
    /* This would call JEPA/pred_hier APIs */

    return 0;
}

/*=============================================================================
 * FORWARD PREDICTION API (Top-Down)
 *=============================================================================*/

int omni_wernicke_predict_phoneme(omni_wernicke_bridge_t* bridge,
                                   omni_phoneme_prediction_t* prediction) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
        return -1;
    }

    /* For now, use uniform distribution as placeholder */
    /* In full implementation, would use:
     * - JEPA latent predictions
     * - Predictive hierarchy expectations
     * - Coarticulation model
     */

    uint32_t num_phonemes = 44;  /* Approximate English phoneme count */

    /* Allocate if needed */
    if (!prediction->phoneme_probs || prediction->num_phonemes != num_phonemes) {
        nimcp_free(prediction->phoneme_probs);
        prediction->phoneme_probs = nimcp_calloc(num_phonemes, sizeof(float));
        if (!prediction->phoneme_probs) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wernicke_apply_to_omni: prediction->phoneme_probs is NULL");
            return -1;
        }
        prediction->num_phonemes = num_phonemes;
    }

    /* Generate predictions based on history */
    if (bridge->phoneme_history_len > 0) {
        /* Use last phoneme to bias predictions (simple bigram model) */
        float last_phoneme = bridge->phoneme_history[bridge->phoneme_history_len - 1];
        for (uint32_t i = 0; i < num_phonemes; i++) {
            /* Simple bias based on distance from last phoneme */
            float dist = fabsf((float)i - last_phoneme);
            prediction->phoneme_probs[i] = 1.0f / (1.0f + dist * 0.1f);
        }
        softmax(prediction->phoneme_probs, num_phonemes);
    } else {
        /* Uniform prior */
        float uniform = 1.0f / (float)num_phonemes;
        for (uint32_t i = 0; i < num_phonemes; i++) {
            prediction->phoneme_probs[i] = uniform;
        }
    }

    /* Find most likely phoneme */
    uint32_t best_idx = 0;
    float best_prob = prediction->phoneme_probs[0];
    for (uint32_t i = 1; i < num_phonemes; i++) {
        if (prediction->phoneme_probs[i] > best_prob) {
            best_prob = prediction->phoneme_probs[i];
            best_idx = i;
        }
    }

    prediction->predicted_phoneme = best_idx;
    prediction->confidence = best_prob;
    prediction->coarticulation_bias = 0.0f;  /* TODO: implement */

    bridge->stats.phoneme_predictions++;

    return 0;
}

int omni_wernicke_predict_word(omni_wernicke_bridge_t* bridge,
                                omni_word_prediction_t* prediction) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
        return -1;
    }

    /* Placeholder: would use semantic memory and lexicon */
    /* For now, return empty prediction */

    prediction->num_candidates = 0;
    prediction->cohort_size = 0;
    prediction->uniqueness_point = 0.0f;
    prediction->recognition_complete = false;

    bridge->stats.word_predictions++;

    return 0;
}

int omni_wernicke_predict_semantic(omni_wernicke_bridge_t* bridge,
                                    omni_semantic_prediction_t* prediction) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
        return -1;
    }

    /* Placeholder: would use semantic memory spreading activation */

    prediction->num_concepts = 0;
    prediction->semantic_coherence = 1.0f;
    prediction->n400_magnitude = 0.0f;
    prediction->semantic_violation = false;

    bridge->stats.semantic_predictions++;

    return 0;
}

int omni_wernicke_predict_crossmodal(omni_wernicke_bridge_t* bridge,
                                      omni_crossmodal_prediction_t* prediction) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
        return -1;
    }

    /* Placeholder: would integrate audio and visual predictions */

    prediction->audiovisual_coherence = 1.0f;
    prediction->mcgurk_conflict = false;
    prediction->fusion_weight_audio = 0.6f;
    prediction->fusion_weight_visual = 0.4f;

    return 0;
}

/*=============================================================================
 * BACKWARD INFERENCE API (Bottom-Up)
 *=============================================================================*/

float omni_wernicke_observe_phoneme(omni_wernicke_bridge_t* bridge,
                                     uint32_t phoneme_id,
                                     const float* features,
                                     uint32_t feature_dim) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return 0.0f;
    }

    /* Update phoneme history */
    if (bridge->phoneme_history_len < PHONEME_HISTORY_SIZE) {
        bridge->phoneme_history[bridge->phoneme_history_len++] = (float)phoneme_id;
    } else {
        /* Shift history */
        memmove(bridge->phoneme_history,
                bridge->phoneme_history + 1,
                (PHONEME_HISTORY_SIZE - 1) * sizeof(float));
        bridge->phoneme_history[PHONEME_HISTORY_SIZE - 1] = (float)phoneme_id;
    }

    /* Compute prediction error */
    float pe = 0.0f;
    if (bridge->omni_effects.phoneme_pred.phoneme_probs &&
        phoneme_id < bridge->omni_effects.phoneme_pred.num_phonemes) {
        /* PE = -log(P(observed)) */
        float prob = bridge->omni_effects.phoneme_pred.phoneme_probs[phoneme_id];
        if (prob > 1e-6f) {
            pe = -logf(prob);
        } else {
            pe = 10.0f;  /* High surprise for unexpected phoneme */
        }
    }

    /* Weight by precision */
    pe *= bridge->omni_effects.phoneme_precision;
    bridge->wernicke_effects.phoneme_pe = pe;

    /* Check for surprise */
    bridge->wernicke_effects.phoneme_surprise = (pe > bridge->config.pe_threshold);

    /* Update statistics */
    bridge->stats.avg_phoneme_pe = update_avg(
        bridge->stats.avg_phoneme_pe, pe, bridge->stats.total_updates
    );

    return pe;
}

float omni_wernicke_observe_word(omni_wernicke_bridge_t* bridge,
                                  const char* word) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return 0.0f;
    }
    if (!word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "word is NULL");
        return 0.0f;
    }

    /* Compute word prediction error */
    float pe = 0.0f;

    /* Check against candidates */
    bool found = false;
    if (bridge->omni_effects.word_pred.word_candidates) {
        for (uint32_t i = 0; i < bridge->omni_effects.word_pred.num_candidates; i++) {
            if (bridge->omni_effects.word_pred.word_candidates[i] &&
                strcmp(word, bridge->omni_effects.word_pred.word_candidates[i]) == 0) {
                /* Found in candidates - PE = -log(prob) */
                float prob = bridge->omni_effects.word_pred.word_probs[i];
                if (prob > 1e-6f) {
                    pe = -logf(prob);
                }
                found = true;
                break;
            }
        }
    }

    if (!found) {
        /* Unexpected word - high PE */
        pe = 5.0f;
        bridge->wernicke_effects.word_surprise = true;
    }

    /* Weight by precision */
    pe *= bridge->omni_effects.word_precision;
    bridge->wernicke_effects.word_pe = pe;

    /* Store recognized word */
    nimcp_free(bridge->wernicke_effects.recognized_word);
    bridge->wernicke_effects.recognized_word = strdup(word);

    /* Update statistics */
    bridge->stats.words_recognized++;
    bridge->stats.avg_word_pe = update_avg(
        bridge->stats.avg_word_pe, pe, bridge->stats.words_recognized
    );

    return pe;
}

float omni_wernicke_observe_semantic(omni_wernicke_bridge_t* bridge,
                                      uint32_t concept_id,
                                      float activation) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return 0.0f;
    }

    /* Compute N400-like prediction error */
    float n400 = 0.0f;

    /* Check against predicted concepts */
    bool found = false;
    if (bridge->omni_effects.semantic_pred.predicted_concepts) {
        for (uint32_t i = 0; i < bridge->omni_effects.semantic_pred.num_concepts; i++) {
            if (bridge->omni_effects.semantic_pred.predicted_concepts[i] == concept_id) {
                /* Expected concept - small N400 */
                float expected_act = bridge->omni_effects.semantic_pred.concept_activations[i];
                n400 = fabsf(activation - expected_act);
                found = true;
                break;
            }
        }
    }

    if (!found) {
        /* Unexpected concept - large N400 */
        n400 = activation;  /* Full activation as surprise */
        bridge->wernicke_effects.semantic_anomaly = true;
        bridge->stats.semantic_anomalies++;
    }

    /* Weight by precision */
    n400 *= bridge->omni_effects.semantic_precision;
    bridge->wernicke_effects.semantic_pe = n400;

    /* Check threshold */
    if (n400 > bridge->config.n400_threshold) {
        bridge->omni_effects.semantic_pred.semantic_violation = true;
    }

    bridge->omni_effects.semantic_pred.n400_magnitude = n400;
    bridge->wernicke_effects.recognized_concept = concept_id;

    /* Update statistics */
    bridge->stats.avg_semantic_pe = update_avg(
        bridge->stats.avg_semantic_pe, n400, bridge->stats.total_updates
    );

    return n400;
}

int omni_wernicke_infer_meaning(omni_wernicke_bridge_t* bridge,
                                 uint32_t* concepts,
                                 float* activations,
                                 uint32_t max_concepts,
                                 uint32_t* num_concepts) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!concepts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "concepts is NULL");
        return -1;
    }
    if (!activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "activations is NULL");
        return -1;
    }
    if (!num_concepts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "num_concepts is NULL");
        return -1;
    }

    /* Backward inference: given word context, infer concepts */
    /* Would use semantic memory for spreading activation */

    *num_concepts = 0;

    /* Placeholder: would integrate with semantic memory */
    if (bridge->semantic_memory) {
        /* semantic_memory_get_related(bridge->semantic_memory, ...) */
    }

    return 0;
}

/*=============================================================================
 * CROSS-MODAL API
 *=============================================================================*/

float omni_wernicke_process_audiovisual(omni_wernicke_bridge_t* bridge,
                                         const float* audio_features,
                                         uint32_t audio_dim,
                                         const float* visual_features,
                                         uint32_t visual_dim) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return 0.0f;
    }

    /* Compute audiovisual coherence */
    /* Simplified: compare feature norms as proxy for coherence */
    float audio_norm = 0.0f;
    float visual_norm = 0.0f;

    if (audio_features && audio_dim > 0) {
        for (uint32_t i = 0; i < audio_dim; i++) {
            audio_norm += audio_features[i] * audio_features[i];
        }
        audio_norm = sqrtf(audio_norm);
    }

    if (visual_features && visual_dim > 0) {
        for (uint32_t i = 0; i < visual_dim; i++) {
            visual_norm += visual_features[i] * visual_features[i];
        }
        visual_norm = sqrtf(visual_norm);
    }

    /* Coherence based on norm ratio */
    float coherence = 1.0f;
    if (audio_norm > 0.0f && visual_norm > 0.0f) {
        float ratio = audio_norm / visual_norm;
        if (ratio > 1.0f) ratio = 1.0f / ratio;
        coherence = ratio;
    }

    bridge->omni_effects.crossmodal.audiovisual_coherence = coherence;

    /* Check for McGurk-like conflict */
    if (coherence < bridge->config.av_coherence_threshold) {
        bridge->omni_effects.crossmodal.mcgurk_conflict = true;
        bridge->stats.av_conflicts++;
    } else {
        bridge->omni_effects.crossmodal.mcgurk_conflict = false;
    }

    return coherence;
}

bool omni_wernicke_has_mcgurk_conflict(const omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->omni_effects.crossmodal.mcgurk_conflict;
}

int omni_wernicke_get_fused_percept(const omni_wernicke_bridge_t* bridge,
                                     uint32_t* fused_phoneme,
                                     float* confidence) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!fused_phoneme) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fused_phoneme is NULL");
        return -1;
    }
    if (!confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "confidence is NULL");
        return -1;
    }

    /* Return predicted phoneme with fusion-weighted confidence */
    *fused_phoneme = bridge->omni_effects.phoneme_pred.predicted_phoneme;

    float audio_conf = bridge->omni_effects.phoneme_pred.confidence *
                       bridge->omni_effects.crossmodal.fusion_weight_audio;
    float visual_conf = bridge->omni_effects.crossmodal.audiovisual_coherence *
                        bridge->omni_effects.crossmodal.fusion_weight_visual;

    *confidence = audio_conf + visual_conf;

    return 0;
}

/*=============================================================================
 * PRECISION API
 *=============================================================================*/

int omni_wernicke_set_precision(omni_wernicke_bridge_t* bridge,
                                 omni_wernicke_level_t level,
                                 float precision) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    precision = nimcp_clamp01(precision);

    switch (level) {
        case OMNI_WERNICKE_LEVEL_PHONEME:
            bridge->omni_effects.phoneme_precision = precision;
            break;
        case OMNI_WERNICKE_LEVEL_SYLLABLE:
            /* Use phoneme precision for syllable */
            bridge->omni_effects.phoneme_precision = precision;
            break;
        case OMNI_WERNICKE_LEVEL_WORD:
            bridge->omni_effects.word_precision = precision;
            break;
        case OMNI_WERNICKE_LEVEL_PHRASE:
        case OMNI_WERNICKE_LEVEL_SENTENCE:
        case OMNI_WERNICKE_LEVEL_DISCOURSE:
            bridge->omni_effects.semantic_precision = precision;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wernicke_has_mcgurk_conflict: operation failed");
            return -1;
    }

    return 0;
}

float omni_wernicke_get_precision(const omni_wernicke_bridge_t* bridge,
                                   omni_wernicke_level_t level) {
    if (!bridge) return 0.0f;

    switch (level) {
        case OMNI_WERNICKE_LEVEL_PHONEME:
        case OMNI_WERNICKE_LEVEL_SYLLABLE:
            return bridge->omni_effects.phoneme_precision;
        case OMNI_WERNICKE_LEVEL_WORD:
            return bridge->omni_effects.word_precision;
        case OMNI_WERNICKE_LEVEL_PHRASE:
        case OMNI_WERNICKE_LEVEL_SENTENCE:
        case OMNI_WERNICKE_LEVEL_DISCOURSE:
            return bridge->omni_effects.semantic_precision;
        default:
            return 0.0f;
    }
}

int omni_wernicke_update_precision(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Adapt precision based on prediction success */
    /* Higher PE → lower precision (less confident predictions) */
    /* Lower PE → higher precision (more confident predictions) */

    float pe_factor;

    /* Phoneme precision */
    pe_factor = 1.0f / (1.0f + bridge->wernicke_effects.phoneme_pe);
    bridge->omni_effects.phoneme_precision =
        0.9f * bridge->omni_effects.phoneme_precision + 0.1f * pe_factor;

    /* Word precision */
    pe_factor = 1.0f / (1.0f + bridge->wernicke_effects.word_pe);
    bridge->omni_effects.word_precision =
        0.9f * bridge->omni_effects.word_precision + 0.1f * pe_factor;

    /* Semantic precision */
    pe_factor = 1.0f / (1.0f + bridge->wernicke_effects.semantic_pe);
    bridge->omni_effects.semantic_precision =
        0.9f * bridge->omni_effects.semantic_precision + 0.1f * pe_factor;

    return 0;
}

/*=============================================================================
 * WORLD MODEL API
 *=============================================================================*/

int omni_wernicke_get_world_update(const omni_wernicke_bridge_t* bridge,
                                    omni_wernicke_world_update_t* update) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!update) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update is NULL");
        return -1;
    }
    *update = bridge->world_update;
    return 0;
}

int omni_wernicke_apply_world_update(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Would apply updates to world model via JEPA/pred_hier */
    if (bridge->world_update.significant_update) {
        /* Apply belief updates */
    }

    return 0;
}

void omni_wernicke_clear_world_update(omni_wernicke_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->world_update.updated_concepts);
    nimcp_free(bridge->world_update.concept_deltas);
    memset(&bridge->world_update, 0, sizeof(omni_wernicke_world_update_t));
}

/*=============================================================================
 * KNOWLEDGE GRAPH API
 *=============================================================================*/

int omni_wernicke_kg_register(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->brain_kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_kg is NULL");
        return -1;
    }

    /* brain_kg is stored as void* in the header — cast to real type here. */
    brain_kg_t* kg = (brain_kg_t*)bridge->brain_kg;

    /* Idempotent: add the omni-wernicke coordinator node only if missing.
     * Naming follows docs/claude/kg-node-naming-registry.md §2 (cog_<family>_<name>).
     * Note: the "wernicke" structural node is owned by wernicke_kg_wiring — we
     * only link to it, never recreate it. */
    const char* coord_name = "cog_language_wernicke_omni";
    brain_kg_node_id_t coord_id = brain_kg_find_node(kg, coord_name);
    if (coord_id == BRAIN_KG_INVALID_NODE) {
        coord_id = brain_kg_add_node(
            kg,
            coord_name,
            BRAIN_KG_NODE_COGNITIVE,
            "Omnidirectional inference bridge for Wernicke (prediction + N400 surprise)"
        );
    }

    if (coord_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "omni_wernicke_kg_register: failed to create coordinator node");
        return -1;
    }

    /* Link coordinator to wernicke root and to omni_world_model, if present. */
    brain_kg_node_id_t wernicke_root = brain_kg_find_node(kg, "wernicke");
    if (wernicke_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, coord_id, wernicke_root,
                          BRAIN_KG_EDGE_INTEGRATES_WITH,
                          "omni layer provides predictions to wernicke", 0.9f);
    }
    brain_kg_node_id_t owm = brain_kg_find_node(kg, "omni_world_model");
    if (owm != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owm, coord_id,
                          BRAIN_KG_EDGE_SENDS_TO,
                          "world model feeds omni-wernicke predictions", 0.85f);
    }

    bridge->kg_sync.kg_registered = true;

    /* Set capabilities */
    bridge->kg_sync.comprehension_capability = 1.0f;
    bridge->kg_sync.prediction_capability = 0.8f;

    return 0;
}

int omni_wernicke_kg_sync(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Synchronize current capabilities with KG */
    /* Update based on recent performance */

    float avg_pe = (bridge->wernicke_effects.phoneme_pe +
                    bridge->wernicke_effects.word_pe +
                    bridge->wernicke_effects.semantic_pe) / 3.0f;

    /* Lower PE = better comprehension capability */
    bridge->kg_sync.comprehension_capability = 1.0f / (1.0f + avg_pe);

    /* Prediction capability based on confidence */
    bridge->kg_sync.prediction_capability =
        (bridge->omni_effects.phoneme_pred.confidence +
         bridge->wernicke_effects.comprehension_confidence) / 2.0f;

    bridge->kg_sync.omni_sync_active = true;

    return 0;
}

int omni_wernicke_get_kg_sync(const omni_wernicke_bridge_t* bridge,
                               omni_wernicke_kg_sync_t* sync) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!sync) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sync is NULL");
        return -1;
    }
    *sync = bridge->kg_sync;
    return 0;
}

/*=============================================================================
 * QUERY API
 *=============================================================================*/

int omni_wernicke_get_omni_effects(const omni_wernicke_bridge_t* bridge,
                                    omni_to_wernicke_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "effects is NULL");
        return -1;
    }
    *effects = bridge->omni_effects;
    return 0;
}

int omni_wernicke_get_wernicke_effects(const omni_wernicke_bridge_t* bridge,
                                        wernicke_to_omni_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "effects is NULL");
        return -1;
    }
    *effects = bridge->wernicke_effects;
    return 0;
}

int omni_wernicke_get_stats(const omni_wernicke_bridge_t* bridge,
                             omni_wernicke_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int omni_wernicke_reset_stats(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(omni_wernicke_stats_t));
    return 0;
}

omni_wernicke_mode_t omni_wernicke_get_mode(const omni_wernicke_bridge_t* bridge) {
    if (!bridge) return OMNI_WERNICKE_MODE_LISTENING;
    return bridge->omni_effects.mode;
}

int omni_wernicke_set_mode(omni_wernicke_bridge_t* bridge,
                            omni_wernicke_mode_t mode) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->omni_effects.mode = mode;
    return 0;
}

omni_wernicke_level_t omni_wernicke_get_level(const omni_wernicke_bridge_t* bridge) {
    if (!bridge) return OMNI_WERNICKE_LEVEL_PHONEME;
    return bridge->omni_effects.processing_level;
}

float omni_wernicke_get_comprehension(const omni_wernicke_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->wernicke_effects.comprehension_confidence;
}

float omni_wernicke_get_free_energy(const omni_wernicke_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->wernicke_effects.free_energy;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *=============================================================================*/

/**
 * @brief Handle prediction request from omnidirectional inference
 *
 * Triggers forward prediction of expected phonemes/words/concepts
 */
static nimcp_error_t handle_wernicke_predict_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_wernicke_bridge_t* bridge = (omni_wernicke_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Update comprehension state based on prediction request */
    omni_wernicke_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle direction switch (forward prediction <-> backward inference)
 *
 * Switches between predictive and inferential processing modes
 */
static nimcp_error_t handle_wernicke_direction_switch(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_wernicke_bridge_t* bridge = (omni_wernicke_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Direction switch affects processing mode */
    omni_wernicke_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle precision update from predictive hierarchy
 *
 * Updates precision weights for phoneme/word/semantic processing
 */
static nimcp_error_t handle_wernicke_precision_update(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_wernicke_bridge_t* bridge = (omni_wernicke_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Update precision for language comprehension */
    omni_wernicke_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle forward message from predictive hierarchy
 *
 * Processes top-down predictions from higher language areas
 */
static nimcp_error_t handle_wernicke_pred_hier_forward(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_wernicke_bridge_t* bridge = (omni_wernicke_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Forward pass: predictions from higher levels */
    omni_wernicke_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle backward message from predictive hierarchy
 *
 * Processes bottom-up prediction errors from sensory input
 */
static nimcp_error_t handle_wernicke_pred_hier_backward(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_wernicke_bridge_t* bridge = (omni_wernicke_bridge_t*)user_data;
    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    /* Backward pass: prediction errors from lower levels */
    omni_wernicke_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * BIO-ASYNC API
 *=============================================================================*/

int omni_wernicke_connect_bio_async(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_WERNICKE_BRIDGE,
        .module_name = "omni_wernicke_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    if (!ctx) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    bridge->bio_context = ctx;

    /* Register message handlers for language comprehension */
    bio_router_register_handler(ctx, BIO_MSG_OMNI_PREDICT_REQUEST,
                                 handle_wernicke_predict_request);
    bio_router_register_handler(ctx, BIO_MSG_OMNI_DIRECTION_SWITCH,
                                 handle_wernicke_direction_switch);
    bio_router_register_handler(ctx, BIO_MSG_OMNI_PRECISION_UPDATE,
                                 handle_wernicke_precision_update);
    bio_router_register_handler(ctx, BIO_MSG_PRED_HIER_FORWARD,
                                 handle_wernicke_pred_hier_forward);
    bio_router_register_handler(ctx, BIO_MSG_PRED_HIER_BACKWARD,
                                 handle_wernicke_pred_hier_backward);

    bridge->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_wernicke_disconnect_bio_async(omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_wernicke_is_bio_async_connected(const omni_wernicke_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}

/*=============================================================================
 * STRING CONVERSION API
 *=============================================================================*/

const char* omni_wernicke_level_to_string(omni_wernicke_level_t level) {
    switch (level) {
        case OMNI_WERNICKE_LEVEL_PHONEME:   return "phoneme";
        case OMNI_WERNICKE_LEVEL_SYLLABLE:  return "syllable";
        case OMNI_WERNICKE_LEVEL_WORD:      return "word";
        case OMNI_WERNICKE_LEVEL_PHRASE:    return "phrase";
        case OMNI_WERNICKE_LEVEL_SENTENCE:  return "sentence";
        case OMNI_WERNICKE_LEVEL_DISCOURSE: return "discourse";
        default:                            return "unknown";
    }
}

const char* omni_wernicke_mode_to_string(omni_wernicke_mode_t mode) {
    switch (mode) {
        case OMNI_WERNICKE_MODE_LISTENING:     return "listening";
        case OMNI_WERNICKE_MODE_COMPREHENDING: return "comprehending";
        case OMNI_WERNICKE_MODE_PREDICTING:    return "predicting";
        case OMNI_WERNICKE_MODE_MONITORING:    return "monitoring";
        case OMNI_WERNICKE_MODE_REHEARSING:    return "rehearsing";
        default:                               return "unknown";
    }
}

const char* omni_wernicke_modality_to_string(omni_wernicke_modality_t modality) {
    switch (modality) {
        case OMNI_WERNICKE_AUDIO_ONLY:     return "audio_only";
        case OMNI_WERNICKE_VISUAL_ONLY:    return "visual_only";
        case OMNI_WERNICKE_AUDIOVISUAL:    return "audiovisual";
        case OMNI_WERNICKE_MOTOR_FEEDBACK: return "motor_feedback";
        default:                           return "unknown";
    }
}

const char* omni_wernicke_disambig_to_string(omni_wernicke_disambig_t strategy) {
    switch (strategy) {
        case OMNI_WERNICKE_DISAMBIG_FREQ:      return "frequency";
        case OMNI_WERNICKE_DISAMBIG_CONTEXT:   return "context";
        case OMNI_WERNICKE_DISAMBIG_BAYESIAN:  return "bayesian";
        case OMNI_WERNICKE_DISAMBIG_SPREADING: return "spreading";
        default:                               return "unknown";
    }
}
