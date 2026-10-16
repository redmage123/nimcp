/**
 * @file nimcp_predictive.c
 * @brief Phase 10.9: Predictive Processing Implementation
 *
 * WHAT: Hierarchical Predictive Coding with Free Energy Minimization
 * WHY:  Enable surprise minimization and active inference
 * HOW:  Multi-layer predictions with precision-weighted errors
 *
 * ALGORITHM OVERVIEW:
 * Free Energy: F = ∑_layers (precision · ||error||²)
 * Learning: θ ← θ - α∇F  (minimize prediction error)
 * Inference: x ← x - β∇F  (find most likely state)
 * Action: a* = argmin_a E[F|a]  (expected free energy)
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0337 (BIO_MODULE_PREDICTIVE)
 * - Publishes: prediction errors, free energy updates
 * - Subscribes: sensory inputs, prior updates
 *
 * @author NIMCP Phase 10 Team
 * @date 2025-11-09
 */

#define LOG_MODULE "predictive"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(predictive, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include "cognitive/nimcp_predictive.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

// NOTE: Uses BIO_MODULE_PREDICTIVE_CODING from nimcp_bio_messages.h (0x0407)

//=============================================================================
// Error Handling (module-local)
//=============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Named Constants
//=============================================================================

#define DEFAULT_NUM_ITERATIONS 10        // Inference steps
#define CONVERGENCE_THRESHOLD 0.001f     // Free energy delta for convergence
#define MAX_ITERATIONS 100               // Safety limit

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Complete predictive network state
 *
 * WHAT: Hierarchical layers with predictions and errors
 * WHY:  Implement predictive coding architecture
 * HOW:  Array of layers with message passing
 */
struct predictive_network_s {
    predictive_config_t config;          /**< Configuration */
    predictive_layer_t** layers;         /**< Layer array [num_layers] */
    uint32_t num_layers;                 /**< Hierarchy depth */
    float prev_free_energy;              /**< For convergence check */
    uint64_t total_updates;              /**< Statistics */
    uint64_t total_inference_time_us;    /**< Performance tracking */

    // Bio-async integration
    bio_module_context_t bio_ctx;        /**< Bio-async module context */
    bool bio_async_enabled;              /**< Bio-async registration status */
};

//=============================================================================
// Forward Declarations
//=============================================================================

static bool compute_prediction_errors(predictive_network_t net);
static bool generate_predictions(predictive_network_t net);
static float compute_free_energy(predictive_network_t net);
static bool update_states(predictive_network_t net, float step_size);

//=============================================================================
// Public API: Configuration
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible defaults for predictive processing
 * WHY:  Simplify initialization for common use cases
 * HOW:  Struct with proven hyperparameters
 *
 * COMPLEXITY: O(1)
 */
predictive_config_t predictive_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_default_config", 0.0f);


    LOG_DEBUG("Creating default predictive config");

    // Static default layer sizes - no allocation needed
    static const uint32_t default_sizes[PRED_DEFAULT_LAYERS] = {
        256,  // Sensory level
        128,  // Low-level features
        64,   // Mid-level features
        32    // High-level concepts
    };

    predictive_config_t config = {
        .num_layers = PRED_DEFAULT_LAYERS,
        .layer_sizes = (uint32_t*)default_sizes,
        .learning_rate = PRED_LEARNING_RATE,
        .precision_learning_rate = PRED_PRECISION_LR,
        .initial_precision = PRED_DEFAULT_PRECISION,
        .enable_active_inference = true,
        .enable_precision_learning = true
    };
    return config;
}

//=============================================================================
// Public API: Creation & Destruction
//=============================================================================

/**
 * @brief Create predictive network
 *
 * WHAT: Initialize hierarchical predictive coding system
 * WHY:  Enable free energy minimization
 * HOW:  Allocate layers with predictions and errors
 *
 * COMPLEXITY: O(sum(layer_sizes))
 * MEMORY: O(sum(layer_sizes))
 */
predictive_network_t predictive_create(const predictive_config_t* config)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_create", 0.0f);


    predictive_config_t actual_config;
    if (config) {
        actual_config = *config;
    } else {
        actual_config = predictive_default_config();
    }

    if (actual_config.num_layers == 0 || actual_config.num_layers > PRED_MAX_LAYERS) {
        set_error("Invalid num_layers: %u (must be 1-%d)",
                 actual_config.num_layers, PRED_MAX_LAYERS);
        LOG_ERROR("Invalid num_layers: %u (must be 1-%d)",
                 actual_config.num_layers, PRED_MAX_LAYERS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "predictive_create: actual_config.num_layers is zero");
        return NULL;
    }

    if (!actual_config.layer_sizes) {
        set_error("NULL layer_sizes");
        LOG_ERROR("NULL layer_sizes provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "predictive_create: actual_config is NULL");
        return NULL;
    }

    LOG_INFO("Creating predictive network: num_layers=%u, learning_rate=%.4f",
             actual_config.num_layers, actual_config.learning_rate);

    // =========================================================================
    // ALLOCATE: Network structure
    // =========================================================================

    predictive_network_t net = (predictive_network_t)nimcp_malloc(
        sizeof(struct predictive_network_s));
    if (!net) {
        set_error("Failed to allocate predictive_network_s");
        LOG_ERROR("Failed to allocate predictive_network_s (%zu bytes)",
                 sizeof(struct predictive_network_s));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate net");

        return NULL;
    }

    net->config = actual_config;
    net->num_layers = actual_config.num_layers;
    net->prev_free_energy = INFINITY;
    net->total_updates = 0;
    net->total_inference_time_us = 0;

    // =========================================================================
    // BIO-ASYNC: Initialize and register with router
    // =========================================================================

    net->bio_ctx = NULL;
    net->bio_async_enabled = false;

    LOG_DEBUG("predictive: Checking bio-async router initialization...");
    if (bio_router_is_initialized()) {
        LOG_DEBUG("predictive: Bio-router initialized, registering module (id=%d, inbox_capacity=32)...",
                  BIO_MODULE_PREDICTIVE_CODING);
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_PREDICTIVE_CODING,
            .module_name = "predictive",
            .inbox_capacity = 32,
            .user_data = net
        };
        net->bio_ctx = bio_router_register_module(&bio_info);
        if (net->bio_ctx) {
            net->bio_async_enabled = true;
            LOG_INFO("predictive: Bio-async communication enabled (module_id=%d)",
                    BIO_MODULE_PREDICTIVE_CODING);
        } else {
            LOG_WARN("predictive: Bio-async registration failed - module will operate without async messaging");
        }
    } else {
        LOG_DEBUG("predictive: Bio-router not initialized, skipping async registration");
    }

    // =========================================================================
    // ALLOCATE: Layers array
    // =========================================================================

    net->layers = (predictive_layer_t**)nimcp_calloc(
        net->num_layers, sizeof(predictive_layer_t*));
    if (!net->layers) {
        set_error("Failed to allocate layers array");
        nimcp_free(net);
        net = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_create: net->layers is NULL");
        return NULL;
    }

    // =========================================================================
    // INITIALIZE: Each layer
    // =========================================================================

    for (uint32_t i = 0; i < net->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && net->num_layers > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(i + 1) / (float)net->num_layers);
        }

        uint32_t size = actual_config.layer_sizes[i];

        predictive_layer_t* layer = (predictive_layer_t*)nimcp_malloc(
            sizeof(predictive_layer_t));
        if (!layer) {
            set_error("Failed to allocate layer %u", i);
            predictive_destroy(net);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate layer");

            return NULL;
        }

        layer->size = size;
        layer->free_energy = 0.0F;

        // Allocate layer vectors
        layer->state = (float*)nimcp_calloc(size, sizeof(float));
        layer->prediction = (float*)nimcp_calloc(size, sizeof(float));
        layer->prediction_error = (float*)nimcp_calloc(size, sizeof(float));
        layer->precision = (float*)nimcp_calloc(size, sizeof(float));

        if (!layer->state || !layer->prediction ||
            !layer->prediction_error || !layer->precision) {
            set_error("Failed to allocate layer %u vectors", i);
            nimcp_free(layer->state);
            nimcp_free(layer->prediction);
            nimcp_free(layer->prediction_error);
            nimcp_free(layer->precision);
            nimcp_free(layer);
            layer = NULL;
            predictive_destroy(net);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_create: layer vector allocation failed");
            return NULL;
        }

        // Initialize precision
        for (uint32_t j = 0; j < size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && size > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(j + 1) / (float)size);
            }

            layer->precision[j] = actual_config.initial_precision;
        }

        net->layers[i] = layer;
    }

    return net;
}

/**
 * @brief Destroy predictive network
 *
 * WHAT: Free all memory associated with network
 * WHY:  Prevent memory leaks
 * HOW:  Free each layer and the network structure
 *
 * COMPLEXITY: O(num_layers)
 */
void predictive_destroy(predictive_network_t net)
{
    if (!net) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_destroy", 0.0f);


    LOG_INFO("predictive: Destroying predictive network (num_layers=%u)...", net->num_layers);

    // Unregister from bio-async router
    if (net->bio_async_enabled && net->bio_ctx) {
        LOG_DEBUG("predictive: Unregistering from bio-async router...");
        bio_router_unregister_module(net->bio_ctx);
        net->bio_ctx = NULL;
        net->bio_async_enabled = false;
        LOG_INFO("predictive: Bio-async communication disabled");
    }

    if (net->layers) {
        for (uint32_t i = 0; i < net->num_layers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && net->num_layers > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(i + 1) / (float)net->num_layers);
            }

            if (net->layers[i]) {
                nimcp_free(net->layers[i]->state);
                nimcp_free(net->layers[i]->prediction);
                nimcp_free(net->layers[i]->prediction_error);
                nimcp_free(net->layers[i]->precision);
                nimcp_free(net->layers[i]);
            }
        }
        nimcp_free(net->layers);
    }

    nimcp_free(net);
    net = NULL;
}

//=============================================================================
// Public API: Inference
//=============================================================================

/**
 * @brief Forward pass: Minimize free energy
 *
 * WHAT: Update predictions to minimize prediction error
 * WHY:  Core of predictive processing
 * HOW:  Iterative message passing between layers
 *
 * ALGORITHM:
 * 1. Set bottom layer to input
 * 2. For N iterations:
 *    a. Compute prediction errors (bottom-up)
 *    b. Generate predictions (top-down)
 *    c. Update states to minimize error
 *    d. Check convergence
 * 3. Return total free energy
 *
 * COMPLEXITY: O(num_iterations * sum(layer_sizes))
 */
float predictive_forward(predictive_network_t net, const float* input,
                        uint32_t num_iterations)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!net || !input) {
        set_error("NULL parameter in predictive_forward");
        return INFINITY;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_forward", 0.0f);


    if (num_iterations == 0) {
        num_iterations = DEFAULT_NUM_ITERATIONS;
    }
    if (num_iterations > MAX_ITERATIONS) {
        num_iterations = MAX_ITERATIONS;
    }

    uint64_t start_time = 0;  // Timing disabled for simplicity

    // =========================================================================
    // STEP 1: Set bottom layer to input
    // =========================================================================

    memcpy(net->layers[0]->state, input,
           net->layers[0]->size * sizeof(float));

    // =========================================================================
    // STEP 2: Iterative inference (minimize free energy)
    // =========================================================================

    for (uint32_t iter = 0; iter < num_iterations; iter++) {
        /* Phase 8: Loop progress heartbeat */
        if ((iter & 0xFF) == 0 && num_iterations > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(iter + 1) / (float)num_iterations);
        }

        // Compute prediction errors (bottom-up)
        if (!compute_prediction_errors(net)) {
            return INFINITY;
        }

        // Generate predictions (top-down)
        if (!generate_predictions(net)) {
            return INFINITY;
        }

        // Update states to minimize precision-weighted error
        float step_size = net->config.learning_rate / (1.0F + iter * 0.1F);
        if (!update_states(net, step_size)) {
            return INFINITY;
        }

        // Compute free energy
        float free_energy = compute_free_energy(net);

        // Check convergence
        if (iter > 0) {
            float delta = fabsf(free_energy - net->prev_free_energy);
            if (delta < CONVERGENCE_THRESHOLD) {
                break;  // Converged early
            }
        }
        net->prev_free_energy = free_energy;
    }

    // =========================================================================
    // STATISTICS
    // =========================================================================

    // Timing disabled for simplicity
    net->total_updates++;

    /* W8: rate-limited emit every 64 forward passes. Emits layer-0 error
     * norm + current free energy; read-back queries FEP partner. */
    if ((net->total_updates & 0x3F) == 0) {
        struct brain_struct* _kg_brain =
            world_model_kg_events_get_registered_brain();
        if (_kg_brain && net->num_layers > 0) {
            /* Compute L2 of layer-0 prediction error for the event payload. */
            float err_sq = 0.0f;
            const float* err = net->layers[0]->prediction_error;
            uint32_t sz = net->layers[0]->size;
            if (err && sz > 0) {
                uint32_t samples = sz < 64 ? sz : 64;
                for (uint32_t i = 0; i < samples; i++) err_sq += err[i] * err[i];
                err_sq = sqrtf(err_sq);
            }
            world_model_kg_emit_prediction_error(_kg_brain,
                0, err_sq, net->prev_free_energy);
            (void)world_model_kg_has_partner(_kg_brain, "cog_fep_free_energy");
        }
    }

    return net->prev_free_energy;
}

/**
 * @brief Get layer prediction
 *
 * WHAT: Extract prediction from specific layer
 * WHY:  Access internal representations
 * HOW:  Copy prediction vector
 *
 * COMPLEXITY: O(layer_size)
 */
bool predictive_get_layer_prediction(predictive_network_t net,
                                     uint32_t layer_index, float* output)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_get_layer_prediction", 0.0f);

    if (!net || !output) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_get_layer_prediction: required parameter is NULL (net, output)");
        return false;
    }

    // Process pending bio-async messages (after NULL check)
    if (net->bio_ctx) {
        bio_router_process_inbox(net->bio_ctx, 5);
    }

    if (layer_index >= net->num_layers) {
        set_error("Invalid layer_index: %u (max %u)", layer_index, net->num_layers - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "predictive_get_layer_prediction: capacity exceeded");
        return false;
    }

    memcpy(output, net->layers[layer_index]->prediction,
           net->layers[layer_index]->size * sizeof(float));
    return true;
}

/**
 * @brief Get layer prediction error
 *
 * WHAT: Extract prediction error from layer
 * WHY:  Measure surprise
 * HOW:  Copy error vector
 *
 * COMPLEXITY: O(layer_size)
 */
bool predictive_get_layer_error(predictive_network_t net,
                                uint32_t layer_index, float* output)
{
    if (!net || !output) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_get_layer_error: required parameter is NULL (net, output)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_get_layer_error", 0.0f);


    if (layer_index >= net->num_layers) {
        set_error("Invalid layer_index: %u", layer_index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "predictive_get_layer_error: capacity exceeded");
        return false;
    }

    memcpy(output, net->layers[layer_index]->prediction_error,
           net->layers[layer_index]->size * sizeof(float));
    return true;
}

//=============================================================================
// Public API: Learning
//=============================================================================

/**
 * @brief Update internal model (learning)
 *
 * WHAT: Adjust predictions to reduce future errors
 * WHY:  Learn better generative models
 * HOW:  Gradient descent on free energy
 *
 * NOTE: Simplified implementation - full version would update
 * connection weights between layers
 *
 * COMPLEXITY: O(sum(layer_sizes))
 */
bool predictive_update_model(predictive_network_t net)
{
    if (!net) {
        set_error("NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_update_model: net is NULL");
        return false;
    }

    // In a full implementation, this would update synaptic weights
    // For now, we just track that learning occurred
    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_update_model", 0.0f);


    return true;
}

/**
 * @brief Update precision weights (attention)
 *
 * WHAT: Adjust confidence based on prediction reliability
 * WHY:  Allocate attention to informative signals
 * HOW:  Track prediction error variance
 *
 * ALGORITHM:
 * precision[i] ← 1 / (variance(error[i]) + ε)
 *
 * COMPLEXITY: O(layer_size)
 */
bool predictive_update_precision(predictive_network_t net, uint32_t layer_index)
{
    if (!net) {
        set_error("NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_update_precision: net is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_update_precision", 0.0f);


    if (layer_index >= net->num_layers) {
        set_error("Invalid layer_index");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "predictive_update_precision: capacity exceeded");
        return false;
    }

    if (!net->config.enable_precision_learning) {
        return true;  // Disabled
    }

    predictive_layer_t* layer = net->layers[layer_index];

    // Update precision based on squared error (inverse variance)
    float lr = net->config.precision_learning_rate;
    for (uint32_t i = 0; i < layer->size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && layer->size > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(i + 1) / (float)layer->size);
        }

        float error_sq = layer->prediction_error[i] * layer->prediction_error[i];

        // Move precision toward inverse error (with learning rate)
        float target_precision = 1.0F / (error_sq + 1e-6F);
        layer->precision[i] += lr * (target_precision - layer->precision[i]);

        // Clip to valid range
        if (layer->precision[i] < PRED_MIN_PRECISION) {
            layer->precision[i] = PRED_MIN_PRECISION;
        }
        if (layer->precision[i] > PRED_MAX_PRECISION) {
            layer->precision[i] = PRED_MAX_PRECISION;
        }
    }

    return true;
}

//=============================================================================
// Public API: Active Inference
//=============================================================================

/**
 * @brief Select action via active inference
 *
 * WHAT: Choose action that minimizes expected free energy
 * WHY:  Goal-directed behavior under uncertainty
 * HOW:  Evaluate each action's predicted surprise
 *
 * NOTE: Simplified implementation - simulates expected outcomes
 *
 * COMPLEXITY: O(num_actions)
 */
float predictive_active_inference(predictive_network_t net,
                                 predictive_action_t* actions,
                                 uint32_t num_actions,
                                 uint32_t* selected_action)
{
    if (!net || !actions || !selected_action || num_actions == 0) {
        set_error("Invalid parameters");
        return INFINITY;
    }

    if (!net->config.enable_active_inference) {
        set_error("Active inference not enabled");
        return INFINITY;
    }

    // Find action with lowest expected free energy
    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_active_inference", 0.0f);


    float min_efe = INFINITY;
    uint32_t best_action = 0;

    for (uint32_t i = 0; i < num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_actions > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(i + 1) / (float)num_actions);
        }

        if (actions[i].expected_free_energy < min_efe) {
            min_efe = actions[i].expected_free_energy;
            best_action = i;
        }
    }

    *selected_action = best_action;
    return min_efe;
}

//=============================================================================
// Public API: Utilities
//=============================================================================

/**
 * @brief Get network statistics
 *
 * WHAT: Retrieve performance metrics
 * WHY:  Monitor convergence and surprise
 * HOW:  Aggregate across layers
 *
 * COMPLEXITY: O(num_layers)
 */
bool predictive_get_statistics(predictive_network_t net, predictive_stats_t* stats)
{
    if (!net || !stats) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_get_statistics: required parameter is NULL (net, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_get_statistics", 0.0f);


    stats->total_free_energy = 0.0F;
    stats->average_precision = 0.0F;
    stats->max_prediction_error = 0.0F;
    uint32_t total_units = 0;

    for (uint32_t i = 0; i < net->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && net->num_layers > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(i + 1) / (float)net->num_layers);
        }

        predictive_layer_t* layer = net->layers[i];

        // Accumulate free energy
        stats->total_free_energy += layer->free_energy;

        // Average precision across layers
        for (uint32_t j = 0; j < layer->size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && layer->size > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(j + 1) / (float)layer->size);
            }

            stats->average_precision += layer->precision[j];

            float abs_error = fabsf(layer->prediction_error[j]);
            if (abs_error > stats->max_prediction_error) {
                stats->max_prediction_error = abs_error;
            }
        }
        total_units += layer->size;
    }

    if (total_units > 0) {
        stats->average_precision /= total_units;
    }

    stats->num_updates = (uint32_t)net->total_updates;
    stats->total_inference_time_us = net->total_inference_time_us;

    return true;
}

/**
 * @brief Print network state
 *
 * WHAT: Human-readable network dump
 * WHY:  Debugging and visualization
 * HOW:  Print all layers and metrics
 *
 * COMPLEXITY: O(num_layers)
 */
void predictive_print_state(predictive_network_t net)
{
    if (!net) {
        printf("NULL predictive network\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_print_state", 0.0f);


    printf("=== PREDICTIVE NETWORK STATE ===\n");
    printf("Layers: %u\n", net->num_layers);
    printf("Total updates: %lu\n", (unsigned long)net->total_updates);
    printf("Total free energy: %.4f\n", net->prev_free_energy);

    for (uint32_t i = 0; i < net->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && net->num_layers > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(i + 1) / (float)net->num_layers);
        }

        predictive_layer_t* layer = net->layers[i];

        // Compute layer statistics
        float avg_error = 0.0F;
        float avg_precision = 0.0F;
        for (uint32_t j = 0; j < layer->size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && layer->size > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(j + 1) / (float)layer->size);
            }

            avg_error += fabsf(layer->prediction_error[j]);
            avg_precision += layer->precision[j];
        }
        avg_error /= layer->size;
        avg_precision /= layer->size;

        printf("Layer %u: size=%u, error=%.4f, precision=%.4f, FE=%.4f\n",
               i, layer->size, avg_error, avg_precision, layer->free_energy);
    }
    printf("=================================\n");
}

/**
 * @brief Reset network state
 *
 * WHAT: Clear all predictions and errors
 * WHY:  Start fresh inference
 * HOW:  Zero out all layer states
 *
 * COMPLEXITY: O(sum(layer_sizes))
 */
bool predictive_reset(predictive_network_t net)
{
    if (!net) {
        set_error("NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_reset: net is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_reset", 0.0f);


    for (uint32_t i = 0; i < net->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && net->num_layers > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(i + 1) / (float)net->num_layers);
        }

        predictive_layer_t* layer = net->layers[i];
        memset(layer->state, 0, layer->size * sizeof(float));
        memset(layer->prediction, 0, layer->size * sizeof(float));
        memset(layer->prediction_error, 0, layer->size * sizeof(float));
        layer->free_energy = 0.0F;
    }

    net->prev_free_energy = INFINITY;
    return true;
}

/**
 * @brief Get number of layers
 *
 * COMPLEXITY: O(1)
 */
uint32_t predictive_get_num_layers(predictive_network_t net)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_get_num_layers", 0.0f);


    return net ? net->num_layers : 0;
}

/**
 * @brief Get layer size
 *
 * COMPLEXITY: O(1)
 */
uint32_t predictive_get_layer_size(predictive_network_t net, uint32_t layer_index)
{
    if (!net || layer_index >= net->num_layers) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_get_layer_size", 0.0f);


    return net->layers[layer_index]->size;
}

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Compute prediction errors (bottom-up)
 *
 * WHAT: Calculate mismatch between prediction and actual state
 * WHY:  Core of predictive coding
 * HOW:  error = state - prediction
 *
 * COMPLEXITY: O(sum(layer_sizes))
 */
static bool compute_prediction_errors(predictive_network_t net)
{
    for (uint32_t i = 0; i < net->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && net->num_layers > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(i + 1) / (float)net->num_layers);
        }

        predictive_layer_t* layer = net->layers[i];

        for (uint32_t j = 0; j < layer->size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && layer->size > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(j + 1) / (float)layer->size);
            }

            layer->prediction_error[j] = layer->state[j] - layer->prediction[j];
        }
    }
    return true;
}

/**
 * @brief Generate predictions (top-down)
 *
 * WHAT: Predict lower layer from higher layer
 * WHY:  Top-down generative model
 * HOW:  prediction[i] = f(state[i+1])
 *
 * NOTE: Simplified - assumes identity mapping
 * Full version would use learned weights
 *
 * COMPLEXITY: O(sum(layer_sizes))
 */
static bool generate_predictions(predictive_network_t net)
{
    // Top layer has no prediction (prior)
    // Propagate downward
    for (uint32_t i = 0; i < net->num_layers - 1; i++) {
        predictive_layer_t* upper = net->layers[i + 1];
        predictive_layer_t* lower = net->layers[i];

        // Simplified: downsample upper state to predict lower
        uint32_t ratio = (upper->size > 0) ? (lower->size / upper->size) : 1;
        if (ratio == 0) ratio = 1;

        for (uint32_t j = 0; j < lower->size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && lower->size > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(j + 1) / (float)lower->size);
            }

            uint32_t upper_idx = j / (ratio > 0 ? ratio : 1);
            if (upper_idx >= upper->size) {
                upper_idx = upper->size - 1;
            }
            lower->prediction[j] = upper->state[upper_idx];
        }
    }

    return true;
}

/**
 * @brief Compute total free energy
 *
 * WHAT: Sum precision-weighted squared errors
 * WHY:  Measure total surprise
 * HOW:  F = ∑_layers ∑_units precision[i] * error[i]²
 *
 * COMPLEXITY: O(sum(layer_sizes))
 */
static float compute_free_energy(predictive_network_t net)
{
    float total_fe = 0.0F;

    for (uint32_t i = 0; i < net->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && net->num_layers > 256) {
            predictive_heartbeat("predictive_loop",
                             (float)(i + 1) / (float)net->num_layers);
        }

        predictive_layer_t* layer = net->layers[i];

        float layer_fe = 0.0F;
        for (uint32_t j = 0; j < layer->size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && layer->size > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(j + 1) / (float)layer->size);
            }

            float error = layer->prediction_error[j];
            layer_fe += layer->precision[j] * error * error;
        }

        layer->free_energy = layer_fe;
        total_fe += layer_fe;
    }

    return total_fe;
}

/**
 * @brief Update states to minimize free energy
 *
 * WHAT: Gradient descent on precision-weighted error
 * WHY:  Find most likely states
 * HOW:  state ← state - step_size * ∇F
 *
 * COMPLEXITY: O(sum(layer_sizes))
 */
static bool update_states(predictive_network_t net, float step_size)
{
    // Update middle layers (not bottom, which is clamped to input)
    for (uint32_t i = 1; i < net->num_layers; i++) {
        predictive_layer_t* layer = net->layers[i];

        for (uint32_t j = 0; j < layer->size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && layer->size > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(j + 1) / (float)layer->size);
            }

            // Gradient = precision * error
            float gradient = layer->precision[j] * layer->prediction_error[j];

            // Update state to minimize error
            layer->state[j] -= step_size * gradient;
        }
    }

    return true;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int predictive_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    predictive_heartbeat("predictive_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Predictive");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                predictive_heartbeat("predictive_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Predictive");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Predictive");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void predictive_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_predictive_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int predictive_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_training_begin: NULL argument");
        return -1;
    }
    predictive_heartbeat("predictive_training_begin", 0.0f);
    return 0;
}

int predictive_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_training_end: NULL argument");
        return -1;
    }
    predictive_heartbeat("predictive_training_end", 1.0f);
    return 0;
}

int predictive_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_training_step: NULL argument");
        return -1;
    }
    predictive_heartbeat("predictive_training_step", progress);
    return 0;
}
