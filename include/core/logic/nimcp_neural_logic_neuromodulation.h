/**
 * @file nimcp_neural_logic_neuromodulation.h
 * @brief MODULE 4: Neural Logic Neuromodulation - Apply DA/ACh Modulation to Gates
 * @version 3.0.0
 * @date 2025-11-20
 *
 * WHAT: Neuromodulator-based threshold modulation for logic gates
 * WHY:  Single Responsibility: Implement neurochemical influence on logical reasoning
 * HOW:  Read brain DA/ACh levels, modulate gate thresholds based on biological principles
 *
 * SINGLE RESPONSIBILITY PRINCIPLE (SRP):
 * - SOLE RESPONSIBILITY: Apply neuromodulation to logic gate parameters
 * - DOES: Read neuromodulators, compute modulated thresholds, update gate states
 * - DOES NOT: Evaluate gates (MODULE 2), build circuits (MODULE 3), attach networks (MODULE 1)
 *
 * BIOLOGICAL MOTIVATION:
 *
 * DOPAMINE (DA):
 * - HIGH DA (0.8-1.0): Lowers thresholds → permissive logic → exploratory reasoning
 *   - Clinical: Mania, creative thinking, loose associations
 *   - Example: Accepts weak evidence, makes logical leaps
 *
 * - LOW DA (0.0-0.3): Raises thresholds → rigid logic → perseverative reasoning
 *   - Clinical: Depression, black-and-white thinking, inflexible rules
 *   - Example: Requires strong evidence, rejects novel conclusions
 *
 * ACETYLCHOLINE (ACh):
 * - HIGH ACh (0.8-1.0): Precise thresholds → accurate logic → focused attention
 *   - Clinical: Normal cognition, high working memory capacity
 *   - Example: Catches logical errors, maintains consistency
 *
 * - LOW ACh (0.0-0.3): Imprecise thresholds → error-prone logic → distracted reasoning
 *   - Clinical: ADHD, dementia, confabulation, attentional deficits
 *   - Example: Misses contradictions, makes logical errors
 *
 * MODULATION FORMULA:
 *   threshold_modulated = threshold_base * (1.0 - DA * 0.3) * (1.0 + ACh * 0.2)
 *
 * EXAMPLE SCENARIOS:
 * - High DA (0.9), High ACh (0.9): threshold * 0.73 * 1.18 = 0.86x (flexible + precise)
 * - Low DA (0.1), Low ACh (0.1): threshold * 0.97 * 1.02 = 0.99x (rigid + imprecise)
 * - High DA (0.9), Low ACh (0.2): threshold * 0.73 * 1.04 = 0.76x (flexible + error-prone = risky)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURAL_LOGIC_NEUROMODULATION_H
#define NIMCP_NEURAL_LOGIC_NEUROMODULATION_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuron_types/nimcp_neural_logic.h"
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Neuromodulation Constants
//=============================================================================

/** Dopamine modulation factor (reduces threshold by up to 30%) */
#define DA_MODULATION_FACTOR 0.3f

/** Acetylcholine modulation factor (increases precision by up to 30%) */
// BUG FIX: Increased from 0.2 to 0.3 so high ACh can overcome baseline DA
#define ACH_MODULATION_FACTOR 0.3f

//=============================================================================
// MODULE 4: Neuromodulation API
//=============================================================================

/**
 * @brief Apply dopamine modulation to single logic gate
 *
 * WHAT: Modulate gate threshold based on dopamine level
 * WHY:  Implement DA-driven flexibility vs rigidity in logical reasoning
 * HOW:  Read brain DA, compute factor, update gate threshold
 *
 * @param brain Brain instance with attached logic network and neuromodulator system
 * @param gate_id Logic gate neuron ID
 * @param da_level Dopamine level [0,1] (0=depleted, 0.5=baseline, 1.0=saturated)
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - brain->logic == NULL → false + error log
 * - Invalid gate_id → false + error log
 * - da_level < 0 or > 1 → clamp to [0,1] + warning
 *
 * BEHAVIOR:
 * - Reads current gate threshold
 * - Computes DA factor: (1.0 - da_level * 0.3)
 * - Updates gate threshold: threshold_base * da_factor
 * - Logs modulation event
 *
 * BIOLOGICAL EFFECTS:
 * - da_level = 0.0: No modulation (threshold * 1.0)
 * - da_level = 0.5: Slight reduction (threshold * 0.85)
 * - da_level = 1.0: Maximum reduction (threshold * 0.7)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * // Depression simulation (low DA = rigid logic)
 * apply_dopamine_modulation(brain, and_gate, 0.2f);
 * // Gate becomes more strict (higher effective threshold)
 *
 * // Mania simulation (high DA = loose logic)
 * apply_dopamine_modulation(brain, and_gate, 0.9f);
 * // Gate becomes permissive (lower effective threshold)
 * ```
 */
NIMCP_EXPORT bool apply_dopamine_modulation(
    brain_t brain,
    uint32_t gate_id,
    float da_level
);

/**
 * @brief Apply acetylcholine modulation to single logic gate
 *
 * WHAT: Modulate gate threshold precision based on acetylcholine level
 * WHY:  Implement ACh-driven attention and precision in logical reasoning
 * HOW:  Read brain ACh, compute factor, update gate threshold precision
 *
 * @param brain Brain instance with attached logic network and neuromodulator system
 * @param gate_id Logic gate neuron ID
 * @param ach_level Acetylcholine level [0,1] (0=depleted, 0.5=baseline, 1.0=saturated)
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - brain->logic == NULL → false + error log
 * - Invalid gate_id → false + error log
 * - ach_level < 0 or > 1 → clamp to [0,1] + warning
 *
 * BEHAVIOR:
 * - Reads current gate threshold
 * - Computes ACh factor: (1.0 + ach_level * 0.2)
 * - Updates gate threshold: threshold_base * ach_factor
 * - Logs modulation event
 *
 * BIOLOGICAL EFFECTS:
 * - ach_level = 0.0: No modulation (threshold * 1.0)
 * - ach_level = 0.5: Slight precision increase (threshold * 1.1)
 * - ach_level = 1.0: Maximum precision (threshold * 1.2)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * // ADHD simulation (low ACh = imprecise logic)
 * apply_acetylcholine_modulation(brain, and_gate, 0.1f);
 * // Gate becomes less precise (misses logical errors)
 *
 * // High attention (high ACh = precise logic)
 * apply_acetylcholine_modulation(brain, and_gate, 0.9f);
 * // Gate becomes very precise (catches logical errors)
 * ```
 */
NIMCP_EXPORT bool apply_acetylcholine_modulation(
    brain_t brain,
    uint32_t gate_id,
    float ach_level
);

/**
 * @brief Update all gate neuromodulation based on current brain state
 *
 * WHAT: Apply DA and ACh modulation to all gates in logic network
 * WHY:  Synchronize entire logic system with brain neuromodulator state
 * HOW:  Read brain neuromodulators, iterate gates, apply combined modulation
 *
 * @param brain Brain instance with attached logic network and neuromodulator system
 * @return Number of gates modulated, 0 on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → 0 + error log
 * - brain->logic == NULL → 0 + error log
 * - brain->neuromodulator_system == NULL → 0 + error log
 *
 * BEHAVIOR:
 * - Reads brain->neuromodulator_system for DA and ACh levels
 * - Queries neural_logic_get_stats() for gate count
 * - Iterates all gates (0 to total_gates-1)
 * - For each gate:
 *   - Reads base threshold
 *   - Computes combined modulation: threshold * (1-DA*0.3) * (1+ACh*0.2)
 *   - Updates gate threshold
 * - Returns count of successfully modulated gates
 *
 * COMBINED MODULATION FORMULA:
 *   threshold_mod = threshold_base * (1.0 - DA * 0.3) * (1.0 + ACh * 0.2)
 *
 * USAGE PATTERNS:
 * - Call periodically (e.g., every 100ms) during reasoning tasks
 * - Call after significant neuromodulator changes (reward, stress, attention shift)
 * - Call before complex logical evaluations to ensure consistency
 *
 * COMPLEXITY: O(n) where n = number of gates in network
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * // Update all gates based on current brain state
 * uint32_t count = update_all_gate_modulation(brain);
 * printf("Modulated %u logic gates\n", count);
 *
 * // Now all logic operations reflect current DA/ACh state
 * float inputs[2] = {1.0f, 1.0f};
 * float output;
 * brain_evaluate_logic_gate(brain, and_gate, inputs, 2, &output);
 * ```
 */
NIMCP_EXPORT uint32_t update_all_gate_modulation(brain_t brain);

/**
 * @brief Get modulated threshold for gate (query without updating)
 *
 * WHAT: Calculate what threshold would be with current neuromodulation
 * WHY:  Preview modulation effects without modifying gate state
 * HOW:  Read brain DA/ACh, apply modulation formula, return result
 *
 * @param brain Brain instance with neuromodulator system
 * @param base_threshold Unmodulated threshold value
 * @param modulated_threshold Output: modulated threshold (OUT parameter)
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - NULL modulated_threshold → false + error log
 * - brain->neuromodulator_system == NULL → use default DA=0.5, ACh=0.5
 *
 * BEHAVIOR:
 * - Reads brain->neuromodulator_system for DA and ACh
 * - Computes: threshold_base * (1.0 - DA * 0.3) * (1.0 + ACh * 0.2)
 * - Writes result to modulated_threshold
 * - Does NOT modify any gate state
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe for read-only access
 *
 * EXAMPLE:
 * ```c
 * float base = 1.5f;
 * float modulated;
 * get_modulated_threshold(brain, base, &modulated);
 * printf("Base: %.3f, Modulated: %.3f\n", base, modulated);
 * // Example output: Base: 1.500, Modulated: 1.276 (with DA=0.8, ACh=0.6)
 * ```
 */
NIMCP_EXPORT bool get_modulated_threshold(
    brain_t brain,
    float base_threshold,
    float* modulated_threshold
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LOGIC_NEUROMODULATION_H
