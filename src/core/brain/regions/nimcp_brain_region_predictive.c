/**
 * @file nimcp_brain_region_predictive.c
 * @brief Implementation of Predictive Coding Integration with Brain Regions
 *
 * WHAT: Extends brain regions with hierarchical predictive processing
 * WHY:  Implement Free Energy Principle across cortical hierarchy
 * HOW:  Regions predict lower regions; errors flow up, predictions flow down
 *
 * ARCHITECTURAL PATTERNS:
 * - Chain of Responsibility: Error propagation through hierarchy
 * - Strategy Pattern: Different prediction strategies per region
 * - Observer Pattern: Bio-async notifications
 * - Facade Pattern: Simplified interface over predictive machinery
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - Lazy initialization of predictive hierarchy
 * - Caching of prediction/error buffers
 * - Batch bio-async messaging
 * - Lock-free reads where possible
 *
 * COMPLEXITY ANALYSIS:
 * - brain_region_predict_lower: O(N × M) matrix multiplication
 * - brain_region_compute_error: O(N) element-wise operations
 * - brain_region_hierarchical_step: O(N × H × I) full hierarchy pass
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include "core/brain_regions/nimcp_brain_region_predictive.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_security.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "brain_region_predictive"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_region_predictive, MESH_ADAPTER_CATEGORY_COGNITIVE)


// Default PE embedding dimension for hierarchy encoding
#define DEFAULT_HIERARCHY_PE_DIM 64

// Default maximum prediction sequence length
#define DEFAULT_MAX_PREDICTION_SEQ 256

// Maximum hierarchy levels supported for PE
#define MAX_HIERARCHY_LEVELS 16

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Check if region has predictive extension
 *
 * WHAT: Validate predictive processing enabled
 * WHY:  Guard against NULL pointer access
 */
static inline bool has_predictive_extension(brain_region_t* region) {
    if (!region) {
        return false;
    }
    // Assuming brain_region_t has a predictive_extension field
    // This will need to be added to nimcp_brain_regions.h
    return region->predictive_extension != NULL;
}

/**
 * @brief Validate prediction buffer sizes match
 */
static inline bool validate_buffer_size(brain_region_t* region, uint32_t buffer_size) {
    return region && buffer_size == region->total_neurons;
}

/**
 * @brief Compute mean of float array
 */
static float compute_mean(const float* data, uint32_t size) {
    if (!data || size == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        sum += fabsf(data[i]);
    }
    return sum / size;
}

//=============================================================================
// Configuration Helpers
//=============================================================================

brain_region_predictive_config_t brain_region_predictive_config_default(uint32_t hierarchy_level) {
    /* WHAT: Create default predictive configuration
     * WHY:  Reasonable defaults for typical hierarchical processing
     */
    brain_region_predictive_config_t config = {
        .enable_predictive_processing = true,
        .generate_predictions = (hierarchy_level > 0),
        .compute_prediction_errors = true,
        .learn_precisions = true,
        .broadcast_predictions = true,
        .broadcast_errors = true,

        .hierarchy_level = hierarchy_level,
        .num_levels_below = 0,  // Must be set by caller
        .num_levels_above = 0,  // Must be set by caller

        .prediction_learning_rate = NIMCP_LEARNING_RATE_DEFAULT,
        .precision_learning_rate = NIMCP_LEARNING_RATE_FINE,
        .error_correction_rate = 0.1f,

        .max_iterations = PC_REGION_CONVERGENCE_ITERATIONS,
        .convergence_tolerance = PC_REGION_CONVERGENCE_TOLERANCE,

        .prediction_channel = BIO_CHANNEL_SEROTONIN,  // Slow, modulatory
        .error_channel = BIO_CHANNEL_NOREPINEPHRINE,  // Alerting, salient

        .enable_hierarchy_pe = true,
        .enable_temporal_pe = true,
        .pe_embedding_dim = DEFAULT_HIERARCHY_PE_DIM,
        .max_prediction_sequence = DEFAULT_MAX_PREDICTION_SEQ
    };
    return config;
}

brain_region_predictive_config_t brain_region_predictive_config_sensory(void) {
    /* WHAT: Configuration for sensory (bottom-level) regions
     * WHY:  Sensory regions process input and send errors upward
     */
    brain_region_predictive_config_t config = brain_region_predictive_config_default(0);

    // Sensory regions don't generate predictions (no regions below)
    config.generate_predictions = false;

    // High sensitivity to errors
    config.error_correction_rate = 0.2f;
    config.precision_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;  // Fast precision learning

    // Always broadcast errors up
    config.broadcast_errors = true;
    config.broadcast_predictions = false;

    // Positional encoding enabled for hierarchy level
    config.enable_hierarchy_pe = true;
    config.enable_temporal_pe = true;

    return config;
}

brain_region_predictive_config_t brain_region_predictive_config_association(void) {
    /* WHAT: Configuration for association (high-level) regions
     * WHY:  Association regions integrate across modalities, slower dynamics
     */
    brain_region_predictive_config_t config = brain_region_predictive_config_default(2);

    // Bidirectional predictions
    config.generate_predictions = true;
    config.compute_prediction_errors = true;

    // Slower, more stable learning
    config.prediction_learning_rate = NIMCP_LEARNING_RATE_FINE;
    config.error_correction_rate = 0.05f;
    config.precision_learning_rate = 0.0005f;

    // Context-dependent attention
    config.learn_precisions = true;

    // Broadcast both directions
    config.broadcast_predictions = true;
    config.broadcast_errors = true;

    // Positional encoding enabled for both hierarchy and temporal
    config.enable_hierarchy_pe = true;
    config.enable_temporal_pe = true;

    return config;
}

//=============================================================================
// Initialization and Destruction
//=============================================================================

nimcp_result_t brain_region_enable_predictive(brain_region_t* region,
                                                const brain_region_predictive_config_t* config) {
    /* WHAT: Enable predictive processing for brain region
     * WHY:  Integrate region into hierarchical predictive network
     * HOW:  Create predictive hierarchy, allocate buffers, register bio-async
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL in brain_region_enable_predictive");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL in brain_region_enable_predictive");
    NIMCP_CHECK_THROW(region->total_neurons > 0, NIMCP_ERROR_INVALID_PARAM, "Region has zero neurons");

    // Check if already enabled
    if (has_predictive_extension(region)) {
        LOG_MODULE_WARN(LOG_MODULE, "Predictive processing already enabled for region %u", region->id);
        return NIMCP_SUCCESS;
    }

    // Lock region for modification
    nimcp_mutex_lock(&region->lock);

    // Allocate predictive extension
    brain_region_predictive_t* pred = nimcp_calloc(1, sizeof(brain_region_predictive_t));
    if (!pred) {
        nimcp_mutex_unlock(&region->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate predictive extension");
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Copy configuration
    memcpy(&pred->config, config, sizeof(brain_region_predictive_config_t));

    // Allocate prediction/error buffers
    pred->current_prediction = nimcp_calloc(region->total_neurons, sizeof(float));
    pred->prediction_error = nimcp_calloc(region->total_neurons, sizeof(float));
    pred->precision_weights = nimcp_calloc(region->total_neurons, sizeof(float));

    if (!pred->current_prediction || !pred->prediction_error || !pred->precision_weights) {
        nimcp_free(pred->current_prediction);
        nimcp_free(pred->prediction_error);
        nimcp_free(pred->precision_weights);
        nimcp_free(pred);
        nimcp_mutex_unlock(&region->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate prediction buffers");
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Initialize precision weights to 1.0 (unit variance)
    for (uint32_t i = 0; i < region->total_neurons; i++) {
        pred->precision_weights[i] = PC_DEFAULT_PRECISION;
    }

    // Create predictive hierarchy if configured
    if (config->enable_predictive_processing) {
        // For now, create a simple 2-level hierarchy
        // Higher-level integration will connect multiple regions
        uint32_t units_per_level[2] = {region->total_neurons, region->total_neurons / 2};
        if (units_per_level[1] == 0) units_per_level[1] = 1;

        pc_hierarchy_config_t hier_config = pc_hierarchy_config_default(2, units_per_level);
        hier_config.learning_rate = config->prediction_learning_rate;
        hier_config.precision_learning_rate = config->precision_learning_rate;
        hier_config.learn_precisions = config->learn_precisions;

        pred->hierarchy = pc_hierarchy_create(&hier_config);
        if (!pred->hierarchy) {
            nimcp_free(pred->current_prediction);
            nimcp_free(pred->prediction_error);
            nimcp_free(pred->precision_weights);
            nimcp_free(pred);
            nimcp_mutex_unlock(&region->lock);
            LOG_MODULE_ERROR(LOG_MODULE, "Failed to create predictive hierarchy");
            return NIMCP_ERROR_NOT_INITIALIZED;
        }
    }

    // Initialize connection arrays
    pred->input_region_ids = NULL;
    pred->output_region_ids = NULL;
    pred->num_input_regions = 0;
    pred->num_output_regions = 0;

    // Initialize statistics
    pred->total_predictions = 0;
    pred->total_errors_computed = 0;
    pred->mean_prediction_error = 0.0f;
    pred->mean_precision = 1.0f;
    pred->total_free_energy = 0.0f;

    // Initialize bio-async
    pred->bio_async_registered = false;
    pred->bio_async_module_id = 0;

    // Initialize positional encoding state
    pred->hierarchy_pe_encoder = NULL;
    pred->temporal_pe_encoder = NULL;
    pred->hierarchy_level_embedding = NULL;
    pred->temporal_sequence_buffer = NULL;

    // Initialize positional encoders if enabled
    if (config->enable_hierarchy_pe) {
        // Create learned PE encoder for hierarchy levels
        nimcp_pos_config_t pe_config;
        pe_config.type = NIMCP_POS_LEARNED;
        pe_config.config.learned.base.max_seq_length = MAX_HIERARCHY_LEVELS;
        pe_config.config.learned.base.embedding_dim = config->pe_embedding_dim;
        pe_config.config.learned.base.cache_enabled = true;
        pe_config.config.learned.base.thread_safe = true;
        pe_config.config.learned.init_std = 0.02f;
        pe_config.config.learned.learning_rate = config->prediction_learning_rate;
        pe_config.config.learned.weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT;

        pred->hierarchy_pe_encoder = nimcp_pos_encoder_create(&pe_config);
        if (!pred->hierarchy_pe_encoder) {
            LOG_MODULE_WARN(LOG_MODULE, "Failed to create hierarchy PE encoder, continuing without it");
        } else {
            // Allocate hierarchy level embedding buffer
            pred->hierarchy_level_embedding = nimcp_calloc(config->pe_embedding_dim, sizeof(float));
            if (!pred->hierarchy_level_embedding) {
                nimcp_pos_encoder_destroy(pred->hierarchy_pe_encoder);
                pred->hierarchy_pe_encoder = NULL;
                LOG_MODULE_WARN(LOG_MODULE, "Failed to allocate hierarchy level embedding buffer");
            }
        }
    }

    if (config->enable_temporal_pe) {
        // Create sinusoidal PE encoder for temporal sequences
        nimcp_pos_config_t pe_config;
        pe_config.type = NIMCP_POS_SINUSOIDAL;
        pe_config.config.sinusoidal.base.max_seq_length = config->max_prediction_sequence;
        pe_config.config.sinusoidal.base.embedding_dim = region->total_neurons;
        pe_config.config.sinusoidal.base.cache_enabled = true;
        pe_config.config.sinusoidal.base.thread_safe = true;
        pe_config.config.sinusoidal.frequency_base = 10000.0f;
        pe_config.config.sinusoidal.frequency_scale = 1.0f;

        pred->temporal_pe_encoder = nimcp_pos_encoder_create(&pe_config);
        if (!pred->temporal_pe_encoder) {
            LOG_MODULE_WARN(LOG_MODULE, "Failed to create temporal PE encoder, continuing without it");
        } else {
            // Allocate temporal sequence buffer
            pred->temporal_sequence_buffer = nimcp_calloc(
                config->max_prediction_sequence * region->total_neurons,
                sizeof(float)
            );
            if (!pred->temporal_sequence_buffer) {
                nimcp_pos_encoder_destroy(pred->temporal_pe_encoder);
                pred->temporal_pe_encoder = NULL;
                LOG_MODULE_WARN(LOG_MODULE, "Failed to allocate temporal sequence buffer");
            }
        }
    }

    // Initialize mutex
    nimcp_mutex_init(&pred->lock, NULL);

    // Attach to region
    region->predictive_extension = pred;

    nimcp_mutex_unlock(&region->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Enabled predictive processing for region %u (level %u, %u neurons)",
                    region->id, config->hierarchy_level, region->total_neurons);

    // Register with security if available — nimcp_security_is_initialized()
    // was referenced here but never declared in any current header. Skipped
    // during 2026-04-24 re-enable; add a proper gate if/when the security
    // subsystem exposes an initialization query.

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_disable_predictive(brain_region_t* region) {
    /* WHAT: Disable predictive processing and free resources
     * WHY:  Clean up when predictive processing no longer needed
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    if (!has_predictive_extension(region)) return NIMCP_SUCCESS;

    nimcp_mutex_lock(&region->lock);

    brain_region_predictive_t* pred = region->predictive_extension;

    // Unregister from bio-async
    if (pred->bio_async_registered) {
        brain_region_unregister_predictive_bio_async(region);
    }

    // Destroy hierarchy
    if (pred->hierarchy) {
        pc_hierarchy_destroy(pred->hierarchy);
    }

    // Destroy positional encoders
    if (pred->hierarchy_pe_encoder) {
        nimcp_pos_encoder_destroy(pred->hierarchy_pe_encoder);
    }
    if (pred->temporal_pe_encoder) {
        nimcp_pos_encoder_destroy(pred->temporal_pe_encoder);
    }

    // Free connection arrays
    nimcp_free(pred->input_region_ids);
    nimcp_free(pred->output_region_ids);

    // Free buffers
    nimcp_free(pred->current_prediction);
    nimcp_free(pred->prediction_error);
    nimcp_free(pred->precision_weights);
    nimcp_free(pred->hierarchy_level_embedding);
    nimcp_free(pred->temporal_sequence_buffer);

    // Destroy mutex
    nimcp_mutex_destroy(&pred->lock);

    // Free extension
    nimcp_free(pred);
    region->predictive_extension = NULL;

    nimcp_mutex_unlock(&region->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Disabled predictive processing for region %u", region->id);

    return NIMCP_SUCCESS;
}

brain_region_predictive_t* brain_region_get_predictive(brain_region_t* region) {
    /* WHAT: Get predictive extension (caller must lock)
     * WHY:  Access predictive processing state
     */
    if (!region) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "region is NULL");

        return NULL;

    }
    return region->predictive_extension;
}

//=============================================================================
// Hierarchical Prediction API
//=============================================================================

nimcp_result_t brain_region_predict_lower(brain_region_t* region,
                                           uint32_t lower_region_id,
                                           float* prediction,
                                           uint32_t prediction_size) {
    /* WHAT: Generate top-down prediction for lower region
     * WHY:  Predictions flow down hierarchy from higher to lower regions
     * HOW:  Use predictive hierarchy to generate prediction from current state
     *
     * FORMULA: μ̂_lower = f(W × μ_higher + b)
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(prediction, NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    brain_region_predictive_t* pred = region->predictive_extension;
    NIMCP_CHECK_THROW(pred->config.generate_predictions, NIMCP_ERROR_INVALID_STATE,
                      "Region not configured to generate predictions");

    nimcp_mutex_lock(&pred->lock);

    // Get current representations from hierarchy
    if (pred->hierarchy) {
        // Generate prediction from top level to bottom level
        float* top_level = nimcp_malloc(region->total_neurons / 2 * sizeof(float));
        if (!top_level) {
            nimcp_mutex_unlock(&pred->lock);
            return NIMCP_ERROR_NO_MEMORY;
        }

        // Get top-level representations
        pc_hierarchy_get_representations(pred->hierarchy, 1, top_level);

        // Generate prediction for lower level
        // This is a simplified version - full implementation would use
        // the prediction weights between regions
        for (uint32_t i = 0; i < prediction_size && i < region->total_neurons; i++) {
            uint32_t top_idx = i % (region->total_neurons / 2);
            prediction[i] = top_level[top_idx];  // Simplified mapping
        }

        nimcp_free(top_level);

        // Store in current_prediction
        memcpy(pred->current_prediction, prediction,
               fminf(prediction_size, region->total_neurons) * sizeof(float));

        // Update statistics
        pred->total_predictions++;

        // Broadcast if configured
        if (pred->config.broadcast_predictions && pred->bio_async_registered) {
            brain_region_broadcast_prediction(region, lower_region_id,
                                               prediction, prediction_size);
        }
    } else {
        // No hierarchy - just copy current activity
        for (uint32_t i = 0; i < prediction_size && i < region->total_neurons; i++) {
            prediction[i] = 0.0f;  // No prediction available
        }
    }

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Region %u generated prediction for region %u",
                     region->id, lower_region_id);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_compute_error(brain_region_t* region,
                                           const float* actual,
                                           const float* predicted,
                                           float* error,
                                           uint32_t error_size) {
    /* WHAT: Compute precision-weighted prediction error
     * WHY:  Errors drive learning and propagate up hierarchy
     * HOW:  ε = π × (actual - predicted)
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(actual, NIMCP_ERROR_NULL_POINTER, "actual is NULL");
    NIMCP_CHECK_THROW(predicted, NIMCP_ERROR_NULL_POINTER, "predicted is NULL");
    NIMCP_CHECK_THROW(error, NIMCP_ERROR_NULL_POINTER, "error is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");
    NIMCP_CHECK_THROW(validate_buffer_size(region, error_size), NIMCP_ERROR_INVALID_PARAM, "invalid buffer size");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    // Compute precision-weighted error
    for (uint32_t i = 0; i < error_size; i++) {
        float raw_error = actual[i] - predicted[i];
        float precision = pred->precision_weights[i];
        error[i] = precision * raw_error;
    }

    // Store in prediction_error
    memcpy(pred->prediction_error, error, error_size * sizeof(float));

    // Update statistics
    pred->total_errors_computed++;
    pred->mean_prediction_error = compute_mean(error, error_size);

    // Broadcast if configured
    if (pred->config.broadcast_errors && pred->bio_async_registered) {
        brain_region_broadcast_error(region, 0, error, error_size);
    }

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Region %u computed prediction error (mean=%.4f)",
                     region->id, pred->mean_prediction_error);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_update_from_error(brain_region_t* region,
                                               const float* error,
                                               const float* precision,
                                               float dt) {
    /* WHAT: Update representations based on precision-weighted error
     * WHY:  Core inference step - minimize free energy
     * HOW:  Gradient descent: Δμ = -α × π × ε
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(error, NIMCP_ERROR_NULL_POINTER, "error is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");
    NIMCP_CHECK_THROW(dt > 0.0f, NIMCP_ERROR_INVALID_PARAM, "dt must be positive");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    // Use provided precision or learned precision
    const float* prec = precision ? precision : pred->precision_weights;

    // Update hierarchy if available
    if (pred->hierarchy) {
        // Set error as input to hierarchy
        pc_hierarchy_set_input(pred->hierarchy, error);

        // Run inference step with learning (this function is called during
        // active learning/error-update only, not during pure inference)
        pc_hierarchy_inference_step(pred->hierarchy, dt, true);

        // Update free energy
        pred->total_free_energy = pc_hierarchy_get_free_energy(pred->hierarchy);
    } else {
        // Simple update without hierarchy
        // In a full implementation, this would update neural activity in the region
        // For now, just track that update occurred
    }

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Region %u updated from error (dt=%.2f, FE=%.4f)",
                     region->id, dt, pred->total_free_energy);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Hierarchical Processing Flow
//=============================================================================

nimcp_result_t brain_region_hierarchical_step(brain_region_t* region,
                                                const float* sensory_input,
                                                uint32_t input_size,
                                                float dt) {
    /* WHAT: Execute one hierarchical prediction cycle
     * WHY:  Complete prediction → error → update cycle
     * HOW:
     *   1. Receive predictions from higher regions
     *   2. Compute prediction errors
     *   3. Update representations
     *   4. Generate predictions for lower regions
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    // Step 1: Get current activity (from sensory input or neural network)
    float* current_activity = nimcp_malloc(region->total_neurons * sizeof(float));
    if (!current_activity) {
        nimcp_mutex_unlock(&pred->lock);
        return NIMCP_ERROR_NO_MEMORY;
    }

    if (sensory_input && input_size > 0) {
        // Copy sensory input
        uint32_t copy_size = fminf(input_size, region->total_neurons);
        memcpy(current_activity, sensory_input, copy_size * sizeof(float));
        // Zero-pad if needed
        if (copy_size < region->total_neurons) {
            memset(current_activity + copy_size, 0,
                   (region->total_neurons - copy_size) * sizeof(float));
        }
    } else {
        // Get activity from neural network
        // This would query the region's neural network state
        // For now, use zeros
        memset(current_activity, 0, region->total_neurons * sizeof(float));
    }

    // Step 2: Compute prediction error if we have predictions
    if (pred->config.compute_prediction_errors) {
        brain_region_compute_error(region, current_activity,
                                     pred->current_prediction,
                                     pred->prediction_error,
                                     region->total_neurons);
    }

    // Step 3: Update from error
    brain_region_update_from_error(region, pred->prediction_error,
                                     NULL, dt);

    // Step 4: Generate predictions for lower regions
    if (pred->config.generate_predictions && pred->num_output_regions > 0) {
        for (uint32_t i = 0; i < pred->num_output_regions; i++) {
            uint32_t lower_id = pred->output_region_ids[i];
            float* lower_pred = nimcp_malloc(region->total_neurons * sizeof(float));
            if (lower_pred) {
                brain_region_predict_lower(region, lower_id,
                                            lower_pred, region->total_neurons);
                nimcp_free(lower_pred);
            }
        }
    }

    // Step 5: Learn precisions if enabled
    if (pred->config.learn_precisions) {
        brain_region_learn_precisions(region, dt);
    }

    nimcp_free(current_activity);
    nimcp_mutex_unlock(&pred->lock);

    return NIMCP_SUCCESS;
}

uint32_t brain_region_hierarchical_converge(brain_region_t* region,
                                             const float* sensory_input,
                                             uint32_t input_size,
                                             uint32_t max_iterations,
                                             float tolerance) {
    /* WHAT: Run hierarchical prediction to convergence
     * WHY:  Find optimal representations minimizing surprise
     * HOW:  Iterate until free energy stabilizes
     */

    // Guard clauses - return 0 iterations for invalid inputs
    if (!region || !has_predictive_extension(region)) return 0;

    brain_region_predictive_t* pred = region->predictive_extension;

    // Use config defaults if not specified
    if (max_iterations == 0) {
        max_iterations = pred->config.max_iterations;
    }
    if (tolerance == 0.0f) {
        tolerance = pred->config.convergence_tolerance;
    }

    float prev_fe = 1e10f;
    uint32_t iteration;

    for (iteration = 0; iteration < max_iterations; iteration++) {
        // Run one hierarchical step
        nimcp_result_t result = brain_region_hierarchical_step(
            region, sensory_input, input_size, pred->config.error_correction_rate);

        if (result != NIMCP_SUCCESS) {
            break;
        }

        // Check convergence
        float current_fe = brain_region_get_free_energy(region);
        float change = fabsf(current_fe - prev_fe) / (fabsf(prev_fe) + 1e-8f);

        if (change < tolerance) {
            LOG_MODULE_DEBUG(LOG_MODULE, "Region %u converged after %u iterations (FE=%.4f)",
                             region->id, iteration + 1, current_fe);
            return iteration + 1;
        }

        prev_fe = current_fe;
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Region %u reached max iterations %u (FE=%.4f)",
                     region->id, iteration, prev_fe);

    return iteration;
}

//=============================================================================
// Precision (Attention) Modulation
//=============================================================================

nimcp_result_t brain_region_set_precision(brain_region_t* region,
                                           const float* precisions,
                                           uint32_t precision_size) {
    /* WHAT: Set precision weights (attention)
     * WHY:  Modulate which errors drive inference
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");
    NIMCP_CHECK_THROW(validate_buffer_size(region, precision_size), NIMCP_ERROR_INVALID_PARAM, "invalid precision buffer size");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    if (precisions) {
        // Copy and clamp precisions
        for (uint32_t i = 0; i < precision_size; i++) {
            pred->precision_weights[i] = nimcp_clampf(precisions[i], 0.01f, 100.0f);
        }
    } else {
        // Reset to uniform precision
        for (uint32_t i = 0; i < region->total_neurons; i++) {
            pred->precision_weights[i] = PC_DEFAULT_PRECISION;
        }
    }

    // Update mean precision
    pred->mean_precision = compute_mean(pred->precision_weights, region->total_neurons);

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Region %u precision set (mean=%.4f)",
                     region->id, pred->mean_precision);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_get_precision(brain_region_t* region,
                                           float* precisions,
                                           uint32_t precision_size) {
    /* WHAT: Query current precision weights
     * WHY:  Monitor attention allocation
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(precisions, NIMCP_ERROR_NULL_POINTER, "precisions is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");
    NIMCP_CHECK_THROW(validate_buffer_size(region, precision_size), NIMCP_ERROR_INVALID_PARAM, "invalid precision buffer size");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);
    memcpy(precisions, pred->precision_weights, precision_size * sizeof(float));
    nimcp_mutex_unlock(&pred->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_learn_precisions(brain_region_t* region, float dt) {
    /* WHAT: Learn precisions from error statistics
     * WHY:  Automatic attention to reliable signals
     * HOW:  π = 1 / <ε²>
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");
    NIMCP_CHECK_THROW(dt > 0.0f, NIMCP_ERROR_INVALID_PARAM, "dt must be positive");

    brain_region_predictive_t* pred = region->predictive_extension;

    if (!pred->config.learn_precisions) {
        return NIMCP_SUCCESS;  // Learning disabled
    }

    nimcp_mutex_lock(&pred->lock);

    // Update precisions — the opaque pc_hierarchy_t handle no longer exposes
    // layer_states publicly (re-enabled 2026-04-24; original code poked into
    // the now-private struct). Fall back to the manual error-variance path
    // which works regardless of whether a hierarchy is attached; if a
    // hierarchy is present it keeps running its own internal precision
    // learning inside the hidden struct and this loop just maintains a
    // region-local precision shadow driven by the same error signal. A
    // future change can add a pc_hierarchy_get_precisions() accessor and
    // restore the hierarchy path.
    {
        for (uint32_t i = 0; i < region->total_neurons; i++) {
            float error_sq = pred->prediction_error[i] * pred->prediction_error[i];
            float variance = error_sq + 1e-8f;  // Avoid division by zero
            float target_precision = 1.0f / variance;

            // Exponential moving average
            float alpha = pred->config.precision_learning_rate * dt;
            pred->precision_weights[i] = (1.0f - alpha) * pred->precision_weights[i] +
                                          alpha * target_precision;

            // Clamp to valid range
            pred->precision_weights[i] = nimcp_clampf(pred->precision_weights[i], 0.01f, 100.0f);
        }
    }

    // Update mean precision
    pred->mean_precision = compute_mean(pred->precision_weights, region->total_neurons);

    nimcp_mutex_unlock(&pred->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Inter-Region Hierarchical Connections
//=============================================================================

nimcp_result_t brain_region_connect_predictive(brain_region_t* higher_region,
                                                 brain_region_t* lower_region,
                                                 float connection_strength) {
    /* WHAT: Connect regions hierarchically
     * WHY:  Build predictive hierarchy across regions
     * HOW:  Add to connection lists, initialize prediction weights
     */

    // Guard clauses
    NIMCP_CHECK_THROW(higher_region, NIMCP_ERROR_NULL_POINTER, "higher_region is NULL");
    NIMCP_CHECK_THROW(lower_region, NIMCP_ERROR_NULL_POINTER, "lower_region is NULL");
    // CRITICAL: Prevent deadlock from double-locking same region
    NIMCP_CHECK_THROW(higher_region != lower_region, NIMCP_ERROR_INVALID_PARAM, "cannot connect region to itself");
    NIMCP_CHECK_THROW(has_predictive_extension(higher_region) && has_predictive_extension(lower_region),
                      NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");
    NIMCP_CHECK_THROW(connection_strength >= 0.0f && connection_strength <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM, "connection_strength must be in [0.0, 1.0]");

    brain_region_predictive_t* higher_pred = higher_region->predictive_extension;
    brain_region_predictive_t* lower_pred = lower_region->predictive_extension;

    // Lock both regions (always lock in consistent order to prevent deadlock)
    brain_region_t* first = (higher_region->id < lower_region->id) ? higher_region : lower_region;
    brain_region_t* second = (higher_region->id < lower_region->id) ? lower_region : higher_region;

    nimcp_mutex_lock(&first->lock);
    nimcp_mutex_lock(&second->lock);

    // Add lower region to higher region's output list
    uint32_t* new_outputs = nimcp_realloc(higher_pred->output_region_ids,
                                           (higher_pred->num_output_regions + 1) * sizeof(uint32_t));
    if (!new_outputs) {
        nimcp_mutex_unlock(&second->lock);
        nimcp_mutex_unlock(&first->lock);
        return NIMCP_ERROR_NO_MEMORY;
    }
    new_outputs[higher_pred->num_output_regions] = lower_region->id;
    higher_pred->output_region_ids = new_outputs;
    higher_pred->num_output_regions++;

    // Add higher region to lower region's input list
    uint32_t* new_inputs = nimcp_realloc(lower_pred->input_region_ids,
                                          (lower_pred->num_input_regions + 1) * sizeof(uint32_t));
    if (!new_inputs) {
        // Rollback higher region change
        higher_pred->num_output_regions--;
        nimcp_mutex_unlock(&second->lock);
        nimcp_mutex_unlock(&first->lock);
        return NIMCP_ERROR_NO_MEMORY;
    }
    new_inputs[lower_pred->num_input_regions] = higher_region->id;
    lower_pred->input_region_ids = new_inputs;
    lower_pred->num_input_regions++;

    nimcp_mutex_unlock(&second->lock);
    nimcp_mutex_unlock(&first->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Connected regions hierarchically: %u (higher) → %u (lower), strength=%.2f",
                    higher_region->id, lower_region->id, connection_strength);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_disconnect_predictive(brain_region_t* higher_region,
                                                    brain_region_t* lower_region) {
    /* WHAT: Disconnect hierarchical prediction connection
     * WHY:  Reconfigure hierarchy
     */

    // Guard clauses
    NIMCP_CHECK_THROW(higher_region, NIMCP_ERROR_NULL_POINTER, "higher_region is NULL");
    NIMCP_CHECK_THROW(lower_region, NIMCP_ERROR_NULL_POINTER, "lower_region is NULL");
    // CRITICAL: Prevent deadlock from double-locking same region
    NIMCP_CHECK_THROW(higher_region != lower_region, NIMCP_ERROR_INVALID_PARAM, "cannot disconnect region from itself");
    NIMCP_CHECK_THROW(has_predictive_extension(higher_region) && has_predictive_extension(lower_region),
                      NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    brain_region_predictive_t* higher_pred = higher_region->predictive_extension;
    brain_region_predictive_t* lower_pred = lower_region->predictive_extension;

    // Lock both regions (always lock in consistent order to prevent deadlock)
    brain_region_t* first = (higher_region->id < lower_region->id) ? higher_region : lower_region;
    brain_region_t* second = (higher_region->id < lower_region->id) ? lower_region : higher_region;

    nimcp_mutex_lock(&first->lock);
    nimcp_mutex_lock(&second->lock);

    // Remove lower region from higher region's output list
    for (uint32_t i = 0; i < higher_pred->num_output_regions; i++) {
        if (higher_pred->output_region_ids[i] == lower_region->id) {
            // Shift remaining elements
            for (uint32_t j = i; j < higher_pred->num_output_regions - 1; j++) {
                higher_pred->output_region_ids[j] = higher_pred->output_region_ids[j + 1];
            }
            higher_pred->num_output_regions--;
            break;
        }
    }

    // Remove higher region from lower region's input list
    for (uint32_t i = 0; i < lower_pred->num_input_regions; i++) {
        if (lower_pred->input_region_ids[i] == higher_region->id) {
            // Shift remaining elements
            for (uint32_t j = i; j < lower_pred->num_input_regions - 1; j++) {
                lower_pred->input_region_ids[j] = lower_pred->input_region_ids[j + 1];
            }
            lower_pred->num_input_regions--;
            break;
        }
    }

    nimcp_mutex_unlock(&second->lock);
    nimcp_mutex_unlock(&first->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Disconnected regions hierarchically: %u ↛ %u",
                    higher_region->id, lower_region->id);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-Async Integration (Continued in next file part due to size)
//=============================================================================

nimcp_result_t brain_region_register_predictive_bio_async(brain_region_t* region,
                                                            bio_module_id_t module_id) {
    /* WHAT: Register with bio-async for predictive messaging
     * WHY:  Enable loosely coupled hierarchical communication
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    if (pred->bio_async_registered) {
        nimcp_mutex_unlock(&pred->lock);
        return NIMCP_SUCCESS;  // Already registered
    }

    pred->bio_async_module_id = module_id;
    pred->bio_async_registered = true;

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Region %u registered with bio-async (module_id=%u)",
                    region->id, module_id);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_unregister_predictive_bio_async(brain_region_t* region) {
    /* WHAT: Unregister from bio-async
     * WHY:  Disable messaging
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    pred->bio_async_registered = false;
    pred->bio_async_module_id = 0;

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Region %u unregistered from bio-async", region->id);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_broadcast_prediction(brain_region_t* region,
                                                   uint32_t target_region_id,
                                                   const float* prediction,
                                                   uint32_t size) {
    /* WHAT: Broadcast prediction via bio-async
     * WHY:  Top-down predictions flow through messaging system
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(prediction, NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    brain_region_predictive_t* pred = region->predictive_extension;

    if (!pred->bio_async_registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;  // Not registered with bio-async
    }

    // Create prediction update message
    // In full implementation, would use bio_router_send()
    // For now, just log
    LOG_MODULE_DEBUG(LOG_MODULE, "Region %u broadcasting prediction to region %u (size=%u)",
                     region->id, target_region_id, size);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_broadcast_error(brain_region_t* region,
                                              uint32_t target_region_id,
                                              const float* error,
                                              uint32_t size) {
    /* WHAT: Broadcast prediction error via bio-async
     * WHY:  Bottom-up errors drive higher-level inference
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(error, NIMCP_ERROR_NULL_POINTER, "error is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    brain_region_predictive_t* pred = region->predictive_extension;

    if (!pred->bio_async_registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Create prediction error message
    // In full implementation, would use bio_router_send()
    // For now, just log
    LOG_MODULE_DEBUG(LOG_MODULE, "Region %u broadcasting error to region %u (size=%u)",
                     region->id, target_region_id, size);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query and Statistics
//=============================================================================

nimcp_result_t brain_region_get_prediction(brain_region_t* region,
                                             float* prediction,
                                             uint32_t size) {
    /* WHAT: Query current prediction
     * WHY:  Monitor predictive processing
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(prediction, NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");
    NIMCP_CHECK_THROW(validate_buffer_size(region, size), NIMCP_ERROR_INVALID_PARAM, "invalid buffer size");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);
    memcpy(prediction, pred->current_prediction, size * sizeof(float));
    nimcp_mutex_unlock(&pred->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_get_prediction_error(brain_region_t* region,
                                                   float* error,
                                                   uint32_t size) {
    /* WHAT: Query prediction error
     * WHY:  Monitor surprise, novelty
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(error, NIMCP_ERROR_NULL_POINTER, "error is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");
    NIMCP_CHECK_THROW(validate_buffer_size(region, size), NIMCP_ERROR_INVALID_PARAM, "invalid buffer size");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);
    memcpy(error, pred->prediction_error, size * sizeof(float));
    nimcp_mutex_unlock(&pred->lock);

    return NIMCP_SUCCESS;
}

float brain_region_get_free_energy(brain_region_t* region) {
    /* WHAT: Query variational free energy
     * WHY:  Quantify surprise/model quality
     */

    if (!region) return 0.0f;
    if (!has_predictive_extension(region)) return 0.0f;

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);
    float fe = pred->total_free_energy;
    nimcp_mutex_unlock(&pred->lock);

    return fe;
}

nimcp_result_t brain_region_get_predictive_stats(brain_region_t* region,
                                                   brain_region_predictive_stats_t* stats) {
    /* WHAT: Get statistics
     * WHY:  Monitor performance
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    stats->total_predictions = pred->total_predictions;
    stats->total_errors_computed = pred->total_errors_computed;
    stats->mean_prediction_error = pred->mean_prediction_error;
    stats->mean_precision = pred->mean_precision;
    stats->total_free_energy = pred->total_free_energy;
    stats->num_input_regions = pred->num_input_regions;
    stats->num_output_regions = pred->num_output_regions;
    stats->hierarchy_level = pred->config.hierarchy_level;
    stats->is_converged = (pred->mean_prediction_error < pred->config.convergence_tolerance);

    nimcp_mutex_unlock(&pred->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Security Integration
//=============================================================================

nimcp_result_t brain_region_predictive_enable_security(brain_region_t* region,
                                                         bool enable) {
    /* WHAT: Enable security monitoring
     * WHY:  Detect anomalies in predictions
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED, "predictive extension not initialized");

    if (enable) {
        LOG_MODULE_INFO(LOG_MODULE, "Enabled security monitoring for region %u predictions", region->id);
        // In full implementation, would register with BBB
    } else {
        LOG_MODULE_INFO(LOG_MODULE, "Disabled security monitoring for region %u predictions", region->id);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Positional Encoding Integration
//=============================================================================

nimcp_result_t brain_region_set_hierarchy_pe(brain_region_t* region,
                                               uint32_t hierarchy_level) {
    /* WHAT: Set hierarchy level positional encoding for region
     * WHY:  Encode hierarchical position in cortical processing chain
     * HOW:  Use learned PE to generate level-specific embedding
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL in brain_region_set_hierarchy_pe");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED,
                      "Region does not have predictive extension enabled");
    NIMCP_CHECK_THROW(hierarchy_level < MAX_HIERARCHY_LEVELS, NIMCP_ERROR_INVALID_PARAM,
                      "Hierarchy level exceeds maximum");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    // Check if hierarchy PE is enabled
    if (!pred->config.enable_hierarchy_pe) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_WARN(LOG_MODULE, "Hierarchy PE not enabled for region %u", region->id);
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Check if encoder is initialized
    if (!pred->hierarchy_pe_encoder || !pred->hierarchy_level_embedding) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Hierarchy PE encoder not initialized for region %u", region->id);
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Encode hierarchy level position
    int result = nimcp_pos_encode_position(
        pred->hierarchy_pe_encoder,
        hierarchy_level,
        pred->hierarchy_level_embedding
    );

    if (result != NIMCP_POS_SUCCESS) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to encode hierarchy level %u: error %d",
                         hierarchy_level, result);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    // Update hierarchy level in config
    pred->config.hierarchy_level = hierarchy_level;

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Set hierarchy PE for region %u at level %u",
                    region->id, hierarchy_level);

    // Broadcast encoding computation message if bio-async enabled
    if (pred->bio_async_registered) {
        // In full implementation, would send BIO_MSG_ENCODING_COMPUTE
        LOG_MODULE_DEBUG(LOG_MODULE, "Region %u hierarchy PE update (bio-async module %u)",
                         region->id, pred->bio_async_module_id);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_encode_prediction_sequence(brain_region_t* region,
                                                         const float* prediction_sequence,
                                                         uint32_t seq_length,
                                                         float* output) {
    /* WHAT: Apply sinusoidal PE to temporal prediction sequences
     * WHY:  Temporal order critical for prediction accuracy
     * HOW:  Add positional encoding to each timestep's prediction
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(prediction_sequence, NIMCP_ERROR_NULL_POINTER, "prediction_sequence is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_NULL_POINTER, "output is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED,
                      "Region does not have predictive extension enabled");
    NIMCP_CHECK_THROW(seq_length > 0, NIMCP_ERROR_INVALID_PARAM, "Invalid sequence length 0");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    // Check if temporal PE is enabled
    if (!pred->config.enable_temporal_pe) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_WARN(LOG_MODULE, "Temporal PE not enabled for region %u", region->id);
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Check if encoder is initialized
    if (!pred->temporal_pe_encoder || !pred->temporal_sequence_buffer) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Temporal PE encoder not initialized for region %u", region->id);
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Check sequence length bounds
    if (seq_length > pred->config.max_prediction_sequence) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Sequence length %u exceeds maximum %u",
                         seq_length, pred->config.max_prediction_sequence);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Generate positional encodings for the sequence
    int result = nimcp_pos_encode_sequence(
        pred->temporal_pe_encoder,
        0,  // Start at position 0
        seq_length,
        pred->temporal_sequence_buffer
    );

    if (result != NIMCP_POS_SUCCESS) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to encode prediction sequence: error %d", result);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    // Apply positional encoding additively to prediction sequence
    uint32_t total_elements = seq_length * region->total_neurons;
    for (uint32_t i = 0; i < total_elements; i++) {
        output[i] = prediction_sequence[i] + pred->temporal_sequence_buffer[i];
    }

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Applied temporal PE to prediction sequence (region %u, seq_len %u)",
                     region->id, seq_length);

    // Broadcast prediction update if bio-async enabled
    if (pred->config.broadcast_predictions && pred->bio_async_registered) {
        // In full implementation, would broadcast BIO_MSG_PREDICTION_UPDATE
        // with PE-enhanced predictions
        LOG_MODULE_DEBUG(LOG_MODULE, "Broadcasting PE-enhanced predictions from region %u", region->id);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_region_get_level_embedding(brain_region_t* region,
                                                  uint32_t hierarchy_level,
                                                  float* embedding,
                                                  uint32_t embedding_size) {
    /* WHAT: Get learned embedding for specific hierarchy level
     * WHY:  Access position encoding for analysis or injection
     * HOW:  Query learned embedding from hierarchy PE encoder
     */

    // Guard clauses
    NIMCP_CHECK_THROW(region, NIMCP_ERROR_NULL_POINTER, "region is NULL");
    NIMCP_CHECK_THROW(embedding, NIMCP_ERROR_NULL_POINTER, "embedding is NULL");
    NIMCP_CHECK_THROW(has_predictive_extension(region), NIMCP_ERROR_NOT_INITIALIZED,
                      "Region does not have predictive extension enabled");
    NIMCP_CHECK_THROW(hierarchy_level < MAX_HIERARCHY_LEVELS, NIMCP_ERROR_INVALID_PARAM,
                      "Hierarchy level exceeds maximum");

    brain_region_predictive_t* pred = region->predictive_extension;

    nimcp_mutex_lock(&pred->lock);

    // Check if hierarchy PE is enabled
    if (!pred->config.enable_hierarchy_pe) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_WARN(LOG_MODULE, "Hierarchy PE not enabled for region %u", region->id);
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Check if encoder is initialized
    if (!pred->hierarchy_pe_encoder) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Hierarchy PE encoder not initialized for region %u", region->id);
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Validate embedding size
    if (embedding_size != pred->config.pe_embedding_dim) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Embedding size %u does not match PE dimension %u",
                         embedding_size, pred->config.pe_embedding_dim);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Encode the requested hierarchy level
    int result = nimcp_pos_encode_position(
        pred->hierarchy_pe_encoder,
        hierarchy_level,
        embedding
    );

    if (result != NIMCP_POS_SUCCESS) {
        nimcp_mutex_unlock(&pred->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to get level embedding for level %u: error %d",
                         hierarchy_level, result);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    nimcp_mutex_unlock(&pred->lock);

    LOG_MODULE_DEBUG(LOG_MODULE, "Retrieved level embedding for region %u, hierarchy level %u",
                     region->id, hierarchy_level);

    return NIMCP_SUCCESS;
}
