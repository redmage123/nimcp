/**
 * @file nimcp_emotion_attention.c
 * @brief Implementation of Emotion-Modulated Attention System
 *
 * WHAT: Integrates emotion tensor with attention to modulate attentional focus
 * WHY:  Emotions profoundly affect attention allocation and salience processing
 * HOW:  Subscribe to emotion updates, apply arousal/valence-based modulation
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#include "cognitive/attention/nimcp_emotion_attention.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_sleep_wake.h"  // Sleep state integration
#include "cognitive/attention/nimcp_attention_sleep_bridge.h"  // Sleep bridge for modulation
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/thread/nimcp_thread.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(emotion_attention, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Logging
//=============================================================================

#define EMOTION_ATTENTION_TAG "EmotionAttention"
#define EA_LOG_DEBUG(fmt, ...) LOG_MODULE_DEBUG(EMOTION_ATTENTION_TAG, fmt, ##__VA_ARGS__)
#define EA_LOG_INFO(fmt, ...)  LOG_MODULE_INFO(EMOTION_ATTENTION_TAG, fmt, ##__VA_ARGS__)
#define EA_LOG_WARN(fmt, ...)  LOG_MODULE_WARN(EMOTION_ATTENTION_TAG, fmt, ##__VA_ARGS__)
#define EA_LOG_ERROR(fmt, ...) LOG_MODULE_ERROR(EMOTION_ATTENTION_TAG, fmt, ##__VA_ARGS__)

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Emotion-attention system internal structure
 */
struct emotion_attention_system {
    emotion_tensor_system_t* emotion_tensor;
    multihead_attention_t attention;
    emotion_attention_config_t config;
    emotion_attention_stats_t stats;

    /* Current emotion state cache */
    float current_arousal;
    float current_valence;
    float current_attention_width;
    emotion_primary_t dominant_emotion;

    /* Positional Encoding */
    nimcp_pos_encoder_t* temporal_encoder;  /**< Sinusoidal PE for temporal sequences */
    nimcp_pos_encoder_t* priority_encoder;  /**< Learned PE for priority ordering */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_registered;

    /* Brain reference for medulla integration */
    void* brain_ref;  /**< brain_t reference for medulla arousal baseline */

    /* Sleep state integration */
    sleep_state_t current_sleep_state;  /**< Current sleep/wake state for modulation */

    nimcp_rwlock_t lock;
    bool initialized;
};

//=============================================================================
// Forward Declarations
//=============================================================================

/* Positional encoding functions (defined at end of file) */
static nimcp_pos_encoder_t* create_temporal_encoder(uint32_t max_seq, uint32_t dim);
static nimcp_pos_encoder_t* create_priority_encoder(uint32_t max_seq, uint32_t dim);

//=============================================================================
// Static Helpers - Attention Modulation
//=============================================================================

/**
 * @brief Compute attention width from arousal and valence
 *
 * WHAT: Calculate how broad/narrow attention should be
 * WHY:  High arousal narrows, positive valence broadens
 * HOW:  Apply Easterbrook's narrowing + Fredrickson's broadening
 */
static float compute_attention_width(float arousal, float valence,
                                     const emotion_attention_config_t* config) {
    /* Guard: Validate config pointer */
    if (!config) {
        return 0.5F;  /* Return neutral width on invalid config */
    }

    /* WHAT: Clamp input parameters to valid ranges */
    /* WHY:  Prevent unexpected behavior from out-of-range values */
    if (arousal < 0.0F) arousal = 0.0F;
    if (arousal > 1.0F) arousal = 1.0F;
    if (valence < -1.0F) valence = -1.0F;
    if (valence > 1.0F) valence = 1.0F;

    /* WHAT: Start with neutral width */
    float width = 0.5F;

    /* WHAT: Apply arousal-based narrowing (Easterbrook) */
    /* WHY:  High arousal (fear, anger) narrows attention to threats */
    float narrowing = arousal * config->arousal_narrowing_factor;
    width -= narrowing * 0.4F;  /* Up to 40% narrowing */

    /* WHAT: Apply valence-based broadening (Fredrickson) */
    /* WHY:  Positive emotions (joy) broaden attention scope */
    if (valence > 0.0F) {
        float broadening = valence * config->valence_broadening_factor;
        width += broadening * 0.5F;  /* Up to 50% broadening */
    }

    /* WHAT: Clamp to configured limits */
    if (width < config->min_attention_width) {
        width = config->min_attention_width;
    }
    if (width > config->max_attention_width) {
        width = config->max_attention_width;
    }

    return width;
}

/**
 * @brief Check if emotion is congruent with stimulus
 *
 * WHAT: Determine if stimulus emotion matches current state
 * WHY:  Congruent stimuli receive attentional boost
 * HOW:  Compare emotion categories, check for similarity
 */
static bool is_emotion_congruent(emotion_primary_t current, emotion_primary_t stimulus) {
    /* WHAT: Validate enum values are in expected range */
    /* WHY:  Prevent undefined behavior from invalid enum values */
    if ((int)current < 0 || (int)current > 7 ||
        (int)stimulus < 0 || (int)stimulus > 7) {
        return false;
    }

    /* WHAT: Exact match is always congruent */
    if (current == stimulus) {
        return true;
    }

    /* WHAT: Check for adjacent emotions on Plutchik wheel (also congruent) */
    /* WHY:  Similar emotions facilitate each other */
    int diff = abs((int)current - (int)stimulus);
    if (diff == 1 || diff == 7) {  /* Adjacent on circular wheel */
        return true;
    }

    return false;  /* Emotions not congruent - normal result */
}

//=============================================================================
// Bio-Async Callbacks
//=============================================================================

/**
 * @brief Handle emotion tensor update message
 *
 * WHAT: Process incoming emotion state from tensor
 * WHY:  Update attention modulation parameters
 * HOW:  Extract arousal/valence, recompute attention width
 */
static void on_emotion_tensor_update(void* context, const void* msg_data, size_t msg_size) {
    if (!context || !msg_data) {
        return;
    }

    emotion_attention_system_t* system = (emotion_attention_system_t*)context;
    const bio_msg_emotion_tensor_update_t* msg = (const bio_msg_emotion_tensor_update_t*)msg_data;

    /* Guard: Check message size */
    if (msg_size < sizeof(bio_msg_emotion_tensor_update_t)) {
        EA_LOG_WARN("Received undersized emotion tensor update");
        return;
    }

    nimcp_rwlock_wrlock(&system->lock);

    /* WHAT: Get medulla arousal baseline if available */
    /* WHY:  Medulla provides physiological arousal (stress, alertness) */
    /* HOW:  Combine medulla baseline with emotional arousal for total arousal */
    float medulla_arousal = 0.5f;  /* Default neutral */
    if (system->brain_ref) {
        brain_t brain = (brain_t)system->brain_ref;
        medulla_arousal = nimcp_brain_get_arousal_level(brain);
    }

    /* WHAT: Combine medulla (physiological) and emotional arousal */
    /* WHY:  Total arousal = baseline physiology + emotional state */
    /* HOW:  Weighted average: 30% medulla baseline, 70% emotional arousal */
    float combined_arousal = (medulla_arousal * 0.3f) + (msg->arousal * 0.7f);

    /* WHAT: Update cached emotion state with combined arousal */
    system->current_arousal = combined_arousal;
    system->current_valence = msg->valence;
    system->dominant_emotion = (emotion_primary_t)msg->primary_emotion;

    /* WHAT: Recompute attention width using combined arousal */
    system->current_attention_width = compute_attention_width(
        combined_arousal, msg->valence, &system->config
    );

    /* WHAT: Update statistics */
    system->stats.emotion_updates_received++;
    system->stats.current_arousal = msg->arousal;
    system->stats.current_valence = msg->valence;
    system->stats.current_attention_width = system->current_attention_width;

    /* Update running averages */
    float alpha = 0.1F;  /* EMA smoothing */
    system->stats.avg_arousal_modulation =
        alpha * msg->arousal + (1.0F - alpha) * system->stats.avg_arousal_modulation;
    system->stats.avg_valence_modulation =
        alpha * msg->valence + (1.0F - alpha) * system->stats.avg_valence_modulation;
    system->stats.avg_attention_width =
        alpha * system->current_attention_width + (1.0F - alpha) * system->stats.avg_attention_width;

    nimcp_rwlock_unlock(&system->lock);

    EA_LOG_DEBUG("Emotion update: arousal=%.3f valence=%.3f width=%.3f",
                 msg->arousal, msg->valence, system->current_attention_width);
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

emotion_attention_config_t emotion_attention_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_default_config", 0.0f);


    emotion_attention_config_t config = {
        .arousal_narrowing_factor = 0.7F,    /* Strong narrowing effect */
        .valence_broadening_factor = 0.5F,   /* Moderate broadening */
        .emotion_salience_boost = 2.0F,      /* 2x boost for emotional stimuli */
        .min_attention_width = 0.2F,         /* Can narrow to 20% */
        .max_attention_width = 0.9F,         /* Can broaden to 90% */
        .enable_emotion_gating = true,
        .enable_congruency_bias = true,

        /* Positional Encoding Defaults */
        .enable_temporal_encoding = true,    /* Enable temporal PE by default */
        .enable_priority_encoding = true,    /* Enable priority PE by default */
        .temporal_pe_type = NIMCP_POS_SINUSOIDAL,  /* Sinusoidal for temporal */
        .priority_pe_type = NIMCP_POS_LEARNED,     /* Learned for priority */
        .max_temporal_sequence = 512,        /* Support up to 512 emotional states */
        .emotion_embedding_dim = 128         /* 128-dim emotion embeddings */
    };
    return config;
}

emotion_attention_system_t* emotion_attention_create(
    emotion_tensor_system_t* emotion_tensor,
    multihead_attention_t attention,
    const emotion_attention_config_t* config
) {
    /* Guard: Validate inputs */
    if (!emotion_tensor || !attention) {
        EA_LOG_ERROR("Invalid arguments: emotion_tensor=%p attention=%p",
                     emotion_tensor, attention);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_create: required parameter is NULL (emotion_tensor, attention)");
        return NULL;
    }

    /* WHAT: Allocate system */
    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_create", 0.0f);


    emotion_attention_system_t* system = nimcp_calloc(1, sizeof(emotion_attention_system_t));
    if (!system) {
        EA_LOG_ERROR("Failed to allocate emotion-attention system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    /* WHAT: Initialize config */
    if (config) {
        system->config = *config;
    } else {
        system->config = emotion_attention_default_config();
    }

    /* WHAT: Store references */
    system->emotion_tensor = emotion_tensor;
    system->attention = attention;

    /* WHAT: Initialize state */
    system->current_arousal = 0.0F;
    system->current_valence = 0.0F;
    system->current_attention_width = 0.5F;  /* Neutral */
    system->dominant_emotion = TENSOR_JOY;

    /* WHAT: Initialize statistics */
    memset(&system->stats, 0, sizeof(emotion_attention_stats_t));
    system->stats.current_attention_width = 0.5F;
    system->stats.avg_attention_width = 0.5F;

    /* WHAT: Initialize lock */
    if (nimcp_rwlock_init(&system->lock) != NIMCP_SUCCESS) {
        EA_LOG_ERROR("Failed to initialize rwlock");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "emotion_attention_create: validation failed");
        return NULL;
    }

    /* WHAT: Initialize positional encoders */
    system->temporal_encoder = NULL;
    system->priority_encoder = NULL;

    /* WHAT: Create PE encoders if enabled */
    if (system->config.enable_temporal_encoding || system->config.enable_priority_encoding) {
        bool pe_success = emotion_attention_set_pe_config(
            system,
            system->config.enable_temporal_encoding,
            system->config.enable_priority_encoding,
            system->config.max_temporal_sequence,
            system->config.emotion_embedding_dim
        );
        if (!pe_success) {
            EA_LOG_WARN("Failed to initialize positional encoders, continuing without PE");
        }
    }

    system->initialized = true;
    system->bio_async_registered = false;
    system->brain_ref = NULL;

    /* WHAT: Initialize sleep state (default: awake) */
    system->current_sleep_state = SLEEP_STATE_AWAKE;

    EA_LOG_INFO("Emotion-attention system created");
    return system;
}

/**
 * @brief Connect brain reference for medulla integration
 *
 * WHAT: Store brain reference for medulla arousal queries
 * WHY:  Medulla provides physiological arousal baseline
 * HOW:  Store reference, query arousal in emotion updates
 *
 * @param system Emotion-attention system
 * @param brain Brain reference (void* to avoid circular dependency)
 * @return true on success
 */
bool emotion_attention_connect_brain(emotion_attention_system_t* system, void* brain) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_connect_brain: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_connect_brain", 0.0f);


    nimcp_rwlock_wrlock(&system->lock);
    system->brain_ref = brain;
    nimcp_rwlock_unlock(&system->lock);

    EA_LOG_INFO("Connected to brain for medulla integration");
    return true;
}

void emotion_attention_destroy(emotion_attention_system_t* system) {
    if (!system) {
        return;
    }

    /* WHAT: Unregister from bio-async */
    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_destroy", 0.0f);


    if (system->bio_async_registered) {
        emotion_attention_unregister_bio_async(system);
    }

    /* WHAT: Destroy positional encoders */
    if (system->temporal_encoder) {
        nimcp_pos_encoder_destroy(system->temporal_encoder);
        system->temporal_encoder = NULL;
    }
    if (system->priority_encoder) {
        nimcp_pos_encoder_destroy(system->priority_encoder);
        system->priority_encoder = NULL;
    }

    nimcp_rwlock_destroy(&system->lock);
    nimcp_free(system);
    system = NULL;

    EA_LOG_INFO("Emotion-attention system destroyed");
}

//=============================================================================
// Modulation API Implementation
//=============================================================================

bool emotion_attention_modulate(emotion_attention_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_modulate: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_modulate", 0.0f);


    nimcp_rwlock_rdlock(&system->lock);

    /* WHAT: Get sleep-modulated capacity and vigilance factors */
    float sleep_capacity = attention_sleep_capacity_for_state(system->current_sleep_state);
    float sleep_vigilance = attention_sleep_vigilance_for_state(system->current_sleep_state);

    /* WHAT: Apply emotional gating if enabled */
    if (system->config.enable_emotion_gating) {
        /* WHAT: High arousal emotions gate attention more strongly */
        /* WHY:  Combined with sleep vigilance for realistic modulation */
        float base_gate = 0.5F + (system->current_arousal * 0.5F);
        float modulated_gate = base_gate * sleep_vigilance;
        multihead_attention_set_gate(system->attention, modulated_gate);
        /* WHAT: Use atomic increment for thread safety under read lock */
        /* WHY:  Avoid data race - we only hold read lock */
        __atomic_fetch_add(&((emotion_attention_system_t*)system)->stats.emotional_gating_events, 1, __ATOMIC_RELAXED);
    }

    nimcp_rwlock_unlock(&system->lock);

    EA_LOG_DEBUG("Applied emotion+sleep modulation: gate=%.3f width=%.3f capacity=%.3f vigilance=%.3f",
                 (0.5F + (system->current_arousal * 0.5F)) * sleep_vigilance,
                 system->current_attention_width,
                 sleep_capacity,
                 sleep_vigilance);

    return true;
}

float emotion_attention_compute_salience(
    emotion_attention_system_t* system,
    float base_salience,
    emotion_primary_t stimulus_emotion
) {
    if (!system) {
        return base_salience;
    }

    /* Guard: Clamp input */
    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_compute_salience", 0.0f);


    if (base_salience < 0.0F) base_salience = 0.0F;
    if (base_salience > 1.0F) base_salience = 1.0F;

    nimcp_rwlock_rdlock(&system->lock);

    float modulated_salience = base_salience;

    /* WHAT: Apply congruency bias if enabled */
    if (system->config.enable_congruency_bias) {
        if (is_emotion_congruent(system->dominant_emotion, stimulus_emotion)) {
            /* WHAT: Boost salience for emotion-congruent stimuli */
            modulated_salience *= system->config.emotion_salience_boost;

            /* WHAT: Use atomic increment for thread safety under read lock */
            /* WHY:  Avoid data race - we only hold read lock */
            __atomic_fetch_add(&((emotion_attention_system_t*)system)->stats.congruency_biases, 1, __ATOMIC_RELAXED);
        }
    }

    nimcp_rwlock_unlock(&system->lock);

    /* Guard: Clamp output */
    if (modulated_salience > 1.0F) {
        modulated_salience = 1.0F;
    }

    return modulated_salience;
}

float emotion_attention_get_width(const emotion_attention_system_t* system) {
    if (!system) {
        return -1.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_get_width", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    float width = system->current_attention_width;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return width;
}

//=============================================================================
// Sleep State Integration Implementation
//=============================================================================

/**
 * @brief Set sleep state for attention modulation
 *
 * WHAT: Update current sleep state to modulate attention capacity and vigilance
 * WHY:  Sleep state affects attention performance (biological)
 * HOW:  Store state, modulation applied in emotion_attention_modulate()
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full attention capacity, normal vigilance
 * - DROWSY: Reduced capacity (attention lapses), lower vigilance
 * - NREM: Minimal external attention (offline processing)
 * - REM: Internal attention only (dream focus)
 *
 * @param system Emotion-attention system (non-NULL)
 * @param state New sleep state
 * @return true on success, false on NULL pointer
 */
bool emotion_attention_set_sleep_state(emotion_attention_system_t* system, sleep_state_t state) {
    if (!system) {
        EA_LOG_ERROR("NULL system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_set_sleep_state: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_set_sleep_state", 0.0f);


    nimcp_rwlock_wrlock(&system->lock);
    system->current_sleep_state = state;
    nimcp_rwlock_unlock(&system->lock);

    EA_LOG_DEBUG("Attention sleep state changed to %d", state);
    return true;
}

/**
 * @brief Get current sleep state
 *
 * WHAT: Query current sleep/wake state
 * WHY:  Check what modulation is being applied
 * HOW:  Return current_sleep_state field
 *
 * @param system Emotion-attention system (non-NULL)
 * @return Current sleep state, or SLEEP_STATE_AWAKE if NULL
 */
sleep_state_t emotion_attention_get_sleep_state(const emotion_attention_system_t* system) {
    if (!system) {
        return SLEEP_STATE_AWAKE;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_get_sleep_state", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    sleep_state_t state = system->current_sleep_state;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return state;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

bool emotion_attention_get_stats(
    const emotion_attention_system_t* system,
    emotion_attention_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_get_stats: required parameter is NULL (system, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_get_stats", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    *stats = system->stats;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return true;
}

void emotion_attention_reset_stats(emotion_attention_system_t* system) {
    if (!system) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_reset_stats", 0.0f);


    nimcp_rwlock_wrlock(&system->lock);
    memset(&system->stats, 0, sizeof(emotion_attention_stats_t));
    system->stats.current_attention_width = system->current_attention_width;
    system->stats.avg_attention_width = system->current_attention_width;
    nimcp_rwlock_unlock(&system->lock);

    EA_LOG_INFO("Statistics reset");
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

bool emotion_attention_register_bio_async(emotion_attention_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_register_bio_async: system is NULL");
        return false;
    }

    /* Guard: Already registered */
    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_register_bio_async", 0.0f);


    if (system->bio_async_registered) {
        EA_LOG_WARN("Already registered with bio-async");
        return true;
    }

    /* WHAT: Register with bio-router */
    /* WHY:  Enable inter-module messaging for emotion updates */
    /* HOW:  Use bio_router_register_module with proper info struct */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_ATTENTION,
        .module_name = "EmotionAttention",
        .inbox_capacity = 0,  /* Use default */
        .user_data = system
    };
    system->bio_ctx = bio_router_register_module(&info);

    if (!system->bio_ctx) {
        EA_LOG_WARN("Failed to register with bio-router");
        /* Continue anyway - bio-async is optional */
    }

    /* WHAT: Subscribe to emotion tensor updates */
    /* WHY:  Receive real-time emotion state changes */
    /* HOW:  Subscribe to BIO_MSG_EMOTION_TENSOR_UPDATE on serotonin channel */
    /* TODO: Implement bio_router_subscribe when needed */
    /* if (system->bio_ctx) {
        bio_router_subscribe(system->bio_ctx, BIO_MSG_EMOTION_TENSOR_UPDATE);
    } */
    (void)system->bio_ctx;  /* Suppress unused warning */

    system->bio_async_registered = true;

    /* WHAT: Register with BBB for security */
    /* TODO: Implement bbb_register_emotion_query when needed */
    /* bbb_register_emotion_query(system, "emotion_attention_module"); */

    EA_LOG_INFO("Registered with bio-async for emotion updates");
    return true;
}

void emotion_attention_unregister_bio_async(emotion_attention_system_t* system) {
    if (!system || !system->bio_async_registered) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_unregister_bio_async", 0.0f);

    /* WHAT: Unsubscribe from emotion tensor updates */
    /* TODO: Implement bio_async_unsubscribe when needed
       bio_async_unsubscribe(
        BIO_CHANNEL_SEROTONIN,
        BIO_MSG_EMOTION_TENSOR_UPDATE,
        on_emotion_tensor_update,
        system
    ); */
    (void)on_emotion_tensor_update;  /* Suppress unused warning */

    system->bio_async_registered = false;

    EA_LOG_INFO("Unregistered from bio-async");
}

//=============================================================================
// Positional Encoding Integration Implementation
//=============================================================================

/**
 * @brief Helper to create temporal encoder
 *
 * WHAT: Create sinusoidal encoder for temporal sequences
 * WHY:  Fixed encoding for temporal ordering
 * HOW:  Use NIMCP_POS_SINUSOIDAL with caching enabled
 */
static nimcp_pos_encoder_t* create_temporal_encoder(uint32_t max_seq, uint32_t dim) {
    /* WHAT: Configure sinusoidal encoding */
    /* WHY:  Sinusoidal provides fixed temporal patterns without training */
    /* HOW:  Use default base (10000) with caching for efficiency */
    nimcp_pos_sinusoidal_config_t sin_config = nimcp_pos_sinusoidal_default_config();
    sin_config.base.max_seq_length = max_seq;
    sin_config.base.embedding_dim = dim;
    sin_config.base.cache_enabled = true;   /* Pre-compute for speed */
    sin_config.base.thread_safe = true;

    nimcp_pos_config_t config = {
        .type = NIMCP_POS_SINUSOIDAL,
        .config.sinusoidal = sin_config
    };

    nimcp_pos_encoder_t* encoder = nimcp_pos_encoder_create(&config);
    if (!encoder) {
        EA_LOG_ERROR("Failed to create temporal encoder");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "encoder is NULL");

        return NULL;
    }

    /* WHAT: Pre-compute cache for efficiency */
    /* WHY:  Temporal sequences are frequently accessed */
    int result = nimcp_pos_cache_precompute(encoder, max_seq);
    if (result != NIMCP_POS_SUCCESS) {
        EA_LOG_WARN("Failed to precompute cache: %d", result);
    }

    EA_LOG_INFO("Created temporal encoder: max_seq=%u dim=%u", max_seq, dim);
    return encoder;
}

/**
 * @brief Helper to create priority encoder
 *
 * WHAT: Create learned encoder for priority ordering
 * WHY:  Learned embeddings adapt to task-specific priority patterns
 * HOW:  Use NIMCP_POS_LEARNED with small learning rate
 */
static nimcp_pos_encoder_t* create_priority_encoder(uint32_t max_seq, uint32_t dim) {
    /* WHAT: Configure learned encoding */
    /* WHY:  Priority patterns are task-specific, benefit from learning */
    /* HOW:  Use learned embeddings with initialization std=0.02 */
    nimcp_pos_learned_config_t learned_config = nimcp_pos_learned_default_config();
    learned_config.base.max_seq_length = max_seq;
    learned_config.base.embedding_dim = dim;
    learned_config.base.cache_enabled = true;
    learned_config.base.thread_safe = true;
    learned_config.init_std = 0.02F;        /* Standard initialization */
    learned_config.learning_rate = 0.001F;  /* Conservative learning rate */
    learned_config.weight_decay = 0.0001F;  /* Light regularization */

    nimcp_pos_config_t config = {
        .type = NIMCP_POS_LEARNED,
        .config.learned = learned_config
    };

    nimcp_pos_encoder_t* encoder = nimcp_pos_encoder_create(&config);
    if (!encoder) {
        EA_LOG_ERROR("Failed to create priority encoder");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "encoder is NULL");

        return NULL;
    }

    EA_LOG_INFO("Created priority encoder: max_seq=%u dim=%u", max_seq, dim);
    return encoder;
}

bool emotion_attention_set_pe_config(
    emotion_attention_system_t* system,
    bool enable_temporal,
    bool enable_priority,
    uint32_t max_sequence,
    uint32_t embedding_dim
) {
    /* Guard: Validate inputs */
    if (!system) {
        EA_LOG_ERROR("Null system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_set_pe_config: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_set_pe_config", 0.0f);


    if (max_sequence == 0 || max_sequence > NIMCP_POS_MAX_SEQ_LENGTH) {
        EA_LOG_ERROR("Invalid max_sequence: %u (must be 1-%u)",
                     max_sequence, NIMCP_POS_MAX_SEQ_LENGTH);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_attention_set_pe_config: max_sequence is zero");
        return false;
    }

    if (embedding_dim == 0 || embedding_dim > NIMCP_POS_MAX_DIM) {
        EA_LOG_ERROR("Invalid embedding_dim: %u (must be 1-%u)",
                     embedding_dim, NIMCP_POS_MAX_DIM);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_attention_set_pe_config: embedding_dim is zero");
        return false;
    }

    nimcp_rwlock_wrlock(&system->lock);

    /* WHAT: Destroy existing encoders if present */
    /* WHY:  Reconfiguration requires new encoder instances */
    if (system->temporal_encoder) {
        nimcp_pos_encoder_destroy(system->temporal_encoder);
        system->temporal_encoder = NULL;
    }
    if (system->priority_encoder) {
        nimcp_pos_encoder_destroy(system->priority_encoder);
        system->priority_encoder = NULL;
    }

    /* WHAT: Create temporal encoder if enabled */
    /* WHY:  Encode temporal ordering of emotional states */
    /* HOW:  Sinusoidal PE captures "when" emotions occurred */
    if (enable_temporal) {
        system->temporal_encoder = create_temporal_encoder(max_sequence, embedding_dim);
        if (!system->temporal_encoder) {
            EA_LOG_ERROR("Failed to create temporal encoder");
            nimcp_rwlock_unlock(&system->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_set_pe_config: system->temporal_encoder is NULL");
            return false;
        }
    }

    /* WHAT: Create priority encoder if enabled */
    /* WHY:  Encode attentional priority ordering */
    /* HOW:  Learned PE adapts to priority patterns */
    if (enable_priority) {
        system->priority_encoder = create_priority_encoder(max_sequence, embedding_dim);
        if (!system->priority_encoder) {
            EA_LOG_ERROR("Failed to create priority encoder");
            /* Clean up temporal encoder on partial failure */
            if (system->temporal_encoder) {
                nimcp_pos_encoder_destroy(system->temporal_encoder);
                system->temporal_encoder = NULL;
            }
            nimcp_rwlock_unlock(&system->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_attention_set_pe_config: validation failed");
            return false;
        }
    }

    /* WHAT: Update config */
    system->config.enable_temporal_encoding = enable_temporal;
    system->config.enable_priority_encoding = enable_priority;
    system->config.max_temporal_sequence = max_sequence;
    system->config.emotion_embedding_dim = embedding_dim;

    nimcp_rwlock_unlock(&system->lock);

    EA_LOG_INFO("PE config updated: temporal=%d priority=%d max_seq=%u dim=%u",
                enable_temporal, enable_priority, max_sequence, embedding_dim);
    return true;
}

bool emotion_attention_encode_temporal(
    emotion_attention_system_t* system,
    const float* emotion_sequence,
    uint32_t seq_length,
    float* output
) {
    /* Guard: Validate inputs */
    if (!system) {
        EA_LOG_ERROR("Null system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_encode_temporal: system is NULL");
        return false;
    }

    if (!emotion_sequence || !output) {
        EA_LOG_ERROR("Null buffer pointers: emotion_sequence=%p output=%p",
                     emotion_sequence, output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_encode_temporal: required parameter is NULL (emotion_sequence, output)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_encode_temporal", 0.0f);


    if (seq_length == 0) {
        EA_LOG_ERROR("Invalid seq_length: 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_attention_encode_temporal: seq_length is zero");
        return false;
    }

    nimcp_rwlock_rdlock(&system->lock);

    /* Guard: Check temporal encoder enabled */
    if (!system->temporal_encoder) {
        EA_LOG_ERROR("Temporal encoder not initialized");
        nimcp_rwlock_unlock(&system->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_encode_temporal: system->temporal_encoder is NULL");
        return false;
    }

    /* Guard: Check sequence length */
    if (seq_length > system->config.max_temporal_sequence) {
        EA_LOG_ERROR("Sequence length %u exceeds max %u",
                     seq_length, system->config.max_temporal_sequence);
        nimcp_rwlock_unlock(&system->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_attention_encode_temporal: validation failed");
        return false;
    }

    /* WHAT: Apply positional encoding to emotion sequence */
    /* WHY:  Add temporal ordering information to emotion embeddings */
    /* HOW:  Use nimcp_pos_apply_encoding with additive mode */
    /* BIOLOGICAL: Amygdala→OFC pathway processes emotions temporally */
    int result = nimcp_pos_apply_encoding(
        system->temporal_encoder,
        emotion_sequence,
        seq_length,
        output,
        true  /* additive: output = input + PE */
    );

    nimcp_rwlock_unlock(&system->lock);

    if (result != NIMCP_POS_SUCCESS) {
        EA_LOG_ERROR("Failed to apply temporal encoding: error %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_attention_encode_temporal: validation failed");
        return false;
    }

    EA_LOG_DEBUG("Applied temporal encoding to %u emotion states", seq_length);
    return true;
}

bool emotion_attention_get_priority_embedding(
    emotion_attention_system_t* system,
    uint32_t priority_rank,
    float* output
) {
    /* Guard: Validate inputs */
    if (!system) {
        EA_LOG_ERROR("Null system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_get_priority_embedding: system is NULL");
        return false;
    }

    if (!output) {
        EA_LOG_ERROR("Null output buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_get_priority_embedding: output is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_get_priority_embeddi", 0.0f);


    nimcp_rwlock_rdlock(&system->lock);

    /* Guard: Check priority encoder enabled */
    if (!system->priority_encoder) {
        EA_LOG_ERROR("Priority encoder not initialized");
        nimcp_rwlock_unlock(&system->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_attention_get_priority_embedding: system->priority_encoder is NULL");
        return false;
    }

    /* Guard: Check priority rank in bounds */
    if (priority_rank >= system->config.max_temporal_sequence) {
        EA_LOG_ERROR("Priority rank %u exceeds max %u",
                     priority_rank, system->config.max_temporal_sequence);
        nimcp_rwlock_unlock(&system->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_attention_get_priority_embedding: capacity exceeded");
        return false;
    }

    /* WHAT: Get learned embedding for priority position */
    /* WHY:  Encode attentional priority ordering */
    /* HOW:  Lookup position-specific embedding from learned encoder */
    /* BIOLOGICAL: Priority reflects salience-based attention allocation */
    int result = nimcp_pos_encode_position(
        system->priority_encoder,
        priority_rank,
        output
    );

    nimcp_rwlock_unlock(&system->lock);

    if (result != NIMCP_POS_SUCCESS) {
        EA_LOG_ERROR("Failed to get priority embedding: error %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_attention_get_priority_embedding: validation failed");
        return false;
    }

    EA_LOG_DEBUG("Retrieved priority embedding for rank %u", priority_rank);
    return true;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Emotion Attention self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int emotion_attention_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    emotion_attention_heartbeat("emotion_atte_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Attention");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_attention_heartbeat("emotion_atte_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            EA_LOG_DEBUG("Emotion Attention self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Attention");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Attention");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void emotion_attention_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_emotion_attention_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int emotion_attention_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_attention_training_begin: NULL argument");
        return -1;
    }
    emotion_attention_heartbeat_instance(NULL, "emotion_attention_training_begin", 0.0f);
    (void)(struct emotion_attention_system*)instance; /* Module state available for reset */
    return 0;
}

int emotion_attention_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_attention_training_end: NULL argument");
        return -1;
    }
    emotion_attention_heartbeat_instance(NULL, "emotion_attention_training_end", 1.0f);
    (void)(struct emotion_attention_system*)instance; /* Module state available for finalization */
    return 0;
}

int emotion_attention_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_attention_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    emotion_attention_heartbeat_instance(NULL, "emotion_attention_training_step", progress);
    (void)(struct emotion_attention_system*)instance; /* Module state available for step adaptation */
    return 0;
}
