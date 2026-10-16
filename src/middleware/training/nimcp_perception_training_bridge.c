#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_perception_training_bridge.c - Perception-Training Bridge Implementation
//=============================================================================
//
// WHAT: Bidirectional bridge integrating 3 perception cortices with training
//       pipeline for perception-aware learning rate and sample prioritization
//
// WHY:  Models how perceptual quality affects learning. High visual confidence
//       boosts LR (clear input → enhanced plasticity), poor audio quality
//       reduces it (noisy input → conservative learning)
//
// HOW:  Perception → Training: Modulate LR and sample weights based on states
//       Training → Perception: Signal sensitivity adjustments, consolidation
//       Integrates with cognitive-training, training-logic, training-immune
//
// BIOLOGICAL BASIS:
// - Visual confidence: Clear visual features → attention-gated LTP enhancement
// - Audio quality: Signal-to-noise ratio modulates sensory gating
// - Speech salience: Phonological salience guides memory prioritization
// - Novelty detection: Novel patterns → curiosity-driven exploration boost
// - Multi-modal integration: Cross-modal binding enhances consolidation
//
//=============================================================================

#include "middleware/training/nimcp_perception_training_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "middleware/training/nimcp_training_logic_bridge.h"
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

/*=============================================================================
 * PERCEPTION CORTEX API DECLARATIONS
 *
 * The bridge header forward-declares cortex types as pointer typedefs
 * (e.g., typedef struct visual_cortex_struct* visual_cortex_t) which conflicts
 * with the cortex headers' non-pointer typedefs. To avoid type conflicts, we
 * declare the specific functions we need using the bridge's pointer types.
 *============================================================================*/

/* Visual cortex training state */
typedef struct {
    float* gabor_activations;
    uint32_t gabor_size;
    float* conv_activations;
    uint32_t conv_size;
    float* pool_output;
    uint32_t pool_output_size;
    float confidence;
    float novelty;
    uint64_t timestamp_ms;
    bool valid;
} visual_training_state_t;

/* Visual cortex stats */
typedef struct {
    uint32_t images_processed;
    uint32_t memories_stored;
    float avg_processing_time;
    float memory_usage_mb;
} visual_cortex_stats_local_t;

/* Audio cortex training state */
typedef struct {
    float* mel_features;
    uint32_t num_mel_filters;
    float* mfcc_features;
    uint32_t num_mfcc;
    float quality;
    float speech_salience;
    float temporal_coherence;
    uint64_t timestamp_ms;
    bool valid;
} audio_training_state_t;

/* Audio cortex stats */
typedef struct {
    uint64_t frames_processed;
    uint32_t memories_stored;
    float avg_processing_time;
} audio_cortex_stats_local_t;

/* Speech cortex stats */
typedef struct {
    uint64_t frames_processed;
    uint32_t phonemes_detected;
    uint32_t words_recognized;
    float avg_processing_time_ms;
    float phoneme_accuracy;
} speech_cortex_stats_local_t;

/* Extern declarations using the bridge's pointer types (struct X_struct*) */
extern int visual_cortex_get_training_state(visual_cortex_t cortex, visual_training_state_t* state);
extern bool visual_cortex_get_stats(const visual_cortex_t cortex, visual_cortex_stats_local_t* stats);
extern int audio_cortex_get_training_state(audio_cortex_t cortex, audio_training_state_t* state);
extern bool audio_cortex_get_stats(const audio_cortex_t cortex, audio_cortex_stats_local_t* stats);
extern bool speech_cortex_get_stats(const speech_cortex_t cortex, speech_cortex_stats_local_t* stats);
extern float speech_cortex_get_phoneme_confidence(speech_cortex_t cortex);

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(perception_training_bridge)

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

/* BIO_MODULE_PERCEPTION_TRAINING should be 0x0523 */
#ifndef BIO_MODULE_PERCEPTION_TRAINING
#define BIO_MODULE_PERCEPTION_TRAINING 0x0523
#endif

/* Clamping limits */
#define PERCEPTION_LR_FACTOR_MIN  0.5f   /**< Minimum LR factor */
#define PERCEPTION_LR_FACTOR_MAX  1.5f   /**< Maximum LR factor */
#define PERCEPTION_SAMPLE_WEIGHT_MIN 0.1f /**< Minimum sample weight */
#define PERCEPTION_SAMPLE_WEIGHT_MAX 2.0f /**< Maximum sample weight */

/* Default skip thresholds */
#define PERCEPTION_DEFAULT_SKIP_VISUAL_THRESHOLD   0.3f
#define PERCEPTION_DEFAULT_SKIP_AUDIO_THRESHOLD    0.3f
#define PERCEPTION_DEFAULT_SKIP_SPEECH_THRESHOLD   0.3f

/* Metric history size */
#define PERCEPTION_HISTORY_SIZE  50  /**< Number of metric values to track */

/*=============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/**
 * @brief Main bridge structure
 *
 * WHAT: Internal state for perception-training bridge
 * WHY:  Encapsulates all bridge data and integrations
 * HOW:  Single struct with all subsystems
 */
struct perception_training_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    perception_training_config_t config;

    /* Connected perception cortices (may be NULL) */
    visual_cortex_t visual_cortex;
    audio_cortex_t audio_cortex;
    speech_cortex_t speech_cortex;

    /* Connected training components */
    nimcp_brain_training_ctx_t* training_ctx;
    cognitive_training_bridge_t* cognitive_training;
    training_logic_bridge_t* training_logic;
    training_plasticity_bridge_t* training_plasticity;
    training_immune_system_t* training_immune;

    /* Current effects */
    perception_training_effects_t perception_effects;
    training_perception_effects_t training_effects;

    /* Loss history for trend analysis */
    float* loss_history;
    uint32_t history_head;
    uint32_t history_count;
    float prev_loss;

    /* Statistics */
    perception_training_stats_t stats;

    /* State */
    bool running;
    uint64_t last_update_ms;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Extract perception state from cortices
 *
 * WHAT: Queries all connected perception cortices for current state
 * WHY:  Aggregates perceptual metrics for modulation computation
 * HOW:  Calls getter functions on each cortex, handles NULL safely
 */
static int extract_perception_state(perception_training_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    perception_training_effects_t* effects = &bridge->perception_effects;

    /* Visual: Query training state from visual cortex for confidence/novelty */
    if (bridge->visual_cortex && bridge->config.enable_visual) {
        visual_training_state_t vts;
        if (visual_cortex_get_training_state(bridge->visual_cortex, &vts) == 0 && vts.valid) {
            effects->visual_confidence = vts.confidence;
            effects->visual_novelty = vts.novelty;
        } else {
            /* Fallback: use processing stats as proxy — more images processed
             * implies warmed-up cortex with higher confidence */
            visual_cortex_stats_local_t vstats;
            if (visual_cortex_get_stats(bridge->visual_cortex, &vstats)) {
                /* Proxy confidence from processing time: faster processing → higher confidence.
                 * Map avg_processing_time [0.1ms, 50ms] → confidence [0.9, 0.3] */
                float time_proxy = vstats.avg_processing_time > 0.0f
                    ? nimcp_clampf(1.0f - (vstats.avg_processing_time - 0.1f) / 50.0f, 0.3f, 0.9f)
                    : 0.5f;
                effects->visual_confidence = time_proxy;
                /* Proxy novelty from memories: more stored → less novel new input */
                float mem_ratio = (float)vstats.memories_stored / 1000.0f;
                effects->visual_novelty = nimcp_clampf(1.0f - mem_ratio, 0.1f, 0.9f);
            } else {
                effects->visual_confidence = 0.5f;
                effects->visual_novelty = 0.5f;
            }
        }
    }

    /* Audio: Query training state from audio cortex for quality/salience/coherence */
    if (bridge->audio_cortex && bridge->config.enable_audio) {
        audio_training_state_t ats;
        if (audio_cortex_get_training_state(bridge->audio_cortex, &ats) == 0 && ats.valid) {
            effects->audio_quality = ats.quality;
            effects->speech_salience = ats.speech_salience;
            effects->temporal_coherence = ats.temporal_coherence;
        } else {
            /* Fallback: use audio cortex stats as proxy */
            audio_cortex_stats_local_t astats;
            if (audio_cortex_get_stats(bridge->audio_cortex, &astats)) {
                /* Proxy quality from processing time: faster → better quality/simpler signal */
                float time_proxy = astats.avg_processing_time > 0.0f
                    ? nimcp_clampf(1.0f - (astats.avg_processing_time - 0.5f) / 20.0f, 0.3f, 0.95f)
                    : 0.5f;
                effects->audio_quality = time_proxy;
                /* Proxy speech salience from frames processed — active processing implies speech */
                effects->speech_salience = astats.frames_processed > 0 ? 0.6f : 0.2f;
                /* Proxy temporal coherence from memory count — more memories → more coherent */
                float mem_ratio = (float)astats.memories_stored / 500.0f;
                effects->temporal_coherence = nimcp_clampf(0.4f + mem_ratio * 0.5f, 0.3f, 0.9f);
            } else {
                effects->audio_quality = 0.5f;
                effects->speech_salience = 0.4f;
                effects->temporal_coherence = 0.5f;
            }
        }
    }

    /* Speech: Query speech cortex for comprehension/accuracy/prosody */
    if (bridge->speech_cortex && bridge->config.enable_speech) {
        speech_cortex_stats_local_t sstats;
        if (speech_cortex_get_stats(bridge->speech_cortex, &sstats)) {
            /* Phoneme accuracy is directly available from stats */
            effects->phoneme_accuracy = sstats.phoneme_accuracy;
            /* Comprehension from phoneme confidence API */
            float phoneme_conf = speech_cortex_get_phoneme_confidence(bridge->speech_cortex);
            effects->comprehension = phoneme_conf;
            /* Proxy prosody confidence from recognition confidence:
             * high phoneme accuracy + many words recognized → good prosody extraction */
            float word_ratio = sstats.words_recognized > 0
                ? nimcp_clampf((float)sstats.words_recognized / (float)(sstats.phonemes_detected + 1), 0.0f, 1.0f)
                : 0.0f;
            effects->prosody_confidence = nimcp_clampf(
                (sstats.phoneme_accuracy * 0.5f + word_ratio * 0.5f), 0.1f, 0.95f);
        } else {
            /* Fallback: use phoneme confidence as sole signal */
            float phoneme_conf = speech_cortex_get_phoneme_confidence(bridge->speech_cortex);
            if (phoneme_conf >= 0.0f) {
                effects->comprehension = phoneme_conf;
                effects->phoneme_accuracy = phoneme_conf;
                effects->prosody_confidence = phoneme_conf * 0.8f;
            } else {
                effects->comprehension = 0.5f;
                effects->phoneme_accuracy = 0.5f;
                effects->prosody_confidence = 0.4f;
            }
        }
    }

    effects->valid = true;
    effects->last_update_ms = get_time_ms();

    return NIMCP_SUCCESS;
}

/**
 * @brief Compute learning rate modulation factor
 *
 * WHAT: Calculates multiplicative LR factor from perception states
 * WHY:  Adjusts learning rate based on perceptual quality
 * HOW:  Combines effects from all cortices with configured strengths
 *
 * FORMULAS:
 * - Visual: High confidence → boost (factor 1.0-1.2)
 * - Audio: High quality → stable (factor 1.0-1.1)
 * - Speech: High comprehension → boost (factor 1.0-1.15)
 * - Low quality → conservative (factor 0.7-1.0)
 * Final factor clamped to [0.5, 1.5]
 */
static float compute_lr_modulation(
    const perception_training_bridge_t* bridge,
    const perception_training_effects_t* effects)
{
    float lr_factor = 1.0f;

    /* Visual: High confidence boosts LR (1.0-1.2) */
    if (bridge->config.enable_visual) {
        float visual_boost = (effects->visual_confidence - 0.5f) * 0.4f;
        lr_factor *= (1.0f + bridge->config.visual_strength * visual_boost);
    }

    /* Audio: High quality stabilizes/boosts LR (1.0-1.1) */
    if (bridge->config.enable_audio) {
        float audio_boost = (effects->audio_quality - 0.5f) * 0.2f;
        lr_factor *= (1.0f + bridge->config.audio_strength * audio_boost);
    }

    /* Speech: High comprehension boosts LR (1.0-1.15) */
    if (bridge->config.enable_speech) {
        float speech_boost = (effects->comprehension - 0.5f) * 0.3f;
        lr_factor *= (1.0f + bridge->config.speech_strength * speech_boost);
    }

    /* Clamp to safety bounds */
    return nimcp_clampf(lr_factor, PERCEPTION_LR_FACTOR_MIN, PERCEPTION_LR_FACTOR_MAX);
}

/**
 * @brief Compute sample weight from perception salience
 *
 * WHAT: Calculates sample importance weight from perception effects
 * WHY:  Prioritize high-salience, high-quality samples for training
 * HOW:  Combines speech salience, visual novelty, audio quality
 *
 * BIOLOGICAL BASIS: Salient stimuli get prioritized encoding in memory
 * consolidation (attentional modulation of hippocampal plasticity).
 */
static float compute_sample_weight(
    const perception_training_bridge_t* bridge,
    const perception_training_effects_t* effects)
{
    float weight = 1.0f;

    /* Speech salience: High salience increases priority (1.0-1.5) */
    if (bridge->config.enable_speech) {
        float salience_boost = effects->speech_salience * 0.5f;
        weight *= (1.0f + bridge->config.speech_strength * salience_boost);
    }

    /* Visual novelty: Novel patterns increase priority (1.0-1.3) */
    if (bridge->config.enable_visual) {
        float novelty_boost = effects->visual_novelty * 0.3f;
        weight *= (1.0f + bridge->config.visual_strength * novelty_boost);
    }

    /* Audio quality: Poor quality reduces priority (0.7-1.0) */
    if (bridge->config.enable_audio) {
        if (effects->audio_quality < 0.5f) {
            float quality_penalty = (0.5f - effects->audio_quality) * 0.6f;
            weight *= (1.0f - bridge->config.audio_strength * quality_penalty);
        }
    }

    /* Clamp to configured range */
    return nimcp_clampf(weight, PERCEPTION_SAMPLE_WEIGHT_MIN, PERCEPTION_SAMPLE_WEIGHT_MAX);
}

/**
 * @brief Check if sample should be skipped
 *
 * WHAT: Determines if perception quality is too low for training
 * WHY:  Avoid training on corrupted or ambiguous samples
 * HOW:  Checks each cortex against configured thresholds
 *
 * BIOLOGICAL BASIS: Sensory gating filters low-quality input before
 * it reaches higher cognitive processing (thalamic gating).
 */
static bool should_skip_sample(
    const perception_training_bridge_t* bridge,
    const perception_training_effects_t* effects)
{
    if (!bridge->config.enable_emergency_skip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "should_skip_sample: bridge->config is NULL");
        return false;  /* Skip disabled */
    }

    /* Skip if visual confidence too low */
    if (bridge->config.enable_visual) {
        if (effects->visual_confidence < bridge->config.skip_visual_confidence_threshold) {
            return true;
        }
    }

    /* Skip if audio quality too low */
    if (bridge->config.enable_audio) {
        if (effects->audio_quality < bridge->config.skip_audio_quality_threshold) {
            return true;
        }
    }

    /* Skip if speech comprehension too low */
    if (bridge->config.enable_speech) {
        if (effects->comprehension < bridge->config.skip_speech_comprehension_threshold) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Sync perception state to cognitive training bridge
 *
 * WHAT: Updates cognitive bridge with perception-derived signals
 * WHY:  Perception novelty affects cognitive curiosity and attention
 * HOW:  Sets cognitive effects based on perception state
 */
static int sync_to_cognitive(perception_training_bridge_t* bridge) {
    if (!bridge->cognitive_training || !bridge->config.enable_cognitive_training) {
        return NIMCP_SUCCESS;
    }

    /* TODO: When cognitive training bridge API is available:
     * - Visual novelty → curiosity module (exploration drive)
     * - Speech salience → attention module (focus)
     * - Multi-modal coherence → introspection (confidence)
     */

    (void)bridge->cognitive_training;  /* Unused for now */

    return NIMCP_SUCCESS;
}

/**
 * @brief Sync perception state to training-logic bridge
 *
 * WHAT: Updates training-logic with perception conditions
 * WHY:  Allows logic gates to incorporate perception quality
 * HOW:  Sets numeric conditions for perception metrics
 */
static int sync_to_logic(perception_training_bridge_t* bridge) {
    if (!bridge->training_logic || !bridge->config.enable_training_logic) {
        return NIMCP_SUCCESS;
    }

    /* Set numeric conditions */
    training_logic_set_numeric_condition(
        bridge->training_logic,
        "visual_confidence",
        bridge->perception_effects.visual_confidence
    );

    training_logic_set_numeric_condition(
        bridge->training_logic,
        "audio_quality",
        bridge->perception_effects.audio_quality
    );

    training_logic_set_numeric_condition(
        bridge->training_logic,
        "speech_comprehension",
        bridge->perception_effects.comprehension
    );

    /* Set boolean condition: PERCEPTION_QUALITY */
    bool perception_ok =
        bridge->perception_effects.visual_confidence > 0.5f &&
        bridge->perception_effects.audio_quality > 0.5f;

    training_logic_set_condition(
        bridge->training_logic,
        TRAINING_COND_PERCEPTION_QUALITY,
        perception_ok
    );

    return NIMCP_SUCCESS;
}

/**
 * @brief Sync perception failures to training-immune system
 *
 * WHAT: Reports perception quality issues as potential threats
 * WHY:  Low-quality input may indicate data corruption or attack
 * HOW:  Signals immune system when quality drops below threshold
 */
static int sync_to_immune(perception_training_bridge_t* bridge) {
    if (!bridge->training_immune || !bridge->config.enable_training_immune) {
        return NIMCP_SUCCESS;
    }

    /* TODO: When training immune API is available:
     * - Very low visual confidence → signal instability
     * - Very low audio quality → signal potential corruption
     * - Very low speech comprehension → signal anomaly
     */

    (void)bridge->training_immune;  /* Unused for now */

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
    perception_training_bridge_t* bridge,
    float loss)
{
    if (!bridge || !bridge->loss_history) {
        return;
    }

    uint32_t idx = (bridge->history_head + bridge->history_count)
                   % PERCEPTION_HISTORY_SIZE;

    bridge->loss_history[idx] = loss;

    if (bridge->history_count < PERCEPTION_HISTORY_SIZE) {
        bridge->history_count++;
    } else {
        bridge->history_head = (bridge->history_head + 1) % PERCEPTION_HISTORY_SIZE;
    }
}

/*=============================================================================
 * PUBLIC API - LIFECYCLE
 *============================================================================*/

void perception_training_default_config(perception_training_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(perception_training_config_t));

    /* Mode */
    config->mode = PERCEPTION_TRAINING_MODE_AUTOMATIC;
    config->priority = PERCEPTION_PRIORITY_BALANCED;

    /* Enable all cortices */
    config->enable_visual = true;
    config->enable_audio = true;
    config->enable_speech = true;

    /* Modulation strengths (0-1) */
    config->visual_strength = 0.5f;
    config->audio_strength = 0.4f;
    config->speech_strength = 0.4f;

    /* LR modulation limits */
    config->lr_min_factor = PERCEPTION_TRAINING_DEFAULT_LR_MIN_FACTOR;
    config->lr_max_factor = PERCEPTION_TRAINING_DEFAULT_LR_MAX_FACTOR;
    config->lr_confidence_scale = 0.3f;
    config->lr_quality_scale = 0.3f;

    /* Sample weight limits */
    config->sample_min_weight = PERCEPTION_TRAINING_DEFAULT_SAMPLE_MIN_WEIGHT;
    config->sample_max_weight = PERCEPTION_TRAINING_DEFAULT_SAMPLE_MAX_WEIGHT;
    config->sample_salience_threshold = 0.6f;

    /* Skip thresholds */
    config->skip_visual_confidence_threshold = PERCEPTION_DEFAULT_SKIP_VISUAL_THRESHOLD;
    config->skip_audio_quality_threshold = PERCEPTION_DEFAULT_SKIP_AUDIO_THRESHOLD;
    config->skip_speech_comprehension_threshold = PERCEPTION_DEFAULT_SKIP_SPEECH_THRESHOLD;

    /* Attention scaling */
    config->enable_attention_scaling = true;
    config->attention_boost_factor = 1.5f;

    /* Integration */
    config->enable_cognitive_training = true;
    config->enable_training_logic = true;
    config->enable_training_immune = true;
    config->enable_bio_async = true;

    /* Update settings */
    config->update_interval_ms = PERCEPTION_TRAINING_DEFAULT_UPDATE_INTERVAL_MS;
    config->disable_auto_update = false;

    /* Safety */
    config->max_modulation_change_per_step = 0.1f;
    config->enable_emergency_skip = false;  /* Disabled by default */
}

perception_training_bridge_t* perception_training_create(
    const perception_training_config_t* config)
{
    /* Use default config if not provided */
    perception_training_config_t default_config;
    if (!config) {
        perception_training_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge */
    perception_training_bridge_t* bridge = (perception_training_bridge_t*)nimcp_malloc(
        sizeof(perception_training_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perception_training_create: bridge allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(perception_training_bridge_t));

    /* Store config */
    memcpy(&bridge->config, config, sizeof(perception_training_config_t));

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "perception_training") != 0) { goto cleanup; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perception_training_create: mutex creation failed");
        goto cleanup;
    }

    /* Allocate loss history buffer */
    bridge->loss_history = (float*)nimcp_malloc(
        sizeof(float) * PERCEPTION_HISTORY_SIZE
    );
    if (!bridge->loss_history) {
        NIMCP_LOGGING_ERROR("Failed to allocate history buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perception_training_create: loss_history allocation failed");
        goto cleanup;
    }
    memset(bridge->loss_history, 0, sizeof(float) * PERCEPTION_HISTORY_SIZE);

    /* Initialize default modulation factors to 1.0 (no modulation) */
    bridge->perception_effects.lr_factor = 1.0f;
    bridge->perception_effects.sample_weight = 1.0f;
    bridge->perception_effects.skip_sample = false;

    NIMCP_LOGGING_INFO("Created Perception-Training bridge");

    return bridge;

cleanup:
    nimcp_free(bridge->loss_history);
    if (bridge->base.mutex) { bridge_base_cleanup(&bridge->base); }
    nimcp_free(bridge);
    return NULL;
}

void perception_training_destroy(perception_training_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        perception_training_disconnect_bio_async(bridge);
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

    NIMCP_LOGGING_INFO("Destroyed Perception-Training bridge");
}

int perception_training_start(perception_training_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async && !bridge->base.bio_async_enabled) {
        int result = perception_training_connect_bio_async(bridge);
        if (result != NIMCP_SUCCESS) {
            NIMCP_LOGGING_WARN("Bio-async connection failed, continuing without it");
        }
    }

    /* Extract initial perception state */
    if (!bridge->config.disable_auto_update) {
        extract_perception_state(bridge);
    }

    bridge->running = true;
    bridge->last_update_ms = get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Started Perception-Training bridge");

    return NIMCP_SUCCESS;
}

int perception_training_stop(perception_training_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->running = false;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        perception_training_disconnect_bio_async(bridge);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Stopped Perception-Training bridge");

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - PERCEPTION CORTEX CONNECTIONS
 *============================================================================*/

int perception_training_connect_visual_cortex(
    perception_training_bridge_t* bridge,
    visual_cortex_t visual_cortex)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->visual_cortex = visual_cortex;
    if (visual_cortex) {
        NIMCP_LOGGING_INFO("Connected visual cortex to Perception-Training bridge");
        bridge->stats.visual_connected = true;
    } else {
        bridge->stats.visual_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_connect_audio_cortex(
    perception_training_bridge_t* bridge,
    audio_cortex_t audio_cortex)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->audio_cortex = audio_cortex;
    if (audio_cortex) {
        NIMCP_LOGGING_INFO("Connected audio cortex to Perception-Training bridge");
        bridge->stats.audio_connected = true;
    } else {
        bridge->stats.audio_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_connect_speech_cortex(
    perception_training_bridge_t* bridge,
    speech_cortex_t speech_cortex)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->speech_cortex = speech_cortex;
    if (speech_cortex) {
        NIMCP_LOGGING_INFO("Connected speech cortex to Perception-Training bridge");
        bridge->stats.speech_connected = true;
    } else {
        bridge->stats.speech_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - TRAINING COMPONENT CONNECTIONS
 *============================================================================*/

int perception_training_connect_training_context(
    perception_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_ctx = training_ctx;
    if (training_ctx) {
        NIMCP_LOGGING_INFO("Connected brain training context to Perception-Training bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_connect_cognitive_training(
    perception_training_bridge_t* bridge,
    cognitive_training_bridge_t* cognitive_training)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->cognitive_training = cognitive_training;
    if (cognitive_training) {
        NIMCP_LOGGING_INFO("Connected cognitive-training bridge to Perception-Training bridge");
        bridge->stats.cognitive_training_connected = true;
    } else {
        bridge->stats.cognitive_training_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_connect_training_logic(
    perception_training_bridge_t* bridge,
    training_logic_bridge_t* training_logic)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_logic = training_logic;
    if (training_logic) {
        NIMCP_LOGGING_INFO("Connected training-logic bridge to Perception-Training bridge");
        bridge->stats.training_logic_connected = true;
    } else {
        bridge->stats.training_logic_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_connect_training_plasticity(
    perception_training_bridge_t* bridge,
    training_plasticity_bridge_t* training_plasticity)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_plasticity = training_plasticity;
    if (training_plasticity) {
        NIMCP_LOGGING_INFO("Connected training-plasticity bridge to Perception-Training bridge");
        bridge->stats.training_plasticity_connected = true;
    } else {
        bridge->stats.training_plasticity_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_connect_training_immune(
    perception_training_bridge_t* bridge,
    training_immune_system_t* training_immune)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_immune = training_immune;
    if (training_immune) {
        NIMCP_LOGGING_INFO("Connected training-immune system to Perception-Training bridge");
        bridge->stats.training_immune_connected = true;
    } else {
        bridge->stats.training_immune_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - PERCEPTION → TRAINING
 *============================================================================*/

int perception_training_update(perception_training_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    (void)delta_ms;  /* Currently unused, for future time-based updates */

    nimcp_mutex_lock(bridge->base.mutex);

    /* Extract current perception state */
    int result = extract_perception_state(bridge);
    if (result != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return result;
    }

    /* Compute modulation factors */
    float lr_factor = compute_lr_modulation(bridge, &bridge->perception_effects);
    float sample_weight = compute_sample_weight(bridge, &bridge->perception_effects);
    bool skip = should_skip_sample(bridge, &bridge->perception_effects);

    /* Store in perception effects */
    bridge->perception_effects.lr_factor = lr_factor;
    bridge->perception_effects.sample_weight = sample_weight;
    bridge->perception_effects.skip_sample = skip;

    /* Sync to other bridges */
    sync_to_cognitive(bridge);
    sync_to_logic(bridge);
    sync_to_immune(bridge);

    /* Update stats */
    bridge->stats.total_update_calls++;
    bridge->stats.avg_visual_confidence =
        (bridge->stats.avg_visual_confidence * 0.95f) +
        (bridge->perception_effects.visual_confidence * 0.05f);
    bridge->stats.avg_audio_quality =
        (bridge->stats.avg_audio_quality * 0.95f) +
        (bridge->perception_effects.audio_quality * 0.05f);
    bridge->stats.avg_speech_comprehension =
        (bridge->stats.avg_speech_comprehension * 0.95f) +
        (bridge->perception_effects.comprehension * 0.05f);
    bridge->stats.avg_lr_factor =
        (bridge->stats.avg_lr_factor * 0.95f) + (lr_factor * 0.05f);

    if (lr_factor < bridge->stats.min_lr_factor || bridge->stats.min_lr_factor == 0.0f) {
        bridge->stats.min_lr_factor = lr_factor;
    }
    if (lr_factor > bridge->stats.max_lr_factor) {
        bridge->stats.max_lr_factor = lr_factor;
    }

    bridge->last_update_ms = get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_get_effects(
    const perception_training_bridge_t* bridge,
    perception_training_effects_t* effects)
{
    if (!bridge || !effects) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->perception_effects, sizeof(perception_training_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float perception_training_get_modulated_lr(
    const perception_training_bridge_t* bridge,
    float base_lr)
{
    if (!bridge) {
        return base_lr;
    }

    return base_lr * bridge->perception_effects.lr_factor;
}

float perception_training_get_sample_weight(
    const perception_training_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }

    return bridge->perception_effects.sample_weight;
}

bool perception_training_should_skip_sample(
    const perception_training_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    if (bridge->perception_effects.skip_sample) {
        /* Update stats */
        ((perception_training_bridge_t*)bridge)->stats.samples_skipped++;
    }

    return bridge->perception_effects.skip_sample;
}

int perception_training_get_attention_scaling(
    const perception_training_bridge_t* bridge,
    float* factors,
    uint32_t num_features)
{
    if (!bridge || !factors) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (num_features == 0) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Use visual attention weights if available, otherwise uniform scaling */
    if (bridge->config.enable_attention_scaling &&
        bridge->perception_effects.visual_attention_weights != NULL &&
        bridge->perception_effects.num_visual_features >= num_features) {
        /* Apply per-feature attention weights with boost */
        for (uint32_t i = 0; i < num_features; i++) {
            factors[i] = bridge->config.attention_boost_factor *
                        bridge->perception_effects.visual_attention_weights[i];
        }
    } else {
        /* Uniform scaling */
        for (uint32_t i = 0; i < num_features; i++) {
            factors[i] = 1.0f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - TRAINING → PERCEPTION
 *============================================================================*/

int perception_training_update_metrics(
    perception_training_bridge_t* bridge,
    float loss,
    float grad_norm)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Add to history */
    add_loss_to_history(bridge, loss);

    /* Update training effects */
    bridge->training_effects.loss_current = loss;
    bridge->training_effects.gradient_norm = grad_norm;

    /* Compute loss delta and trend */
    if (bridge->history_count > 1) {
        bridge->training_effects.loss_delta = loss - bridge->prev_loss;
        bridge->training_effects.loss_improved = (bridge->training_effects.loss_delta < 0);

        /* Smoothed trend */
        bridge->training_effects.loss_trend =
            (bridge->training_effects.loss_trend * 0.9f) +
            (bridge->training_effects.loss_delta * 0.1f);
    }

    /* Check gradient stability */
    bridge->training_effects.gradient_stable =
        (grad_norm > 1e-7f && grad_norm < 1000.0f);

    bridge->prev_loss = loss;
    bridge->training_effects.timestamp_ms = get_time_ms();
    bridge->training_effects.valid = true;

    /* Update stats */
    bridge->stats.total_modulations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_signal_event(
    perception_training_bridge_t* bridge,
    perception_training_feedback_t event,
    float magnitude)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (event < 0 || event >= PERCEPTION_TRAINING_FEEDBACK_COUNT) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update stats */
    bridge->stats.total_feedback_events++;
    bridge->stats.feedback_by_type[event]++;

    /* Process event */
    switch (event) {
        case PERCEPTION_TRAINING_FEEDBACK_SENSITIVITY_BOOST:
            /* Increase perceptual sensitivity (training improving) */
            bridge->training_effects.boost_sensitivity = true;
            /* TODO: Signal to perception cortices when APIs available */
            break;

        case PERCEPTION_TRAINING_FEEDBACK_LOAD_REDUCE:
            /* Reduce processing load (training struggling) */
            bridge->training_effects.reduce_load = true;
            /* TODO: Signal to perception cortices when APIs available */
            break;

        case PERCEPTION_TRAINING_FEEDBACK_NOVELTY_SEEK:
            /* Seek novel perceptual patterns (plateau) */
            bridge->training_effects.seek_novelty = true;
            /* TODO: Signal to perception cortices when APIs available */
            break;

        case PERCEPTION_TRAINING_FEEDBACK_CONSOLIDATE:
            /* Consolidate perception memories (mastery) */
            bridge->training_effects.consolidate = true;
            /* TODO: Signal to perception cortices when APIs available */
            break;

        default:
            break;
    }

    (void)magnitude;  /* For future use in scaling feedback intensity */

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int perception_training_update_perception_state(
    perception_training_bridge_t* bridge)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    int result = extract_perception_state(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int perception_training_apply_feedback(
    perception_training_bridge_t* bridge)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Check for events to trigger based on training effects */
    if (bridge->training_effects.loss_improved) {
        perception_training_signal_event(
            bridge,
            PERCEPTION_TRAINING_FEEDBACK_SENSITIVITY_BOOST,
            0.3f
        );
    }

    if (!bridge->training_effects.gradient_stable) {
        perception_training_signal_event(
            bridge,
            PERCEPTION_TRAINING_FEEDBACK_LOAD_REDUCE,
            0.5f
        );
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - BIO-ASYNC
 *============================================================================*/

int perception_training_connect_bio_async(perception_training_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_PERCEPTION_TRAINING,
        .module_name = "perception_training_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        bridge->stats.bio_async_connected = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return NIMCP_ERROR_OPERATION_FAILED;
}

int perception_training_disconnect_bio_async(perception_training_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already disconnected */
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    bridge->stats.bio_async_connected = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool perception_training_is_bio_async_connected(
    const perception_training_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}

int perception_training_process_inbox(perception_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perception_training_process_inbox: bridge is NULL");
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

int perception_training_get_stats(
    const perception_training_bridge_t* bridge,
    perception_training_stats_t* stats)
{
    if (!bridge || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(perception_training_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int perception_training_reset_stats(perception_training_bridge_t* bridge) {
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Preserve connection status */
    bool visual_conn = bridge->stats.visual_connected;
    bool audio_conn = bridge->stats.audio_connected;
    bool speech_conn = bridge->stats.speech_connected;
    bool cognitive_conn = bridge->stats.cognitive_training_connected;
    bool logic_conn = bridge->stats.training_logic_connected;
    bool plasticity_conn = bridge->stats.training_plasticity_connected;
    bool immune_conn = bridge->stats.training_immune_connected;
    bool bio_conn = bridge->stats.bio_async_connected;

    memset(&bridge->stats, 0, sizeof(perception_training_stats_t));

    /* Restore connection status */
    bridge->stats.visual_connected = visual_conn;
    bridge->stats.audio_connected = audio_conn;
    bridge->stats.speech_connected = speech_conn;
    bridge->stats.cognitive_training_connected = cognitive_conn;
    bridge->stats.training_logic_connected = logic_conn;
    bridge->stats.training_plasticity_connected = plasticity_conn;
    bridge->stats.training_immune_connected = immune_conn;
    bridge->stats.bio_async_connected = bio_conn;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Reset Perception-Training statistics");

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - UTILITIES
 *============================================================================*/

const char* perception_training_modulation_to_string(
    perception_training_modulation_t modulation)
{
    switch (modulation) {
        case PERCEPTION_TRAINING_MODULATION_LR:
            return "LEARNING_RATE";
        case PERCEPTION_TRAINING_MODULATION_SAMPLE_WEIGHT:
            return "SAMPLE_WEIGHT";
        case PERCEPTION_TRAINING_MODULATION_ATTENTION:
            return "ATTENTION";
        case PERCEPTION_TRAINING_MODULATION_SKIP_SAMPLE:
            return "SKIP_SAMPLE";
        default:
            return "UNKNOWN";
    }
}

const char* perception_training_feedback_to_string(
    perception_training_feedback_t event)
{
    switch (event) {
        case PERCEPTION_TRAINING_FEEDBACK_SENSITIVITY_BOOST:
            return "SENSITIVITY_BOOST";
        case PERCEPTION_TRAINING_FEEDBACK_LOAD_REDUCE:
            return "LOAD_REDUCE";
        case PERCEPTION_TRAINING_FEEDBACK_NOVELTY_SEEK:
            return "NOVELTY_SEEK";
        case PERCEPTION_TRAINING_FEEDBACK_CONSOLIDATE:
            return "CONSOLIDATE";
        default:
            return "UNKNOWN";
    }
}

const char* perception_training_mode_to_string(
    perception_training_mode_t mode)
{
    switch (mode) {
        case PERCEPTION_TRAINING_MODE_DISABLED:
            return "DISABLED";
        case PERCEPTION_TRAINING_MODE_MONITOR_ONLY:
            return "MONITOR_ONLY";
        case PERCEPTION_TRAINING_MODE_ADVISORY:
            return "ADVISORY";
        case PERCEPTION_TRAINING_MODE_AUTOMATIC:
            return "AUTOMATIC";
        case PERCEPTION_TRAINING_MODE_COORDINATED:
            return "COORDINATED";
        default:
            return "UNKNOWN";
    }
}

void perception_training_dump_state(const perception_training_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    NIMCP_LOGGING_INFO("=== Perception-Training Bridge State ===");
    NIMCP_LOGGING_INFO("Mode: %s",
                       perception_training_mode_to_string(bridge->config.mode));
    NIMCP_LOGGING_INFO("Running: %s", bridge->running ? "true" : "false");

    NIMCP_LOGGING_INFO("Perception Effects:");
    NIMCP_LOGGING_INFO("  visual_confidence=%.3f visual_novelty=%.3f",
                       bridge->perception_effects.visual_confidence,
                       bridge->perception_effects.visual_novelty);
    NIMCP_LOGGING_INFO("  audio_quality=%.3f speech_salience=%.3f",
                       bridge->perception_effects.audio_quality,
                       bridge->perception_effects.speech_salience);
    NIMCP_LOGGING_INFO("  comprehension=%.3f phoneme_accuracy=%.3f",
                       bridge->perception_effects.comprehension,
                       bridge->perception_effects.phoneme_accuracy);

    NIMCP_LOGGING_INFO("Modulation:");
    NIMCP_LOGGING_INFO("  lr_factor=%.3f sample_weight=%.3f skip=%s",
                       bridge->perception_effects.lr_factor,
                       bridge->perception_effects.sample_weight,
                       bridge->perception_effects.skip_sample ? "yes" : "no");

    NIMCP_LOGGING_INFO("Training Effects:");
    NIMCP_LOGGING_INFO("  loss=%.6f gradient_norm=%.6f",
                       bridge->training_effects.loss_current,
                       bridge->training_effects.gradient_norm);
    NIMCP_LOGGING_INFO("  loss_improved=%s gradient_stable=%s",
                       bridge->training_effects.loss_improved ? "yes" : "no",
                       bridge->training_effects.gradient_stable ? "yes" : "no");

    NIMCP_LOGGING_INFO("Statistics:");
    NIMCP_LOGGING_INFO("  update_calls=%lu modulations=%lu samples_skipped=%lu",
                       bridge->stats.total_update_calls,
                       bridge->stats.total_modulations,
                       bridge->stats.samples_skipped);
    NIMCP_LOGGING_INFO("  avg_lr_factor=%.3f (min=%.3f max=%.3f)",
                       bridge->stats.avg_lr_factor,
                       bridge->stats.min_lr_factor,
                       bridge->stats.max_lr_factor);

    NIMCP_LOGGING_INFO("======================================");
}

/*=============================================================================
 * TEST API
 *============================================================================*/

int perception_training_set_effects_for_testing(
    perception_training_bridge_t* bridge,
    const perception_training_effects_t* effects)
{
    if (!bridge || !effects) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Copy the provided effects, preserving attention pointer handling */
    float* existing_attention = bridge->perception_effects.visual_attention_weights;
    uint32_t existing_num_features = bridge->perception_effects.num_visual_features;

    memcpy(&bridge->perception_effects, effects, sizeof(perception_training_effects_t));

    /* If new effects don't have attention weights, restore the existing one */
    if (!effects->visual_attention_weights) {
        bridge->perception_effects.visual_attention_weights = existing_attention;
        bridge->perception_effects.num_visual_features = existing_num_features;
    }

    bridge->perception_effects.valid = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}
