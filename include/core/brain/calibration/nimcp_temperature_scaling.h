//=============================================================================
// nimcp_temperature_scaling.h - Post-hoc confidence calibration (Layer C)
//=============================================================================
/**
 * @file nimcp_temperature_scaling.h
 * @brief Temperature scaling for post-hoc confidence calibration.
 *
 * WHAT: A learned scalar T multiplies pre-softmax logits by 1/T before the
 *       softmax. T>1 softens the distribution (lowers max prob), T<1 sharpens
 *       it. T=1.0 is identity (no behavior change).
 *
 * WHY:  Modern neural nets are systematically over-confident — the softmax max
 *       does not match observed accuracy. Temperature scaling (Guo et al. 2017,
 *       "On Calibration of Modern Neural Networks") fits a single scalar T on
 *       a held-out validation set to minimize NLL, which empirically also
 *       reduces ECE (Expected Calibration Error). It is the simplest, most
 *       robust calibration technique that does not change argmax accuracy.
 *
 * HOW:  This file ships the *infrastructure* now. Calibration is meant to run
 *       later, once the brain's language stage is online and we have a held-
 *       out validation set with (logits, labels) pairs. Until then, T stays at
 *       its default 1.0 and behavior is unchanged.
 *
 * USAGE (later, once a validation set is available):
 *   // Collect logits + labels on held-out set...
 *   float T_opt;
 *   float ece_after;
 *   nimcp_calibrate_temperature(logits_2d, labels, n_samples, n_classes,
 *                               &T_opt, &ece_after);
 *   // Or, for a brain handle that stores T + metadata:
 *   nimcp_brain_calibrate_temperature(brain, logits_2d, labels, n, k);
 *
 * USAGE (inference path, once T is set):
 *   float probs[n_classes];
 *   nimcp_softmax_with_temperature(logits, n_classes, brain->decoder_temperature,
 *                                  probs);
 *   // Use max(probs) as the calibrated confidence.
 *
 * REFERENCE:
 *   Guo, C., Pleiss, G., Sun, Y., & Weinberger, K. Q. (2017).
 *   "On Calibration of Modern Neural Networks." ICML.
 */

#ifndef NIMCP_TEMPERATURE_SCALING_H
#define NIMCP_TEMPERATURE_SCALING_H

#include "utils/error/nimcp_error_codes.h"
#include <stdint.h>

/* Forward declaration of the opaque brain handle. The full struct is only
 * needed in the .c file (which includes nimcp_brain_internal.h). Defining
 * brain_t as a pointer-typedef here would conflict with the anonymous-
 * struct typedef in nimcp_brain.h, so we declare brain_t conditionally. */
#ifndef NIMCP_BRAIN_T_FWD_DEFINED
#define NIMCP_BRAIN_T_FWD_DEFINED
struct brain_struct;
typedef struct brain_struct* brain_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply temperature scaling to logits in-place.
 *
 * Divides each element of `logits` by T. If T is within an epsilon of 1.0,
 * this is a no-op (no FP rounding) so the default path is bit-for-bit
 * identity.
 *
 * @param logits  In/out array of n logits.
 * @param n       Length of logits array (must be > 0).
 * @param T       Temperature. Must be > 0 (else NIMCP_ERROR_INVALID_PARAM).
 * @return NIMCP_OK on success, NIMCP_ERROR_NULL_POINTER or
 *         NIMCP_ERROR_INVALID_PARAM on bad input.
 */
nimcp_error_t nimcp_apply_temperature(float* logits, uint32_t n, float T);

/**
 * @brief Numerically stable softmax with temperature.
 *
 * Computes out_probs[i] = exp((logits[i] - max(logits)) / T) / Z, where Z is
 * the normalizing constant. Subtracting the max stabilizes against
 * exp() overflow. Output sums to 1.0 to within ~1e-6 (single precision).
 *
 * @param logits     Length-n input logits (read-only).
 * @param n          Number of classes (must be > 0).
 * @param T          Temperature (must be > 0).
 * @param out_probs  Length-n output buffer for probabilities.
 * @return NIMCP_OK on success.
 */
nimcp_error_t nimcp_softmax_with_temperature(const float* logits,
                                             uint32_t n,
                                             float T,
                                             float* out_probs);

/**
 * @brief Find the temperature that minimizes NLL on held-out (logits, labels).
 *
 * Layout of `logits_array`: row-major, shape [n_samples * n_classes],
 * i.e. the logits for sample i are at logits_array[i * n_classes + 0 .. n_classes-1].
 * Each labels[i] is a class index in [0, n_classes).
 *
 * Search is a coarse grid over [T_MIN, T_MAX] with step 0.1, followed by a
 * golden-section refine inside the bracket containing the minimum. Both NLL
 * and ECE (15-bin) are evaluated at the optimum and ECE is returned for
 * record-keeping.
 *
 * Does NOT modify any brain state. Use nimcp_brain_calibrate_temperature for
 * the brain-aware wrapper.
 *
 * @param logits_array  [n_samples * n_classes] flat array of logits.
 * @param labels        [n_samples] array of class indices.
 * @param n_samples     Number of validation samples (must be > 0).
 * @param n_classes     Number of output classes (must be >= 2).
 * @param out_T         [out] Optimal temperature (always finite, in [0.5, 5.0]).
 * @param out_ece       [out] ECE at the optimum (15-bin Expected Calibration Error).
 * @return NIMCP_OK on success.
 */
nimcp_error_t nimcp_calibrate_temperature(const float* logits_array,
                                          const uint32_t* labels,
                                          uint32_t n_samples,
                                          uint32_t n_classes,
                                          float* out_T,
                                          float* out_ece);

/**
 * @brief Brain-aware wrapper: calibrate then store the result on the brain.
 *
 * Calls nimcp_calibrate_temperature on the supplied data, then writes the
 * resulting T + ECE + timestamp into brain->decoder_temperature,
 * brain->decoder_temperature_calibrated_ece, and
 * brain->decoder_temperature_calibrated_at_us.
 *
 * Layout of `logits_array`: array of n_samples row pointers, each pointing
 * to n_classes logits. (Convenience for callers who already hold per-sample
 * pointers.)
 *
 * @param brain         Brain handle.
 * @param logits_array  [n_samples] array of pointers to per-sample logits.
 * @param labels        [n_samples] array of class indices.
 * @param n_samples     Number of validation samples.
 * @param n_classes     Number of output classes.
 * @return NIMCP_OK on success.
 */
nimcp_error_t nimcp_brain_calibrate_temperature(brain_t brain,
                                                const float** logits_array,
                                                const uint32_t* labels,
                                                uint32_t n_samples,
                                                uint32_t n_classes);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NIMCP_TEMPERATURE_SCALING_H */
