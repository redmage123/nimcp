/**
 * @file nimcp_plasticity_constants.h
 * @brief Common constants for plasticity learning rules
 *
 * WHAT: Centralized numerical constants shared across plasticity modules
 * WHY:  Ensure consistent numerical behavior across all learning rules
 * HOW:  Define threshold constants in one place, include where needed
 *
 * DESIGN RATIONALE:
 * - Different plasticity modules (BCM, STDP, calcium, eligibility) were using
 *   inconsistent denormal thresholds (1e-9 vs 1e-10 vs others)
 * - This header standardizes to a single value for the same conceptual operation
 * - Prevents subtle numerical divergence between learning rules
 */

#ifndef NIMCP_PLASTICITY_CONSTANTS_H
#define NIMCP_PLASTICITY_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WHAT: Threshold for flushing subnormal/denormal floats to zero
 * WHY:  Subnormal floats (< ~1.175e-38) cause 10-100x performance degradation
 *       on most CPUs due to microcode-assisted handling. Values below this
 *       threshold have negligible contribution to plasticity computations.
 * HOW:  Any float value below this threshold is set to 0.0f
 *
 * VALUE SELECTION:
 * - 1e-10f is well above the IEEE 754 single-precision denormal range (~1.4e-45)
 * - Small enough that plasticity contributions at this scale are negligible
 * - Used consistently across: STDP traces, BCM exponential decay, calcium dynamics,
 *   triplet STDP traces, and attention entropy computation
 *
 * USAGE:
 *   if (synapse->pre_trace < NIMCP_DENORMAL_THRESHOLD) synapse->pre_trace = 0.0f;
 *   if (isnan(exp_result) || exp_result < NIMCP_DENORMAL_THRESHOLD) exp_result = 0.0f;
 */
#define NIMCP_DENORMAL_THRESHOLD 1e-10f

/**
 * WHAT: Threshold for flushing exponential decay results to zero
 * WHY:  exp(-x) for large x produces values that are computationally negligible
 *       and may approach denormal range. This threshold is used after exponential
 *       decay computations (exp(-dt/tau)) where the result represents a decay factor.
 * HOW:  Same value as NIMCP_DENORMAL_THRESHOLD for consistency
 *
 * NOTE: Previously BCM and calcium used 1e-9f while STDP used 1e-10f.
 *       Standardized to 1e-10f (the more permissive threshold) to avoid
 *       prematurely zeroing out decay factors that still contribute to learning.
 */
#define NIMCP_DENORMAL_EXP_THRESHOLD NIMCP_DENORMAL_THRESHOLD

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLASTICITY_CONSTANTS_H */
