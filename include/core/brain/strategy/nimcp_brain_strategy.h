//=============================================================================
// nimcp_brain_strategy.h - Brain Task Strategy Pattern API
//=============================================================================
/**
 * @file nimcp_brain_strategy.h
 * @brief Task-specific learning and optimization strategies for NIMCP brain
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Encapsulates task-specific algorithms (classification, regression, etc.)
 * - Factory Pattern: Creates strategies based on task type
 * - Analysis API: Monitoring, statistics, and explainability
 * - Optimization API: Pruning, inference optimization, threshold recommendations
 *
 * DESIGN DECISIONS:
 * - Clean separation: Strategy logic independent of core brain implementation
 * - Extensibility: Easy to add new task types
 * - No nested ifs: All validation uses guard clauses
 * - Thread-safe: Error handling uses thread-local storage
 *
 * PERFORMANCE:
 * - Strategy creation: O(1)
 * - Transform output: O(n) where n = num_outputs
 * - Compute loss: O(n)
 * - Pruning: O(n*c) where c = connections per neuron
 */

#ifndef NIMCP_BRAIN_STRATEGY_H
#define NIMCP_BRAIN_STRATEGY_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/**
 * @brief Opaque task strategy handle
 *
 * WHY: Encapsulates task-specific behavior (learning rate, output transform, loss)
 * PATTERN: Strategy pattern - swappable algorithms
 */
typedef struct task_strategy task_strategy_t;

//=============================================================================
// Strategy Factory API
//=============================================================================

/**
 * @brief Create strategy for task type
 *
 * WHY: Factory pattern for strategy creation
 * Centralizes strategy instantiation logic
 *
 * COMPLEXITY: O(1) - simple allocation and assignment
 *
 * @param task Task type (BRAIN_TASK_CLASSIFICATION, etc.)
 * @return Strategy instance or NULL on error
 */
task_strategy_t* strategy_create(brain_task_t task);

/**
 * @brief Destroy strategy
 *
 * COMPLEXITY: O(1)
 *
 * @param strategy Strategy to destroy
 */
void strategy_destroy(task_strategy_t* strategy);

//=============================================================================
// Strategy Access API
//=============================================================================

/**
 * @brief Get recommended learning rate for task
 *
 * COMPLEXITY: O(1)
 *
 * @param strategy Strategy handle
 * @return Learning rate (typically 0.001 - 0.05)
 */
float strategy_get_learning_rate(task_strategy_t* strategy);

/**
 * @brief Transform raw output to task-specific format
 *
 * WHY: Different tasks need different output transformations:
 * - Classification: Softmax (probability distribution)
 * - Regression: No transform (raw values)
 * - Pattern matching: Binary threshold
 * - Association: Normalization
 *
 * COMPLEXITY: O(num_outputs)
 *
 * @param strategy Strategy handle
 * @param output Output vector to transform (modified in-place)
 * @param size Output vector size
 */
void strategy_transform_output(task_strategy_t* strategy, float* output, uint32_t size);

/**
 * @brief Compute task-specific loss
 *
 * WHY: Different tasks use different loss functions:
 * - Classification: Cross-entropy
 * - Regression: MSE
 * - Pattern matching: Binary cross-entropy
 * - Association: Cosine distance
 *
 * COMPLEXITY: O(num_outputs)
 *
 * @param strategy Strategy handle
 * @param predicted Predicted output
 * @param target Target output
 * @param size Vector size
 * @return Loss value (non-negative)
 */
float strategy_compute_loss(task_strategy_t* strategy, const float* predicted,
                           const float* target, uint32_t size);

/**
 * @brief Get task type for strategy
 *
 * COMPLEXITY: O(1)
 *
 * @param strategy Strategy handle
 * @return Task type
 */
brain_task_t strategy_get_task_type(task_strategy_t* strategy);

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
bool brain_get_stats(brain_t brain, brain_stats_t* stats);

/**
 * @brief Get number of input features for this brain
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @return Number of inputs, or 0 if brain is NULL
 */
uint32_t brain_get_num_inputs(brain_t brain);

/**
 * @brief Get number of output features for this brain
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @return Number of outputs, or 0 if brain is NULL
 */
uint32_t brain_get_num_outputs(brain_t brain);

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
systems_consolidation_system_t* brain_get_systems_consolidation(brain_t brain);

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
bool brain_get_cow_stats(brain_t brain, brain_cow_stats_t* cow_stats);

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
void brain_print_info(brain_t brain);

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
 * @param neuron_ids Output array of neuron IDs (must have space for top_n)
 * @param importances Output array of importance scores (must have space for top_n)
 * @return Number of neurons returned (may be less than top_n)
 */
uint32_t brain_get_top_neurons(brain_t brain, uint32_t top_n, uint32_t* neuron_ids,
                               float* importances);

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
                            char* explanation, uint32_t max_length);

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
 * @param threshold Prune synapses with weight < threshold (typically 0.01-0.1)
 * @return Number of synapses pruned
 */
uint32_t brain_prune(brain_t brain, float threshold);

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
bool brain_optimize_for_inference(brain_t brain);

/**
 * @brief Get recommended pruning threshold
 *
 * WHY: Provides heuristic for safe pruning
 * Balances sparsity vs accuracy
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain handle
 * @param target_sparsity Desired sparsity (0-1, where 0.9 means 90% sparse)
 * @return Recommended threshold, or -1.0 on error
 */
float brain_recommend_pruning_threshold(brain_t brain, float target_sparsity);

//=============================================================================
// Error Handling API
//=============================================================================

/**
 * @brief Get last error message
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Uses thread-local storage
 *
 * @return Error string or NULL if no error
 */
const char* brain_get_last_error(void);

/**
 * @brief Clear last error
 *
 * COMPLEXITY: O(1)
 */
void brain_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_STRATEGY_H
