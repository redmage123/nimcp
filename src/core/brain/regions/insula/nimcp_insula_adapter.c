/**
 * @file nimcp_insula_adapter.c
 * @brief Implementation of Insula brain adapter
 *
 * WHAT: Unified adapter connecting Insula sub-modules to the brain system
 * WHY:  Enable interoception, emotional awareness, and social emotion processing
 * HOW:  Orchestrates body sensing, emotional processing, and social cognition
 *
 * @version Phase I1: Insula Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/insula/nimcp_insula_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(insula_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define INSULA_LOG_MODULE "INSULA"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Somatic marker entry
 */
typedef struct somatic_marker_node {
    uint32_t context;                    /**< Decision context ID */
    float valence;                       /**< Good/bad association */
    float confidence;                    /**< Marker strength */
    double creation_time;                /**< When created */
    struct somatic_marker_node* next;    /**< Hash chain */
} somatic_marker_node_t;

/**
 * @brief Interoceptive channel state
 */
typedef struct {
    float current_value;                 /**< Current signal value */
    float baseline;                      /**< Baseline/setpoint */
    float sensitivity;                   /**< Channel sensitivity */
    float reliability;                   /**< Signal reliability */
    double last_update_ms;               /**< Last update time */
    float history[8];                    /**< Recent history buffer */
    uint32_t history_idx;                /**< Circular buffer index */
} intero_channel_state_t;

/**
 * @brief Internal adapter structure
 */
struct insula_adapter {
    /* Configuration */
    insula_config_t config;

    /* Interoception state */
    intero_channel_state_t* intero_channels;
    insula_body_state_t body_state;

    /* Emotional state */
    insula_emotional_state_t emotional_state;
    float emotional_momentum[3];          /* Valence, arousal, dominance momentum */

    /* Social state */
    insula_social_state_t social_state;

    /* Somatic markers (hash table) */
    somatic_marker_node_t** somatic_markers;
    uint32_t somatic_capacity;
    uint32_t somatic_count;

    /* Integrated output */
    insula_output_t current_output;

    /* Callbacks */
    insula_body_callback_t body_callback;
    void* body_user_data;
    insula_emotion_callback_t emotion_callback;
    void* emotion_user_data;
    insula_social_callback_t social_callback;
    void* social_user_data;
    insula_alarm_callback_t alarm_callback;
    void* alarm_user_data;

    /* Bio-async context */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* State */
    insula_status_t status;
    insula_error_t last_error;
    double current_time_ms;

    /* Statistics */
    insula_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Hash function for somatic marker contexts
 */
static uint32_t hash_context(uint32_t context, uint32_t capacity) {
    return context % capacity;
}

/**
 * @brief Sigmoid function for smooth transitions
 */
static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Exponential moving average
 */
static float ema(float current, float new_val, float alpha) {
    return alpha * new_val + (1.0f - alpha) * current;
}

/**
 * @brief Set error state
 */
static void set_error(insula_adapter_t* adapter, insula_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != INSULA_ERROR_NONE) {
        adapter->status = INSULA_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", INSULA_LOG_MODULE, error);
    }
}

/**
 * @brief Emit body state change
 */
static void emit_body_change(insula_adapter_t* adapter) {
    if (adapter->body_callback) {
        adapter->body_callback(&adapter->body_state, adapter->body_user_data);
    }
}

/**
 * @brief Emit emotional state change
 */
static void emit_emotion_change(insula_adapter_t* adapter) {
    if (adapter->emotion_callback) {
        adapter->emotion_callback(&adapter->emotional_state, adapter->emotion_user_data);
    }
}

/**
 * @brief Emit social emotion event
 */
static void emit_social_event(insula_adapter_t* adapter,
                               insula_social_emotion_t type,
                               float intensity) {
    if (adapter->social_callback) {
        adapter->social_callback(type, intensity, adapter->social_user_data);
    }
}

/**
 * @brief Emit alarm
 */
static void emit_alarm(insula_adapter_t* adapter, const char* type, float urgency) {
    if (adapter->alarm_callback) {
        adapter->alarm_callback(type, urgency, adapter->alarm_user_data);
    }
}

/**
 * @brief Update body state from interoceptive channels
 */
static void update_body_state_from_channels(insula_adapter_t* adapter) {
    insula_body_state_t* state = &adapter->body_state;
    intero_channel_state_t* channels = adapter->intero_channels;

    /* Vital signals */
    state->heart_rate = channels[INTERO_CHANNEL_CARDIAC].current_value * 120.0f + 40.0f;
    state->heart_rate_variability = channels[INTERO_CHANNEL_CARDIAC].reliability * 100.0f;
    state->respiratory_rate = channels[INTERO_CHANNEL_RESPIRATORY].current_value * 20.0f + 8.0f;
    state->respiratory_depth = channels[INTERO_CHANNEL_RESPIRATORY].reliability;
    state->body_temperature = channels[INTERO_CHANNEL_THERMAL].current_value * 4.0f + 35.0f;

    /* Metabolic state */
    state->hunger_level = channels[INTERO_CHANNEL_HUNGER].current_value;
    state->thirst_level = channels[INTERO_CHANNEL_THIRST].current_value;
    state->fatigue_level = channels[INTERO_CHANNEL_FATIGUE].current_value;
    state->energy_level = 1.0f - channels[INTERO_CHANNEL_FATIGUE].current_value;

    /* Arousal state */
    state->physiological_arousal = channels[INTERO_CHANNEL_AROUSAL].current_value;
    state->stress_level = channels[INTERO_CHANNEL_STRESS].current_value;
    state->pain_level = channels[INTERO_CHANNEL_PAIN].current_value;
    state->comfort_level = channels[INTERO_CHANNEL_PLEASURE].current_value -
                           channels[INTERO_CHANNEL_DISCOMFORT].current_value;
    state->comfort_level = nimcp_clampf(state->comfort_level, 0.0f, 1.0f);

    /* Homeostatic error - average deviation from baseline */
    float total_error = 0.0f;
    for (int i = 0; i < INTERO_CHANNEL_COUNT; i++) {
        float deviation = fabsf(channels[i].current_value - channels[i].baseline);
        total_error += deviation * channels[i].sensitivity;
    }
    state->homeostatic_error = total_error / INTERO_CHANNEL_COUNT;

    /* Allostatic load accumulates slowly */
    if (state->stress_level > 0.5f) {
        state->allostatic_load = ema(state->allostatic_load,
                                      state->allostatic_load + 0.001f, 0.1f);
    } else {
        state->allostatic_load = ema(state->allostatic_load,
                                      state->allostatic_load * 0.99f, 0.01f);
    }

    state->timestamp_ms = adapter->current_time_ms;
}

/**
 * @brief Update emotional state from body and inputs
 */
static void update_emotional_dynamics(insula_adapter_t* adapter, float dt_sec) {
    insula_emotional_state_t* state = &adapter->emotional_state;
    const insula_body_state_t* body = &adapter->body_state;

    /* Body state influences emotion (interoceptive inference) */
    float body_valence = (body->comfort_level - body->pain_level) * 0.3f;
    float body_arousal = body->physiological_arousal * 0.5f;

    /* Apply momentum with decay */
    float decay = expf(-dt_sec / 2.0f);  /* 2 second time constant */
    adapter->emotional_momentum[0] *= decay;
    adapter->emotional_momentum[1] *= decay;
    adapter->emotional_momentum[2] *= decay;

    /* Update core dimensions with momentum */
    state->valence = ema(state->valence,
                          state->valence + adapter->emotional_momentum[0] + body_valence,
                          0.3f);
    state->arousal = ema(state->arousal,
                          state->arousal + adapter->emotional_momentum[1] + body_arousal,
                          0.3f);

    /* Clamp to valid range */
    state->valence = nimcp_clampf(state->valence, -1.0f, 1.0f);
    state->arousal = nimcp_clampf(state->arousal, -1.0f, 1.0f);
    state->dominance = nimcp_clampf(state->dominance, -1.0f, 1.0f);

    /* Derive categorical emotions from dimensional space */
    /* Using simplified mapping */
    state->joy = (state->valence > 0) ? state->valence * (0.5f + state->arousal * 0.5f) : 0.0f;
    state->sadness = (state->valence < 0 && state->arousal < 0) ?
                      (-state->valence) * (0.5f - state->arousal * 0.5f) : 0.0f;
    state->fear = (state->valence < 0 && state->arousal > 0 && state->dominance < 0) ?
                   (-state->valence) * state->arousal : 0.0f;
    state->anger = (state->valence < 0 && state->arousal > 0 && state->dominance > 0) ?
                    (-state->valence) * state->arousal : 0.0f;

    /* Meta-emotional metrics */
    float valence_change = fabsf(adapter->emotional_momentum[0]);
    float arousal_change = fabsf(adapter->emotional_momentum[1]);
    state->emotional_stability = 1.0f - nimcp_clampf(valence_change + arousal_change, 0.0f, 1.0f);
    state->emotional_intensity = sqrtf(state->valence * state->valence +
                                         state->arousal * state->arousal);

    state->timestamp_ms = adapter->current_time_ms;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

insula_config_t insula_default_config(void) {
    insula_config_t config;
    memset(&config, 0, sizeof(config));

    /* Interoception settings */
    config.interoception_channels = INSULA_DEFAULT_INTEROCEPTION_CHANNELS;
    config.interoception_sensitivity = 0.5f;
    config.interoception_update_hz = INSULA_DEFAULT_UPDATE_RATE_HZ;
    config.enable_cardiac_awareness = true;
    config.enable_respiratory_awareness = true;
    config.enable_gastric_awareness = true;

    /* Emotional awareness settings */
    config.emotion_dimensions = INSULA_DEFAULT_EMOTION_DIMENSIONS;
    config.emotional_sensitivity = 0.6f;
    config.emotional_integration_ms = INSULA_DEFAULT_INTEGRATION_WINDOW_MS;
    config.enable_valence_tracking = true;
    config.enable_arousal_tracking = true;

    /* Social emotion settings */
    config.social_emotion_types = INSULA_DEFAULT_SOCIAL_EMOTION_TYPES;
    config.social_sensitivity = 0.5f;
    config.enable_disgust_processing = true;
    config.enable_empathy_processing = true;
    config.enable_trust_processing = true;

    /* Body mapping */
    config.body_map_resolution = INSULA_DEFAULT_BODY_MAP_RESOLUTION;
    config.enable_body_ownership = true;
    config.enable_agency_sense = true;

    /* Integration */
    config.enable_limbic_integration = true;
    config.enable_somatosensory_integration = true;
    config.enable_prefrontal_integration = true;

    /* Event system */
    config.enable_events = true;

    /* Training */
    config.enable_training = false;
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;

    /* Bio-async */
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_SEROTONIN;

    return config;
}

insula_adapter_t* insula_create(const insula_config_t* config) {
    LOG_INFO("[%s] Creating Insula adapter", INSULA_LOG_MODULE);

    insula_adapter_t* adapter = (insula_adapter_t*)nimcp_calloc(1, sizeof(insula_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", INSULA_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_create: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG("[%s] Using provided configuration", INSULA_LOG_MODULE);
    } else {
        adapter->config = insula_default_config();
        LOG_DEBUG("[%s] Using default configuration", INSULA_LOG_MODULE);
    }

    /* Allocate interoceptive channels */
    LOG_DEBUG("[%s] Allocating interoceptive channels", INSULA_LOG_MODULE);
    adapter->intero_channels = (intero_channel_state_t*)nimcp_calloc(
        INTERO_CHANNEL_COUNT, sizeof(intero_channel_state_t));
    if (!adapter->intero_channels) {
        LOG_ERROR("[%s] Failed to allocate intero channels", INSULA_LOG_MODULE);
        insula_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_create: adapter->intero_channels is NULL");
        return NULL;
    }

    /* Initialize channel baselines and sensitivities */
    for (int i = 0; i < INTERO_CHANNEL_COUNT; i++) {
        adapter->intero_channels[i].baseline = 0.5f;
        adapter->intero_channels[i].current_value = 0.5f;
        adapter->intero_channels[i].sensitivity = adapter->config.interoception_sensitivity;
        adapter->intero_channels[i].reliability = 0.8f;
    }

    /* Allocate somatic markers hash table */
    LOG_DEBUG("[%s] Allocating somatic markers", INSULA_LOG_MODULE);
    adapter->somatic_capacity = 256;
    adapter->somatic_markers = (somatic_marker_node_t**)nimcp_calloc(
        adapter->somatic_capacity, sizeof(somatic_marker_node_t*));
    if (!adapter->somatic_markers) {
        LOG_ERROR("[%s] Failed to allocate somatic markers", INSULA_LOG_MODULE);
        insula_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_create: adapter->somatic_markers is NULL");
        return NULL;
    }

    /* Initialize emotional state to neutral */
    adapter->emotional_state.valence = 0.0f;
    adapter->emotional_state.arousal = 0.0f;
    adapter->emotional_state.dominance = 0.0f;
    adapter->emotional_state.emotional_clarity = 0.5f;

    /* Initialize body state */
    adapter->body_state.heart_rate = 70.0f;
    adapter->body_state.respiratory_rate = 14.0f;
    adapter->body_state.body_temperature = 37.0f;
    adapter->body_state.comfort_level = 0.7f;

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    adapter->default_channel = adapter->config.default_channel;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", INSULA_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_INSULA,
            .module_name = "insula_region",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            LOG_INFO("[%s] Bio-async registered successfully", INSULA_LOG_MODULE);
        } else {
            LOG_WARNING("[%s] Failed to register with bio-async router", INSULA_LOG_MODULE);
        }
    }

    /* Initialize state */
    adapter->status = INSULA_STATUS_IDLE;
    adapter->last_error = INSULA_ERROR_NONE;
    adapter->current_time_ms = 0.0;

    LOG_INFO("[%s] Insula adapter created successfully", INSULA_LOG_MODULE);
    return adapter;
}

void insula_destroy(insula_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying Insula adapter", INSULA_LOG_MODULE);

    /* Unregister from bio-async */
    if (adapter->bio_ctx) {
        LOG_DEBUG("[%s] Unregistering from bio-async router", INSULA_LOG_MODULE);
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Free interoceptive channels */
    if (adapter->intero_channels) {
        nimcp_free(adapter->intero_channels);
    }

    /* Free somatic markers */
    if (adapter->somatic_markers) {
        for (uint32_t i = 0; i < adapter->somatic_capacity; i++) {
            somatic_marker_node_t* node = adapter->somatic_markers[i];
            while (node) {
                somatic_marker_node_t* next = node->next;
                nimcp_free(node);
                node = next;
            }
        }
        nimcp_free(adapter->somatic_markers);
    }

    LOG_DEBUG("[%s] Insula adapter destroyed", INSULA_LOG_MODULE);
    nimcp_free(adapter);
}

bool insula_reset(insula_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Resetting adapter state", INSULA_LOG_MODULE);

    /* Reset interoceptive channels */
    for (int i = 0; i < INTERO_CHANNEL_COUNT; i++) {
        adapter->intero_channels[i].current_value = adapter->intero_channels[i].baseline;
        adapter->intero_channels[i].history_idx = 0;
    }

    /* Reset emotional state */
    memset(&adapter->emotional_state, 0, sizeof(adapter->emotional_state));
    adapter->emotional_state.emotional_clarity = 0.5f;
    memset(adapter->emotional_momentum, 0, sizeof(adapter->emotional_momentum));

    /* Reset social state */
    memset(&adapter->social_state, 0, sizeof(adapter->social_state));

    /* Reset body state to defaults */
    memset(&adapter->body_state, 0, sizeof(adapter->body_state));
    adapter->body_state.heart_rate = 70.0f;
    adapter->body_state.respiratory_rate = 14.0f;
    adapter->body_state.body_temperature = 37.0f;

    /* Reset output */
    memset(&adapter->current_output, 0, sizeof(adapter->current_output));

    /* Reset state */
    adapter->status = INSULA_STATUS_IDLE;
    adapter->last_error = INSULA_ERROR_NONE;

    LOG_DEBUG("[%s] Adapter reset complete", INSULA_LOG_MODULE);
    return true;
}

/*=============================================================================
 * INTEROCEPTION API
 *===========================================================================*/

bool insula_update_interoception(insula_adapter_t* adapter,
                                   const insula_intero_signal_t* signal) {
    if (!adapter || !signal) {
        set_error(adapter, INSULA_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_reset: required parameter is NULL (adapter, signal)");
        return false;
    }

    if (signal->channel >= INTERO_CHANNEL_COUNT) {
        set_error(adapter, INSULA_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "insula_reset: capacity exceeded");
        return false;
    }

    adapter->status = INSULA_STATUS_INTEROCEPTION;

    intero_channel_state_t* channel = &adapter->intero_channels[signal->channel];

    /* Update channel with temporal smoothing */
    float alpha = 0.3f * channel->sensitivity;
    channel->current_value = ema(channel->current_value, signal->intensity, alpha);
    channel->reliability = ema(channel->reliability, signal->reliability, 0.1f);
    channel->last_update_ms = signal->timestamp_ms;

    /* Store in history buffer */
    channel->history[channel->history_idx] = signal->intensity;
    channel->history_idx = (channel->history_idx + 1) % 8;

    /* Update body state */
    update_body_state_from_channels(adapter);

    adapter->stats.intero_signals_processed++;

    /* Check for homeostatic alarms */
    float deviation = fabsf(signal->intensity - channel->baseline);
    if (deviation > 0.4f) {
        adapter->current_output.homeostatic_alarm = true;
        adapter->stats.homeostatic_alarms++;
        emit_alarm(adapter, "homeostatic", deviation);
    }

    return true;
}

bool insula_update_interoception_batch(insula_adapter_t* adapter,
                                         const insula_intero_signal_t* signals,
                                         uint32_t count) {
    if (!adapter || !signals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_reset: required parameter is NULL (adapter, signals)");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (!insula_update_interoception(adapter, &signals[i])) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insula_reset: insula_update_interoception is NULL");
            return false;
        }
    }

    return true;
}

bool insula_get_body_state(const insula_adapter_t* adapter,
                            insula_body_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_reset: required parameter is NULL (adapter, state)");
        return false;
    }
    *state = adapter->body_state;
    return true;
}

bool insula_set_interoceptive_sensitivity(insula_adapter_t* adapter,
                                            int channel,
                                            float sensitivity) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_reset: adapter is NULL");
        return false;
    }

    sensitivity = nimcp_clampf(sensitivity, 0.0f, 1.0f);

    if (channel < 0) {
        /* Set all channels */
        for (int i = 0; i < INTERO_CHANNEL_COUNT; i++) {
            adapter->intero_channels[i].sensitivity = sensitivity;
        }
    } else if (channel < INTERO_CHANNEL_COUNT) {
        adapter->intero_channels[channel].sensitivity = sensitivity;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insula_reset: validation failed");
        return false;
    }

    return true;
}

/*=============================================================================
 * EMOTIONAL AWARENESS API
 *===========================================================================*/

bool insula_process_emotion(insula_adapter_t* adapter,
                             float valence,
                             float arousal,
                             const char* source) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_reset: adapter is NULL");
        return false;
    }

    adapter->status = INSULA_STATUS_EMOTIONAL;

    /* Add to emotional momentum */
    float source_weight = 0.5f;  /* Default weight */
    if (source) {
        /* Could adjust weight based on source type */
        LOG_DEBUG("[%s] Processing emotion from source: %s", INSULA_LOG_MODULE, source);
    }

    adapter->emotional_momentum[0] += valence * source_weight * adapter->config.emotional_sensitivity;
    adapter->emotional_momentum[1] += arousal * source_weight * adapter->config.emotional_sensitivity;

    /* Update emotional state */
    float dt_sec = 0.1f;  /* Assume 100ms update */
    update_emotional_dynamics(adapter, dt_sec);

    adapter->stats.emotional_updates++;

    /* Check for emotional alarms */
    if (adapter->emotional_state.emotional_intensity > 0.8f) {
        adapter->current_output.emotional_alarm = true;
        emit_alarm(adapter, "emotional", adapter->emotional_state.emotional_intensity);
    }

    /* Emit callback */
    emit_emotion_change(adapter);

    return true;
}

bool insula_get_emotional_state(const insula_adapter_t* adapter,
                                  insula_emotional_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_reset: required parameter is NULL (adapter, state)");
        return false;
    }
    *state = adapter->emotional_state;
    return true;
}

bool insula_create_somatic_marker(insula_adapter_t* adapter,
                                    uint32_t context,
                                    float valence) {
    if (!adapter || !adapter->somatic_markers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_reset: required parameter is NULL (adapter, adapter->somatic_markers)");
        return false;
    }

    /* Create new marker node */
    somatic_marker_node_t* node = (somatic_marker_node_t*)nimcp_calloc(
        1, sizeof(somatic_marker_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_reset: node is NULL");
        return false;
    }

    node->context = context;
    node->valence = nimcp_clampf(valence, -1.0f, 1.0f);
    node->confidence = 0.8f;
    node->creation_time = adapter->current_time_ms;
    node->next = NULL;

    /* Insert into hash table */
    uint32_t idx = hash_context(context, adapter->somatic_capacity);

    /* Check for existing marker and update */
    somatic_marker_node_t* existing = adapter->somatic_markers[idx];
    while (existing) {
        if (existing->context == context) {
            /* Update existing marker with learning */
            existing->valence = ema(existing->valence, valence, 0.3f);
            existing->confidence = ema(existing->confidence, 1.0f, 0.1f);
            existing->creation_time = adapter->current_time_ms;
            nimcp_free(node);
            return true;
        }
        existing = existing->next;
    }

    /* Insert new node at head */
    node->next = adapter->somatic_markers[idx];
    adapter->somatic_markers[idx] = node;
    adapter->somatic_count++;

    /* Also update emotional state */
    adapter->emotional_state.has_somatic_marker = true;
    adapter->emotional_state.somatic_valence = valence;

    LOG_DEBUG("[%s] Created somatic marker for context %u (valence=%.2f)",
              INSULA_LOG_MODULE, context, valence);

    return true;
}

bool insula_query_somatic_marker(const insula_adapter_t* adapter,
                                   uint32_t context,
                                   float* valence,
                                   float* confidence) {
    if (!adapter || !adapter->somatic_markers || !valence || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, adapter->somatic_markers, valence, confidence)");
        return false;
    }

    uint32_t idx = hash_context(context, adapter->somatic_capacity);
    somatic_marker_node_t* node = adapter->somatic_markers[idx];

    while (node) {
        if (node->context == context) {
            *valence = node->valence;
            *confidence = node->confidence;
            return true;
        }
        node = node->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
    return false;  /* No marker found */
}

/*=============================================================================
 * DISGUST AND SOCIAL EMOTION API
 *===========================================================================*/

float insula_process_disgust(insula_adapter_t* adapter,
                              insula_disgust_type_t stimulus_type,
                              float intensity,
                              bool is_moral) {
    if (!adapter) return 0.0f;
    if (!adapter->config.enable_disgust_processing) return 0.0f;

    adapter->status = INSULA_STATUS_SOCIAL;

    /* Calculate disgust response based on type */
    float base_response = intensity * adapter->config.social_sensitivity;

    /* Moral disgust is typically more cognitive, physical more visceral */
    float type_modifier = 1.0f;
    switch (stimulus_type) {
        case DISGUST_CORE:
            type_modifier = 1.2f;  /* Strong visceral response */
            break;
        case DISGUST_ANIMAL_REMINDER:
            type_modifier = 1.0f;
            break;
        case DISGUST_INTERPERSONAL:
            type_modifier = 0.8f;
            break;
        case DISGUST_MORAL:
        case DISGUST_SOCIO_MORAL:
            type_modifier = is_moral ? 1.1f : 0.9f;
            break;
        default:
            break;
    }

    float response = nimcp_clampf(base_response * type_modifier, 0.0f, 1.0f);

    /* Update emotional state */
    adapter->emotional_state.disgust = ema(adapter->emotional_state.disgust, response, 0.5f);
    adapter->emotional_state.valence = ema(adapter->emotional_state.valence, -response * 0.5f, 0.3f);

    /* Update social state */
    adapter->social_state.disgust_intensity = response;
    adapter->social_state.disgust_type = stimulus_type;

    adapter->stats.disgust_responses++;

    /* Emit social callback */
    emit_social_event(adapter, SOCIAL_EMOTION_DISGUST, response);

    LOG_DEBUG("[%s] Disgust response: type=%d, intensity=%.2f, response=%.2f",
              INSULA_LOG_MODULE, stimulus_type, intensity, response);

    return response;
}

float insula_process_empathy(insula_adapter_t* adapter,
                              float other_valence,
                              float other_arousal,
                              float similarity) {
    if (!adapter) return 0.0f;
    if (!adapter->config.enable_empathy_processing) return 0.0f;

    adapter->status = INSULA_STATUS_SOCIAL;

    /* Empathic resonance scales with similarity and sensitivity */
    float resonance = fabsf(other_valence) * similarity * adapter->config.social_sensitivity;
    resonance = nimcp_clampf(resonance, 0.0f, 1.0f);

    /* Mirror the other's emotional state (scaled) */
    float mirror_weight = resonance * 0.3f;
    adapter->emotional_momentum[0] += other_valence * mirror_weight;
    adapter->emotional_momentum[1] += other_arousal * mirror_weight;

    /* Update social state */
    adapter->social_state.empathic_resonance = resonance;
    adapter->social_state.empathic_concern = (other_valence < 0) ? resonance : 0.0f;
    adapter->social_state.perspective_taking = similarity;

    adapter->stats.empathy_responses++;

    /* Emit social callback */
    if (resonance > 0.3f) {
        emit_social_event(adapter, SOCIAL_EMOTION_EMPATHY, resonance);
    }

    return resonance;
}

float insula_assess_trust(insula_adapter_t* adapter,
                           float face_trustworthiness,
                           float behavior_reliability,
                           float reciprocity) {
    if (!adapter) return 0.5f;
    if (!adapter->config.enable_trust_processing) return 0.5f;

    adapter->status = INSULA_STATUS_SOCIAL;

    /* Weighted combination of trust cues */
    float trust = face_trustworthiness * 0.2f +    /* Face cue (first impression) */
                  behavior_reliability * 0.4f +    /* Past behavior (most important) */
                  reciprocity * 0.4f;              /* Reciprocal behavior */

    trust = nimcp_clampf(trust, 0.0f, 1.0f);

    /* Detect betrayal (sharp trust drop) - check before updating */
    float previous_estimate = adapter->social_state.trustworthiness_estimate;

    /* Update social state */
    adapter->social_state.trust_level = ema(adapter->social_state.trust_level, trust, 0.3f);
    adapter->social_state.trustworthiness_estimate = trust;

    /* Check for betrayal: new trust is low but previous estimate was high */
    if (trust < 0.3f && previous_estimate > 0.6f) {
        adapter->social_state.betrayal_detected = true;
        adapter->stats.trust_violations++;
        emit_social_event(adapter, SOCIAL_EMOTION_TRUST, -0.8f);
        emit_alarm(adapter, "trust_violation", 0.8f);
    }

    return trust;
}

float insula_process_fairness(insula_adapter_t* adapter,
                               float own_outcome,
                               float other_outcome,
                               bool is_self_disadvantaged) {
    if (!adapter) return 0.0f;

    adapter->status = INSULA_STATUS_SOCIAL;

    /* Calculate fairness */
    float difference = own_outcome - other_outcome;
    float fairness = 0.0f;

    if (fabsf(difference) < 0.1f) {
        /* Fair - approximately equal */
        fairness = 1.0f;
    } else if (is_self_disadvantaged) {
        /* Disadvantageous inequity - stronger response */
        fairness = -fabsf(difference) * 1.5f;
    } else {
        /* Advantageous inequity - milder response */
        fairness = -fabsf(difference) * 0.5f;
    }

    fairness = nimcp_clampf(fairness, -1.0f, 1.0f);

    /* Update social state */
    adapter->social_state.fairness_assessment = fairness;
    adapter->social_state.inequity_aversion = (fairness < 0) ? -fairness : 0.0f;
    adapter->social_state.social_norm_violation = (fairness < -0.5f);

    /* Unfairness triggers disgust-like response */
    if (fairness < -0.5f) {
        adapter->emotional_state.contempt = ema(adapter->emotional_state.contempt,
                                                  -fairness, 0.4f);
        emit_social_event(adapter, SOCIAL_EMOTION_FAIRNESS, fairness);
    }

    return fairness;
}

float insula_process_rejection(insula_adapter_t* adapter,
                                float rejection_intensity,
                                float source_importance) {
    if (!adapter) return 0.0f;

    adapter->status = INSULA_STATUS_SOCIAL;

    /* Social pain scales with rejection intensity and source importance */
    float social_pain = rejection_intensity * source_importance * adapter->config.social_sensitivity;
    social_pain = nimcp_clampf(social_pain, 0.0f, 1.0f);

    /* Update social state */
    adapter->social_state.rejection_sensitivity = social_pain;
    adapter->social_state.social_pain = social_pain;
    adapter->social_state.belonging_need = ema(adapter->social_state.belonging_need,
                                                  social_pain * 1.2f, 0.5f);
    adapter->social_state.belonging_need = nimcp_clampf(adapter->social_state.belonging_need, 0.0f, 1.0f);

    /* Rejection affects emotional state */
    if (social_pain > 0.3f) {
        adapter->emotional_momentum[0] -= social_pain * 0.5f;  /* Negative valence */
        adapter->emotional_state.sadness = ema(adapter->emotional_state.sadness,
                                                  social_pain, 0.4f);

        emit_social_event(adapter, SOCIAL_EMOTION_REJECTION, social_pain);

        if (social_pain > 0.7f) {
            adapter->current_output.social_alarm = true;
            emit_alarm(adapter, "social_rejection", social_pain);
        }
    }

    return social_pain;
}

bool insula_get_social_state(const insula_adapter_t* adapter,
                               insula_social_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, state)");
        return false;
    }
    *state = adapter->social_state;
    return true;
}

/*=============================================================================
 * INTEGRATION API
 *===========================================================================*/

bool insula_integrate(insula_adapter_t* adapter, insula_output_t* output) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_integrate: adapter is NULL");
        return false;
    }

    adapter->status = INSULA_STATUS_INTEGRATION;

    /* Update body state from channels */
    update_body_state_from_channels(adapter);

    /* Update emotional dynamics */
    float dt_sec = 0.1f;  /* Assume 100ms update */
    update_emotional_dynamics(adapter, dt_sec);

    /* Build integrated output */
    adapter->current_output.body_state = adapter->body_state;
    adapter->current_output.emotional_state = adapter->emotional_state;
    adapter->current_output.social_state = adapter->social_state;

    /* Calculate integration metrics */
    float total_intero = 0.0f;
    for (int i = 0; i < INTERO_CHANNEL_COUNT; i++) {
        total_intero += adapter->intero_channels[i].reliability;
    }
    adapter->current_output.interoceptive_accuracy = total_intero / INTERO_CHANNEL_COUNT;
    adapter->current_output.emotional_awareness = adapter->emotional_state.emotional_clarity;
    adapter->current_output.social_sensitivity = adapter->config.social_sensitivity;

    /* Decision guidance from emotional state */
    if (adapter->emotional_state.valence > 0) {
        adapter->current_output.approach_motivation =
            adapter->emotional_state.valence * (0.5f + adapter->emotional_state.arousal * 0.5f);
        adapter->current_output.avoidance_motivation = 0.0f;
    } else {
        adapter->current_output.approach_motivation = 0.0f;
        adapter->current_output.avoidance_motivation =
            -adapter->emotional_state.valence * (0.5f + adapter->emotional_state.arousal * 0.5f);
    }

    /* Risk assessment from body signals and emotions */
    adapter->current_output.risk_assessment =
        adapter->body_state.stress_level * 0.3f +
        adapter->emotional_state.fear * 0.4f +
        adapter->social_state.social_pain * 0.3f;
    adapter->current_output.risk_assessment =
        nimcp_clampf(adapter->current_output.risk_assessment, 0.0f, 1.0f);

    /* Set urgent flag if any alarm is active */
    adapter->current_output.urgent_signal =
        adapter->current_output.homeostatic_alarm ||
        adapter->current_output.emotional_alarm ||
        adapter->current_output.social_alarm;

    adapter->current_output.timestamp_ms = adapter->current_time_ms;

    adapter->stats.integration_cycles++;
    adapter->status = INSULA_STATUS_READY;

    if (output) {
        *output = adapter->current_output;
    }

    return true;
}

bool insula_step(insula_adapter_t* adapter, double time_ms) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_step: adapter is NULL");
        return false;
    }

    double dt_ms = time_ms - adapter->current_time_ms;
    adapter->current_time_ms = time_ms;

    /* Update emotional dynamics */
    float dt_sec = (float)(dt_ms / 1000.0);
    if (dt_sec > 0.0f && dt_sec < 1.0f) {
        update_emotional_dynamics(adapter, dt_sec);
    }

    /* Decay somatic marker confidence over time */
    /* TODO: Implement marker decay */

    /* Reset alarm flags periodically */
    adapter->current_output.homeostatic_alarm = false;
    adapter->current_output.emotional_alarm = false;
    adapter->current_output.social_alarm = false;
    adapter->current_output.urgent_signal = false;

    return true;
}

bool insula_get_output(const insula_adapter_t* adapter, insula_output_t* output) {
    if (!adapter || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_get_output: required parameter is NULL (adapter, output)");
        return false;
    }
    *output = adapter->current_output;
    return true;
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

bool insula_set_body_callback(insula_adapter_t* adapter,
                                insula_body_callback_t callback,
                                void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_get_output: adapter is NULL");
        return false;
    }
    adapter->body_callback = callback;
    adapter->body_user_data = user_data;
    return true;
}

bool insula_set_emotion_callback(insula_adapter_t* adapter,
                                   insula_emotion_callback_t callback,
                                   void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_get_output: adapter is NULL");
        return false;
    }
    adapter->emotion_callback = callback;
    adapter->emotion_user_data = user_data;
    return true;
}

bool insula_set_social_callback(insula_adapter_t* adapter,
                                  insula_social_callback_t callback,
                                  void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_get_output: adapter is NULL");
        return false;
    }
    adapter->social_callback = callback;
    adapter->social_user_data = user_data;
    return true;
}

bool insula_set_alarm_callback(insula_adapter_t* adapter,
                                 insula_alarm_callback_t callback,
                                 void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_get_output: adapter is NULL");
        return false;
    }
    adapter->alarm_callback = callback;
    adapter->alarm_user_data = user_data;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

insula_status_t insula_get_status(const insula_adapter_t* adapter) {
    if (!adapter) return INSULA_STATUS_ERROR;
    return adapter->status;
}

insula_error_t insula_get_last_error(const insula_adapter_t* adapter) {
    if (!adapter) return INSULA_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* insula_error_string(insula_error_t error) {
    switch (error) {
        case INSULA_ERROR_NONE: return "No error";
        case INSULA_ERROR_INVALID_INPUT: return "Invalid input";
        case INSULA_ERROR_INTEROCEPTION_FAILURE: return "Interoception processing failed";
        case INSULA_ERROR_EMOTIONAL_FAILURE: return "Emotional processing failed";
        case INSULA_ERROR_SOCIAL_FAILURE: return "Social emotion processing failed";
        case INSULA_ERROR_INTEGRATION_FAILURE: return "Integration failed";
        case INSULA_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case INSULA_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* insula_status_string(insula_status_t status) {
    switch (status) {
        case INSULA_STATUS_IDLE: return "Idle";
        case INSULA_STATUS_INTEROCEPTION: return "Processing interoception";
        case INSULA_STATUS_EMOTIONAL: return "Processing emotions";
        case INSULA_STATUS_SOCIAL: return "Processing social emotions";
        case INSULA_STATUS_INTEGRATION: return "Integrating";
        case INSULA_STATUS_READY: return "Ready";
        case INSULA_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool insula_get_stats(const insula_adapter_t* adapter, insula_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_get_stats: required parameter is NULL (adapter, stats)");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool insula_get_config(const insula_adapter_t* adapter, insula_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_get_config: required parameter is NULL (adapter, config)");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t insula_get_bio_context(insula_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->bio_ctx;
}

uint32_t insula_process_bio_messages(insula_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG("[%s] Processed %u bio-async messages", INSULA_LOG_MODULE, processed);
    }
    return processed;
}

nimcp_error_t insula_broadcast_body_state(insula_adapter_t* adapter,
                                            const insula_body_state_t* state) {
    if (!adapter || !state) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS;  /* Not an error if disabled */

    LOG_DEBUG("[%s] Broadcasting body state update", INSULA_LOG_MODULE);

    /* TODO: Implement bio-async broadcast for body state */

    return NIMCP_SUCCESS;
}

nimcp_error_t insula_broadcast_emotional_state(insula_adapter_t* adapter,
                                                 const insula_emotional_state_t* state) {
    if (!adapter || !state) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS;

    LOG_DEBUG("[%s] Broadcasting emotional state update", INSULA_LOG_MODULE);

    /* TODO: Implement bio-async broadcast for emotional state */

    return NIMCP_SUCCESS;
}

nimcp_error_t insula_broadcast_social_alarm(insula_adapter_t* adapter,
                                              insula_social_emotion_t emotion,
                                              float intensity) {
    if (!adapter) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS;

    LOG_INFO("[%s] Broadcasting social alarm: emotion=%d, intensity=%.2f",
             INSULA_LOG_MODULE, emotion, intensity);

    /* TODO: Implement bio-async broadcast for social alarms */

    return NIMCP_SUCCESS;
}
