//=============================================================================
// nimcp_brain_strategy.c - Brain Task Strategy Implementation
//=============================================================================
/**
 * @file nimcp_brain_strategy.c
 * @brief Implementation of task-specific learning and optimization strategies
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Task-specific algorithms encapsulated in function pointers
 * - Factory Pattern: Creates strategies based on task type
 * - Analysis API: Monitoring, statistics, and explainability
 * - Optimization API: Pruning, inference optimization
 *
 * EXTRACTED FROM: nimcp_brain.c
 * - Lines 374-674: Strategy Pattern (task_strategy struct, implementations, factory)
 * - Lines 8227-8502: Analysis & Monitoring API
 * - Lines 8503-8598: Optimization API
 *
 * DESIGN DECISIONS:
 * - No nested ifs: All validation uses early returns (guard clauses)
 * - Thread-safe: Error handling uses thread-local storage
 * - Clean separation: Independent from main brain implementation
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

#include "core/brain/strategy/nimcp_brain_strategy.h"
#include "constants/nimcp_buffer_constants.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_STRATEGY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_strategy, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Forward Declarations for Private Functions
//=============================================================================

// Helper function for COW network management (implemented in nimcp_brain.c)
// We need to declare it here to satisfy the linker
extern bool ensure_writable_network(brain_t brain);
extern void clear_cache(brain_t brain);

// Error handling (shared across all brain modules via external linkage)
void set_error(const char* format, ...);

//=============================================================================
// Strategy Pattern - Task-Specific Behaviors
//=============================================================================

/**
 * @brief Task strategy interface
 *
 * NOTE: struct task_strategy is defined in nimcp_brain_internal.h (lines 379-399)
 *       which is included via nimcp_brain_strategy.h. No redefinition needed.
 *
 * WHY: Different tasks (classification, regression) need different:
 * - Learning rates
 * - Output transformations
 * - Performance metrics
 *
 * PATTERN: Strategy pattern - encapsulates algorithm families
 */
// struct task_strategy removed - using definition from nimcp_brain_internal.h

//=============================================================================
// Strategy Implementations - Classification
//=============================================================================

/**
 * @brief Classification strategy - softmax output, cross-entropy loss
 *
 * WHY: Classification needs probabilities summing to 1.0
 * WHEN: Multi-class or binary classification tasks
 * COMPLEXITY: O(n) for softmax normalization
 */
static float strategy_classification_lr(void)
{
    return 0.01F;
}

static void strategy_classification_transform(float* output, uint32_t size)
{
    // Softmax normalization for probability distribution
    float max_val = output[0];
    for (uint32_t i = 1; i < size; i++) {
        if (output[i] > max_val)
            max_val = output[i];
    }

    float sum = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        output[i] = expf(output[i] - max_val);
        sum += output[i];
    }

    for (uint32_t i = 0; i < size; i++) {
        output[i] /= sum;
    }
}

static float strategy_classification_loss(const float* pred, const float* target, uint32_t size)
{
    // Cross-entropy loss
    float loss = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        if (target[i] > 0.0F) {
            loss -= target[i] * logf(fmaxf(pred[i], 1e-10F));
        }
    }
    return loss;
}

//=============================================================================
// Strategy Implementations - Regression
//=============================================================================

/**
 * @brief Regression strategy - linear output, MSE loss
 *
 * WHY: Regression predicts continuous values
 * WHEN: Predicting real-valued outputs
 * COMPLEXITY: O(n) for MSE calculation
 */
static float strategy_regression_lr(void)
{
    return 0.005F;
}

static void strategy_regression_transform(float* output, uint32_t size)
{
    // No transformation - use raw values
    (void) output;
    (void) size;
}

static float strategy_regression_loss(const float* pred, const float* target, uint32_t size)
{
    // Mean squared error
    float loss = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        float diff = pred[i] - target[i];
        loss += diff * diff;
    }
    return loss / size;
}

//=============================================================================
// Strategy Implementations - Pattern Matching
//=============================================================================

/**
 * @brief Pattern matching strategy - high LR, binary output
 *
 * WHY: Pattern matching needs fast adaptation
 * WHEN: Recognizing specific patterns quickly
 * COMPLEXITY: O(n)
 */
static float strategy_pattern_lr(void)
{
    return 0.02F;
}

static void strategy_pattern_transform(float* output, uint32_t size)
{
    // Threshold to binary
    for (uint32_t i = 0; i < size; i++) {
        output[i] = output[i] > 0.5F ? 1.0F : 0.0F;
    }
}

static float strategy_pattern_loss(const float* pred, const float* target, uint32_t size)
{
    // Binary cross-entropy
    float loss = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        float p = fmaxf(fminf(pred[i], 0.9999F), 0.0001F);
        loss -= target[i] * logf(p) + (1.0F - target[i]) * logf(1.0F - p);
    }
    return loss / size;
}

//=============================================================================
// Strategy Implementations - Association Learning
//=============================================================================

/**
 * @brief Association learning strategy - Hebbian-focused
 *
 * WHY: Association learning uses different plasticity rules
 * WHEN: Building associative memories
 * COMPLEXITY: O(n)
 */
static float strategy_association_lr(void)
{
    return 0.05F;
}

static void strategy_association_transform(float* output, uint32_t size)
{
    // Normalize to unit range
    float max_val = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        if (fabsf(output[i]) > max_val)
            max_val = fabsf(output[i]);
    }

    if (max_val > 0.0F) {
        for (uint32_t i = 0; i < size; i++) {
            output[i] /= max_val;
        }
    }
}

static float strategy_association_loss(const float* pred, const float* target, uint32_t size)
{
    // Cosine distance
    float dot = 0.0F, norm_p = 0.0F, norm_t = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        dot += pred[i] * target[i];
        norm_p += pred[i] * pred[i];
        norm_t += target[i] * target[i];
    }
    float cosine = dot / (sqrtf(norm_p) * sqrtf(norm_t) + 1e-10F);
    return 1.0F - cosine;
}

//=============================================================================
// Strategy Factory
//=============================================================================

/**
 * @brief Create strategy for task type
 *
 * WHY: Factory pattern for strategy creation
 * Centralizes strategy instantiation logic
 *
 * COMPLEXITY: O(1) - simple allocation and assignment
 *
 * @param task Task type
 * @return Strategy instance or NULL on error
 */
task_strategy_t* strategy_create(brain_task_t task)
{
    task_strategy_t* strategy = nimcp_calloc(1, sizeof(task_strategy_t));
    if (!strategy) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "strategy is NULL");

        return NULL;

    }

    strategy->task_type = task;

    switch (task) {
        case BRAIN_TASK_CLASSIFICATION:
            strategy->get_learning_rate = strategy_classification_lr;
            strategy->transform_output = strategy_classification_transform;
            strategy->compute_loss = strategy_classification_loss;
            break;

        case BRAIN_TASK_REGRESSION:
            strategy->get_learning_rate = strategy_regression_lr;
            strategy->transform_output = strategy_regression_transform;
            strategy->compute_loss = strategy_regression_loss;
            break;

        case BRAIN_TASK_PATTERN_MATCHING:
            strategy->get_learning_rate = strategy_pattern_lr;
            strategy->transform_output = strategy_pattern_transform;
            strategy->compute_loss = strategy_pattern_loss;
            break;

        case BRAIN_TASK_ASSOCIATION:
            strategy->get_learning_rate = strategy_association_lr;
            strategy->transform_output = strategy_association_transform;
            strategy->compute_loss = strategy_association_loss;
            break;

        default:
            // Default to classification
            strategy->get_learning_rate = strategy_classification_lr;
            strategy->transform_output = strategy_classification_transform;
            strategy->compute_loss = strategy_classification_loss;
            break;
    }

    return strategy;
}

/**
 * @brief Destroy strategy
 *
 * COMPLEXITY: O(1)
 */
void strategy_destroy(task_strategy_t* strategy)
{
    nimcp_free(strategy);
}

//=============================================================================
// Strategy Access API
//=============================================================================

float strategy_get_learning_rate(task_strategy_t* strategy)
{
    if (!strategy || !strategy->get_learning_rate) {
        return 0.01F;  // Default learning rate
    }
    return strategy->get_learning_rate();
}

void strategy_transform_output(task_strategy_t* strategy, float* output, uint32_t size)
{
    if (!strategy || !strategy->transform_output || !output) {
        return;
    }
    strategy->transform_output(output, size);
}

float strategy_compute_loss(task_strategy_t* strategy, const float* predicted,
                           const float* target, uint32_t size)
{
    if (!strategy || !strategy->compute_loss || !predicted || !target) {
        return -1.0F;  // Error indicator
    }
    return strategy->compute_loss(predicted, target, size);
}

brain_task_t strategy_get_task_type(task_strategy_t* strategy)
{
    if (!strategy) {
        return BRAIN_TASK_CLASSIFICATION;  // Default
    }
    return strategy->task_type;
}

//=============================================================================
// Error Handling
//=============================================================================

static __thread char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/**
 * @brief Set error message with printf-style formatting
 *
 * WHY: Thread-local error storage prevents race conditions
 * Thread-safe error reporting without locks
 *
 * COMPLEXITY: O(n) where n = message length
 *
 * EXTERNAL LINKAGE: Shared across all brain modules to ensure consistent error state
 *
 * @param format Printf-style format string
 */
void set_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(last_error, sizeof(last_error), format, args);
    va_end(args);
}

/**
 * @brief Get last error message
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Uses thread-local storage
 *
 * @return Error string or NULL if no error
 */
const char* brain_get_last_error(void)
{
    return last_error[0] != '\0' ? last_error : NULL;
}

/**
 * @brief Clear last error
 *
 * COMPLEXITY: O(1)
 */
void brain_clear_error(void)
{
    last_error[0] = '\0';
}

//=============================================================================
// Analysis & Monitoring API
//=============================================================================

/**
 * @brief Get brain statistics
 *
 * WHY: Provides performance and training metrics
 * Essential for monitoring and debugging
 *
 * COMPLEXITY: O(1) - mostly copying cached stats
 *
 * @param brain Brain handle
 * @param stats Output statistics
 * @return true on success
 */
bool brain_get_stats(brain_t brain, brain_stats_t* stats)
{
    // Guard: Validate parameters
    if (!brain || !stats) {
        set_error("Invalid parameters to brain_get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_stats: required parameter is NULL (brain, stats)");
        return false;
    }

    // If this is a snapshot, return the preserved stats
    if (brain->is_snapshot) {
        *stats = brain->snapshot_stats;
        brain_clear_error();
        return true;
    }

    // Get performance from adaptive network
    network_performance_t perf;
    adaptive_network_get_performance(brain->network, &perf);

    // Copy to brain stats
    stats->size = brain->config.size;
    stats->num_neurons = brain->stats.num_neurons;
    stats->num_synapses = brain->stats.num_synapses;
    stats->num_active_synapses = brain->stats.num_active_synapses;
    stats->total_inferences = brain->stats.total_inferences;  // Use brain's counter, not network's
    stats->total_learning_steps = perf.total_learning_steps;
    stats->quantum_annealing_runs = brain->stats.quantum_annealing_runs;  // Copy quantum annealing counter
    stats->avg_sparsity = brain->stats.avg_sparsity;  // Use brain's stats, updated by update_inference_stats
    stats->avg_inference_time_us = brain->stats.avg_inference_time_us;  // Use brain's stats, updated by update_inference_stats
    stats->current_learning_rate = brain->config.learning_rate;
    stats->accuracy = perf.accuracy;
    stats->running_accuracy = brain->stats.running_accuracy;
    stats->memory_bytes = perf.memory_usage_bytes;
    strncpy(stats->task_name, brain->config.task_name, sizeof(stats->task_name) - 1);

    brain_clear_error();
    return true;
}

bool brain_get_network_metrics(brain_t brain, brain_network_metrics_t* metrics) {
    if (!brain || !metrics) return false;
    metrics->last_ann_loss = brain->network_metrics.last_ann_loss;
    metrics->last_cnn_loss = brain->network_metrics.last_cnn_loss;
    metrics->last_snn_loss = brain->network_metrics.last_snn_loss;
    metrics->last_lnn_loss = brain->network_metrics.last_lnn_loss;
    metrics->ema_ann_loss  = brain->network_metrics.ema_ann_loss;
    metrics->ema_cnn_loss  = brain->network_metrics.ema_cnn_loss;
    metrics->ema_snn_loss  = brain->network_metrics.ema_snn_loss;
    metrics->ema_lnn_loss  = brain->network_metrics.ema_lnn_loss;
    metrics->ann_steps     = brain->network_metrics.ann_steps;
    metrics->cnn_steps     = brain->network_metrics.cnn_steps;
    metrics->snn_steps     = brain->network_metrics.snn_steps;
    metrics->lnn_steps     = brain->network_metrics.lnn_steps;
    metrics->trn_mean_rate_hz = brain->network_metrics.trn_mean_rate_hz;
    return true;
}

/**
 * @brief Get number of input features for this brain
 */
uint32_t brain_get_num_inputs(brain_t brain)
{
    if (!brain) {
        return 0;
    }
    return brain->config.num_inputs;
}

/**
 * @brief Get number of output features for this brain
 */
uint32_t brain_get_num_outputs(brain_t brain)
{
    if (!brain) {
        return 0;
    }
    return brain->config.num_outputs;
}

/**
 * @brief Get systems consolidation subsystem
 *
 * WHAT: Access the brain's systems consolidation component
 * WHY:  Allow other modules (e.g., mental health) to interact with memory consolidation
 * HOW:  Return pointer to systems consolidation subsystem
 *
 * THREAD SAFETY: Thread-safe (read-only access to pointer)
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @return Pointer to systems consolidation, or NULL if brain is NULL or consolidation not initialized
 */
systems_consolidation_system_t* brain_get_systems_consolidation(brain_t brain)
{
    // GUARD: NULL check
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_systems_consolidation: validation failed");
        return NULL;
    }

    // Lazy initialization: Initialize on first access if deferred
    if (!brain->systems_consolidation && brain->config.lazy_consolidation_init) {
        nimcp_platform_mutex_lock(&brain->cache_mutex);
        // Double-check after acquiring lock (another thread may have initialized)
        if (!brain->systems_consolidation) {
            extern bool nimcp_brain_factory_init_consolidation_subsystem(brain_t brain);
            nimcp_brain_factory_init_consolidation_subsystem(brain);
        }
        nimcp_platform_mutex_unlock(&brain->cache_mutex);
    }

    return brain->systems_consolidation;
}

/**
 * @brief Get COW statistics for brain
 *
 * WHAT: Report copy-on-write memory sharing status
 * WHY:  Allow monitoring of memory efficiency gains
 * HOW:  Check is_cow_clone flag and calculate shared/private memory
 *
 * THREAD SAFETY: Thread-safe (read-only access)
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @param cow_stats Output COW statistics
 * @return true on success
 */
bool brain_get_cow_stats(brain_t brain, brain_cow_stats_t* cow_stats)
{
    // Guard: Validate parameters
    if (!brain || !cow_stats) {
        set_error("Invalid parameters to brain_get_cow_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_cow_stats: required parameter is NULL (brain, cow_stats)");
        return false;
    }

    if (brain->is_cow_clone && brain->network) {
        // This is a COW clone
        cow_stats->is_cow_clone = true;

        // Phase 3: Check if network is still shared or if COW was triggered
        if (brain->owns_network) {
            // COW was triggered - clone now owns private network copy
            cow_stats->cow_ref_count = 1;  // Only this brain owns the network
            cow_stats->cow_shared_bytes = 0;  // No longer sharing

            // All network memory is now private
            network_performance_t perf;
            adaptive_network_get_performance(brain->network, &perf);
            cow_stats->cow_private_bytes = sizeof(struct brain_struct) + perf.memory_usage_bytes;
        } else {
            // Network is still shared (Phase 3 read-only inference)
            // Use reference count if available, otherwise assume 2
            cow_stats->cow_ref_count = brain->network_refcount_atomic ? atomic_load(brain->network_refcount_atomic) : 2;

            // Calculate shared bytes (network size)
            network_performance_t perf;
            adaptive_network_get_performance(brain->network, &perf);
            cow_stats->cow_shared_bytes = perf.memory_usage_bytes;

            // Calculate private bytes (brain struct + labels + caches)
            cow_stats->cow_private_bytes = sizeof(struct brain_struct);
        }

        // Add size of label strings
        for (uint32_t i = 0; i < brain->num_output_labels; i++) {
            if (brain->output_labels && brain->output_labels[i]) {
                cow_stats->cow_private_bytes += strlen(brain->output_labels[i]) + 1;
            }
        }

        // Add size of label array
        if (brain->output_labels) {
            cow_stats->cow_private_bytes += brain->num_output_labels * sizeof(char*);
        }

    } else {
        // Not a COW clone - owns all memory
        cow_stats->is_cow_clone = false;
        cow_stats->cow_ref_count = 1;
        cow_stats->cow_shared_bytes = 0;

        // All memory is private
        network_performance_t perf;
        if (brain->network) {
            adaptive_network_get_performance(brain->network, &perf);
            cow_stats->cow_private_bytes = perf.memory_usage_bytes;
        } else {
            cow_stats->cow_private_bytes = 0;
        }

        // Add brain structure overhead
        cow_stats->cow_private_bytes += sizeof(struct brain_struct);
    }

    brain_clear_error();
    return true;
}

/**
 * @brief Print brain info to stdout
 *
 * WHY: Convenient debugging and monitoring
 * Human-readable status display
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 */
void brain_print_info(brain_t brain)
{
    if (!brain)
        return;

    brain_stats_t stats;
    brain_get_stats(brain, &stats);

    printf("Brain: %s\n", stats.task_name);
    printf("  Size: %u neurons, %u synapses\n", stats.num_neurons, stats.num_synapses);
    printf("  Training: %lu learning steps\n", stats.total_learning_steps);
    printf("  Inferences: %lu\n", stats.total_inferences);
    printf("  Avg inference time: %.3f ms\n", stats.avg_inference_time_us / 1000.0);
    printf("  Avg sparsity: %.1f%%\n", stats.avg_sparsity * 100.0);
    printf("  Memory usage: %.2f MB\n", stats.memory_bytes / (1024.0 * 1024.0));
    printf("  Learning rate: %.4f\n", stats.current_learning_rate);
}

/**
 * @brief Get most important neurons
 *
 * WHY: Identifies which neurons contribute most to decisions
 * Useful for pruning and interpretability
 *
 * COMPLEXITY: O(n*log(k)) where n = total_neurons, k = top_n
 *
 * @param brain Brain handle
 * @param top_n Number of neurons to return
 * @param neuron_ids Output array of neuron IDs
 * @param importances Output array of importance scores
 * @return Number of neurons returned
 */
uint32_t brain_get_top_neurons(brain_t brain, uint32_t top_n, uint32_t* neuron_ids,
                               float* importances)
{
    // Guard: Validate parameters
    if (!brain || !neuron_ids || !importances) {
        set_error("Invalid parameters to brain_get_top_neurons");
        return 0;
    }

    // Get neuron rankings from adaptive network
    neuron_importance_t* rankings = nimcp_malloc(top_n * sizeof(neuron_importance_t));
    if (!rankings) {
        set_error("Failed to allocate rankings array (%u neurons)", top_n);
        return 0;
    }

    uint32_t count = adaptive_network_rank_neurons(brain->network, rankings, top_n);

    // Extract IDs and importances
    for (uint32_t i = 0; i < count; i++) {
        neuron_ids[i] = rankings[i].neuron_id;
        importances[i] = rankings[i].importance;
    }

    nimcp_free(rankings);

    brain_clear_error();
    return count;
}

/**
 * @brief Explain why brain made a decision
 *
 * WHY: Provides human-readable explanation of decision
 * Critical for trust and debugging
 *
 * COMPLEXITY: O(k) where k = num_active_neurons
 *
 * @param brain Brain handle
 * @param features Input that led to decision
 * @param num_features Feature count
 * @param explanation Output buffer
 * @param max_length Max explanation length
 * @return true on success
 */
bool brain_explain_decision(brain_t brain, const float* features, uint32_t num_features,
                            char* explanation, uint32_t max_length)
{
    // Guard: Validate parameters
    if (!brain || !features || !explanation) {
        set_error("Invalid parameters to brain_explain_decision");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_explain_decision: required parameter is NULL (brain, features, explanation)");
        return false;
    }

    uint32_t written =
        adaptive_network_explain(brain->network, features, num_features, explanation, max_length);

    brain_clear_error();
    return written > 0;
}

//=============================================================================
// Optimization API
//=============================================================================

/**
 * @brief Prune weak connections
 *
 * WHY: Removes low-weight synapses to improve efficiency
 * Reduces memory and speeds up inference
 *
 * COMPLEXITY: O(n*c) where c = connections per neuron
 * BENEFIT: 2-10x inference speedup possible
 *
 * @param brain Brain handle
 * @param threshold Prune synapses with weight < threshold
 * @return Number of synapses pruned
 */
uint32_t brain_prune(brain_t brain, float threshold)
{
    // Guard: Validate brain
    if (!brain) {
        set_error("Null brain provided to brain_prune");
        return 0;
    }

    // Phase 2: Ensure network is writable before pruning
    if (!ensure_writable_network(brain)) {
        return 0;  // Error already set
    }

    uint32_t pruned = adaptive_network_prune(brain->network, threshold);

    brain->stats.num_active_synapses -= pruned;

    // Invalidate cache after pruning
    clear_cache(brain);

    brain_clear_error();
    return pruned;
}

/**
 * @brief Optimize brain for inference
 *
 * WHY: Prepares brain for production deployment
 * Performs aggressive optimization for speed
 *
 * COMPLEXITY: O(n*c)
 * BENEFIT: Can achieve 5-10x speedup
 *
 * @param brain Brain handle
 * @return true on success
 */
bool brain_optimize_for_inference(brain_t brain)
{
    // Guard: Validate brain
    if (!brain) {
        set_error("Null brain provided to brain_optimize_for_inference");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_optimize_for_inference: brain is NULL");
        return false;
    }

    // Aggressive pruning for target sparsity
    float threshold = brain_recommend_pruning_threshold(brain, 0.90F);
    brain_prune(brain, threshold);

    brain_clear_error();
    return true;
}

/**
 * @brief Get recommended pruning threshold
 *
 * WHY: Provides heuristic for safe pruning
 * Balances sparsity vs accuracy
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @param target_sparsity Desired sparsity (0-1)
 * @return Recommended threshold
 */
float brain_recommend_pruning_threshold(brain_t brain, float target_sparsity)
{
    // Guard: Validate brain
    if (!brain) {
        set_error("Null brain provided to brain_recommend_pruning_threshold");
        return -1.0F;  // Return negative value to indicate error
    }

    // Heuristic: lower threshold for higher sparsity
    float threshold = 0.1F * (1.0F - target_sparsity);

    brain_clear_error();
    return threshold;
}
