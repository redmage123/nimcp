/**
 * @file nimcp_contextual_language.c
 * @brief Contextual Language Adaptation Implementation
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include "core/brain_regions/nimcp_contextual_language.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(contextual_language)

//=============================================================================
// Constants
//=============================================================================

#define DEFAULT_MAX_CONTEXT_HISTORY 100
#define DEFAULT_ADAPTATION_RATE 0.5f
#define CONTEXTUAL_MAX_MESSAGE 1024
#define FEATURE_DIM 16  // Neural feature dimension for context detection

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Context history entry
 */
typedef struct {
    context_state_t state;
    uint64_t timestamp_us;
    float confidence;
} context_history_entry_t;

/**
 * @brief Contextual language instance
 */
typedef struct contextual_language_struct {
    // Configuration
    contextual_language_config_t config;

    // Current state
    context_state_t current_context;
    uint64_t last_update_us;

    // Context history (circular buffer)
    context_history_entry_t* history;
    uint32_t history_size;
    uint32_t history_head;
    uint32_t history_count;

    // Neural context detector (simple linear model)
    float context_weights[CONTEXT_TYPE_COUNT][FEATURE_DIM];
    float context_biases[CONTEXT_TYPE_COUNT];

    // Transformation matrices (source → target adaptations)
    float transformation_cache[CONTEXT_TYPE_COUNT][CONTEXT_TYPE_COUNT][CONTEXTUAL_MAX_MESSAGE];
    bool transformation_learned[CONTEXT_TYPE_COUNT][CONTEXT_TYPE_COUNT];

    // Statistics
    contextual_language_stats_t stats;

    // Bio-async integration
    bio_module_context_t bio_ctx;

    // Thread safety
    nimcp_mutex_t lock;
} contextual_language_struct;

//=============================================================================
// Thread-local Error Storage
//=============================================================================

static __thread char error_buffer[NIMCP_ERROR_BUFFER_SIZE] = {0};

/**
 * @brief Set error message
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(error_buffer, sizeof(error_buffer), format, args);
    va_end(args);
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Initialize default context weights
 *
 * WHAT: Set up initial neural weights for context detection
 * WHY:  Provide reasonable starting point before learning
 * HOW:  Hand-crafted weights based on context characteristics
 */
static void init_context_weights(contextual_language_t cl) {
    // Guard clause
    if (!cl) return;

    // Clear all weights
    memset(cl->context_weights, 0, sizeof(cl->context_weights));
    memset(cl->context_biases, 0, sizeof(cl->context_biases));

    // FORMAL: High formality, precision, low emotion
    cl->context_weights[CONTEXT_FORMAL][0] = 0.8F;   // Lexical complexity
    cl->context_weights[CONTEXT_FORMAL][1] = 0.9F;   // Formality markers
    cl->context_weights[CONTEXT_FORMAL][7] = -0.5F;  // Negative emotion weight
    cl->context_biases[CONTEXT_FORMAL] = 0.2F;

    // CASUAL: Low formality, moderate emotion
    cl->context_weights[CONTEXT_CASUAL][0] = -0.3F;  // Lower complexity
    cl->context_weights[CONTEXT_CASUAL][1] = -0.7F;  // Informal markers
    cl->context_weights[CONTEXT_CASUAL][4] = 0.4F;   // Positive emotion OK
    cl->context_biases[CONTEXT_CASUAL] = -0.1F;

    // TECHNICAL: High precision, technical terms
    cl->context_weights[CONTEXT_TECHNICAL][2] = 0.9F;  // Technical terms
    cl->context_weights[CONTEXT_TECHNICAL][6] = 0.8F;  // Precision
    cl->context_weights[CONTEXT_TECHNICAL][3] = -0.3F; // Low emotion
    cl->context_biases[CONTEXT_TECHNICAL] = 0.3F;

    // EMOTIONAL: High emotional content
    cl->context_weights[CONTEXT_EMOTIONAL][3] = 0.9F;  // Emotion markers
    cl->context_weights[CONTEXT_EMOTIONAL][4] = 0.7F;  // Sentiment
    cl->context_weights[CONTEXT_EMOTIONAL][5] = 0.6F;  // Arousal
    cl->context_biases[CONTEXT_EMOTIONAL] = 0.1F;

    // URGENT: High urgency, priority markers
    cl->context_weights[CONTEXT_URGENT][6] = 0.9F;   // Urgency indicators
    cl->context_weights[CONTEXT_URGENT][7] = 0.8F;   // Time pressure
    cl->context_weights[CONTEXT_URGENT][8] = 0.7F;   // Priority
    cl->context_biases[CONTEXT_URGENT] = 0.4F;

    // LEARNING: Moderate complexity, clear structure
    cl->context_weights[CONTEXT_LEARNING][0] = 0.4F;  // Moderate complexity
    cl->context_weights[CONTEXT_LEARNING][6] = 0.6F;  // Clear precision
    cl->context_weights[CONTEXT_LEARNING][9] = 0.5F;  // Explanatory markers
    cl->context_biases[CONTEXT_LEARNING] = 0.0F;
}

/**
 * @brief Softmax activation for context probabilities
 */
static void softmax(float* logits, uint32_t size, float* probs) {
    // Guard clauses
    if (!logits || !probs || size == 0) return;

    // Find max for numerical stability
    float max_logit = logits[0];
    for (uint32_t i = 1; i < size; i++) {
        if (logits[i] > max_logit) max_logit = logits[i];
    }

    // Compute exp and sum
    float sum = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        probs[i] = expf(logits[i] - max_logit);
        sum += probs[i];
    }

    // Normalize
    if (sum > 0.0F) {
        for (uint32_t i = 0; i < size; i++) {
            probs[i] /= sum;
        }
    }
}

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Handle context detection messages
 */
static nimcp_error_t handle_context_detected(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    // Implementation placeholder for bio-async integration
    (void)msg;
    (void)msg_size;
    (void)response_promise;
    (void)user_data;
    return NIMCP_SUCCESS;
}

//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the wiring orchestrator with message types discovered from KG
 * HANDLES_MESSAGE relations. Registers the appropriate handlers dynamically.
 */
static int contextual_language_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    contextual_language_t cl = (contextual_language_t)user_data;
    if (!cl) {
        LOG_WARNING("Wiring callback: contextual_language is NULL");
        return 0;
    }

    LOG_DEBUG("KG wiring callback: registering %u message handlers", message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_BRAIN_STATE_QUERY:
                bio_router_register_handler(ctx, message_types[i],
                    handle_context_detected);
                LOG_DEBUG("Registered handler: BIO_MSG_BRAIN_STATE_QUERY");
                break;

            default:
                LOG_DEBUG("Unknown message type 0x%04X in wiring callback",
                          message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// Core API Implementation
//=============================================================================

contextual_language_t contextual_language_create(
    const contextual_language_config_t* config
) {
    // Allocate structure
    contextual_language_t cl = (contextual_language_t)nimcp_malloc(sizeof(contextual_language_struct));
    if (!cl) {
        set_error("Failed to allocate contextual language structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cl is NULL");

        return NULL;
    }
    memset(cl, 0, sizeof(contextual_language_struct));

    // Set configuration
    if (config) {
        cl->config = *config;
    } else {
        cl->config.max_context_history = DEFAULT_MAX_CONTEXT_HISTORY;
        cl->config.enable_auto_adaptation = true;
        cl->config.adaptation_rate = DEFAULT_ADAPTATION_RATE;
        cl->config.enable_bio_async = false;
    }

    // Initialize mutex
    if (nimcp_mutex_init(&cl->lock, NULL) != 0) {
        set_error("Failed to initialize mutex");
        nimcp_free(cl);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "contextual_language_create: validation failed");
        return NULL;
    }

    // Allocate history buffer
    cl->history_size = cl->config.max_context_history;
    cl->history = (context_history_entry_t*)nimcp_calloc(
        cl->history_size, sizeof(context_history_entry_t)
    );
    if (!cl->history) {
        set_error("Failed to allocate context history buffer");
        nimcp_mutex_destroy(&cl->lock);
        nimcp_free(cl);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "contextual_language_create: cl->history is NULL");
        return NULL;
    }

    // Initialize neural weights
    init_context_weights(cl);

    // Set initial context (CASUAL as default)
    cl->current_context.current_context = CONTEXT_CASUAL;
    cl->current_context.formality_level = 0.3F;
    cl->current_context.precision_level = 0.5F;
    cl->current_context.emotional_tone = 0.2F;
    cl->current_context.urgency_level = 0.0F;

    cl->last_update_us = get_time_us();

    // Bio-async registration
    if (cl->config.enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_BRAIN_REGION,  // Using brain region ID
            .module_name = "contextual_language",
            .inbox_capacity = 64,
            .user_data = cl
        };
        cl->bio_ctx = bio_router_register_module(&info);

        if (cl->bio_ctx) {
            /* Try KG-driven wiring callback first */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_BRAIN_REGION,
                (void*)contextual_language_wiring_handler_callback,
                cl
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fall back to legacy direct registration */
                LOG_DEBUG("KG wiring not available, using legacy registration");

                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(
                        cl->bio_ctx,
                        BIO_MSG_BRAIN_STATE_QUERY,  // Reuse existing message type
                        handle_context_detected
                    )
                );
            }
        }
    }

    LOG_INFO("Contextual language system created (history=%u, bio_async=%s)",
             cl->history_size, cl->config.enable_bio_async ? "enabled" : "disabled");

    return cl;
}

void contextual_language_destroy(contextual_language_t cl) {
    // Guard clause
    if (!cl) return;

    LOG_INFO("Destroying contextual language system");

    // Unregister from bio-async
    if (cl->bio_ctx) {
        bio_router_unregister_module(cl->bio_ctx);
        cl->bio_ctx = NULL;
    }

    // Free history
    if (cl->history) {
        nimcp_free(cl->history);
        cl->history = NULL;
    }

    // Destroy mutex
    nimcp_mutex_destroy(&cl->lock);

    // Free structure
    nimcp_free(cl);
}

int contextual_detect_context(
    contextual_language_t cl,
    const float* message_features,
    uint32_t feature_size,
    context_state_t* detected
) {
    // Guard clauses
    if (!cl || !message_features || !detected || feature_size == 0) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_detect_context: required parameter is NULL (cl, message_features, detected)");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);

    // Compute logits for each context
    float logits[CONTEXT_TYPE_COUNT] = {0};
    for (uint32_t ctx = 0; ctx < CONTEXT_TYPE_COUNT; ctx++) {
        logits[ctx] = cl->context_biases[ctx];
        for (uint32_t i = 0; i < MIN(feature_size, FEATURE_DIM); i++) {
            logits[ctx] += cl->context_weights[ctx][i] * message_features[i];
        }
    }

    // Convert to probabilities
    float probs[CONTEXT_TYPE_COUNT];
    softmax(logits, CONTEXT_TYPE_COUNT, probs);

    // Find max probability context
    uint32_t max_ctx = 0;
    float max_prob = probs[0];
    for (uint32_t i = 1; i < CONTEXT_TYPE_COUNT; i++) {
        if (probs[i] > max_prob) {
            max_prob = probs[i];
            max_ctx = i;
        }
    }

    // Set detected context
    detected->current_context = (communication_context_t)max_ctx;
    contextual_get_default_state(detected->current_context, detected);

    // Add to history
    if (cl->history_count < cl->history_size) {
        cl->history_count++;
    }
    cl->history[cl->history_head].state = *detected;
    cl->history[cl->history_head].timestamp_us = get_time_us();
    cl->history[cl->history_head].confidence = max_prob;
    cl->history_head = (cl->history_head + 1) % cl->history_size;

    // Update stats
    cl->stats.total_detections++;
    cl->stats.context_distribution[max_ctx]++;

    nimcp_mutex_unlock(&cl->lock);

    LOG_DEBUG("Detected context: %s (confidence=%.2f)",
              contextual_get_context_name(detected->current_context), max_prob);

    return 0;
}

int contextual_adapt_message(
    contextual_language_t cl,
    const float* original,
    uint32_t size,
    const context_state_t* target_context,
    float* adapted,
    uint32_t* adapted_size
) {
    // Guard clauses
    if (!cl || !original || !target_context || !adapted || !adapted_size) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_adapt_message: required parameter is NULL (cl, original, target_context, adapted, adapted_size)");
        return -1;
    }

    if (size > CONTEXTUAL_MAX_MESSAGE) {
        set_error("Message too large");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contextual_adapt_message: validation failed");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);

    uint32_t source_ctx = cl->current_context.current_context;
    uint32_t target_ctx = target_context->current_context;

    // Check if transformation is learned
    if (cl->transformation_learned[source_ctx][target_ctx]) {
        // Apply learned transformation
        *adapted_size = size;
        for (uint32_t i = 0; i < size; i++) {
            float transform = cl->transformation_cache[source_ctx][target_ctx][i];
            adapted[i] = original[i] * (1.0F - cl->config.adaptation_rate) +
                         transform * cl->config.adaptation_rate;
        }
    } else {
        // Simple interpolation based on context parameters
        *adapted_size = size;
        float formality_delta = target_context->formality_level -
                                cl->current_context.formality_level;
        float precision_delta = target_context->precision_level -
                               cl->current_context.precision_level;

        for (uint32_t i = 0; i < size; i++) {
            adapted[i] = original[i];
            // Simple adjustment (placeholder for more sophisticated adaptation)
            adapted[i] += formality_delta * 0.1F + precision_delta * 0.1F;
            adapted[i] = fmaxf(0.0F, fminf(1.0F, adapted[i])); // Clamp to [0,1]
        }
    }

    // Update current context
    cl->current_context = *target_context;
    cl->last_update_us = get_time_us();

    // Update stats
    cl->stats.total_adaptations++;
    if (source_ctx != target_ctx) {
        cl->stats.context_switches++;
    }

    nimcp_mutex_unlock(&cl->lock);

    return 0;
}

int contextual_learn_context_mapping(
    contextual_language_t cl,
    const context_state_t* source,
    const context_state_t* target,
    const float* transformation,
    uint32_t trans_size
) {
    // Guard clauses
    if (!cl || !source || !target || !transformation) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_learn_context_mapping: required parameter is NULL (cl, source, target, transformation)");
        return -1;
    }

    if (trans_size > CONTEXTUAL_MAX_MESSAGE) {
        set_error("Transformation too large");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contextual_learn_context_mapping: validation failed");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);

    uint32_t src_ctx = source->current_context;
    uint32_t tgt_ctx = target->current_context;

    // Store transformation
    memcpy(cl->transformation_cache[src_ctx][tgt_ctx], transformation,
           trans_size * sizeof(float));
    cl->transformation_learned[src_ctx][tgt_ctx] = true;

    // Update stats
    cl->stats.learning_updates++;

    nimcp_mutex_unlock(&cl->lock);

    LOG_DEBUG("Learned transformation: %s -> %s",
              contextual_get_context_name(src_ctx),
              contextual_get_context_name(tgt_ctx));

    return 0;
}

int contextual_get_current_context(
    contextual_language_t cl,
    context_state_t* state
) {
    // Guard clauses
    if (!cl || !state) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_get_current_context: required parameter is NULL (cl, state)");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);
    *state = cl->current_context;
    nimcp_mutex_unlock(&cl->lock);

    return 0;
}

int contextual_set_current_context(
    contextual_language_t cl,
    const context_state_t* state
) {
    // Guard clauses
    if (!cl || !state) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_set_current_context: required parameter is NULL (cl, state)");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);
    cl->current_context = *state;
    cl->last_update_us = get_time_us();
    nimcp_mutex_unlock(&cl->lock);

    return 0;
}

int contextual_get_history(
    contextual_language_t cl,
    context_state_t* history,
    uint32_t max_count,
    uint32_t* count
) {
    // Guard clauses
    if (!cl || !history || !count) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_get_history: required parameter is NULL (cl, history, count)");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);

    uint32_t num_entries = MIN(max_count, cl->history_count);
    uint32_t start = (cl->history_head + cl->history_size - num_entries) % cl->history_size;

    for (uint32_t i = 0; i < num_entries; i++) {
        uint32_t idx = (start + i) % cl->history_size;
        history[i] = cl->history[idx].state;
    }

    *count = num_entries;

    nimcp_mutex_unlock(&cl->lock);

    return 0;
}

int contextual_clear_history(contextual_language_t cl) {
    // Guard clause
    if (!cl) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_clear_history: cl is NULL");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);
    cl->history_head = 0;
    cl->history_count = 0;
    memset(cl->history, 0, cl->history_size * sizeof(context_history_entry_t));
    nimcp_mutex_unlock(&cl->lock);

    return 0;
}

int contextual_get_stats(
    contextual_language_t cl,
    contextual_language_stats_t* stats
) {
    // Guard clauses
    if (!cl || !stats) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_get_stats: required parameter is NULL (cl, stats)");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);
    *stats = cl->stats;
    nimcp_mutex_unlock(&cl->lock);

    return 0;
}

int contextual_reset_stats(contextual_language_t cl) {
    // Guard clause
    if (!cl) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contextual_reset_stats: cl is NULL");
        return -1;
    }

    nimcp_mutex_lock(&cl->lock);
    memset(&cl->stats, 0, sizeof(contextual_language_stats_t));
    nimcp_mutex_unlock(&cl->lock);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* contextual_get_context_name(communication_context_t context) {
    switch (context) {
        case CONTEXT_FORMAL: return "formal";
        case CONTEXT_CASUAL: return "casual";
        case CONTEXT_TECHNICAL: return "technical";
        case CONTEXT_EMOTIONAL: return "emotional";
        case CONTEXT_URGENT: return "urgent";
        case CONTEXT_LEARNING: return "learning";
        default: return "unknown";
    }
}

int contextual_get_default_state(
    communication_context_t context,
    context_state_t* state
) {
    // Guard clause
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return -1;

    }

    state->current_context = context;

    switch (context) {
        case CONTEXT_FORMAL:
            state->formality_level = 0.9F;
            state->precision_level = 0.8F;
            state->emotional_tone = 0.0F;
            state->urgency_level = 0.3F;
            break;
        case CONTEXT_CASUAL:
            state->formality_level = 0.2F;
            state->precision_level = 0.5F;
            state->emotional_tone = 0.3F;
            state->urgency_level = 0.1F;
            break;
        case CONTEXT_TECHNICAL:
            state->formality_level = 0.7F;
            state->precision_level = 0.95F;
            state->emotional_tone = 0.0F;
            state->urgency_level = 0.2F;
            break;
        case CONTEXT_EMOTIONAL:
            state->formality_level = 0.3F;
            state->precision_level = 0.4F;
            state->emotional_tone = 0.8F;
            state->urgency_level = 0.4F;
            break;
        case CONTEXT_URGENT:
            state->formality_level = 0.5F;
            state->precision_level = 0.9F;
            state->emotional_tone = -0.2F;
            state->urgency_level = 0.95F;
            break;
        case CONTEXT_LEARNING:
            state->formality_level = 0.5F;
            state->precision_level = 0.7F;
            state->emotional_tone = 0.2F;
            state->urgency_level = 0.0F;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contextual_get_default_state: operation failed");
            return -1;
    }

    return 0;
}

float contextual_compute_distance(
    const context_state_t* state1,
    const context_state_t* state2
) {
    // Guard clauses
    if (!state1 || !state2) return -1.0F;

    float d_formal = state1->formality_level - state2->formality_level;
    float d_precision = state1->precision_level - state2->precision_level;
    float d_emotion = state1->emotional_tone - state2->emotional_tone;
    float d_urgency = state1->urgency_level - state2->urgency_level;

    return sqrtf(d_formal * d_formal + d_precision * d_precision +
                 d_emotion * d_emotion + d_urgency * d_urgency);
}

const char* contextual_language_get_last_error(void) {
    return error_buffer;
}
