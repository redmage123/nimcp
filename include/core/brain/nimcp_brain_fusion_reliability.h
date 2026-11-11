/**
 * @file nimcp_brain_fusion_reliability.h
 * @brief Reliability-weighted ANN/SNN output fusion (2026-11-11)
 *
 * WHY: perform_forward_pass blends the decision output as 0.7·ANN + 0.3·SNN.
 * The SNN term is the output population's firing-rate vector: non-negative,
 * quantised to {0, 10, 20} Hz, norm ~134 and uncorrelated with the training
 * targets. Once the ANN's output shrinks toward the target scale the SNN term
 * outweighs it ~11x, so decide_full stays at cos ~0.03 with its targets while
 * the ANN alone reaches 0.39 (overfit probe trace, Vast, 2026-11-11).
 *
 * Like reliability-weighted cue combination, each pathway is weighted by how
 * well it has been predicting: an EMA of cos(pathway output, learn target)
 * measured in brain_learn_vector before each update. The SNN rate vector is
 * centred and scaled to the ANN's norm before blending so the weights are
 * shares rather than being overridden by magnitude.
 *
 * Enabled by NIMCP_FUSION_RELIABILITY=1. Off (one cached getenv) otherwise,
 * and then nothing here is called with effect: the 0.7/0.3 blend is unchanged.
 */
#ifndef NIMCP_BRAIN_FUSION_RELIABILITY_H
#define NIMCP_BRAIN_FUSION_RELIABILITY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/** True when NIMCP_FUSION_RELIABILITY=1 (cached after the first call). */
bool brain_fusion_rel_enabled(void);

/** Fold the ANN's pre-update cos(output, target) from the latest learn step
 *  into its reliability EMA. No-op when disabled or the ANN did not train. */
void brain_fusion_rel_observe_ann(struct brain_struct* brain);

/** Read the SNN output population's current rates, centre them, and fold
 *  their cos with @p target into the SNN reliability EMA. Call right after
 *  the SNN training step of brain_learn_vector. No-op when disabled. */
void brain_fusion_rel_observe_snn(struct brain_struct* brain, const float* target,
                                  uint32_t target_size);

/** Blend @p snn into @p out (which holds the ANN output) in place, using the
 *  reliability weights. Returns false (and leaves @p out untouched) when
 *  disabled or no reliability has been observed yet; the caller then applies
 *  its default blend. @p w_out (optional, 2 floats) receives {w_ann, w_snn}. */
bool brain_fusion_rel_blend(struct brain_struct* brain, float* out, const float* snn,
                            uint32_t n, float* w_out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_FUSION_RELIABILITY_H */
