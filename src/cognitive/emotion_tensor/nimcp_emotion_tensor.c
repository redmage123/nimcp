/**
 * @file nimcp_emotion_tensor.c
 * @brief Implementation of Tensor-Based Emotional Representation System
 *
 * WHAT: Multi-dimensional tensor for complex, mixed, contradictory emotions
 * WHY:  Scalar valence/arousal cannot capture mixed emotions (bittersweet)
 * HOW:  Multiple emotion channels with interaction dynamics and compounds
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#include "cognitive/knowledge/nimcp_kg_reader.h"

#include "cognitive/nimcp_emotion_tensor.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* pthread.h no longer needed -- using nimcp_rwlock_t from utils/thread/nimcp_thread.h */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(emotion_tensor, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Logging
//=============================================================================

#define EMOTION_TENSOR_TAG "EmotionTensor"
#define TENSOR_LOG_DEBUG(fmt, ...) LOG_MODULE_DEBUG(EMOTION_TENSOR_TAG, fmt, ##__VA_ARGS__)
#define TENSOR_LOG_INFO(fmt, ...)  LOG_MODULE_INFO(EMOTION_TENSOR_TAG, fmt, ##__VA_ARGS__)
#define TENSOR_LOG_WARN(fmt, ...)  LOG_MODULE_WARN(EMOTION_TENSOR_TAG, fmt, ##__VA_ARGS__)
#define TENSOR_LOG_ERROR(fmt, ...) LOG_MODULE_ERROR(EMOTION_TENSOR_TAG, fmt, ##__VA_ARGS__)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal emotion tensor system structure
 */
struct emotion_tensor_system {
    emotion_tensor_t tensor;
    emotion_tensor_config_t config;
    emotion_interaction_matrix_t interactions;
    nimcp_rwlock_t lock;
    bool initialized;
};

//=============================================================================
// Static Helpers - Valence Mapping
//=============================================================================

/**
 * @brief Get valence weight for emotion
 *
 * WHAT: Map emotion to positive/negative valence contribution
 * WHY:  Compute aggregate valence from channels
 * HOW:  Return pre-defined valence weights per Plutchik
 */
static float get_emotion_valence_weight(emotion_primary_t emotion) {
    static const float weights[EMOTION_TENSOR_PRIMARY_COUNT] = {
        [TENSOR_JOY] = 1.0F,           /* Positive */
        [TENSOR_TRUST] = 0.6F,         /* Slightly positive */
        [TENSOR_FEAR] = -0.8F,         /* Negative */
        [TENSOR_SURPRISE] = 0.0F,      /* Neutral (can be either) */
        [TENSOR_SADNESS] = -1.0F,      /* Negative */
        [TENSOR_DISGUST] = -0.7F,      /* Negative */
        [TENSOR_ANGER] = -0.5F,        /* Negative */
        [TENSOR_ANTICIPATION] = 0.4F   /* Slightly positive */
    };
    if (emotion < 0 || emotion >= EMOTION_TENSOR_PRIMARY_COUNT) {
        return 0.0F;
    }
    return weights[emotion];
}

/**
 * @brief Get arousal weight for emotion
 *
 * WHAT: Map emotion to arousal contribution
 * WHY:  Compute aggregate arousal from channels
 * HOW:  Return pre-defined arousal weights per Plutchik
 */
static float get_emotion_arousal_weight(emotion_primary_t emotion) {
    static const float weights[EMOTION_TENSOR_PRIMARY_COUNT] = {
        [TENSOR_JOY] = 0.7F,           /* High arousal */
        [TENSOR_TRUST] = 0.3F,         /* Low arousal */
        [TENSOR_FEAR] = 0.9F,          /* Very high arousal */
        [TENSOR_SURPRISE] = 0.8F,      /* High arousal */
        [TENSOR_SADNESS] = 0.2F,       /* Low arousal */
        [TENSOR_DISGUST] = 0.4F,       /* Medium arousal */
        [TENSOR_ANGER] = 0.9F,         /* Very high arousal */
        [TENSOR_ANTICIPATION] = 0.6F   /* Medium-high arousal */
    };
    if (emotion < 0 || emotion >= EMOTION_TENSOR_PRIMARY_COUNT) {
        return 0.0F;
    }
    return weights[emotion];
}

//=============================================================================
// Static Helpers - Compound Emotion Computation
//=============================================================================

/**
 * @brief Compute primary dyad (adjacent emotions on Plutchik wheel)
 *
 * WHAT: Calculate compound from two adjacent primary emotions
 * WHY:  Primary dyads are strongest blends
 * HOW:  Geometric mean of adjacent channel activations
 */
static float compute_primary_dyad(const float* channels, emotion_primary_t e1, emotion_primary_t e2) {
    float a1 = channels[e1];
    float a2 = channels[e2];
    return sqrtf(a1 * a2);
}

/**
 * @brief Compute secondary dyad (one emotion apart)
 *
 * WHAT: Calculate compound from emotions one step apart
 * WHY:  Secondary dyads are moderate blends
 * HOW:  Weighted geometric mean (slightly weaker)
 */
static float compute_secondary_dyad(const float* channels, emotion_primary_t e1, emotion_primary_t e2) {
    float a1 = channels[e1];
    float a2 = channels[e2];
    return sqrtf(a1 * a2) * 0.8F;
}

/**
 * @brief Compute tertiary dyad (opposite emotions)
 *
 * WHAT: Calculate compound from opposing/contradictory emotions
 * WHY:  Tertiary dyads represent mixed/contradictory states
 * HOW:  Product of activations (both must be active)
 */
static float compute_tertiary_dyad(const float* channels, emotion_primary_t e1, emotion_primary_t e2) {
    float a1 = channels[e1];
    float a2 = channels[e2];
    return a1 * a2;
}

/**
 * @brief Update all compound emotions from primary channels
 *
 * WHAT: Recompute all Plutchik dyads
 * WHY:  Compounds derive from primaries
 * HOW:  Apply dyad formulas for all compound types
 */
static void update_compounds(emotion_tensor_t* tensor) {
    const float* c = tensor->channels;

    /* Primary dyads (adjacent on wheel) */
    tensor->compounds[COMPOUND_LOVE] = compute_primary_dyad(c, TENSOR_JOY, TENSOR_TRUST);
    tensor->compounds[COMPOUND_SUBMISSION] = compute_primary_dyad(c, TENSOR_TRUST, TENSOR_FEAR);
    tensor->compounds[COMPOUND_AWE] = compute_primary_dyad(c, TENSOR_FEAR, TENSOR_SURPRISE);
    tensor->compounds[COMPOUND_DISAPPROVAL] = compute_primary_dyad(c, TENSOR_SURPRISE, TENSOR_SADNESS);
    tensor->compounds[COMPOUND_REMORSE] = compute_primary_dyad(c, TENSOR_SADNESS, TENSOR_DISGUST);
    tensor->compounds[COMPOUND_CONTEMPT] = compute_primary_dyad(c, TENSOR_DISGUST, TENSOR_ANGER);
    tensor->compounds[COMPOUND_AGGRESSIVENESS] = compute_primary_dyad(c, TENSOR_ANGER, TENSOR_ANTICIPATION);
    tensor->compounds[COMPOUND_OPTIMISM] = compute_primary_dyad(c, TENSOR_ANTICIPATION, TENSOR_JOY);

    /* Secondary dyads (one apart) */
    tensor->compounds[COMPOUND_GUILT] = compute_secondary_dyad(c, TENSOR_JOY, TENSOR_FEAR);
    tensor->compounds[COMPOUND_CURIOSITY] = compute_secondary_dyad(c, TENSOR_TRUST, TENSOR_SURPRISE);
    tensor->compounds[COMPOUND_DESPAIR] = compute_secondary_dyad(c, TENSOR_FEAR, TENSOR_SADNESS);
    tensor->compounds[COMPOUND_UNBELIEF] = compute_secondary_dyad(c, TENSOR_SURPRISE, TENSOR_DISGUST);
    tensor->compounds[COMPOUND_ENVY] = compute_secondary_dyad(c, TENSOR_SADNESS, TENSOR_ANGER);
    tensor->compounds[COMPOUND_CYNICISM] = compute_secondary_dyad(c, TENSOR_DISGUST, TENSOR_ANTICIPATION);
    tensor->compounds[COMPOUND_PRIDE] = compute_secondary_dyad(c, TENSOR_ANGER, TENSOR_JOY);
    tensor->compounds[COMPOUND_HOPE] = compute_secondary_dyad(c, TENSOR_ANTICIPATION, TENSOR_TRUST);

    /* Tertiary dyads (opposites/contradictory) */
    tensor->compounds[COMPOUND_BITTERSWEETNESS] = compute_tertiary_dyad(c, TENSOR_JOY, TENSOR_SADNESS);
    tensor->compounds[COMPOUND_MORBID_CURIOSITY] = compute_tertiary_dyad(c, TENSOR_TRUST, TENSOR_DISGUST);
    tensor->compounds[COMPOUND_ANXIETY] = compute_tertiary_dyad(c, TENSOR_FEAR, TENSOR_ANGER);
    tensor->compounds[COMPOUND_AMBIVALENCE] = compute_tertiary_dyad(c, TENSOR_SURPRISE, TENSOR_ANTICIPATION);
    tensor->compounds[COMPOUND_DESOLATION] = compute_tertiary_dyad(c, TENSOR_SADNESS, TENSOR_JOY);
    tensor->compounds[COMPOUND_FROZENNESS] = compute_tertiary_dyad(c, TENSOR_DISGUST, TENSOR_TRUST);
    tensor->compounds[COMPOUND_OUTRAGE] = compute_tertiary_dyad(c, TENSOR_ANGER, TENSOR_FEAR);
    tensor->compounds[COMPOUND_NOSTALGIA] = compute_tertiary_dyad(c, TENSOR_ANTICIPATION, TENSOR_SADNESS);
}

//=============================================================================
// Static Helpers - Aggregate Metrics
//=============================================================================

/**
 * @brief Compute aggregate valence from channels
 *
 * WHAT: Calculate overall positive/negative feeling
 * WHY:  Backward compatibility with scalar systems
 * HOW:  Weighted sum of channels by valence weight
 */
static float compute_aggregate_valence(const float* channels) {
    float sum = 0.0F;
    float total_activation = 0.0F;

    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        sum += channels[i] * get_emotion_valence_weight((emotion_primary_t)i);
        total_activation += channels[i];
    }

    if (total_activation < 0.001F) {
        return 0.0F;
    }
    float valence = sum / total_activation;
    return fmaxf(-1.0F, fminf(1.0F, valence));
}

/**
 * @brief Compute aggregate arousal from channels
 *
 * WHAT: Calculate overall activation level
 * WHY:  Backward compatibility with scalar systems
 * HOW:  Weighted average of channels by arousal weight
 */
static float compute_aggregate_arousal(const float* channels) {
    float sum = 0.0F;
    float total_activation = 0.0F;

    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        sum += channels[i] * get_emotion_arousal_weight((emotion_primary_t)i);
        total_activation += channels[i];
    }

    if (total_activation < 0.001F) {
        return 0.0F;
    }
    float arousal = sum / total_activation;
    return fmaxf(0.0F, fminf(1.0F, arousal));
}

/**
 * @brief Compute normalized Shannon entropy of emotion distribution
 *
 * WHAT: Measure diversity of active emotions
 * WHY:  High entropy = mixed emotions, low = focused
 * HOW:  Normalize activations to probabilities, use central stats entropy
 *
 * Uses nimcp_stats_entropy() from utils/statistics for the core computation.
 */
static float compute_emotion_entropy(const float* channels) {
    /* Compute total for normalization */
    float total = 0.0F;
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }
        total += channels[i];
    }

    if (total < 0.001F) {
        return 0.0F;
    }

    /* Normalize to probability distribution */
    float probs[EMOTION_TENSOR_PRIMARY_COUNT];
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        probs[i] = channels[i] / (fabsf(total) > 1e-7f ? total : 1e-7f);
    }

    /* Use central statistics module for entropy computation */
    float entropy = nimcp_stats_entropy(probs, EMOTION_TENSOR_PRIMARY_COUNT);

    /* Normalize to [0, 1] - max entropy is log2(N) */
    float max_entropy = log2f((float)EMOTION_TENSOR_PRIMARY_COUNT);
    return (max_entropy > 0.0F) ? (entropy / max_entropy) : 0.0F;
}

/**
 * @brief Find dominant and secondary emotions
 *
 * WHAT: Identify top 2 active emotions
 * WHY:  Quick summary of emotional state
 * HOW:  Find max and second-max activations
 */
static void find_dominant_emotions(const float* channels,
                                   emotion_primary_t* primary,
                                   emotion_primary_t* secondary,
                                   float* primary_strength,
                                   float* secondary_strength) {
    int max_idx = 0;
    int second_idx = 1;
    float max_val = channels[0];
    float second_val = channels[1];

    if (second_val > max_val) {
        max_idx = 1;
        second_idx = 0;
        max_val = channels[1];
        second_val = channels[0];
    }

    for (int i = 2; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        if (channels[i] > max_val) {
            second_idx = max_idx;
            second_val = max_val;
            max_idx = i;
            max_val = channels[i];
        } else if (channels[i] > second_val) {
            second_idx = i;
            second_val = channels[i];
        }
    }

    *primary = (emotion_primary_t)max_idx;
    *secondary = (emotion_primary_t)second_idx;
    *primary_strength = max_val;
    *secondary_strength = second_val;
}

/**
 * @brief Update all aggregate metrics in tensor
 */
static void update_aggregates(emotion_tensor_t* tensor) {
    tensor->overall_valence = compute_aggregate_valence(tensor->channels);
    tensor->overall_arousal = compute_aggregate_arousal(tensor->channels);
    tensor->emotional_entropy = compute_emotion_entropy(tensor->channels);

    find_dominant_emotions(tensor->channels,
                          &tensor->primary_emotion,
                          &tensor->secondary_emotion,
                          &tensor->primary_strength,
                          &tensor->secondary_strength);

    if (tensor->primary_strength > 0.001F) {
        tensor->blend_ratio = tensor->secondary_strength / tensor->primary_strength;
    } else {
        tensor->blend_ratio = 0.0F;
    }
}

//=============================================================================
// Static Helpers - Temporal Dynamics
//=============================================================================

/**
 * @brief Record current state in dynamics buffer
 *
 * WHAT: Store channel values in temporal history
 * WHY:  Track emotion trajectories over time
 * HOW:  Circular buffer of channel snapshots
 */
static void record_dynamics(emotion_tensor_t* tensor) {
    uint32_t idx = tensor->dynamics_index;

    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        tensor->dynamics[i][idx] = tensor->channels[i];
    }

    tensor->dynamics_index = (idx + 1) % EMOTION_TENSOR_TEMPORAL_WINDOW;
}

/**
 * @brief Compute stability from temporal dynamics
 *
 * WHAT: Measure how stable emotions are over time
 * WHY:  Detect emotional volatility
 * HOW:  Inverse of variance across temporal window
 */
static float compute_stability(const emotion_tensor_t* tensor) {
    float total_variance = 0.0F;

    for (int e = 0; e < EMOTION_TENSOR_PRIMARY_COUNT; e++) {
        /* Phase 8: Loop progress heartbeat */
        if ((e & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(e + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        float sum = 0.0F;
        float sum_sq = 0.0F;

        for (int t = 0; t < EMOTION_TENSOR_TEMPORAL_WINDOW; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && EMOTION_TENSOR_TEMPORAL_WINDOW > 256) {
                emotion_tensor_heartbeat("emotion_tens_loop",
                                 (float)(t + 1) / (float)EMOTION_TENSOR_TEMPORAL_WINDOW);
            }

            float v = tensor->dynamics[e][t];
            sum += v;
            sum_sq += v * v;
        }

        float mean = sum / EMOTION_TENSOR_TEMPORAL_WINDOW;
        float variance = (sum_sq / EMOTION_TENSOR_TEMPORAL_WINDOW) - (mean * mean);
        total_variance += variance;
    }

    float avg_variance = total_variance / (fabsf(EMOTION_TENSOR_PRIMARY_COUNT) > 1e-7f ? EMOTION_TENSOR_PRIMARY_COUNT : 1e-7f);
    /* Stability is inverse of variance, normalized */
    return 1.0F / (1.0F + avg_variance * 10.0F);
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

emotion_tensor_config_t emotion_tensor_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_default_config", 0.0f);


    emotion_tensor_config_t config = {
        .decay_rate = 0.1F,
        .interaction_strength = 0.3F,
        .blend_threshold = 0.2F,
        .dominance_threshold = 0.5F,
        .enable_temporal_dynamics = true,
        .enable_appraisals = true,
        .enable_interactions = true
    };
    return config;
}

void emotion_tensor_init_interaction_matrix(emotion_interaction_matrix_t* matrix) {
    if (!matrix) {
        return;
    }

    /* Initialize to neutral (no interaction) */
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_init_interaction_mat", 0.0f);


    memset(matrix, 0, sizeof(emotion_interaction_matrix_t));

    /* Self-reinforcement (diagonal) - emotions reinforce themselves slightly */
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        matrix->matrix[i][i] = 0.1F;
    }

    /* Adjacent emotions facilitate each other (Plutchik wheel) */
    /* Joy <-> Trust */
    matrix->matrix[TENSOR_JOY][TENSOR_TRUST] = 0.2F;
    matrix->matrix[TENSOR_TRUST][TENSOR_JOY] = 0.2F;
    /* Trust <-> Fear (submission) */
    matrix->matrix[TENSOR_TRUST][TENSOR_FEAR] = 0.1F;
    matrix->matrix[TENSOR_FEAR][TENSOR_TRUST] = 0.1F;
    /* Fear <-> Surprise */
    matrix->matrix[TENSOR_FEAR][TENSOR_SURPRISE] = 0.15F;
    matrix->matrix[TENSOR_SURPRISE][TENSOR_FEAR] = 0.15F;
    /* Surprise <-> Sadness */
    matrix->matrix[TENSOR_SURPRISE][TENSOR_SADNESS] = 0.1F;
    matrix->matrix[TENSOR_SADNESS][TENSOR_SURPRISE] = 0.1F;
    /* Sadness <-> Disgust */
    matrix->matrix[TENSOR_SADNESS][TENSOR_DISGUST] = 0.15F;
    matrix->matrix[TENSOR_DISGUST][TENSOR_SADNESS] = 0.15F;
    /* Disgust <-> Anger */
    matrix->matrix[TENSOR_DISGUST][TENSOR_ANGER] = 0.2F;
    matrix->matrix[TENSOR_ANGER][TENSOR_DISGUST] = 0.2F;
    /* Anger <-> Anticipation */
    matrix->matrix[TENSOR_ANGER][TENSOR_ANTICIPATION] = 0.15F;
    matrix->matrix[TENSOR_ANTICIPATION][TENSOR_ANGER] = 0.15F;
    /* Anticipation <-> Joy */
    matrix->matrix[TENSOR_ANTICIPATION][TENSOR_JOY] = 0.25F;
    matrix->matrix[TENSOR_JOY][TENSOR_ANTICIPATION] = 0.25F;

    /* Opposite emotions inhibit each other */
    /* Joy <-> Sadness */
    matrix->matrix[TENSOR_JOY][TENSOR_SADNESS] = -0.3F;
    matrix->matrix[TENSOR_SADNESS][TENSOR_JOY] = -0.3F;
    /* Trust <-> Disgust */
    matrix->matrix[TENSOR_TRUST][TENSOR_DISGUST] = -0.35F;
    matrix->matrix[TENSOR_DISGUST][TENSOR_TRUST] = -0.35F;
    /* Fear <-> Anger */
    matrix->matrix[TENSOR_FEAR][TENSOR_ANGER] = -0.25F;
    matrix->matrix[TENSOR_ANGER][TENSOR_FEAR] = -0.25F;
    /* Surprise <-> Anticipation */
    matrix->matrix[TENSOR_SURPRISE][TENSOR_ANTICIPATION] = -0.2F;
    matrix->matrix[TENSOR_ANTICIPATION][TENSOR_SURPRISE] = -0.2F;
}

emotion_tensor_system_t* emotion_tensor_create(const emotion_tensor_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_create", 0.0f);


    emotion_tensor_system_t* system = nimcp_calloc(1, sizeof(emotion_tensor_system_t));
    if (!system) {
        TENSOR_LOG_ERROR("Failed to allocate emotion tensor system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    /* Initialize config */
    if (config) {
        system->config = *config;
    } else {
        system->config = emotion_tensor_default_config();
    }

    /* Initialize interaction matrix */
    emotion_tensor_init_interaction_matrix(&system->interactions);

    /* Initialize tensor to neutral state */
    memset(&system->tensor, 0, sizeof(emotion_tensor_t));

    /* Initialize lock */
    if (nimcp_rwlock_init(&system->lock) != NIMCP_SUCCESS) {
        TENSOR_LOG_ERROR("Failed to initialize rwlock");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "emotion_tensor_create: validation failed");
        return NULL;
    }

    system->initialized = true;
    TENSOR_LOG_INFO("Emotion tensor system created");
    return system;
}

void emotion_tensor_destroy(emotion_tensor_system_t* system) {
    if (!system) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_destroy", 0.0f);


    nimcp_rwlock_destroy(&system->lock);
    nimcp_free(system);
    system = NULL;
    TENSOR_LOG_INFO("Emotion tensor system destroyed");
}

//=============================================================================
// State Query API Implementation
//=============================================================================

bool emotion_tensor_get(const emotion_tensor_system_t* system, emotion_tensor_t* tensor) {
    if (!system || !tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_get: required parameter is NULL (system, tensor)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_get", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    *tensor = system->tensor;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return true;
}

float emotion_tensor_get_channel(const emotion_tensor_system_t* system, emotion_primary_t emotion) {
    if (!system) {
        return -1.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_get_channel", 0.0f);


    if (emotion < 0 || emotion >= EMOTION_TENSOR_PRIMARY_COUNT) {
        return -1.0F;
    }

    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    float value = system->tensor.channels[emotion];
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return value;
}

float emotion_tensor_get_compound(const emotion_tensor_system_t* system, emotion_compound_t compound) {
    if (!system) {
        return -1.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_get_compound", 0.0f);


    if (compound < 0 || compound >= EMOTION_TENSOR_COMPOUND_COUNT) {
        return -1.0F;
    }

    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    float value = system->tensor.compounds[compound];
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return value;
}

bool emotion_tensor_is_contradictory(const emotion_tensor_system_t* system, float threshold) {
    if (!system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_is_contradictory", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    const float* c = system->tensor.channels;

    /* Check opposing emotion pairs */
    bool contradictory = false;
    if (c[TENSOR_JOY] > threshold && c[TENSOR_SADNESS] > threshold) {
        contradictory = true;
    } else if (c[TENSOR_TRUST] > threshold && c[TENSOR_DISGUST] > threshold) {
        contradictory = true;
    } else if (c[TENSOR_FEAR] > threshold && c[TENSOR_ANGER] > threshold) {
        contradictory = true;
    } else if (c[TENSOR_SURPRISE] > threshold && c[TENSOR_ANTICIPATION] > threshold) {
        contradictory = true;
    }

    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);
    return contradictory;
}

float emotion_tensor_get_valence(const emotion_tensor_system_t* system) {
    if (!system) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_get_valence", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    float valence = system->tensor.overall_valence;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return valence;
}

float emotion_tensor_get_arousal(const emotion_tensor_system_t* system) {
    if (!system) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_get_arousal", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    float arousal = system->tensor.overall_arousal;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return arousal;
}

//=============================================================================
// State Update API Implementation
//=============================================================================

bool emotion_tensor_set_channel(
    emotion_tensor_system_t* system,
    emotion_primary_t emotion,
    float activation,
    uint64_t timestamp_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_set_channel: system is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_set_channel", 0.0f);


    if (emotion < 0 || emotion >= EMOTION_TENSOR_PRIMARY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_tensor_set_channel: capacity exceeded");
        return false;
    }

    activation = fmaxf(0.0F, fminf(1.0F, activation));

    nimcp_rwlock_wrlock(&system->lock);

    system->tensor.channels[emotion] = activation;
    system->tensor.last_update_ms = timestamp_ms;

    update_compounds(&system->tensor);
    update_aggregates(&system->tensor);

    if (system->config.enable_temporal_dynamics) {
        record_dynamics(&system->tensor);
        system->tensor.stability = compute_stability(&system->tensor);
    }

    nimcp_rwlock_unlock(&system->lock);

    TENSOR_LOG_DEBUG("Set %s to %.3f", emotion_tensor_emotion_name(emotion), activation);
    return true;
}

bool emotion_tensor_set_channels(
    emotion_tensor_system_t* system,
    const float* activations,
    uint64_t timestamp_ms
) {
    if (!system || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_set_channels: required parameter is NULL (system, activations)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_set_channels", 0.0f);


    nimcp_rwlock_wrlock(&system->lock);

    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        system->tensor.channels[i] = fmaxf(0.0F, fminf(1.0F, activations[i]));
    }
    system->tensor.last_update_ms = timestamp_ms;

    update_compounds(&system->tensor);
    update_aggregates(&system->tensor);

    if (system->config.enable_temporal_dynamics) {
        record_dynamics(&system->tensor);
        system->tensor.stability = compute_stability(&system->tensor);
    }

    nimcp_rwlock_unlock(&system->lock);

    TENSOR_LOG_DEBUG("Set all emotion channels");
    return true;
}

bool emotion_tensor_set_appraisal(
    emotion_tensor_system_t* system,
    emotion_primary_t emotion,
    appraisal_dimension_t dimension,
    float value
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_set_appraisal: system is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_set_appraisal", 0.0f);


    if (emotion < 0 || emotion >= EMOTION_TENSOR_PRIMARY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_tensor_set_appraisal: capacity exceeded");
        return false;
    }
    if (dimension < 0 || dimension >= APPRAISAL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_tensor_set_appraisal: capacity exceeded");
        return false;
    }

    value = fmaxf(0.0F, fminf(1.0F, value));

    nimcp_rwlock_wrlock(&system->lock);
    system->tensor.appraisals[emotion][dimension] = value;
    nimcp_rwlock_unlock(&system->lock);

    return true;
}

bool emotion_tensor_apply_stimulus(
    emotion_tensor_system_t* system,
    emotion_primary_t emotion,
    float intensity,
    bool is_positive,
    uint64_t timestamp_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_apply_stimulus: system is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_apply_stimulus", 0.0f);


    if (emotion < 0 || emotion >= EMOTION_TENSOR_PRIMARY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_tensor_apply_stimulus: capacity exceeded");
        return false;
    }

    intensity = fmaxf(0.0F, fminf(1.0F, intensity));

    nimcp_rwlock_wrlock(&system->lock);

    float current = system->tensor.channels[emotion];
    float change = is_positive ? intensity : -intensity * 0.5F;
    float new_val = fmaxf(0.0F, fminf(1.0F, current + change));

    system->tensor.channels[emotion] = new_val;
    system->tensor.last_update_ms = timestamp_ms;

    update_compounds(&system->tensor);
    update_aggregates(&system->tensor);

    if (system->config.enable_temporal_dynamics) {
        record_dynamics(&system->tensor);
        system->tensor.stability = compute_stability(&system->tensor);
    }

    nimcp_rwlock_unlock(&system->lock);

    TENSOR_LOG_DEBUG("Applied %s stimulus to %s: %.3f -> %.3f",
              is_positive ? "positive" : "negative",
              emotion_tensor_emotion_name(emotion),
              current, new_val);
    return true;
}

//=============================================================================
// Dynamics API Implementation
//=============================================================================

bool emotion_tensor_update(
    emotion_tensor_system_t* system,
    float delta_time,
    uint64_t timestamp_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_update: system is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_update", 0.0f);


    if (delta_time <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_tensor_update: validation failed");
        return false;
    }

    nimcp_rwlock_wrlock(&system->lock);

    /* Apply decay */
    float decay_factor = expf(-system->config.decay_rate * delta_time);
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        system->tensor.channels[i] *= decay_factor;
    }

    /* Apply interactions if enabled */
    if (system->config.enable_interactions) {
        float new_channels[EMOTION_TENSOR_PRIMARY_COUNT];
        memcpy(new_channels, system->tensor.channels, sizeof(new_channels));

        for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
                emotion_tensor_heartbeat("emotion_tens_loop",
                                 (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
            }

            float interaction_effect = 0.0F;
            for (int j = 0; j < EMOTION_TENSOR_PRIMARY_COUNT; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
                    emotion_tensor_heartbeat("emotion_tens_loop",
                                     (float)(j + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
                }

                if (i != j) {
                    interaction_effect += system->interactions.matrix[i][j] *
                                         system->tensor.channels[j];
                }
            }
            new_channels[i] += interaction_effect * system->config.interaction_strength * delta_time;
            new_channels[i] = fmaxf(0.0F, fminf(1.0F, new_channels[i]));
        }

        memcpy(system->tensor.channels, new_channels, sizeof(new_channels));
    }

    system->tensor.last_update_ms = timestamp_ms;

    update_compounds(&system->tensor);
    update_aggregates(&system->tensor);

    if (system->config.enable_temporal_dynamics) {
        record_dynamics(&system->tensor);
        system->tensor.stability = compute_stability(&system->tensor);
    }

    nimcp_rwlock_unlock(&system->lock);

    return true;
}

bool emotion_tensor_compute_compounds(emotion_tensor_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_compute_compounds: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_compute_compounds", 0.0f);


    nimcp_rwlock_wrlock(&system->lock);
    update_compounds(&system->tensor);
    nimcp_rwlock_unlock(&system->lock);

    return true;
}

bool emotion_tensor_apply_interactions(emotion_tensor_system_t* system, float delta_time) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_apply_interactions: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_apply_interactions", 0.0f);


    nimcp_rwlock_wrlock(&system->lock);

    float new_channels[EMOTION_TENSOR_PRIMARY_COUNT];
    memcpy(new_channels, system->tensor.channels, sizeof(new_channels));

    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
            emotion_tensor_heartbeat("emotion_tens_loop",
                             (float)(i + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
        }

        float interaction_effect = 0.0F;
        for (int j = 0; j < EMOTION_TENSOR_PRIMARY_COUNT; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && EMOTION_TENSOR_PRIMARY_COUNT > 256) {
                emotion_tensor_heartbeat("emotion_tens_loop",
                                 (float)(j + 1) / (float)EMOTION_TENSOR_PRIMARY_COUNT);
            }

            if (i != j) {
                interaction_effect += system->interactions.matrix[i][j] *
                                     system->tensor.channels[j];
            }
        }
        new_channels[i] += interaction_effect * system->config.interaction_strength * delta_time;
        new_channels[i] = fmaxf(0.0F, fminf(1.0F, new_channels[i]));
    }

    memcpy(system->tensor.channels, new_channels, sizeof(new_channels));
    update_compounds(&system->tensor);
    update_aggregates(&system->tensor);

    nimcp_rwlock_unlock(&system->lock);

    return true;
}

bool emotion_tensor_reset(emotion_tensor_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_reset: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_reset", 0.0f);


    nimcp_rwlock_wrlock(&system->lock);

    memset(system->tensor.channels, 0, sizeof(system->tensor.channels));
    memset(system->tensor.compounds, 0, sizeof(system->tensor.compounds));
    memset(system->tensor.dynamics, 0, sizeof(system->tensor.dynamics));
    memset(system->tensor.appraisals, 0, sizeof(system->tensor.appraisals));

    system->tensor.dynamics_index = 0;
    system->tensor.overall_valence = 0.0F;
    system->tensor.overall_arousal = 0.0F;
    system->tensor.emotional_entropy = 0.0F;
    system->tensor.stability = 1.0F;
    system->tensor.primary_emotion = TENSOR_JOY;
    system->tensor.secondary_emotion = TENSOR_TRUST;
    system->tensor.primary_strength = 0.0F;
    system->tensor.secondary_strength = 0.0F;
    system->tensor.blend_ratio = 0.0F;

    nimcp_rwlock_unlock(&system->lock);

    TENSOR_LOG_INFO("Emotion tensor reset to neutral");
    return true;
}

//=============================================================================
// Analysis API Implementation
//=============================================================================

float emotion_tensor_get_entropy(const emotion_tensor_system_t* system) {
    if (!system) {
        return -1.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_get_entropy", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    float entropy = system->tensor.emotional_entropy;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return entropy;
}

float emotion_tensor_get_stability(const emotion_tensor_system_t* system) {
    if (!system) {
        return -1.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_get_stability", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    float stability = system->tensor.stability;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return stability;
}

bool emotion_tensor_get_dominant(
    const emotion_tensor_system_t* system,
    emotion_primary_t* primary,
    emotion_primary_t* secondary,
    float* blend_ratio
) {
    if (!system || !primary || !secondary || !blend_ratio) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_get_dominant: required parameter is NULL (system, primary, secondary, blend_ratio)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_get_dominant", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    *primary = system->tensor.primary_emotion;
    *secondary = system->tensor.secondary_emotion;
    *blend_ratio = system->tensor.blend_ratio;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return true;
}

//=============================================================================
// Utility API Implementation
//=============================================================================

const char* emotion_tensor_emotion_name(emotion_primary_t emotion) {
    static const char* names[EMOTION_TENSOR_PRIMARY_COUNT] = {
        [TENSOR_JOY] = "joy",
        [TENSOR_TRUST] = "trust",
        [TENSOR_FEAR] = "fear",
        [TENSOR_SURPRISE] = "surprise",
        [TENSOR_SADNESS] = "sadness",
        [TENSOR_DISGUST] = "disgust",
        [TENSOR_ANGER] = "anger",
        [TENSOR_ANTICIPATION] = "anticipation"
    };

    if (emotion < 0 || emotion >= EMOTION_TENSOR_PRIMARY_COUNT) {
        return "unknown";
    }
    return names[emotion];
}

const char* emotion_tensor_compound_name(emotion_compound_t compound) {
    static const char* names[EMOTION_TENSOR_COMPOUND_COUNT] = {
        [COMPOUND_LOVE] = "love",
        [COMPOUND_SUBMISSION] = "submission",
        [COMPOUND_AWE] = "awe",
        [COMPOUND_DISAPPROVAL] = "disapproval",
        [COMPOUND_REMORSE] = "remorse",
        [COMPOUND_CONTEMPT] = "contempt",
        [COMPOUND_AGGRESSIVENESS] = "aggressiveness",
        [COMPOUND_OPTIMISM] = "optimism",
        [COMPOUND_GUILT] = "guilt",
        [COMPOUND_CURIOSITY] = "curiosity",
        [COMPOUND_DESPAIR] = "despair",
        [COMPOUND_UNBELIEF] = "unbelief",
        [COMPOUND_ENVY] = "envy",
        [COMPOUND_CYNICISM] = "cynicism",
        [COMPOUND_PRIDE] = "pride",
        [COMPOUND_HOPE] = "hope",
        [COMPOUND_BITTERSWEETNESS] = "bittersweetness",
        [COMPOUND_MORBID_CURIOSITY] = "morbid_curiosity",
        [COMPOUND_ANXIETY] = "anxiety",
        [COMPOUND_AMBIVALENCE] = "ambivalence",
        [COMPOUND_DESOLATION] = "desolation",
        [COMPOUND_FROZENNESS] = "frozenness",
        [COMPOUND_OUTRAGE] = "outrage",
        [COMPOUND_NOSTALGIA] = "nostalgia"
    };

    if (compound < 0 || compound >= EMOTION_TENSOR_COMPOUND_COUNT) {
        return "unknown";
    }
    return names[compound];
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotion_tensor_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_heartbeat("emotion_tens_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Tensor");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_tensor_heartbeat("emotion_tens_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Tensor");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Tensor");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Instance-Level Health Agent (Phase 8 Utility Integration)
 * ============================================================================ */


void emotion_tensor_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_emotion_tensor_instance_health_agent = agent;
}

/* ============================================================================
 * Training Stubs (Phase 8 Utility Integration)
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */

int emotion_tensor_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_training_begin: ctx is NULL");
        return -1;
    }
    emotion_tensor_heartbeat_instance(g_emotion_tensor_instance_health_agent, "emotion_tensor_training_begin", 0.0f);
    return 0;
}

int emotion_tensor_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_training_end: ctx is NULL");
        return -1;
    }
    emotion_tensor_heartbeat_instance(g_emotion_tensor_instance_health_agent, "emotion_tensor_training_end", 1.0f);
    return 0;
}

int emotion_tensor_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_training_step: ctx is NULL");
        return -1;
    }
    emotion_tensor_heartbeat_instance(g_emotion_tensor_instance_health_agent, "emotion_tensor_training_step", progress);
    return 0;
}
