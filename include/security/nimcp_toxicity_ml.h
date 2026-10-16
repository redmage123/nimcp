/**
 * @file nimcp_toxicity_ml.h
 * @brief Small in-process ML toxicity classifier head.
 *
 * WHAT: Two-layer MLP that maps hashed character n-gram features to
 *       (predicted_harm, fairness_violation) scores in [0, 1]. Trained
 *       online with the pattern-based classifier as teacher.
 *
 * WHY:  The pattern classifier is high-precision / low-recall. It catches
 *       blunt structures (group + dehumanizing predicate) but misses coded
 *       or paraphrased toxicity. The ML head generalizes beyond the
 *       enumerated patterns by learning the *distributional* shape of
 *       toxic vs benign text, supervised by the pattern classifier's
 *       verdicts. Over time the ensemble (max of pattern + ml) catches
 *       more than either alone.
 *
 * ARCH: hash(char-trigrams, text) -> [1024 bins]
 *         -> dense(256) + ReLU
 *         -> dense(64)  + ReLU
 *         -> dense(2)   + sigmoid       (harm, fairness)
 *       Total params: ~280K floats (≈1.1MB).
 *
 * TRAIN:
 *   - Online MSE against pattern_classifier scores.
 *   - Step size adapts via SGD with momentum.
 *   - Trained on every classify_with_ml call when teacher labels disagree
 *     by more than dead-zone threshold, and on KG-emit'd training events
 *     replayed through the cycle tick.
 *   - Weights are serialized to data/safety/toxicity_ml.bin (sidecar) on
 *     explicit save, loaded at create if present.
 *
 * POLICY: outputs are SCORES only. Never modifies content; never decides
 * to drop content. Mirror of the pattern classifier's policy. The
 * ensemble takes max(pattern, ml) so neither can lower the other's score.
 *
 * THREAD SAFETY: forward (predict) is lock-free (read-only on weights).
 * Backprop holds an internal mutex; predict()/train() may overlap but
 * concurrent train() calls serialize.
 */
#ifndef NIMCP_TOXICITY_ML_H
#define NIMCP_TOXICITY_ML_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOXICITY_ML_INPUT_DIM    1024
#define TOXICITY_ML_HIDDEN1_DIM  256
#define TOXICITY_ML_HIDDEN2_DIM  64
#define TOXICITY_ML_OUTPUT_DIM   2     /* harm, fairness */

typedef struct toxicity_ml_classifier toxicity_ml_classifier_t;

/**
 * Per-call result.
 */
typedef struct {
    float predicted_harm;     /**< [0, 1] — ML head's harm score.        */
    float fairness_violation; /**< [0, 1] — ML head's fairness score.    */
    float confidence;         /**< max(|harm-0.5|, |fair-0.5|) * 2 ∈ [0,1] */
} toxicity_ml_result_t;

/**
 * Create a fresh ML head. Initializes weights with Xavier-ish randomness.
 * If weights_path is non-NULL and the file exists, weights are loaded;
 * if loading fails, falls back to fresh weights.
 */
toxicity_ml_classifier_t* toxicity_ml_create(const char* weights_path);

/**
 * Destroy. Frees weights + caches.
 */
void toxicity_ml_destroy(toxicity_ml_classifier_t* ml);

/**
 * Forward pass: compute (harm, fairness) for the given text. NUL-terminated.
 * Returns 0 on success, -1 on bad args. Empty text returns (0, 0).
 */
int toxicity_ml_predict(const toxicity_ml_classifier_t* ml,
                         const char* text,
                         toxicity_ml_result_t* out);

/**
 * Online training step. Computes forward, then backward against the
 * target (harm, fairness) labels (typically from the pattern classifier).
 * Returns the post-step MSE loss, or NaN on bad args. Bounded LR.
 *
 * If the ML head's existing prediction is already within `dead_zone` of
 * the target on both outputs, the step is skipped and the function returns
 * the pre-step MSE without modifying weights (no churn).
 */
float toxicity_ml_train_step(toxicity_ml_classifier_t* ml,
                              const char* text,
                              float target_harm,
                              float target_fairness,
                              float learning_rate,
                              float dead_zone);

/**
 * Save weights to disk (atomic write via temp-file + rename). Returns 0 on
 * success.
 */
int toxicity_ml_save(const toxicity_ml_classifier_t* ml,
                      const char* weights_path);

/**
 * Aggregate stats. Any out-pointer may be NULL.
 */
void toxicity_ml_get_stats(const toxicity_ml_classifier_t* ml,
                            uint64_t* out_predictions,
                            uint64_t* out_train_steps,
                            float* out_recent_loss);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOXICITY_ML_H */
