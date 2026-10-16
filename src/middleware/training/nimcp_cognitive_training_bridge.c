#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_cognitive_training_bridge.c - Cognitive-Training Bridge Implementation
//=============================================================================
//
// WHAT: Bidirectional bridge integrating 5 cognitive modules with training
//       pipeline for cognitively-aware learning rate and batch modulation
//
// WHY:  Models how cognitive states (executive load, uncertainty, curiosity,
//       emotion, attention) affect learning. High cognitive load reduces LR
//       (conservative), curiosity boosts exploration, emotion provides feedback
//
// HOW:  Cognitive → Training: Modulate LR and batch size based on states
//       Training → Cognitive: Signal progress/stagnation as emotional feedback
//       Integrates with training-logic bridge for rule-based decisions
//
// BIOLOGICAL BASIS:
// - Executive load: Cognitive fatigue reduces learning capacity
// - Introspection uncertainty: High epistemic uncertainty → conservative learning
// - Curiosity: Exploration drive → increased learning rate
// - Emotion: Positive valence (progress) → boost, negative (frustration) → reduce
// - Attention: Focused attention → better learning efficiency
//
//=============================================================================

#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

/* Forward-declare real module APIs to avoid header conflicts (Python.h, etc.) */
extern float executive_get_cognitive_load(executive_controller_t* exec);
extern float multihead_attention_get_strength(multihead_attention_t mha);
extern float curiosity_get_drive(curiosity_engine_t engine);

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cognitive_training_bridge)

/*=============================================================================
 * TIME HELPERS
 *============================================================================*/

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Returns monotonic clock time in milliseconds
 * WHY:  Used for tracking update intervals
 * HOW:  Uses clock_gettime with CLOCK_MONOTONIC
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/* BIO_MODULE_COGNITIVE_TRAINING should be 0x0521 */
#ifndef BIO_MODULE_COGNITIVE_TRAINING
#define BIO_MODULE_COGNITIVE_TRAINING 0x0521
#endif

/* Clamping limits */
#define COGNITIVE_LR_FACTOR_MIN  0.1f   /**< Minimum LR factor */
#define COGNITIVE_LR_FACTOR_MAX  3.0f   /**< Maximum LR factor */
#define COGNITIVE_BATCH_FACTOR_MIN 0.2f /**< Minimum batch factor */
#define COGNITIVE_BATCH_FACTOR_MAX 2.0f /**< Maximum batch factor */

/* Stagnation detection */
#define COGNITIVE_STAGNATION_THRESHOLD  5  /**< Steps with increasing loss */

/* Loss history size */
#define COGNITIVE_HISTORY_SIZE  100  /**< Number of loss values to track */

/* Divergence types (internal use) */
typedef enum {
    COGNITIVE_DIVERGENCE_NONE = 0,
    COGNITIVE_DIVERGENCE_LOSS_EXPLOSION,
    COGNITIVE_DIVERGENCE_GRAD_EXPLOSION,
    COGNITIVE_DIVERGENCE_LOSS_PLATEAU,
    COGNITIVE_DIVERGENCE_OSCILLATION,
    COGNITIVE_DIVERGENCE_COUNT
} cognitive_training_divergence_t;

/*=============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/**
 * @brief Main bridge structure
 *
 * WHAT: Internal state for cognitive-training bridge
 * WHY:  Encapsulates all bridge data and integrations
 * HOW:  Single struct with all subsystems
 */
struct cognitive_training_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    cognitive_training_config_t config;

    /* Connected cognitive modules (may be NULL) */
    executive_controller_t* executive;
    introspection_context_t introspection;
    multihead_attention_t attention;  /* Already a pointer type */
    curiosity_engine_t curiosity;     /* Already a pointer type */
    emotion_recognition_system_t* emotion;

    /* Connected training components */
    nimcp_brain_training_ctx_t* training_ctx;
    training_logic_bridge_t* training_logic;
    training_plasticity_bridge_t* training_plasticity;
    training_immune_system_t* training_immune;
    perception_training_bridge_t* perception_training;
    cortical_training_bridge_t* cortical_training;

    /* Current effects */
    cognitive_training_effects_t cognitive_effects;
    training_cognitive_effects_t training_effects;

    /* Loss history for trend analysis */
    float* loss_history;
    uint32_t history_head;
    uint32_t history_count;
    uint32_t stagnation_count;  /**< Consecutive steps with loss increase */
    float current_lr;           /**< Current learning rate for tracking */

    /* Statistics */
    cognitive_training_stats_t stats;

    /* State */
    bool running;
    uint64_t last_update_ms;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Extract cognitive state from modules
 *
 * WHAT: Queries all connected cognitive modules for current state
 * WHY:  Aggregates cognitive metrics for modulation computation
 * HOW:  Calls getter functions on each module, handles NULL safely
 */
static int extract_cognitive_state(cognitive_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "extract_cognitive_state: bridge is NULL");

    cognitive_training_effects_t* effects = &bridge->cognitive_effects;

    /* Query cognitive modules for current state.
     * Each module provides specific cognitive signals that modulate training.
     */

    /* Executive: Query cognitive load from executive controller */
    if (bridge->executive && bridge->config.enable_executive) {
        float load = executive_get_cognitive_load(bridge->executive);
        effects->cognitive_load = (load >= 0.0f && load <= 1.0f) ? load : 0.5f;
    }

    /* Introspection: Query uncertainty (no features available at bridge level,
     * so we can't call brain_get_uncertainty which requires input features).
     * Consciousness phi is expensive — skip in hot path.
     * Use moderate defaults; cross-bridge integration overrides below. */
    if (bridge->introspection && bridge->config.enable_introspection) {
        effects->epistemic_uncertainty = 0.3f;
        effects->consciousness_phi = 0.6f;
    }

    /* Attention: Query focus strength from multihead attention module */
    if (bridge->attention && bridge->config.enable_attention) {
        float strength = multihead_attention_get_strength(bridge->attention);
        effects->attention_focus = (strength >= 0.0f && strength <= 1.0f)
                                   ? strength : 0.7f;
    }

    /* Curiosity: Query exploration drive from curiosity engine */
    if (bridge->curiosity && bridge->config.enable_curiosity) {
        float drive = curiosity_get_drive(bridge->curiosity);
        effects->exploration_drive = (drive >= 0.0f && drive <= 1.0f)
                                     ? drive : 0.5f;
    }

    /* Emotion: No valence/arousal getter on emotion_recognition_system_t
     * (emotion_tensor API uses different type). Default to neutral. */
    if (bridge->emotion && bridge->config.enable_emotion) {
        effects->emotional_valence = 0.0f;
        effects->emotional_arousal = 0.5f;
    }

    /*=========================================================================
     * CROSS-BRIDGE INTEGRATION: Perception-Training → Cognitive
     *
     * Propagates perceptual quality and salience to cognitive effects:
     * - visual_confidence → attention_focus (clear vision → better focus)
     * - speech_salience → task_relevance (salient speech → task priority)
     * - visual_novelty → exploration_drive (novel input → curiosity)
     *========================================================================*/
    if (bridge->perception_training) {
        perception_training_effects_t perception_effects;
        if (perception_training_get_effects(bridge->perception_training,
                                            &perception_effects) == 0 &&
            perception_effects.valid) {
            /* Visual confidence boosts attention focus */
            if (perception_effects.visual_confidence > 0.0f) {
                effects->attention_focus = fmaxf(effects->attention_focus,
                                                 perception_effects.visual_confidence);
            }

            /* Speech salience increases task relevance */
            if (perception_effects.speech_salience > 0.0f) {
                effects->task_relevance = fmaxf(effects->task_relevance,
                                                perception_effects.speech_salience);
            }

            /* Visual novelty drives exploration */
            if (perception_effects.visual_novelty > 0.0f) {
                effects->exploration_drive = fmaxf(effects->exploration_drive,
                                                   perception_effects.visual_novelty * 0.8f);
            }

            NIMCP_LOGGING_DEBUG("Perception → Cognitive: focus=%.2f relevance=%.2f explore=%.2f",
                               effects->attention_focus, effects->task_relevance,
                               effects->exploration_drive);
        }
    }

    /*=========================================================================
     * CROSS-BRIDGE INTEGRATION: Cortical-Training → Cognitive
     *
     * Propagates cortical dynamics to cognitive effects:
     * - (1 - burst_rate) → epistemic_uncertainty (low bursts → uncertain)
     * - predictions_stable → metacognitive_confidence (stable → confident)
     * - free_energy → cognitive_load (high FE → high load)
     *========================================================================*/
    if (bridge->cortical_training) {
        cortical_training_effects_t cortical_effects;
        if (cortical_training_get_effects(bridge->cortical_training,
                                          &cortical_effects) == 0 &&
            cortical_effects.valid) {
            /* Low burst rate increases epistemic uncertainty */
            float burst_uncertainty = 1.0f - cortical_effects.burst_rate;
            effects->epistemic_uncertainty = fmaxf(effects->epistemic_uncertainty,
                                                   burst_uncertainty * 0.6f);

            /* Stable predictions boost metacognitive confidence */
            if (cortical_effects.predictions_stable) {
                effects->metacognitive_confidence = fmaxf(effects->metacognitive_confidence,
                                                          0.7f);
            }

            /* High free energy increases cognitive load */
            if (cortical_effects.free_energy > 0.0f) {
                float fe_normalized = fminf(cortical_effects.free_energy / 10.0f, 1.0f);
                effects->cognitive_load = fmaxf(effects->cognitive_load,
                                                fe_normalized * 0.5f);
            }

            NIMCP_LOGGING_DEBUG("Cortical → Cognitive: uncertainty=%.2f confidence=%.2f load=%.2f",
                               effects->epistemic_uncertainty, effects->metacognitive_confidence,
                               effects->cognitive_load);
        }
    }

    effects->valid = true;

    return NIMCP_SUCCESS;
}

/**
 * @brief Compute learning rate modulation factor
 *
 * WHAT: Calculates multiplicative LR factor from cognitive states
 * WHY:  Adjusts learning rate based on cognitive capacity
 * HOW:  Combines effects from all modules with configured strengths
 *
 * FORMULAS:
 * - Executive: High load → conservative (factor 0.7-1.0)
 * - Introspection: High uncertainty → conservative (factor 0.5-1.0)
 * - Curiosity: High exploration → boost (factor 1.0-1.2)
 * - Emotion: Positive valence → boost (factor 0.7-1.3)
 * - Attention: High focus → boost (factor 1.0-1.2)
 * Final factor clamped to [0.1, 3.0]
 */
static float compute_lr_modulation(
    const cognitive_training_bridge_t* bridge,
    const cognitive_training_effects_t* effects)
{
    float lr_factor = 1.0f;

    /* Executive: High cognitive load reduces LR (0.7-1.0) */
    if (bridge->config.enable_executive) {
        float executive_reduction = effects->cognitive_load * 0.3f;
        lr_factor *= (1.0f - bridge->config.executive_strength * executive_reduction);
    }

    /* Introspection: High uncertainty reduces LR (0.5-1.0) */
    if (bridge->config.enable_introspection) {
        float uncertainty_reduction = effects->epistemic_uncertainty * 0.5f;
        lr_factor *= (1.0f - bridge->config.introspection_strength * uncertainty_reduction);
    }

    /* Curiosity: High exploration boosts LR (1.0-1.2) */
    if (bridge->config.enable_curiosity) {
        float curiosity_boost = effects->exploration_drive * 0.2f;
        lr_factor *= (1.0f + bridge->config.curiosity_strength * curiosity_boost);
    }

    /* Emotion: Positive valence boosts, negative reduces (0.7-1.3) */
    if (bridge->config.enable_emotion) {
        float emotion_modulation = effects->emotional_valence * 0.3f;
        lr_factor *= (1.0f + bridge->config.emotion_strength * emotion_modulation);
    }

    /* Attention: High focus boosts LR (1.0-1.2) */
    if (bridge->config.enable_attention) {
        float attention_boost = effects->attention_focus * 0.2f;
        lr_factor *= (1.0f + bridge->config.attention_strength * attention_boost);
    }

    /* Clamp to safety bounds */
    return nimcp_clampf(lr_factor, COGNITIVE_LR_FACTOR_MIN, COGNITIVE_LR_FACTOR_MAX);
}

/**
 * @brief Compute batch size modulation factor
 *
 * WHAT: Calculates batch size scaling from cognitive states
 * WHY:  Adjust batch size based on cognitive load and consciousness
 * HOW:  High load → smaller batches, low phi → smaller batches
 */
static float compute_batch_modulation(
    const cognitive_training_bridge_t* bridge,
    const cognitive_training_effects_t* effects)
{
    float batch_factor = 1.0f;

    /* High cognitive load → smaller batches (less overwhelming) */
    if (bridge->config.enable_executive) {
        if (effects->cognitive_load > bridge->config.batch_cognitive_load_threshold) {
            float overload = effects->cognitive_load - bridge->config.batch_cognitive_load_threshold;
            batch_factor *= (1.0f - overload * 0.5f);
        }
    }

    /* Low consciousness phi → reduce batch intensity */
    if (bridge->config.enable_introspection) {
        batch_factor *= (0.5f + 0.5f * effects->consciousness_phi);
    }

    /* Clamp to safety bounds */
    return nimcp_clampf(batch_factor, COGNITIVE_BATCH_FACTOR_MIN, COGNITIVE_BATCH_FACTOR_MAX);
}

/**
 * @brief Signal training feedback to cognitive modules
 *
 * WHAT: Sends training progress signals to emotion and curiosity
 * WHY:  Training is improving → satisfaction, stagnating → frustration
 * HOW:  Compares current loss to history, signals emotional state
 */
static int signal_training_feedback(
    cognitive_training_bridge_t* bridge,
    float current_loss)
{
    if (!bridge || bridge->history_count < 2) {
        return NIMCP_SUCCESS;  /* Need history for comparison */
    }

    /* Get previous loss */
    uint32_t prev_idx = (bridge->history_head + bridge->history_count - 2)
                        % COGNITIVE_HISTORY_SIZE;
    float prev_loss = bridge->loss_history[prev_idx];

    float loss_delta = current_loss - prev_loss;

    /* Improving (negative delta) → positive emotion */
    if (loss_delta < 0) {
        bridge->stagnation_count = 0;

        /* Signal satisfaction to emotion module: positive valence, moderate arousal */
        if (bridge->emotion && bridge->config.enable_emotion) {
            float improvement_ratio = -loss_delta / (prev_loss + 1e-10f);
            float satisfaction = nimcp_clampf(improvement_ratio * 2.0f, 0.1f, 1.0f);
            bridge->cognitive_effects.emotional_valence = satisfaction;
            bridge->cognitive_effects.emotional_arousal = 0.5f + satisfaction * 0.3f;
            bridge->cognitive_effects.stress_level = 0.0f;
        }

        /* Update training effects */
        bridge->training_effects.loss_trend = -loss_delta / (prev_loss + 1e-10f);
        bridge->training_effects.loss_improved = true;

    } else {
        /* Stagnating/diverging → negative emotion */
        bridge->stagnation_count++;

        if (bridge->stagnation_count >= COGNITIVE_STAGNATION_THRESHOLD) {
            /* Signal frustration to emotion module: negative valence, high arousal */
            if (bridge->emotion && bridge->config.enable_emotion) {
                float severity = nimcp_clampf(
                    (float)(bridge->stagnation_count - COGNITIVE_STAGNATION_THRESHOLD) /
                    (float)COGNITIVE_STAGNATION_THRESHOLD, 0.0f, 1.0f);
                float frustration = 0.3f + severity * 0.5f;
                bridge->cognitive_effects.emotional_valence = -frustration;
                bridge->cognitive_effects.emotional_arousal = 0.6f + frustration * 0.3f;
                bridge->cognitive_effects.stress_level = frustration;
            }

            /* Update training effects */
            bridge->training_effects.loss_trend = -0.5f;
            bridge->training_effects.loss_improved = false;
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Add loss to history buffer
 *
 * WHAT: Circular buffer for loss tracking
 * WHY:  Needed for trend analysis and feedback
 * HOW:  Ring buffer with head/count tracking
 */
static void add_loss_to_history(
    cognitive_training_bridge_t* bridge,
    float loss)
{
    if (!bridge || !bridge->loss_history) {
        return;
    }

    uint32_t idx = (bridge->history_head + bridge->history_count)
                   % COGNITIVE_HISTORY_SIZE;

    bridge->loss_history[idx] = loss;

    if (bridge->history_count < COGNITIVE_HISTORY_SIZE) {
        bridge->history_count++;
    } else {
        bridge->history_head = (bridge->history_head + 1) % COGNITIVE_HISTORY_SIZE;
    }
}

/**
 * @brief Update training-logic bridge with cognitive conditions
 *
 * WHAT: Syncs cognitive state to training-logic for rule-based decisions
 * WHY:  Allows logic gates to incorporate cognitive conditions
 * HOW:  Sets numeric conditions for cognitive_load and uncertainty
 */
static int sync_to_training_logic(cognitive_training_bridge_t* bridge) {
    if (!bridge->training_logic || !bridge->config.enable_training_logic) {
        return NIMCP_SUCCESS;
    }

    /* Set numeric conditions */
    training_logic_set_numeric_condition(
        bridge->training_logic,
        "cognitive_load",
        bridge->cognitive_effects.cognitive_load
    );

    training_logic_set_numeric_condition(
        bridge->training_logic,
        "uncertainty",
        bridge->cognitive_effects.epistemic_uncertainty
    );

    /* Set boolean conditions */
    bool resource_ok = bridge->cognitive_effects.cognitive_load < 0.8f;
    training_logic_set_condition(
        bridge->training_logic,
        TRAINING_COND_RESOURCE_OK,
        resource_ok
    );

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - LIFECYCLE
 *============================================================================*/

void cognitive_training_default_config(cognitive_training_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(cognitive_training_config_t));

    /* Mode */
    config->mode = COGNITIVE_TRAINING_MODE_AUTOMATIC;

    /* Enable all modules */
    config->enable_executive = true;
    config->enable_introspection = true;
    config->enable_attention = true;
    config->enable_curiosity = true;
    config->enable_emotion = true;

    /* Modulation strengths (0-1) */
    config->executive_strength = 0.5f;
    config->introspection_strength = 0.5f;
    config->attention_strength = 0.3f;
    config->curiosity_strength = 0.3f;
    config->emotion_strength = 0.4f;

    /* Thresholds */
    config->batch_cognitive_load_threshold = 0.7f;
    config->exploration_curiosity_threshold = 0.7f;

    /* LR modulation limits */
    config->lr_min_factor = COGNITIVE_TRAINING_DEFAULT_LR_MIN_FACTOR;
    config->lr_max_factor = COGNITIVE_TRAINING_DEFAULT_LR_MAX_FACTOR;

    /* Batch modulation limits */
    config->batch_min_factor = COGNITIVE_TRAINING_DEFAULT_BATCH_MIN_FACTOR;
    config->batch_max_factor = COGNITIVE_TRAINING_DEFAULT_BATCH_MAX_FACTOR;

    /* Gradient scaling limits */
    config->gradient_min_scale = COGNITIVE_TRAINING_DEFAULT_GRADIENT_MIN_SCALE;
    config->gradient_max_scale = COGNITIVE_TRAINING_DEFAULT_GRADIENT_MAX_SCALE;

    /* Integration */
    config->enable_training_logic = true;
    config->enable_bio_async = true;

    /* Testing */
    config->disable_auto_update = false;
}

cognitive_training_bridge_t* cognitive_training_create(
    const cognitive_training_config_t* config)
{
    /* Use default config if not provided */
    cognitive_training_config_t default_config;
    if (!config) {
        cognitive_training_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge */
    cognitive_training_bridge_t* bridge = (cognitive_training_bridge_t*)nimcp_malloc(
        sizeof(cognitive_training_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_training_create: bridge allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(cognitive_training_bridge_t));

    /* Store config */
    memcpy(&bridge->config, config, sizeof(cognitive_training_config_t));

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "cognitive_training") != 0) { goto cleanup; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_training_create: mutex creation failed");
        goto cleanup;
    }

    /* Allocate loss history buffer */
    bridge->loss_history = (float*)nimcp_malloc(
        sizeof(float) * COGNITIVE_HISTORY_SIZE
    );
    if (!bridge->loss_history) {
        NIMCP_LOGGING_ERROR("Failed to allocate history buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_training_create: loss_history allocation failed");
        goto cleanup;
    }
    memset(bridge->loss_history, 0, sizeof(float) * COGNITIVE_HISTORY_SIZE);

    /* Initialize default modulation factors to 1.0 (no modulation) */
    bridge->cognitive_effects.lr_factor = 1.0f;
    bridge->cognitive_effects.batch_size_factor = 1.0f;
    bridge->cognitive_effects.gradient_scale_factor = 1.0f;

    NIMCP_LOGGING_INFO("Created Cognitive-Training bridge");

    return bridge;

cleanup:
    nimcp_free(bridge->loss_history);
    if (bridge->base.mutex) { bridge_base_cleanup(&bridge->base); }
    nimcp_free(bridge);
    return NULL;
}

void cognitive_training_destroy(cognitive_training_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        cognitive_training_disconnect_bio_async(bridge);
    }

    /* Free loss history */
    if (bridge->loss_history) {
        nimcp_free(bridge->loss_history);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed Cognitive-Training bridge");
}

int cognitive_training_start(cognitive_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_start: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async && !bridge->base.bio_async_enabled) {
        int result = cognitive_training_connect_bio_async(bridge);
        if (result != NIMCP_SUCCESS) {
            NIMCP_LOGGING_WARN("Bio-async connection failed, continuing without it");
        }
    }

    /* Extract initial cognitive state */
    if (!bridge->config.disable_auto_update) {
        extract_cognitive_state(bridge);
    }

    bridge->running = true;
    bridge->last_update_ms = get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Started Cognitive-Training bridge");

    return NIMCP_SUCCESS;
}

int cognitive_training_stop(cognitive_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_stop: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->running = false;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        cognitive_training_disconnect_bio_async(bridge);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Stopped Cognitive-Training bridge");

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - COGNITIVE MODULE CONNECTIONS
 *============================================================================*/

int cognitive_training_connect_executive(
    cognitive_training_bridge_t* bridge,
    executive_controller_t* executive)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_executive: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->executive = executive;
    if (executive) {
        NIMCP_LOGGING_INFO("Connected executive controller to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_introspection(
    cognitive_training_bridge_t* bridge,
    introspection_context_t introspection)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_introspection: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->introspection = introspection;
    if (introspection) {
        NIMCP_LOGGING_INFO("Connected introspection context to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_attention(
    cognitive_training_bridge_t* bridge,
    multihead_attention_t attention)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_attention: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->attention = attention;
    if (attention) {
        NIMCP_LOGGING_INFO("Connected multihead attention to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_curiosity(
    cognitive_training_bridge_t* bridge,
    curiosity_engine_t curiosity)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_curiosity: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->curiosity = curiosity;
    if (curiosity) {
        NIMCP_LOGGING_INFO("Connected curiosity engine to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_emotion(
    cognitive_training_bridge_t* bridge,
    emotion_recognition_system_t* emotion)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_emotion: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->emotion = emotion;
    if (emotion) {
        NIMCP_LOGGING_INFO("Connected emotion recognition to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - TRAINING COMPONENT CONNECTIONS
 *============================================================================*/

int cognitive_training_connect_brain_training(
    cognitive_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_brain_training: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_ctx = training_ctx;
    if (training_ctx) {
        NIMCP_LOGGING_INFO("Connected brain training context to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_training_logic(
    cognitive_training_bridge_t* bridge,
    training_logic_bridge_t* training_logic)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_training_logic: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_logic = training_logic;
    if (training_logic) {
        NIMCP_LOGGING_INFO("Connected training-logic bridge to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_training_plasticity(
    cognitive_training_bridge_t* bridge,
    training_plasticity_bridge_t* training_plasticity)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_training_plasticity: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_plasticity = training_plasticity;
    if (training_plasticity) {
        NIMCP_LOGGING_INFO("Connected training-plasticity bridge to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_training_immune(
    cognitive_training_bridge_t* bridge,
    training_immune_system_t* training_immune)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_training_immune: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_immune = training_immune;
    if (training_immune) {
        NIMCP_LOGGING_INFO("Connected training-immune system to Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_perception_training(
    cognitive_training_bridge_t* bridge,
    perception_training_bridge_t* perception_training)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_perception_training: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->perception_training = perception_training;
    bridge->stats.perception_training_connected = (perception_training != NULL);

    if (perception_training) {
        NIMCP_LOGGING_INFO("Connected perception-training bridge to Cognitive-Training bridge");
    } else {
        NIMCP_LOGGING_INFO("Disconnected perception-training bridge from Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_connect_cortical_training(
    cognitive_training_bridge_t* bridge,
    cortical_training_bridge_t* cortical_training)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_cortical_training: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->cortical_training = cortical_training;
    bridge->stats.cortical_training_connected = (cortical_training != NULL);

    if (cortical_training) {
        NIMCP_LOGGING_INFO("Connected cortical-training bridge to Cognitive-Training bridge");
    } else {
        NIMCP_LOGGING_INFO("Disconnected cortical-training bridge from Cognitive-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - COGNITIVE → TRAINING
 *============================================================================*/

int cognitive_training_update(cognitive_training_bridge_t* bridge, uint64_t delta_ms) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_update: bridge is NULL");
    (void)delta_ms;  /* Currently unused, for future time-based updates */

    nimcp_mutex_lock(bridge->base.mutex);

    /* Extract current cognitive state */
    int result = extract_cognitive_state(bridge);
    if (result != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return result;
    }

    /* Compute modulation factors */
    float lr_factor = compute_lr_modulation(bridge, &bridge->cognitive_effects);
    float batch_factor = compute_batch_modulation(bridge, &bridge->cognitive_effects);

    /* Store in cognitive effects */
    bridge->cognitive_effects.lr_factor = lr_factor;
    bridge->cognitive_effects.batch_size_factor = batch_factor;

    /* Sync to training-logic if enabled */
    sync_to_training_logic(bridge);

    /* Update stats */
    bridge->stats.total_update_calls++;
    bridge->last_update_ms = get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float cognitive_training_get_lr_factor(const cognitive_training_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }

    return bridge->cognitive_effects.lr_factor;
}

float cognitive_training_get_batch_factor(const cognitive_training_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }

    return bridge->cognitive_effects.batch_size_factor;
}

float cognitive_training_get_effective_lr(
    const cognitive_training_bridge_t* bridge,
    float base_lr)
{
    if (!bridge) {
        return base_lr;
    }

    return base_lr * bridge->cognitive_effects.lr_factor;
}

uint32_t cognitive_training_get_effective_batch_size(
    const cognitive_training_bridge_t* bridge,
    uint32_t base_batch_size)
{
    if (!bridge) {
        return base_batch_size;
    }

    float factor = bridge->cognitive_effects.batch_size_factor;

    /* Clamp factor to configured limits */
    if (factor < bridge->config.batch_min_factor) {
        factor = bridge->config.batch_min_factor;
    }
    if (factor > bridge->config.batch_max_factor) {
        factor = bridge->config.batch_max_factor;
    }

    float modulated = (float)base_batch_size * factor;

    /* Ensure at least 1 */
    if (modulated < 1.0f) {
        modulated = 1.0f;
    }

    return (uint32_t)modulated;
}

int cognitive_training_get_cognitive_effects(
    const cognitive_training_bridge_t* bridge,
    cognitive_training_effects_t* effects)
{
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "cognitive_training_get_cognitive_effects: NULL argument");

    memcpy(effects, &bridge->cognitive_effects, sizeof(cognitive_training_effects_t));

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - TRAINING → COGNITIVE
 *============================================================================*/

int cognitive_training_update_metrics(
    cognitive_training_bridge_t* bridge,
    float loss,
    float grad_norm,
    float learning_rate,
    uint64_t step)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_update_metrics: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Add to history */
    add_loss_to_history(bridge, loss);

    /* Signal feedback to cognitive modules */
    signal_training_feedback(bridge, loss);

    /* Update training effects */
    bridge->training_effects.loss_current = loss;
    bridge->training_effects.gradient_norm = grad_norm;
    bridge->current_lr = learning_rate;  // Track in bridge, not effects
    bridge->training_effects.current_step = step;

    /* Update stats */
    bridge->stats.total_modulations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_signal_divergence(
    cognitive_training_bridge_t* bridge,
    cognitive_training_divergence_t divergence_type)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_signal_divergence: bridge is NULL");
    NIMCP_CHECK_THROW(divergence_type < COGNITIVE_DIVERGENCE_COUNT, NIMCP_ERROR_INVALID_PARAM,
                      "cognitive_training_signal_divergence: invalid divergence_type %d", divergence_type);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Signal ALARM emotion: high negative valence, maximum arousal, high stress */
    if (bridge->emotion && bridge->config.enable_emotion) {
        /* Divergence is a critical event - trigger strong alarm response */
        bridge->cognitive_effects.emotional_valence = -0.9f;   /* Strong negative */
        bridge->cognitive_effects.emotional_arousal = 1.0f;    /* Maximum arousal */
        bridge->cognitive_effects.stress_level = 0.95f;        /* Near-maximum stress */
        bridge->cognitive_effects.should_checkpoint = true;     /* Emergency checkpoint */
    }

    /* Update stats */
    bridge->stats.feedback_by_type[COGNITIVE_TRAINING_FEEDBACK_ALARM]++;

    NIMCP_LOGGING_WARN("Training divergence signaled: type=%u", divergence_type);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int cognitive_training_get_training_effects(
    const cognitive_training_bridge_t* bridge,
    training_cognitive_effects_t* effects)
{
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "cognitive_training_get_training_effects: NULL argument");

    memcpy(effects, &bridge->training_effects, sizeof(training_cognitive_effects_t));

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - BIO-ASYNC
 *============================================================================*/

int cognitive_training_connect_bio_async(cognitive_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_connect_bio_async: bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_COGNITIVE_TRAINING,
        .module_name = "cognitive_training_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return NIMCP_ERROR_OPERATION_FAILED;
}

int cognitive_training_disconnect_bio_async(cognitive_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_disconnect_bio_async: bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already disconnected */
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool cognitive_training_is_bio_async_connected(
    const cognitive_training_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}

int cognitive_training_process_inbox(cognitive_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_training_process_inbox: bridge is NULL");
        return -1;  /* Return negative for null pointer */
    }

    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;  /* No messages to process */
    }

    return bio_router_process_inbox(bridge->base.bio_ctx, 10);
}

/*=============================================================================
 * PUBLIC API - STATISTICS
 *============================================================================*/

int cognitive_training_get_stats(
    const cognitive_training_bridge_t* bridge,
    cognitive_training_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "cognitive_training_get_stats: NULL argument");

    memcpy(stats, &bridge->stats, sizeof(cognitive_training_stats_t));

    return NIMCP_SUCCESS;
}

int cognitive_training_reset_stats(cognitive_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_reset_stats: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(cognitive_training_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Reset Cognitive-Training statistics");

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - UTILITIES
 *============================================================================*/

const char* cognitive_training_mode_to_string(cognitive_training_mode_t mode) {
    switch (mode) {
        case COGNITIVE_TRAINING_MODE_DISABLED: return "disabled";
        case COGNITIVE_TRAINING_MODE_MONITOR_ONLY: return "monitor_only";
        case COGNITIVE_TRAINING_MODE_ADVISORY: return "advisory";
        case COGNITIVE_TRAINING_MODE_AUTOMATIC: return "automatic";
        default: return "unknown";
    }
}

const char* cognitive_training_divergence_to_string(
    cognitive_training_divergence_t divergence)
{
    switch (divergence) {
        case COGNITIVE_DIVERGENCE_NONE: return "none";
        case COGNITIVE_DIVERGENCE_LOSS_EXPLOSION: return "loss_explosion";
        case COGNITIVE_DIVERGENCE_GRAD_EXPLOSION: return "grad_explosion";
        case COGNITIVE_DIVERGENCE_LOSS_PLATEAU: return "loss_plateau";
        case COGNITIVE_DIVERGENCE_OSCILLATION: return "oscillation";
        default: return "unknown";
    }
}

void cognitive_training_dump_state(const cognitive_training_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    NIMCP_LOGGING_INFO("=== Cognitive-Training Bridge State ===");
    NIMCP_LOGGING_INFO("Mode: %s",
                       cognitive_training_mode_to_string(bridge->config.mode));
    NIMCP_LOGGING_INFO("Running: %s", bridge->running ? "true" : "false");

    NIMCP_LOGGING_INFO("Cognitive Effects:");
    NIMCP_LOGGING_INFO("  cognitive_load=%.3f uncertainty=%.3f",
                       bridge->cognitive_effects.cognitive_load,
                       bridge->cognitive_effects.epistemic_uncertainty);
    NIMCP_LOGGING_INFO("  attention_focus=%.3f exploration_drive=%.3f",
                       bridge->cognitive_effects.attention_focus,
                       bridge->cognitive_effects.exploration_drive);
    NIMCP_LOGGING_INFO("  emotional_valence=%.3f arousal=%.3f",
                       bridge->cognitive_effects.emotional_valence,
                       bridge->cognitive_effects.emotional_arousal);
    NIMCP_LOGGING_INFO("  consciousness_phi=%.3f",
                       bridge->cognitive_effects.consciousness_phi);

    NIMCP_LOGGING_INFO("Modulation:");
    NIMCP_LOGGING_INFO("  lr_factor=%.3f batch_factor=%.3f",
                       bridge->cognitive_effects.lr_factor,
                       bridge->cognitive_effects.batch_size_factor);

    NIMCP_LOGGING_INFO("Training Effects:");
    NIMCP_LOGGING_INFO("  loss=%.6f grad_norm=%.6f lr=%.6e",
                       bridge->training_effects.loss_current,
                       bridge->training_effects.gradient_norm,
                       bridge->current_lr);
    NIMCP_LOGGING_INFO("  trend=%.3f improved=%s step=%lu",
                       bridge->training_effects.loss_trend,
                       bridge->training_effects.loss_improved ? "yes" : "no",
                       bridge->training_effects.current_step);

    NIMCP_LOGGING_INFO("Statistics:");
    NIMCP_LOGGING_INFO("  update_calls=%lu modulations=%lu",
                       bridge->stats.total_update_calls,
                       bridge->stats.total_modulations);
    NIMCP_LOGGING_INFO("  alarm_signals=%lu stagnation_count=%u",
                       bridge->stats.feedback_by_type[COGNITIVE_TRAINING_FEEDBACK_ALARM],
                       bridge->stagnation_count);

    NIMCP_LOGGING_INFO("======================================");
}

//=============================================================================
// Additional API Functions (matching header declarations)
//=============================================================================

int cognitive_training_connect_training_context(
    cognitive_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx
) {
    return cognitive_training_connect_brain_training(bridge, training_ctx);
}

int cognitive_training_get_effects(
    const cognitive_training_bridge_t* bridge,
    cognitive_training_effects_t* effects
) {
    return cognitive_training_get_cognitive_effects(bridge, effects);
}

float cognitive_training_get_modulated_lr(
    const cognitive_training_bridge_t* bridge,
    float base_lr
) {
    return cognitive_training_get_effective_lr(bridge, base_lr);
}

uint32_t cognitive_training_get_modulated_batch_size(
    const cognitive_training_bridge_t* bridge,
    uint32_t base_batch_size
) {
    return cognitive_training_get_effective_batch_size(bridge, base_batch_size);
}

int cognitive_training_get_gradient_scaling(
    const cognitive_training_bridge_t* bridge,
    float* factors,
    uint32_t num_features
) {
    NIMCP_CHECK_THROW(bridge && factors, NIMCP_ERROR_NULL_POINTER, "cognitive_training_get_gradient_scaling: NULL argument");

    /* Zero features is valid - nothing to do */
    if (num_features == 0) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Use attention-based scaling if available, otherwise uniform scaling */
    float global_scale = bridge->cognitive_effects.gradient_scale_factor;

    if (bridge->cognitive_effects.feature_attention != NULL &&
        bridge->cognitive_effects.num_features >= num_features) {
        /* Apply per-feature attention weights */
        for (uint32_t i = 0; i < num_features; i++) {
            factors[i] = global_scale * bridge->cognitive_effects.feature_attention[i];
        }
    } else {
        /* Uniform scaling */
        for (uint32_t i = 0; i < num_features; i++) {
            factors[i] = global_scale;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

bool cognitive_training_should_checkpoint(
    const cognitive_training_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->cognitive_effects.should_checkpoint;
}

float cognitive_training_get_exploration_intensity(
    const cognitive_training_bridge_t* bridge
) {
    if (!bridge) {
        return 0.0f;
    }
    return bridge->cognitive_effects.exploration_drive;
}

int cognitive_training_signal_event(
    cognitive_training_bridge_t* bridge,
    cognitive_training_feedback_t event,
    float magnitude
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_signal_event: bridge is NULL");
    NIMCP_CHECK_THROW(event >= 0 && event < COGNITIVE_TRAINING_FEEDBACK_COUNT, NIMCP_ERROR_INVALID_PARAM,
                      "cognitive_training_signal_event: invalid event %d", event);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update stats */
    bridge->stats.total_feedback_events++;
    bridge->stats.feedback_by_type[event]++;

    /* Process event */
    switch (event) {
        case COGNITIVE_TRAINING_FEEDBACK_SATISFACTION:
            /* Positive emotion, reduce uncertainty */
            bridge->cognitive_effects.emotional_valence =
                fminf(1.0f, bridge->cognitive_effects.emotional_valence + magnitude * 0.2f);
            bridge->cognitive_effects.epistemic_uncertainty *= (1.0f - magnitude * 0.1f);
            break;

        case COGNITIVE_TRAINING_FEEDBACK_FRUSTRATION:
            /* Negative emotion, increase exploration */
            bridge->cognitive_effects.emotional_valence =
                fmaxf(-1.0f, bridge->cognitive_effects.emotional_valence - magnitude * 0.2f);
            bridge->cognitive_effects.exploration_drive =
                fminf(1.0f, bridge->cognitive_effects.exploration_drive + magnitude * 0.1f);
            break;

        case COGNITIVE_TRAINING_FEEDBACK_ALARM:
            /* Stress response, trigger checkpoint */
            bridge->cognitive_effects.stress_level =
                fminf(1.0f, bridge->cognitive_effects.stress_level + magnitude * 0.5f);
            bridge->cognitive_effects.should_checkpoint = true;
            break;

        case COGNITIVE_TRAINING_FEEDBACK_NOVELTY:
            /* Curiosity boost */
            bridge->cognitive_effects.exploration_drive =
                fminf(1.0f, bridge->cognitive_effects.exploration_drive + magnitude * 0.15f);
            break;

        case COGNITIVE_TRAINING_FEEDBACK_MASTERY:
            /* Satisfaction, consolidate */
            bridge->cognitive_effects.emotional_valence =
                fminf(1.0f, bridge->cognitive_effects.emotional_valence + magnitude * 0.3f);
            bridge->cognitive_effects.should_consolidate = true;
            break;

        case COGNITIVE_TRAINING_FEEDBACK_STAGNATION:
            /* Frustration, increase exploration */
            bridge->cognitive_effects.exploration_drive =
                fminf(1.0f, bridge->cognitive_effects.exploration_drive + magnitude * 0.2f);
            bridge->cognitive_effects.emotional_valence -= magnitude * 0.1f;
            break;

        case COGNITIVE_TRAINING_FEEDBACK_BREAKTHROUGH:
            /* Strong positive, boost confidence */
            bridge->cognitive_effects.emotional_valence =
                fminf(1.0f, bridge->cognitive_effects.emotional_valence + magnitude * 0.4f);
            bridge->cognitive_effects.metacognitive_confidence =
                fminf(1.0f, bridge->cognitive_effects.metacognitive_confidence + magnitude * 0.2f);
            break;

        case COGNITIVE_TRAINING_FEEDBACK_CHECKPOINT_OK:
            /* Reduce stress, checkpoint done */
            bridge->cognitive_effects.stress_level *= (1.0f - magnitude * 0.3f);
            bridge->cognitive_effects.should_checkpoint = false;
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int cognitive_training_pattern_learned(
    cognitive_training_bridge_t* bridge,
    const char* pattern_name,
    float novelty
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_pattern_learned: bridge is NULL");
    (void)pattern_name;  /* For future use */

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update curiosity state */
    if (novelty > 0.5f) {
        /* Highly novel pattern - boost satisfaction */
        bridge->cognitive_effects.emotional_valence =
            fminf(1.0f, bridge->cognitive_effects.emotional_valence + novelty * 0.2f);
        bridge->stats.feedback_by_type[COGNITIVE_TRAINING_FEEDBACK_NOVELTY]++;
    }

    /* Reduce knowledge gap */
    bridge->cognitive_effects.knowledge_gap_size *= (1.0f - novelty * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int cognitive_training_checkpoint_complete(
    cognitive_training_bridge_t* bridge,
    uint64_t step
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_checkpoint_complete: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update training effects */
    bridge->training_effects.last_checkpoint_step = step;
    bridge->training_effects.checkpoint_complete = true;

    /* Reduce uncertainty and stress */
    bridge->cognitive_effects.epistemic_uncertainty *= 0.9f;
    bridge->cognitive_effects.stress_level *= 0.8f;
    bridge->cognitive_effects.should_checkpoint = false;

    /* Update stats */
    bridge->stats.checkpoints_triggered++;
    bridge->stats.feedback_by_type[COGNITIVE_TRAINING_FEEDBACK_CHECKPOINT_OK]++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int cognitive_training_update_cognitive_state(
    cognitive_training_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_update_cognitive_state: bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int result = extract_cognitive_state(bridge);
    if (result == NIMCP_SUCCESS) {
        /* Increment stats for the update call */
        bridge->stats.total_update_calls++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int cognitive_training_apply_feedback(
    cognitive_training_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "cognitive_training_apply_feedback: bridge is NULL");

    /* Check for events to trigger based on training effects */
    /* Note: signal_event handles its own locking */
    if (bridge->training_effects.divergence_detected) {
        cognitive_training_signal_event(bridge, COGNITIVE_TRAINING_FEEDBACK_ALARM, 0.8f);
    } else if (bridge->training_effects.stagnation_detected) {
        cognitive_training_signal_event(bridge, COGNITIVE_TRAINING_FEEDBACK_STAGNATION, 0.6f);
    } else if (bridge->training_effects.breakthrough_detected) {
        cognitive_training_signal_event(bridge, COGNITIVE_TRAINING_FEEDBACK_BREAKTHROUGH, 0.9f);
    } else if (bridge->training_effects.loss_improved) {
        cognitive_training_signal_event(bridge, COGNITIVE_TRAINING_FEEDBACK_SATISFACTION, 0.3f);
    }

    return NIMCP_SUCCESS;
}

const char* cognitive_training_modulation_to_string(
    cognitive_training_modulation_t modulation
) {
    switch (modulation) {
        case COGNITIVE_TRAINING_MODULATION_LR:
            return "LEARNING_RATE";
        case COGNITIVE_TRAINING_MODULATION_BATCH_SIZE:
            return "BATCH_SIZE";
        case COGNITIVE_TRAINING_MODULATION_GRADIENT_SCALE:
            return "GRADIENT_SCALE";
        case COGNITIVE_TRAINING_MODULATION_CHECKPOINT:
            return "CHECKPOINT";
        case COGNITIVE_TRAINING_MODULATION_EXPLORATION:
            return "EXPLORATION";
        case COGNITIVE_TRAINING_MODULATION_SAMPLE_PRIORITY:
            return "SAMPLE_PRIORITY";
        case COGNITIVE_TRAINING_MODULATION_EARLY_STOP:
            return "EARLY_STOP";
        default:
            return "UNKNOWN";
    }
}

const char* cognitive_training_feedback_to_string(
    cognitive_training_feedback_t event
) {
    switch (event) {
        case COGNITIVE_TRAINING_FEEDBACK_SATISFACTION:
            return "SATISFACTION";
        case COGNITIVE_TRAINING_FEEDBACK_FRUSTRATION:
            return "FRUSTRATION";
        case COGNITIVE_TRAINING_FEEDBACK_ALARM:
            return "ALARM";
        case COGNITIVE_TRAINING_FEEDBACK_NOVELTY:
            return "NOVELTY";
        case COGNITIVE_TRAINING_FEEDBACK_MASTERY:
            return "MASTERY";
        case COGNITIVE_TRAINING_FEEDBACK_STAGNATION:
            return "STAGNATION";
        case COGNITIVE_TRAINING_FEEDBACK_BREAKTHROUGH:
            return "BREAKTHROUGH";
        case COGNITIVE_TRAINING_FEEDBACK_CHECKPOINT_OK:
            return "CHECKPOINT_OK";
        default:
            return "UNKNOWN";
    }
}

//=============================================================================
// Test API
//=============================================================================

int cognitive_training_set_effects_for_testing(
    cognitive_training_bridge_t* bridge,
    const cognitive_training_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "cognitive_training_set_effects_for_testing: NULL argument");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Copy the provided effects, preserving feature_attention pointer handling */
    float* existing_feature_attention = bridge->cognitive_effects.feature_attention;
    uint32_t existing_num_features = bridge->cognitive_effects.num_features;

    memcpy(&bridge->cognitive_effects, effects, sizeof(cognitive_training_effects_t));

    /* If new effects don't have feature_attention, restore the existing one */
    if (!effects->feature_attention) {
        bridge->cognitive_effects.feature_attention = existing_feature_attention;
        bridge->cognitive_effects.num_features = existing_num_features;
    }

    bridge->cognitive_effects.valid = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}
