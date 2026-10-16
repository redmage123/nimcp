#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_cortical_temporal.c - Cortical Column Temporal Dynamics Implementation
//=============================================================================
/**
 * @file nimcp_cortical_temporal.c
 * @brief Implementation of temporal dynamics and sequence processing
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Intrinsic timescales, adaptation, habituation, sequence detection
 * WHY:  Implement biologically-realistic temporal processing
 * HOW:  Exponential dynamics, weighted history integration, pattern matching
 *
 * @author NIMCP Development Team
 */

#include "core/cortical_columns/nimcp_cortical_temporal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_temporal)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Cortical temporal system implementation
 */
struct cortical_temporal_system {
    temporal_config_t config;
    temporal_state_t state;
    temporal_stats_t stats;

    /** Temporal receptive fields per layer */
    temporal_receptive_field_t trfs[CORTICAL_TEMPORAL_NUM_LAYERS];

    /** Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /** Thread safety */
    nimcp_platform_mutex_t* mutex;

    /** Magic number for validation */
    uint32_t magic;
};

#define CORTICAL_TEMPORAL_MAGIC 0x54454D50  // 'TEMP'

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Validate temporal system handle
 * WHY:  Ensure pointer is non-NULL and magic is correct
 * HOW:  Guard clause pattern
 */
static inline bool validate_system(const cortical_temporal_system_t* system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_system: system is NULL");
        return false;
    }
    if (system->magic != CORTICAL_TEMPORAL_MAGIC) {
        return false;
    }
    return true;
}

/**
 * WHAT: Initialize temporal receptive field weights
 * WHY:  Create exponential temporal kernel
 * HOW:  w(t) = exp(-t/tau) with normalization
 */
static int initialize_trf_weights(
    temporal_receptive_field_t* trf,
    float tau,
    uint32_t num_bins,
    float bin_width_ms
)
{
    // Guard clauses
    if (!trf) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trf is NULL");

        return -1;

    }
    if (num_bins == 0) return -1;
    if (bin_width_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "initialize_trf_weights: validation failed");
        return -1;
    }
    if (tau <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "initialize_trf_weights: validation failed");
        return -1;
    }

    trf->num_bins = num_bins;
    trf->bin_width_ms = bin_width_ms;
    trf->total_window_ms = num_bins * bin_width_ms;

    trf->history_weights = nimcp_calloc(num_bins, sizeof(float));
    if (!trf->history_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_trf_weights: trf->history_weights is NULL");
        return -1;
    }

    // Exponential kernel
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_bins; i++) {
        float t = i * bin_width_ms;
        trf->history_weights[i] = expf(-t / tau);
        sum += trf->history_weights[i];
    }

    // Normalize to sum to 1.0
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < num_bins; i++) {
            trf->history_weights[i] /= sum;
        }
    }

    return 0;
}

/**
 * WHAT: Free temporal receptive field
 * WHY:  Release allocated weights
 * HOW:  Free weights array
 */
static void free_trf(temporal_receptive_field_t* trf)
{
    if (!trf) return;
    if (trf->history_weights) {
        nimcp_free(trf->history_weights);
        trf->history_weights = NULL;
    }
}

/**
 * WHAT: Initialize temporal state
 * WHY:  Allocate history buffers and adaptation states
 * HOW:  Calloc for zero initialization
 */
static int initialize_temporal_state(
    temporal_state_t* state,
    const temporal_config_t* config
)
{
    // Guard clauses
    if (!state || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "initialize_temporal_state: required parameter is NULL (state, config)");
        return -1;
    }
    if (config->num_columns == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "initialize_temporal_state: config->num_columns is zero");
        return -1;
    }
    if (config->history_bins == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "initialize_temporal_state: config->history_bins is zero");
        return -1;
    }

    // Allocate layer activity history
    state->layer_activities = nimcp_calloc(
        CORTICAL_TEMPORAL_NUM_LAYERS,
        sizeof(float*)
    );
    if (!state->layer_activities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_temporal_state: state->layer_activities is NULL");
        return -1;
    }

    for (uint32_t layer = 0; layer < CORTICAL_TEMPORAL_NUM_LAYERS; layer++) {
        state->layer_activities[layer] = nimcp_calloc(
            config->history_bins,
            sizeof(float)
        );
        if (!state->layer_activities[layer]) {
            // Cleanup on failure
            for (uint32_t j = 0; j < layer; j++) {
                nimcp_free(state->layer_activities[j]);
            }
            nimcp_free(state->layer_activities);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_temporal_state: layer_activities alloc failed");
            return -1;
        }
    }

    // Allocate adaptation states
    state->adaptation_states = nimcp_calloc(
        config->num_columns,
        sizeof(float)
    );
    if (!state->adaptation_states) {
        for (uint32_t layer = 0; layer < CORTICAL_TEMPORAL_NUM_LAYERS; layer++) {
            nimcp_free(state->layer_activities[layer]);
        }
        nimcp_free(state->layer_activities);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_temporal_state: adaptation_states alloc failed");
        return -1;
    }

    // Allocate habituation states
    state->habituation_states = nimcp_calloc(
        config->num_columns,
        sizeof(float)
    );
    if (!state->habituation_states) {
        nimcp_free(state->adaptation_states);
        for (uint32_t layer = 0; layer < CORTICAL_TEMPORAL_NUM_LAYERS; layer++) {
            nimcp_free(state->layer_activities[layer]);
        }
        nimcp_free(state->layer_activities);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "initialize_temporal_state: habituation_states alloc failed");
        return -1;
    }

    // Initialize temporal indices
    state->history_index = 0;
    state->current_time_ms = 0;
    state->last_update_ms = 0;

    return 0;
}

/**
 * WHAT: Free temporal state
 * WHY:  Release all allocated memory
 * HOW:  Free arrays in reverse allocation order
 */
static void free_temporal_state(temporal_state_t* state)
{
    if (!state) return;

    if (state->habituation_states) {
        nimcp_free(state->habituation_states);
        state->habituation_states = NULL;
    }

    if (state->adaptation_states) {
        nimcp_free(state->adaptation_states);
        state->adaptation_states = NULL;
    }

    if (state->layer_activities) {
        for (uint32_t layer = 0; layer < CORTICAL_TEMPORAL_NUM_LAYERS; layer++) {
            if (state->layer_activities[layer]) {
                nimcp_free(state->layer_activities[layer]);
            }
        }
        nimcp_free(state->layer_activities);
        state->layer_activities = NULL;
    }
}

//=============================================================================
// Core API Implementation
//=============================================================================

cortical_temporal_system_t* cortical_temporal_create(
    const temporal_config_t* config
)
{
    // Guard clauses
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }
    if (config->num_columns == 0) {
        NIMCP_LOGGING_ERROR("Invalid num_columns: 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_create: config->num_columns is zero");
        return NULL;
    }
    if (config->history_bins == 0) {
        NIMCP_LOGGING_ERROR("Invalid history_bins: 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_create: config->history_bins is zero");
        return NULL;
    }

    // Allocate system
    cortical_temporal_system_t* system = nimcp_calloc(
        1,
        sizeof(cortical_temporal_system_t)
    );
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate temporal system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "system is NULL");

        return NULL;
    }

    // Copy configuration
    system->config = *config;
    system->magic = CORTICAL_TEMPORAL_MAGIC;

    // Initialize temporal state
    if (initialize_temporal_state(&system->state, config) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize temporal state");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cortical_temporal_create: validation failed");
        return NULL;
    }

    // Initialize TRFs for each layer
    float bin_width_ms = config->history_window_ms / config->history_bins;
    for (uint32_t layer = 0; layer < CORTICAL_TEMPORAL_NUM_LAYERS; layer++) {
        float tau = config->layer_timescales[layer].tau;
        if (initialize_trf_weights(
                &system->trfs[layer],
                tau,
                config->history_bins,
                bin_width_ms
            ) != 0)
        {
            NIMCP_LOGGING_ERROR("Failed to initialize TRF for layer %u", layer);
            // Cleanup
            for (uint32_t j = 0; j < layer; j++) {
                free_trf(&system->trfs[j]);
            }
            free_temporal_state(&system->state);
            nimcp_free(system);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_temporal_create: TRF init failed");
            return NULL;
        }
    }

    // Initialize statistics
    memset(&system->stats, 0, sizeof(temporal_stats_t));

    system->bio_async_enabled = false;
    system->mutex = NULL;

    NIMCP_LOGGING_INFO(
        "Created cortical temporal system: %u columns, %u history bins",
        config->num_columns,
        config->history_bins
    );

    return system;
}

void cortical_temporal_destroy(cortical_temporal_system_t* system)
{
    // Guard clause
    if (!validate_system(system)) return;

    // Disconnect bio-async
    if (system->bio_async_enabled) {
        cortical_temporal_disconnect_bio_async(system);
    }

    // Free TRFs
    for (uint32_t layer = 0; layer < CORTICAL_TEMPORAL_NUM_LAYERS; layer++) {
        free_trf(&system->trfs[layer]);
    }

    // Free temporal state
    free_temporal_state(&system->state);

    // Invalidate magic
    system->magic = 0;

    // Free system
    nimcp_free(system);

    NIMCP_LOGGING_DEBUG("Destroyed cortical temporal system");
}

temporal_config_t cortical_temporal_default_config(
    uint32_t num_columns,
    uint32_t neurons_per_column
)
{
    temporal_config_t config;
    memset(&config, 0, sizeof(temporal_config_t));

    // Layer timescales based on Murray et al. (2014)
    config.layer_timescales[0].tau = TEMPORAL_TAU_L1;
    config.layer_timescales[0].adaptation_rate = 0.3f;
    config.layer_timescales[0].recovery_rate = 0.1f;

    config.layer_timescales[1].tau = TEMPORAL_TAU_L23;
    config.layer_timescales[1].adaptation_rate = 0.4f;
    config.layer_timescales[1].recovery_rate = 0.08f;

    config.layer_timescales[2].tau = TEMPORAL_TAU_L4;
    config.layer_timescales[2].adaptation_rate = 0.2f;
    config.layer_timescales[2].recovery_rate = 0.15f;

    config.layer_timescales[3].tau = TEMPORAL_TAU_L5;
    config.layer_timescales[3].adaptation_rate = 0.35f;
    config.layer_timescales[3].recovery_rate = 0.1f;

    config.layer_timescales[4].tau = TEMPORAL_TAU_L6;
    config.layer_timescales[4].adaptation_rate = 0.3f;
    config.layer_timescales[4].recovery_rate = 0.12f;

    // History integration
    config.history_window_ms = TEMPORAL_DEFAULT_HISTORY_WINDOW_MS;
    config.history_bins = TEMPORAL_DEFAULT_HISTORY_BINS;

    // Adaptation dynamics
    config.adaptation_strength = TEMPORAL_DEFAULT_ADAPTATION_STRENGTH;
    config.adaptation_decay = 100.0f;  // 100ms decay

    // Habituation dynamics
    config.habituation_rate = TEMPORAL_DEFAULT_HABITUATION_RATE;
    config.habituation_recovery = 1000.0f;  // 1s recovery

    // Sequence detection
    config.enable_sequence_detection = true;
    config.sequence_threshold = 0.75f;
    config.max_sequence_length = 100;

    // Column configuration
    config.num_columns = num_columns;
    config.neurons_per_column = neurons_per_column;

    return config;
}

//=============================================================================
// Timescale Configuration API
//=============================================================================

int cortical_temporal_set_layer_timescale(
    cortical_temporal_system_t* system,
    uint32_t layer,
    const layer_timescale_t* timescale
)
{
    // Guard clauses
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_set_layer_timescale: validate_system is NULL");
        return -1;
    }
    if (!timescale) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "timescale is NULL");

        return -1;

    }
    if (layer >= CORTICAL_TEMPORAL_NUM_LAYERS) return -1;
    if (timescale->tau <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_set_layer_timescale: validation failed");
        return -1;
    }

    // Update configuration
    system->config.layer_timescales[layer] = *timescale;

    // Reinitialize TRF weights for this layer
    free_trf(&system->trfs[layer]);
    float bin_width_ms = system->config.history_window_ms /
                         system->config.history_bins;

    if (initialize_trf_weights(
            &system->trfs[layer],
            timescale->tau,
            system->config.history_bins,
            bin_width_ms
        ) != 0)
    {
        NIMCP_LOGGING_ERROR("Failed to reinitialize TRF for layer %u", layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_set_layer_timescale: operation failed");
        return -1;
    }

    NIMCP_LOGGING_DEBUG(
        "Set layer %u timescale: tau=%.2fms",
        layer,
        timescale->tau
    );

    return 0;
}

float cortical_temporal_get_effective_timescale(
    const cortical_temporal_system_t* system,
    uint32_t layer
)
{
    // Guard clauses
    if (!validate_system(system)) return -1.0f;
    if (layer >= CORTICAL_TEMPORAL_NUM_LAYERS) return -1.0f;

    return system->config.layer_timescales[layer].tau;
}

//=============================================================================
// Temporal Dynamics API
//=============================================================================

int cortical_temporal_update(
    cortical_temporal_system_t* system,
    float delta_time_ms
)
{
    // Guard clauses
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_update: validate_system is NULL");
        return -1;
    }
    if (delta_time_ms < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_update: validation failed");
        return -1;
    }

    temporal_state_t* state = &system->state;
    const temporal_config_t* config = &system->config;

    // Update time
    state->current_time_ms += (uint64_t)delta_time_ms;
    state->last_update_ms = state->current_time_ms;

    // Update adaptation states (exponential decay)
    float adapt_decay = expf(-delta_time_ms / config->adaptation_decay);
    for (uint32_t col = 0; col < config->num_columns; col++) {
        state->adaptation_states[col] *= adapt_decay;
    }

    // Update habituation states (exponential recovery)
    float habit_recovery = expf(-delta_time_ms / config->habituation_recovery);
    for (uint32_t col = 0; col < config->num_columns; col++) {
        state->habituation_states[col] *= habit_recovery;
    }

    // Advance circular buffer index
    state->history_index = (state->history_index + 1) % config->history_bins;

    // Update statistics
    system->stats.total_updates++;
    system->stats.elapsed_time_ms += (uint64_t)delta_time_ms;

    return 0;
}

float cortical_temporal_apply_adaptation(
    cortical_temporal_system_t* system,
    uint32_t column_id,
    float activity
)
{
    // Guard clauses
    if (!validate_system(system)) return -1.0f;
    if (column_id >= system->config.num_columns) return -1.0f;
    if (activity < 0.0f || activity > 1.0f) return -1.0f;

    temporal_state_t* state = &system->state;
    const temporal_config_t* config = &system->config;

    // Update adaptation state: a(t) = a(t-1) + strength * activity
    float adaptation = state->adaptation_states[column_id];
    adaptation += config->adaptation_strength * activity;

    // Clamp to [0, 1]
    if (adaptation > 1.0f) adaptation = 1.0f;
    if (adaptation < 0.0f) adaptation = 0.0f;

    state->adaptation_states[column_id] = adaptation;

    // Apply adaptation to activity
    float adapted_activity = activity * (1.0f - adaptation);

    return adapted_activity;
}

float cortical_temporal_apply_habituation(
    cortical_temporal_system_t* system,
    uint32_t column_id,
    float activity
)
{
    // Guard clauses
    if (!validate_system(system)) return -1.0f;
    if (column_id >= system->config.num_columns) return -1.0f;
    if (activity < 0.0f || activity > 1.0f) return -1.0f;

    temporal_state_t* state = &system->state;
    const temporal_config_t* config = &system->config;

    // Update habituation state: h(t) = h(t-1) + rate * activity
    float habituation = state->habituation_states[column_id];
    habituation += config->habituation_rate * activity;

    // Clamp to [0, 1]
    if (habituation > 1.0f) habituation = 1.0f;
    if (habituation < 0.0f) habituation = 0.0f;

    state->habituation_states[column_id] = habituation;

    // Apply habituation to activity
    float habituated_activity = activity * (1.0f - habituation);

    return habituated_activity;
}

float cortical_temporal_integrate_history(
    cortical_temporal_system_t* system,
    uint32_t column_id,
    uint32_t layer,
    float current_activity
)
{
    // Guard clauses
    if (!validate_system(system)) return -1.0f;
    if (column_id >= system->config.num_columns) return -1.0f;
    if (layer >= CORTICAL_TEMPORAL_NUM_LAYERS) return -1.0f;

    const temporal_state_t* state = &system->state;
    const temporal_receptive_field_t* trf = &system->trfs[layer];
    const temporal_config_t* config = &system->config;

    // Store current activity in history buffer
    uint32_t idx = state->history_index;
    system->state.layer_activities[layer][idx] = current_activity;

    // Compute weighted sum over history
    float integrated = 0.0f;
    for (uint32_t i = 0; i < config->history_bins; i++) {
        // Index into circular buffer (most recent first)
        uint32_t hist_idx = (idx + config->history_bins - i) %
                            config->history_bins;
        float past_activity = state->layer_activities[layer][hist_idx];
        integrated += trf->history_weights[i] * past_activity;
    }

    return integrated;
}

int cortical_temporal_reset_adaptation(
    cortical_temporal_system_t* system,
    uint32_t column_id
)
{
    // Guard clauses
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_reset_adaptation: validate_system is NULL");
        return -1;
    }
    if (column_id >= system->config.num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_reset_adaptation: capacity exceeded");
        return -1;
    }

    system->state.adaptation_states[column_id] = 0.0f;
    return 0;
}

int cortical_temporal_reset_habituation(
    cortical_temporal_system_t* system,
    uint32_t column_id
)
{
    // Guard clauses
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_reset_habituation: validate_system is NULL");
        return -1;
    }
    if (column_id >= system->config.num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_reset_habituation: capacity exceeded");
        return -1;
    }

    system->state.habituation_states[column_id] = 0.0f;
    return 0;
}

//=============================================================================
// Sequence Detection API
//=============================================================================

sequence_detector_t* cortical_temporal_create_sequence_detector(
    const float* sequence_template,
    uint32_t sequence_length,
    float detection_threshold
)
{
    // Guard clauses
    if (!sequence_template) {
        NIMCP_LOGGING_ERROR("NULL sequence template");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_template is NULL");

        return NULL;
    }
    if (sequence_length == 0) {
        NIMCP_LOGGING_ERROR("Invalid sequence length: 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_create_sequence_detector: sequence_length is zero");
        return NULL;
    }
    if (detection_threshold < 0.0f || detection_threshold > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid threshold: %.2f", detection_threshold);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_create_sequence_detector: threshold out of range [0,1]");
        return NULL;
    }

    // Allocate detector
    sequence_detector_t* detector = nimcp_calloc(
        1,
        sizeof(sequence_detector_t)
    );
    if (!detector) {
        NIMCP_LOGGING_ERROR("Failed to allocate sequence detector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "detector is NULL");

        return NULL;
    }

    // Allocate template
    detector->sequence_template = nimcp_calloc(
        sequence_length,
        sizeof(float)
    );
    if (!detector->sequence_template) {
        NIMCP_LOGGING_ERROR("Failed to allocate sequence template");
        nimcp_free(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_temporal_create_sequence_detector: detector->sequence_template is NULL");
        return NULL;
    }

    // Copy template
    memcpy(
        detector->sequence_template,
        sequence_template,
        sequence_length * sizeof(float)
    );

    // Allocate match scores
    detector->match_scores = nimcp_calloc(
        sequence_length,
        sizeof(float)
    );
    if (!detector->match_scores) {
        NIMCP_LOGGING_ERROR("Failed to allocate match scores");
        nimcp_free(detector->sequence_template);
        nimcp_free(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_temporal_create_sequence_detector: detector->match_scores is NULL");
        return NULL;
    }

    detector->sequence_length = sequence_length;
    detector->detection_threshold = detection_threshold;
    detector->detected = false;
    detector->detection_count = 0;

    NIMCP_LOGGING_DEBUG(
        "Created sequence detector: length=%u, threshold=%.2f",
        sequence_length,
        detection_threshold
    );

    return detector;
}

void cortical_temporal_destroy_sequence_detector(
    sequence_detector_t* detector
)
{
    // Guard clause
    if (!detector) return;

    if (detector->match_scores) {
        nimcp_free(detector->match_scores);
    }

    if (detector->sequence_template) {
        nimcp_free(detector->sequence_template);
    }

    nimcp_free(detector);
}

bool cortical_temporal_detect_sequence(
    cortical_temporal_system_t* system,
    sequence_detector_t* detector,
    uint32_t column_id,
    uint32_t layer
)
{
    // Guard clauses
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_detect_sequence: validate_system is NULL");
        return false;
    }
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_temporal_detect_sequence: detector is NULL");
        return false;
    }
    if (column_id >= system->config.num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_detect_sequence: capacity exceeded");
        return false;
    }
    if (layer >= CORTICAL_TEMPORAL_NUM_LAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_detect_sequence: capacity exceeded");
        return false;
    }

    const temporal_state_t* state = &system->state;
    const temporal_config_t* config = &system->config;

    // Ensure we have enough history
    if (detector->sequence_length > config->history_bins) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_detect_sequence: validation failed");
        return false;
    }

    // Compute match scores using template matching
    float total_match = 0.0f;
    uint32_t idx = state->history_index;

    for (uint32_t i = 0; i < detector->sequence_length; i++) {
        // Index into circular buffer
        uint32_t hist_idx = (idx + config->history_bins - i) %
                            config->history_bins;
        float activity = state->layer_activities[layer][hist_idx];
        float template_val = detector->sequence_template[i];

        // Compute match (1 - |difference|)
        float match = 1.0f - fabsf(activity - template_val);
        if (match < 0.0f) match = 0.0f;

        detector->match_scores[i] = match;
        total_match += match;
    }

    // Average match score
    float avg_match = total_match / detector->sequence_length;

    // Check threshold
    bool detected = (avg_match >= detector->detection_threshold);

    if (detected && !detector->detected) {
        // New detection
        detector->detection_count++;
        system->stats.sequences_detected++;
    }

    detector->detected = detected;
    system->stats.avg_sequence_match = avg_match;

    return detected;
}

//=============================================================================
// Statistics API
//=============================================================================

int cortical_temporal_get_stats(
    const cortical_temporal_system_t* system,
    temporal_stats_t* stats
)
{
    // Guard clauses
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_get_stats: validate_system is NULL");
        return -1;
    }
    if (!stats) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");

        return -1;

    }

    const temporal_state_t* state = &system->state;
    const temporal_config_t* config = &system->config;

    // Copy base stats
    *stats = system->stats;

    // Compute adaptation statistics
    float sum_adapt = 0.0f;
    float max_adapt = 0.0f;
    uint32_t adapted_count = 0;

    for (uint32_t col = 0; col < config->num_columns; col++) {
        float adapt = state->adaptation_states[col];
        sum_adapt += adapt;
        if (adapt > max_adapt) max_adapt = adapt;
        if (adapt > 0.1f) adapted_count++;  // Threshold for "adapted"
    }

    stats->avg_adaptation = sum_adapt / config->num_columns;
    stats->max_adaptation = max_adapt;
    stats->adapted_columns = adapted_count;

    // Compute habituation statistics
    float sum_habit = 0.0f;
    float max_habit = 0.0f;
    uint32_t habituated_count = 0;

    for (uint32_t col = 0; col < config->num_columns; col++) {
        float habit = state->habituation_states[col];
        sum_habit += habit;
        if (habit > max_habit) max_habit = habit;
        if (habit > 0.1f) habituated_count++;  // Threshold for "habituated"
    }

    stats->avg_habituation = sum_habit / config->num_columns;
    stats->max_habituation = max_habit;
    stats->habituated_columns = habituated_count;

    return 0;
}

int cortical_temporal_reset_stats(
    cortical_temporal_system_t* system
)
{
    // Guard clause
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_reset_stats: validate_system is NULL");
        return -1;
    }

    memset(&system->stats, 0, sizeof(temporal_stats_t));
    return 0;
}

//=============================================================================
// Bio-async Integration API
//=============================================================================

int cortical_temporal_connect_bio_async(
    cortical_temporal_system_t* system
)
{
    // Guard clause
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_connect_bio_async: validate_system is NULL");
        return -1;
    }
    if (system->bio_async_enabled) return 0;  // Already connected

    // Register with bio-async router
    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORTICAL_TEMPORAL,
        .module_name = "cortical_temporal",
        .inbox_capacity = 32,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        return 0;
    }
}

int cortical_temporal_disconnect_bio_async(
    cortical_temporal_system_t* system
)
{
    // Guard clauses
    if (!validate_system(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_temporal_disconnect_bio_async: validate_system is NULL");
        return -1;
    }
    if (!system->bio_async_enabled) return 0;

    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool cortical_temporal_is_bio_async_connected(
    const cortical_temporal_system_t* system
)
{
    // Guard clause
    if (!validate_system(system)) {
        return false;
    }
    return system->bio_async_enabled;
}
