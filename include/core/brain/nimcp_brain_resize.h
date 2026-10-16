//=============================================================================
// nimcp_brain_resize.h - Dynamic Brain Resizing API
//=============================================================================
/**
 * @file nimcp_brain_resize.h
 * @brief API for dynamic brain capacity expansion
 *
 * WHAT: Enables brains to grow during training while preserving learned knowledge
 * WHY:  Start small (fast training), scale up automatically as needed
 * HOW:  Transparent neuron addition with zero knowledge loss
 *
 * FEATURES:
 * - Manual resize: `brain_resize(brain, new_size)`
 * - Auto resize: `brain_auto_resize(brain)` (call periodically during training)
 * - Metrics: `brain_get_utilization_metrics(brain, &util, &sat)`
 *
 * EXAMPLE USAGE:
 * ```c
 * // Manual resize:
 * brain_t brain = brain_create("task", BRAIN_SIZE_SMALL, ...);
 * // ... train for a while ...
 * brain_resize(brain, 2000);  // Grow to 2000 neurons
 *
 * // Automatic resize:
 * brain_t brain = brain_create("task", BRAIN_SIZE_TINY, ...);
 * for (int step = 0; step < 1000000; step++) {
 *     brain_learn(brain, features, label, confidence);
 *
 *     // Check for auto-growth every 100 steps
 *     if (step % 100 == 0) {
 *         brain_auto_resize(brain);  // Grows automatically when needed
 *     }
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.8.0
 */

#ifndef NIMCP_BRAIN_RESIZE_H
#define NIMCP_BRAIN_RESIZE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Dynamic Brain Resizing
//=============================================================================

/**
 * @brief Resize brain to new neuron count
 *
 * WHAT: Expand brain capacity while preserving all learned knowledge
 * WHY:  Enable continuous learning without hitting capacity limits
 * HOW:  Create new network, transfer neurons/weights, swap atomically
 *
 * GUARANTEES:
 * - Zero knowledge loss: All weights, biases, states preserved exactly
 * - Atomic operation: Network swap is instantaneous (no partial state)
 * - Memory safe: Old network destroyed only after successful transfer
 * - Config update: brain_config_t updated to reflect new size
 *
 * LIMITATIONS:
 * - Only growth supported (new_neuron_count must be > current)
 * - Temporary 2× memory overhead during transfer
 * - Not thread-safe: Do not call during concurrent brain_learn/brain_decide
 *
 * ERROR HANDLING:
 * - Returns false if new_count ≤ current (shrinking not supported)
 * - Returns false if allocation fails (brain unchanged)
 * - Returns false if transfer fails (brain unchanged)
 *
 * PERFORMANCE:
 * - Time: O(n + m) where n=old neurons, m=new neurons
 * - Space: O(max(n,m) × k) where k=avg connections
 *
 * @param brain Brain to resize (must not be NULL)
 * @param new_neuron_count New total neuron count (must be > current)
 * @return true on success, false on error (brain unchanged)
 */
bool brain_resize(brain_t brain, uint32_t new_neuron_count);

/**
 * @brief Auto-resize brain based on utilization metrics
 *
 * WHAT: Automatically grow brain when capacity is saturated
 * WHY:  Enable continuous learning without manual intervention
 * HOW:  Evaluate metrics, decide if growth needed, resize if so
 *
 * GROWTH TRIGGERS (any causes resize):
 * - High utilization: >90% neurons active for >1000 steps
 * - Weight saturation: >80% weights near ±1.0
 * - Manual trigger: User called with force flag
 *
 * GROWTH POLICY:
 * - TINY (100)  → SMALL (500)   [5× to reach useful size]
 * - SMALL (500) → MEDIUM (1000)  [2× growth]
 * - MEDIUM (1000) → LARGE (5000) [5× growth]
 * - LARGE (5000) → 7500          [1.5× growth]
 * - Custom: Always 1.5× current size
 *
 * USAGE:
 * ```c
 * // During training loop:
 * for (int step = 0; step < MAX_STEPS; step++) {
 *     brain_learn(brain, features, label, confidence);
 *
 *     // Check every 100 steps (adjust frequency as needed)
 *     if (step % 100 == 0) {
 *         if (brain_auto_resize(brain)) {
 *             printf("Brain auto-resized to %u neurons\n",
 *                    brain_get_neuron_count(brain));
 *         }
 *     }
 * }
 * ```
 *
 * PERFORMANCE:
 * - O(n) for metric computation (fast, ~0.1ms for 1K neurons)
 * - O(n+m) if resize triggered (slower, ~100ms for 1K→2K neurons)
 * - Overhead: <1% when no resize needed, amortized cost is minimal
 *
 * @param brain Brain to potentially resize (must not be NULL)
 * @return true if resize occurred, false if no resize needed or error
 */
bool brain_auto_resize(brain_t brain);

/**
 * @brief Get brain current neuron count
 *
 * WHAT: Return current brain capacity in neurons
 * WHY:  Allow monitoring of brain size for metrics/logging
 * HOW:  Access underlying network and query neuron count
 *
 * USAGE:
 * ```c
 * uint32_t size = brain_get_neuron_count(brain);
 * printf("Brain has %u neurons\n", size);
 * ```
 *
 * @param brain Brain to query (must not be NULL)
 * @return Neuron count, or 0 on error
 */
uint32_t brain_get_neuron_count(brain_t brain);

/**
 * @brief Get brain utilization metrics
 *
 * WHAT: Return current capacity utilization statistics
 * WHY:  Enable monitoring and debugging of auto-resize logic
 * HOW:  Compute utilization and saturation on-demand
 *
 * METRICS:
 * - Utilization: Fraction of neurons with activity > 0.01 (0.0-1.0)
 *   - < 0.5: Underutilized, can train with smaller brain
 *   - 0.5-0.9: Healthy utilization
 *   - > 0.9: Saturated, needs more capacity
 *
 * - Saturation: Fraction of weights with |w| > 0.9 (0.0-1.0)
 *   - < 0.3: Healthy weight distribution
 *   - 0.3-0.8: Moderate saturation
 *   - > 0.8: Saturated, weights can't learn effectively
 *
 * USAGE:
 * ```c
 * float util, sat;
 * if (brain_get_utilization_metrics(brain, &util, &sat)) {
 *     printf("Utilization: %.1f%%, Saturation: %.1f%%\n",
 *            util * 100, sat * 100);
 *
 *     if (util > 0.9 || sat > 0.8) {
 *         printf("Warning: Brain at capacity, consider resize\n");
 *     }
 * }
 * ```
 *
 * PERFORMANCE: O(n × k) where k=avg synapses per neuron (~1ms for 1K neurons)
 *
 * @param brain Brain to analyze (must not be NULL)
 * @param utilization Output: neuron utilization ratio [0.0, 1.0]
 * @param saturation Output: weight saturation ratio [0.0, 1.0]
 * @return true on success, false on error
 */
bool brain_get_utilization_metrics(brain_t brain, float* utilization, float* saturation);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_RESIZE_H
