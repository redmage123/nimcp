/**
 * @file nimcp_emotional_system.c
 * @brief Phase 10.2: Integrated Emotional System Implementation
 *
 * WHAT: Centralized emotional processing coordinating tagging, recognition, and regulation
 * WHY:  Emotions enhance memory, guide attention, enable empathy, and ensure ethical behavior
 * HOW:  Integrate emotional_tagging, emotion_recognition, shadow_emotions with unified API
 *
 * IMPLEMENTATION NOTES:
 * - Uses Russell's Circumplex Model for dimensional emotions (valence × arousal)
 * - Integrates Ekman's categorical emotions for recognition
 * - Implements CBT/DBT regulation strategies
 * - Tracks shadow emotions (Dark Triad patterns)
 * - Exponential decay for natural emotion fading
 * - Auto-regulation when intensity exceeds threshold
 *
 * @author NIMCP Development Team
 * @date 2025-11-15
 * @version 1.0.0
 */

#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: affective/social KG */
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"  // Thread safety
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
// SNN/plasticity bridges BEFORE emotional_tagging to avoid emotion_category_t conflict
// (nimcp_emotion_recognition.h has different enum values than nimcp_emotional_tagging.h)
#include "cognitive/emotion/nimcp_emotion_snn_bridge.h"
#include "cognitive/emotion/nimcp_emotion_plasticity_bridge.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "nimcp.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

#define NIMCP_EMOTION_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/emotion/nimcp_emotion_quantum_bridge.h"

#undef LOG_MODULE  // Undefine from quantum bridge header
#define LOG_MODULE "EMOTIONS"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(emotional_system, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define BIO_MODULE_EMOTIONS 0x0320

//=============================================================================
// Forward Declarations
//=============================================================================


//=============================================================================
// Constants
//=============================================================================

#define EMOTION_DECAY_DEFAULT 0.05f         /**< Default decay rate per second */
#define AROUSAL_SENSITIVITY_DEFAULT 1.2f    /**< Default arousal multiplier */
#define VALENCE_SENSITIVITY_DEFAULT 1.0f    /**< Default valence multiplier */
#define REGULATION_THRESHOLD_DEFAULT 0.75f  /**< Default regulation trigger */
#define MAX_SHADOW_TRACKED_DEFAULT 10       /**< Default max shadow emotions */
#define SHADOW_THRESHOLD_DEFAULT 0.6f       /**< Default shadow intervention */
#define REGULATION_STRENGTH 0.3f            /**< How much regulation reduces intensity */

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Emotional system internal structure
 */
struct emotional_system {
    // === Configuration ===
    emotion_config_t config;

    // === Current State ===
    emotion_state_t state;

    // === Statistics ===
    emotion_stats_t stats;

    // === Bio-Async Integration ===
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // === Quantum Bridge ===
    emotion_quantum_bridge_t* quantum_bridge;

    // === SNN and Plasticity Bridges ===
    emotion_snn_bridge_t* snn_bridge;
    emotion_plasticity_bridge_t* plasticity_bridge;
    bool bridges_enabled;

    // === Thread Safety ===
    nimcp_platform_mutex_t mutex;   /**< Protects all emotional system operations */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/*=============================================================================
 * KG-Driven Wiring Callback
 *============================================================================*/

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static nimcp_error_t handle_salience_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static int emotional_system_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO(LOG_MODULE, "emotional_system_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            emotional_system_heartbeat("emotional_sy_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_SALIENCE_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_salience_query);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_SALIENCE_QUERY");
                break;

            default:
                LOG_DEBUG(LOG_MODULE, "  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 *============================================================================*/

/**
 * @brief Handle incoming salience query for emotional boost
 *
 * WHAT: Process salience queries and respond with emotional boost factors
 * WHY:  High arousal events grab attention - emotions modulate salience computation
 * HOW:  Calculate emotional boost based on current emotional state and respond via bio-async
 *
 * BIOLOGICAL BASIS:
 * - Amygdala modulates salience based on emotional significance
 * - High arousal increases attention capture and memory encoding
 * - Valence extremity (positive or negative) enhances salience
 */
static nimcp_error_t handle_salience_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    /* Guard clauses */
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");
    NIMCP_CHECK_THROW(msg_size >= sizeof(bio_msg_salience_query_t), NIMCP_ERROR_INVALID_PARAM, "message size too small");

    const bio_msg_salience_query_t* query = (const bio_msg_salience_query_t*)msg;
    emotional_system_t* system = (emotional_system_t*)user_data;

    LOG_DEBUG(LOG_MODULE, "Received salience query: stimulus=%u, intensity=%.2f, novelty=%.2f",
              query->stimulus_id, query->raw_intensity, query->novelty);

    /* Lock mutex to read state atomically */
    nimcp_platform_mutex_lock(&system->mutex);

    /* Read state values under lock */
    float current_valence = system->state.valence;
    float current_arousal = system->state.arousal;
    float arousal_sensitivity = system->config.arousal_sensitivity;
    bool bio_enabled = system->bio_async_enabled;
    bio_module_context_t bio_ctx = system->bio_ctx;

    nimcp_platform_mutex_unlock(&system->mutex);

    /* Calculate emotional boost based on current emotional state */
    float emotional_boost = 1.0F + current_arousal * arousal_sensitivity;
    emotional_boost = nimcp_clampf(emotional_boost, 1.0F, 3.0F);

    /* Calculate emotionally-modulated salience score */
    /* Formula: base_salience * emotional_boost * (1 + novelty_factor) */
    float base_salience = query->raw_intensity * query->relevance;
    float novelty_factor = query->novelty * 0.5f;  /* Novelty adds up to 50% boost */
    float valence_extremity = fabsf(current_valence);

    /* Emotional salience combines arousal, valence extremity, and emotional boost */
    float emotional_salience = base_salience * emotional_boost * (1.0f + novelty_factor);

    /* Add valence-based modulation (extreme emotions increase salience) */
    emotional_salience *= (1.0f + valence_extremity * 0.3f);

    /* Clamp to valid range */
    if (emotional_salience > 1.0f) emotional_salience = 1.0f;
    if (emotional_salience < 0.0f) emotional_salience = 0.0f;

    /* Determine if immediate attention is required */
    /* High intensity + high arousal + negative valence = urgent attention */
    bool requires_attention = (emotional_salience > 0.7f) ||
                               (current_arousal > 0.8f && current_valence < -0.5f);

    /* Calculate attention priority based on emotional significance */
    float attention_priority = current_arousal * (1.0f + valence_extremity);
    if (attention_priority > 1.0f) attention_priority = 1.0f;

    LOG_DEBUG(LOG_MODULE, "Emotional salience response: boost=%.2f, salience=%.2f, attention=%.2f",
              emotional_boost, emotional_salience, attention_priority);

    /* Send response if bio-async is enabled and we have a valid context */
    if (bio_enabled && bio_ctx) {
        bio_msg_salience_response_t response = {0};

        /* Initialize response header */
        bio_msg_init_header(&response.header, BIO_MSG_SALIENCE_RESPONSE,
                            bio_module_context_get_id(bio_ctx),
                            query->header.source_module,
                            sizeof(response));

        /* Fill response data */
        response.stimulus_id = query->stimulus_id;
        response.salience_score = emotional_salience;
        response.attention_priority = attention_priority;
        response.requires_immediate_attention = requires_attention;

        /* Send response via bio-router (timeout_ms=0 for default) */
        nimcp_error_t send_result = bio_router_send(bio_ctx, &response, sizeof(response), 0);
        if (send_result != NIMCP_SUCCESS) {
            LOG_WARN(LOG_MODULE, "Failed to send salience response: %d", send_result);
        }

        /* Update statistics (needs lock) */
        nimcp_platform_mutex_lock(&system->mutex);
        system->stats.total_updates++;
        nimcp_platform_mutex_unlock(&system->mutex);
    }

    /* Fulfill promise if provided (for async request/response pattern) */
    if (response_promise) {
        bio_msg_salience_response_t response = {0};
        response.stimulus_id = query->stimulus_id;
        response.salience_score = emotional_salience;
        response.attention_priority = attention_priority;
        response.requires_immediate_attention = requires_attention;

        /* Promise fulfillment would go here if the bio-async system supports it */
        /* nimcp_bio_promise_fulfill(response_promise, &response, sizeof(response)); */
        (void)response;  /* Suppress unused warning if promise not used */
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Broadcast emotional state change to other modules
 */
static void bio_broadcast_emotion_state(emotional_system_t* system) {
    if (!system) { return; }

    /* Lock mutex to read state and check bio-async status */
    nimcp_platform_mutex_lock(&system->mutex);

    if (!system->bio_async_enabled || !system->bio_ctx) {
        nimcp_platform_mutex_unlock(&system->mutex);
        return;
    }

    /* Read state values under lock */
    float intensity = system->state.intensity;
    float arousal = system->state.arousal;
    bio_module_context_t bio_ctx = system->bio_ctx;

    nimcp_platform_mutex_unlock(&system->mutex);

    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = 0;
    msg.salience_score = intensity;
    msg.attention_priority = arousal;
    msg.requires_immediate_attention = (intensity > 0.7F);

    bio_router_broadcast(bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast emotion state: intensity=%.2f, arousal=%.2f",
              intensity, arousal);
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Clamp value to range [min, max]
 *
 * WHAT: Ensure value stays within bounds
 * WHY:  Prevent invalid emotional states
 * HOW:  Uses nimcp_clampf() from nimcp_math_helpers.h
 */

/**
 * @brief Calculate emotional intensity from valence and arousal
 *
 * WHAT: Compute overall emotional intensity
 * WHY:  Combine valence extremity with arousal
 * HOW:  intensity = arousal * (1 + |valence|) / 2
 */
static float calculate_intensity(float valence, float arousal) {
    float valence_extremity = fabsf(valence);
    return arousal * (1.0F + valence_extremity) / 2.0F;
}

/**
 * @brief Update running average
 *
 * WHAT: Exponential moving average
 * WHY:  Track average emotional state over time
 * HOW:  new_avg = old_avg * 0.9 + new_value * 0.1
 */
static float update_average(float old_avg, float new_value, uint64_t count) {
    if (count == 0) {
        return new_value;
    }

    // Use exponential moving average with alpha = 0.1
    return old_avg * 0.9F + new_value * 0.1F;
}

/**
 * @brief Calculate emotional stability
 *
 * WHAT: Measure how stable emotions are over time
 * WHY:  Rapid changes indicate instability
 * HOW:  Compare current to average, return 1.0 - difference
 */
static float calculate_stability(float current_valence, float current_arousal,
                                float avg_valence, float avg_arousal) {
    float valence_diff = fabsf(current_valence - avg_valence);
    float arousal_diff = fabsf(current_arousal - avg_arousal);
    float total_diff = (valence_diff + arousal_diff) / 2.0F;

    return nimcp_clampf(1.0F - total_diff, 0.0F, 1.0F);
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

emotion_config_t emotion_system_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_defau", 0.0f);


    emotion_config_t config = {0};

    // Core features enabled by default
    config.enable_emotion_recognition = true;
    config.enable_emotional_tagging = true;
    config.enable_shadow_detection = true;
    config.enable_emotion_regulation = true;
    config.enable_quantum_emotion = true;

    // Integration features enabled
    config.integrate_with_memory = true;
    config.integrate_with_salience = true;
    config.integrate_with_mental_health = true;
    config.integrate_with_ethics = true;

    // Regulation parameters
    config.emotion_decay_rate = EMOTION_DECAY_DEFAULT;
    config.arousal_sensitivity = AROUSAL_SENSITIVITY_DEFAULT;
    config.valence_sensitivity = VALENCE_SENSITIVITY_DEFAULT;
    config.regulation_threshold = REGULATION_THRESHOLD_DEFAULT;

    // Shadow emotion limits
    config.max_shadow_tracked = MAX_SHADOW_TRACKED_DEFAULT;
    config.shadow_intervention_threshold = SHADOW_THRESHOLD_DEFAULT;

    return config;
}

emotional_system_t* emotion_system_create(const emotion_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_creat", 0.0f);


    LOG_DEBUG(LOG_MODULE, "emotion_system_create called");

    // Allocate system with unified memory
    emotional_system_t* system = (emotional_system_t*)nimcp_calloc(1, sizeof(emotional_system_t));
    if (!system) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate emotional system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }
    LOG_DEBUG(LOG_MODULE, "Allocated %zu bytes for emotional system", sizeof(emotional_system_t));

    // Initialize mutex for thread safety
    if (nimcp_platform_mutex_init(&system->mutex, false) != 0) {
        LOG_ERROR(LOG_MODULE, "Failed to initialize mutex");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "emotion_system_create: validation failed");
        return NULL;
    }
    LOG_DEBUG(LOG_MODULE, "Initialized mutex for thread safety");

    // Use provided config or defaults
    if (config) {
        system->config = *config;
        LOG_DEBUG(LOG_MODULE, "Using provided configuration");
    } else {
        system->config = emotion_system_default_config();
        LOG_DEBUG(LOG_MODULE, "Using default configuration");
    }

    // Initialize state to neutral
    memset(&system->state, 0, sizeof(emotion_state_t));
    system->state.valence = 0.0F;
    system->state.arousal = 0.0F;
    system->state.intensity = 0.0F;
    system->state.emotional_stability = 1.0F;
    LOG_INFO(LOG_MODULE, "Initialized emotional state to neutral (valence=0.0, arousal=0.0)");

    // Initialize statistics
    memset(&system->stats, 0, sizeof(emotion_stats_t));
    LOG_DEBUG(LOG_MODULE, "Initialized statistics");

    // Initialize bio-async
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EMOTIONS,
            .module_name = "emotions",
            .inbox_capacity = 64,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;

            /* KG-Driven Wiring: Register callback for orchestrator to invoke */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_EMOTIONS,
                (void*)emotional_system_wiring_handler_callback,
                system
            );

            if (cb_result == NIMCP_SUCCESS) {
                LOG_INFO(LOG_MODULE, "Bio-async registered with KG-driven wiring callback (module_id=0x%04X)", BIO_MODULE_EMOTIONS);
            } else {
                /* Fallback: Direct registration if orchestrator not available */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx, BIO_MSG_SALIENCE_QUERY,
                                                handle_salience_query)
                );
                LOG_INFO(LOG_MODULE, "Bio-async registered with legacy handler registration (module_id=0x%04X)", BIO_MODULE_EMOTIONS);
            }
        }
    }

    // Initialize quantum bridge
    system->quantum_bridge = NULL;
    if (system->config.enable_quantum_emotion) {
        emotion_quantum_config_t quantum_config = emotion_quantum_default_config();
        system->quantum_bridge = emotion_quantum_bridge_create(&quantum_config, system);
        if (system->quantum_bridge) {
            LOG_INFO(LOG_MODULE, "Quantum emotion bridge initialized");
        } else {
            LOG_WARN(LOG_MODULE, "Failed to create quantum bridge, continuing without it");
        }
    }

    // Initialize SNN and Plasticity bridges
    system->snn_bridge = NULL;
    system->plasticity_bridge = NULL;
    system->bridges_enabled = false;

    emotion_snn_config_t snn_config = emotion_snn_config_default();
    system->snn_bridge = emotion_snn_create(&snn_config);
    if (system->snn_bridge) {
        LOG_INFO(LOG_MODULE, "SNN bridge initialized");
    } else {
        LOG_WARN(LOG_MODULE, "Failed to create SNN bridge, continuing without it");
    }

    emotion_plasticity_config_t plasticity_config = emotion_plasticity_config_default();
    system->plasticity_bridge = emotion_plasticity_create(&plasticity_config);
    if (system->plasticity_bridge) {
        LOG_INFO(LOG_MODULE, "Plasticity bridge initialized");
    } else {
        LOG_WARN(LOG_MODULE, "Failed to create plasticity bridge, continuing without it");
    }

    if (system->snn_bridge || system->plasticity_bridge) {
        system->bridges_enabled = true;
    }

    LOG_INFO(LOG_MODULE, "Emotional system created successfully");
    return system;
}

void emotion_system_destroy(emotional_system_t* system) {
    if (!system) {
        LOG_WARN(LOG_MODULE, "emotion_system_destroy called with NULL system");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_destr", 0.0f);


    LOG_DEBUG(LOG_MODULE, "Destroying emotional system");

    // Destroy SNN bridge
    if (system->snn_bridge) {
        emotion_snn_destroy(system->snn_bridge);
        system->snn_bridge = NULL;
        LOG_DEBUG(LOG_MODULE, "Destroyed SNN bridge");
    }

    // Destroy plasticity bridge
    if (system->plasticity_bridge) {
        emotion_plasticity_destroy(system->plasticity_bridge);
        system->plasticity_bridge = NULL;
        LOG_DEBUG(LOG_MODULE, "Destroyed plasticity bridge");
    }

    // Destroy quantum bridge
    if (system->quantum_bridge) {
        emotion_quantum_bridge_destroy(system->quantum_bridge);
        system->quantum_bridge = NULL;
        LOG_DEBUG(LOG_MODULE, "Destroyed quantum bridge");
    }

    // Unregister from bio-router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        LOG_DEBUG(LOG_MODULE, "Unregistered from bio-router");
    }

    LOG_INFO(LOG_MODULE, "Emotional system destroyed (total_updates=%lu, total_regulations=%lu)",
             system->stats.total_updates, system->stats.total_regulations);

    // Destroy mutex
    nimcp_platform_mutex_destroy(&system->mutex);
    LOG_DEBUG(LOG_MODULE, "Destroyed mutex");

    nimcp_free(system);
    system = NULL;
}

//=============================================================================
// State Query API Implementation
//=============================================================================

bool emotion_system_get_state(emotional_system_t* system, emotion_state_t* state) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_get_state: required parameter is NULL (system, state)");
        return false;
    }

    // Lock mutex for thread-safe access (cast away const for mutex operations)
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_get_s", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&system->mutex);

    *state = system->state;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&system->mutex);
    return true;
}

bool emotion_system_get_tag(emotional_system_t* system, emotional_tag_t* tag) {
    if (!system || !tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_get_tag: required parameter is NULL (system, tag)");
        return false;
    }

    // Lock mutex for thread-safe access (cast away const for mutex operations)
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_get_t", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&system->mutex);

    // Convert emotional state to tag
    tag->valence = system->state.valence;
    tag->arousal = system->state.arousal;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&system->mutex);
    return true;
}

bool emotion_system_is_active(emotional_system_t* system, uint32_t emotion_id,
                              float threshold) {
    if (!system) {
        return false;
    }

    // Lock mutex for thread-safe access (cast away const for mutex operations)
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_is_ac", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&system->mutex);

    // Check if the specified emotion is dominant with sufficient confidence
    bool is_active = (system->state.dominant_emotion == emotion_id &&
                      system->state.emotion_confidence >= threshold);

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&system->mutex);
    return is_active;
}

//=============================================================================
// Update API Implementation
//=============================================================================

bool emotion_system_set_state(emotional_system_t* system, float valence, float arousal,
                              uint64_t timestamp_ms) {
    if (!system) {
        LOG_ERROR(LOG_MODULE, "emotion_system_set_state called with NULL system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_get_tag: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_set_s", 0.0f);


    LOG_DEBUG(LOG_MODULE, "Setting emotional state: valence=%.3f, arousal=%.3f", valence, arousal);

    // Clamp inputs to valid ranges
    valence = nimcp_clampf(valence, -1.0F, 1.0F);
    arousal = nimcp_clampf(arousal, 0.0F, 1.0F);

    // Lock mutex for thread-safe state modification
    nimcp_platform_mutex_lock(&system->mutex);

    // Update state
    system->state.valence = valence * system->config.valence_sensitivity;
    system->state.arousal = arousal * system->config.arousal_sensitivity;
    system->state.valence = nimcp_clampf(system->state.valence, -1.0F, 1.0F);
    system->state.arousal = nimcp_clampf(system->state.arousal, 0.0F, 1.0F);

    // Recalculate intensity
    system->state.intensity = calculate_intensity(system->state.valence,
                                                  system->state.arousal);

    LOG_INFO(LOG_MODULE, "Emotional state updated: valence=%.3f, arousal=%.3f, intensity=%.3f",
             system->state.valence, system->state.arousal, system->state.intensity);

    // Update timestamp
    system->state.last_update_ms = timestamp_ms;

    // Update statistics
    system->stats.total_updates++;
    system->stats.avg_valence = update_average(system->stats.avg_valence,
                                              system->state.valence,
                                              system->stats.total_updates);
    system->stats.avg_arousal = update_average(system->stats.avg_arousal,
                                              system->state.arousal,
                                              system->stats.total_updates);

    // Update stability
    system->state.emotional_stability = calculate_stability(
        system->state.valence, system->state.arousal,
        system->stats.avg_valence, system->stats.avg_arousal
    );

    system->stats.avg_stability = update_average(system->stats.avg_stability,
                                                 system->state.emotional_stability,
                                                 system->stats.total_updates);

    LOG_DEBUG(LOG_MODULE, "Statistics: total_updates=%lu, stability=%.3f",
              system->stats.total_updates, system->state.emotional_stability);

    /* Broadcast emotional state change if significant */
    float intensity = system->state.intensity;

    nimcp_platform_mutex_unlock(&system->mutex);

    if (intensity > 0.4F) {
        bio_broadcast_emotion_state(system);
    }

    return true;
}

bool emotion_system_decay(emotional_system_t* system, float delta_time,
                         uint64_t current_time_ms) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_get_tag: system is NULL");
        return false;
    }

    // Lock mutex for thread-safe state modification
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_decay", 0.0f);


    nimcp_platform_mutex_lock(&system->mutex);

    // Apply exponential decay to arousal
    float decay_factor = expf(-system->config.emotion_decay_rate * delta_time);
    system->state.arousal *= decay_factor;

    // Clamp to prevent negative values
    system->state.arousal = nimcp_clampf(system->state.arousal, 0.0F, 1.0F);

    // Recalculate intensity
    system->state.intensity = calculate_intensity(system->state.valence,
                                                  system->state.arousal);

    // Update timestamp
    system->state.last_update_ms = current_time_ms;

    nimcp_platform_mutex_unlock(&system->mutex);
    return true;
}

bool emotion_system_update_multimodal(emotional_system_t* system,
                                      const float* visual_data, uint32_t visual_dim,
                                      const float* audio_data, uint32_t audio_dim,
                                      const char* text, uint64_t timestamp_ms) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_get_tag: system is NULL");
        return false;
    }

    // Process pending bio-async messages before update
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_updat", 0.0f);


    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 10);  // Process up to 10 messages
    }

    // For now, simple placeholder implementation
    // In full implementation, would:
    // 1. Pass to emotion_recognition subsystem
    // 2. Extract emotional features from each modality
    // 3. Fuse multimodal features
    // 4. Update emotional state based on recognition

    // Placeholder: if any input provided, generate small arousal bump
    if (visual_data || audio_data || text) {
        // Lock mutex for thread-safe state read
        nimcp_platform_mutex_lock(&system->mutex);
        float current_arousal = system->state.arousal;
        float current_valence = system->state.valence;
        nimcp_platform_mutex_unlock(&system->mutex);

        float new_arousal = current_arousal + 0.05F;
        emotion_system_set_state(system, current_valence, new_arousal, timestamp_ms);
    }

    return true;
}

//=============================================================================
// Regulation API Implementation
//=============================================================================

/**
 * @brief Internal unlocked regulation helper
 *
 * WHAT: Apply regulation strategy without locking
 * WHY:  Avoid deadlock when called from already-locked context
 * HOW:  Caller must hold mutex before calling this function
 */
static bool emotion_system_regulate_unlocked(emotional_system_t* system, uint32_t strategy) {
    // Apply regulation strategy
    // Strategy 0 = Reappraisal (reduce negative valence)
    // Strategy 1 = Suppression (reduce arousal)
    // Strategy 2 = Distraction (reduce both)

    system->stats.total_regulations++;

    switch (strategy) {
        case 0:  // Reappraisal
            if (system->state.valence < 0.0F) {
                system->state.valence *= (1.0F - REGULATION_STRENGTH);
            }
            break;

        case 1:  // Suppression
            system->state.arousal *= (1.0F - REGULATION_STRENGTH);
            break;

        case 2:  // Distraction
            if (system->state.valence < 0.0F) {
                system->state.valence *= (1.0F - REGULATION_STRENGTH);
            }
            system->state.arousal *= (1.0F - REGULATION_STRENGTH);
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_system_regulate_unlocked: validation failed");
            return false;
    }

    // Recalculate intensity
    system->state.intensity = calculate_intensity(system->state.valence,
                                                  system->state.arousal);

    // Mark as regulating
    system->state.in_self_regulation = true;
    system->stats.successful_regulations++;

    return true;
}

bool emotion_system_regulate(emotional_system_t* system, uint32_t strategy) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_regulate: system is NULL");
        return false;
    }

    if (!system->config.enable_emotion_regulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_regulate: system->config is NULL");
        return false;
    }

    // Lock mutex for thread-safe state modification
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_regul", 0.0f);


    nimcp_platform_mutex_lock(&system->mutex);

    bool result = emotion_system_regulate_unlocked(system, strategy);

    nimcp_platform_mutex_unlock(&system->mutex);
    return result;
}

bool emotion_system_auto_regulate(emotional_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_auto_regulate: system is NULL");
        return false;
    }

    if (!system->config.enable_emotion_regulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_auto_regulate: system->config is NULL");
        return false;
    }

    // Lock mutex for thread-safe state access and modification
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_auto_", 0.0f);


    nimcp_platform_mutex_lock(&system->mutex);

    // Check if regulation is needed
    bool needs_regulation = false;

    // Trigger on high intensity
    if (system->state.intensity >= system->config.regulation_threshold) {
        needs_regulation = true;
    }

    // Trigger on high shadow intensity
    if (system->state.shadow_intensity >= system->config.shadow_intervention_threshold) {
        needs_regulation = true;
    }

    if (!needs_regulation) {
        system->state.in_self_regulation = false;
        nimcp_platform_mutex_unlock(&system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_auto_regulate: needs_regulation is NULL");
        return false;
    }

    // Use quantum walk to find optimal regulation pathway
    if (system->quantum_bridge && emotion_quantum_bridge_is_enabled(system->quantum_bridge)) {
        // Target: neutral valence (0.0), low arousal (0.3)
        float target_valence = 0.0F;
        float target_arousal = 0.3F;

        // Read current state while holding lock
        float current_valence = system->state.valence;
        float current_arousal = system->state.arousal;

        // Unlock before quantum operations (they don't need the emotional system lock)
        nimcp_platform_mutex_unlock(&system->mutex);

        uint32_t steps_required = 0;
        bool pathway_found = emotion_quantum_evaluate_state(
            system->quantum_bridge,
            current_valence,
            current_arousal,
            target_valence,
            target_arousal,
            &steps_required
        );

        if (pathway_found) {
            LOG_DEBUG(LOG_MODULE, "Quantum regulation pathway found: %u steps", steps_required);

            // Get intermediate state predictions
            emotion_quantum_prediction_t pathway[8];
            uint32_t steps_found = 0;
            emotion_quantum_transition(
                system->quantum_bridge,
                current_valence,
                current_arousal,
                target_valence,
                target_arousal,
                pathway,
                8,
                &steps_found
            );

            // Take first step toward target
            if (steps_found > 0) {
                float step_size = 0.3F;  // Partial step
                float delta_v = pathway[0].valence - current_valence;
                float delta_a = pathway[0].arousal - current_arousal;

                // Re-lock to modify state
                nimcp_platform_mutex_lock(&system->mutex);

                system->state.valence += delta_v * step_size;
                system->state.arousal += delta_a * step_size;

                // Clamp to valid ranges
                system->state.valence = nimcp_clampf(system->state.valence, -1.0F, 1.0F);
                system->state.arousal = nimcp_clampf(system->state.arousal, 0.0F, 1.0F);

                // Recalculate intensity
                system->state.intensity = calculate_intensity(system->state.valence,
                                                              system->state.arousal);

                system->state.in_self_regulation = true;
                system->stats.total_regulations++;
                system->stats.successful_regulations++;

                LOG_DEBUG(LOG_MODULE, "Quantum-guided regulation: v=%.2f→%.2f, a=%.2f→%.2f",
                         system->state.valence - delta_v * step_size,
                         system->state.valence,
                         system->state.arousal - delta_a * step_size,
                         system->state.arousal);

                nimcp_platform_mutex_unlock(&system->mutex);
                return true;
            }
        }

        // Quantum path didn't find a solution, re-lock for fallback
        nimcp_platform_mutex_lock(&system->mutex);
    }

    // Fallback to classical regulation
    // Choose strategy based on state
    uint32_t strategy = 0;
    if (system->state.valence < -0.5F && system->state.arousal > 0.7F) {
        strategy = 2;  // Distraction for severe negative high-arousal
    } else if (system->state.valence < 0.0F) {
        strategy = 0;  // Reappraisal for negative valence
    } else {
        strategy = 1;  // Suppression for high arousal
    }

    // Use unlocked version since we already hold the mutex
    bool result = emotion_system_regulate_unlocked(system, strategy);

    nimcp_platform_mutex_unlock(&system->mutex);
    return result;
}

//=============================================================================
// Integration API Implementation
//=============================================================================

float emotion_system_get_salience_boost(emotional_system_t* system) {
    if (!system) {
        return 1.0F;  // No boost if NULL
    }

    // Lock mutex for thread-safe access (cast away const for mutex operations)
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_get_s", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&system->mutex);

    // High arousal increases salience
    // Boost range: [1.0, 2.0]
    float boost = 1.0F + system->state.arousal * system->config.arousal_sensitivity;
    float result = nimcp_clampf(boost, 1.0F, 3.0F);

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&system->mutex);
    return result;
}

float emotion_system_get_memory_priority(emotional_system_t* system) {
    if (!system) {
        return 0.0F;
    }

    // Lock mutex for thread-safe access (cast away const for mutex operations)
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_get_m", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&system->mutex);

    // Memory priority based on emotional intensity
    // More intense emotions → better memory encoding
    // Priority = (arousal + |valence|) / 2

    float valence_extremity = fabsf(system->state.valence);
    float priority = (system->state.arousal + valence_extremity) / 2.0F;
    float result = nimcp_clampf(priority, 0.0F, 1.0F);

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&system->mutex);
    return result;
}

float emotion_system_get_mental_health_impact(emotional_system_t* system) {
    if (!system) {
        return 0.0F;
    }

    // Lock mutex for thread-safe access (cast away const for mutex operations)
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_get_m", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&system->mutex);

    // Mental health impact based on:
    // 1. Extreme negative valence
    // 2. High sustained arousal
    // 3. Low stability
    // 4. Shadow emotion intensity

    float negative_valence_impact = 0.0F;
    if (system->state.valence < 0.0F) {
        negative_valence_impact = fabsf(system->state.valence);
    }

    float arousal_impact = system->state.arousal;
    float stability_impact = 1.0F - system->state.emotional_stability;
    float shadow_impact = system->state.shadow_intensity;

    // Weighted combination
    float impact = (negative_valence_impact * 0.3F +
                   arousal_impact * 0.2F +
                   stability_impact * 0.2F +
                   shadow_impact * 0.3F);

    float result = nimcp_clampf(impact, 0.0F, 1.0F);

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&system->mutex);
    return result;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

bool emotion_system_get_stats(emotional_system_t* system, emotion_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system_get_stats: required parameter is NULL (system, stats)");
        return false;
    }

    // Lock mutex for thread-safe access (cast away const for mutex operations)
    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_emotion_system_get_s", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&system->mutex);

    *stats = system->stats;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&system->mutex);
    return true;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotional_system_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_heartbeat("emotional_sy_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotional_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotional_system_heartbeat("emotional_sy_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotional_System");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotional_System");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * W10 (2026-04-24): KG runtime event emission + read-path bias query.
 *
 * Callers that hold a brain_t AND an emotional_system_t invoke this to sync
 * current dimensional state into brain->internal_kg and query back the stored
 * arousal bias. Null/disabled-safe (no-ops if brain or kg unavailable).
 * Hot-path integration point: emotion_system_set_state() consumers that own
 * the brain handle (e.g. regulation orchestrators) call this post-update.
 * ============================================================================ */
float emotional_system_wave10_kg_sync(struct brain_struct* brain,
                                      emotional_system_t* system)
{
    if (!brain || !system) return 0.5f;
    emotion_state_t s;
    if (!emotion_system_get_state(system, &s)) return 0.5f;
    /* ONE emit at a hot path */
    wave10_emotions_emit_state_change(brain, s.valence, s.arousal, s.intensity);
    /* ONE read/query (downstream consumers use this as salience bias) */
    return wave10_emotions_query_arousal_bias(brain);
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void emotional_system_set_instance_health_agent(emotional_system_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int emotional_system_training_begin(emotional_system_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_training_begin: NULL argument");
        return -1;
    }
    emotional_system_heartbeat_instance(bridge->health_agent, "emotional_sy_training_begin", 0.0f);
    return 0;
}

int emotional_system_training_end(emotional_system_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_training_end: NULL argument");
        return -1;
    }
    emotional_system_heartbeat_instance(bridge->health_agent, "emotional_sy_training_end", 1.0f);
    return 0;
}

int emotional_system_training_step(emotional_system_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_training_step: NULL argument");
        return -1;
    }
    emotional_system_heartbeat_instance(bridge->health_agent, "emotional_sy_training_step", progress);
    return 0;
}
