/**
 * @file nimcp_omni_jepa_bridge.h
 * @brief Omni World Model — JEPA Latent-Space Predictor Bridge
 * @version 1.0.0
 * @date 2026-04-20
 *
 * WHAT: Thin bridge wrapping a JEPA predictor for state-transition learning
 *       in the Omni world model. Given a pair of world-model state vectors
 *       (prev_state → cur_state), drives a single JEPA training step in
 *       latent space and exposes a forward-only predict path for use in
 *       rollouts / imagination.
 * WHY:  The Omni world model emits dense state vectors each tick. Learning
 *       a transition model z_t → z_{t+1} directly in latent space (JEPA
 *       style) gives the world model a self-supervised objective that does
 *       not depend on pixel-level reconstruction and composes cleanly with
 *       the existing FEP / active-inference machinery.
 * HOW:  Internally owns one `jepa_predictor_t` plus two reusable
 *       `jepa_latent_t` buffers (context + target). Each train step copies
 *       the caller's raw float vectors into the latent buffers, calls
 *       `jepa_predictor_train_step`, and accumulates a loss EMA. Follows
 *       the same pattern used by the octopus Phase 4m integration in
 *       `src/cognitive/octopus/nimcp_octopus.c`.
 *
 * OWNERSHIP:
 *   - Caller owns the bridge handle and must call
 *     `omni_jepa_bridge_destroy()` to release it.
 *   - The bridge owns its predictor + latent buffers and frees them on
 *     destroy.
 *   - Input float vectors are never retained; they are copied into the
 *     latent buffers per call.
 *
 * THREAD SAFETY:
 *   - Not internally synchronized. The Omni world model is expected to
 *     call the bridge from its own serialized train/inference loop.
 */

#ifndef NIMCP_OMNI_JEPA_BRIDGE_H
#define NIMCP_OMNI_JEPA_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle for the Omni ↔ JEPA predictor bridge.
 *
 * Internals are intentionally hidden; use only the accessors below.
 */
typedef struct omni_jepa_bridge_s omni_jepa_bridge_t;

/**
 * @brief Create a JEPA bridge sized for `state_dim`-dimensional vectors.
 *
 * Builds a `jepa_predictor_t` with
 * `input_dim == output_dim == hidden_dim == state_dim`, a single hidden
 * layer, `learning_rate=1e-3`, `weight_decay=1e-5`, layer-norm enabled,
 * FEP disabled. Allocates two reusable latent buffers of width
 * `state_dim` for context / target.
 *
 * @param state_dim Width of the Omni world-model state vector. Must be > 0.
 * @return Bridge handle on success, NULL on allocation / config failure.
 */
omni_jepa_bridge_t* omni_jepa_bridge_create(uint32_t state_dim);

/**
 * @brief Destroy a bridge created by `omni_jepa_bridge_create`.
 *
 * Releases the underlying JEPA predictor and latent buffers. Safe to
 * call with NULL.
 *
 * @param b Bridge handle (may be NULL).
 */
void omni_jepa_bridge_destroy(omni_jepa_bridge_t* b);

/**
 * @brief Perform one JEPA training step on a (prev_state → cur_state) pair.
 *
 * Copies the caller's float vectors into the bridge's latent buffers,
 * invokes `jepa_predictor_train_step(predictor, ctx, tgt, &loss)`, and
 * updates `n_steps` + `last_loss` on success.
 *
 * @param b          Bridge handle.
 * @param prev_state Context vector (z_t). Must have at least `dim` elements.
 * @param cur_state  Target vector  (z_{t+1}). Must have at least `dim` elements.
 * @param dim        Dimensionality of both vectors. Must equal the
 *                   `state_dim` the bridge was created with.
 * @param loss_out   Optional out-param; populated with the step loss on
 *                   success. May be NULL.
 * @return 0 on success, -1 on NULL inputs, dim mismatch, or predictor error.
 */
int omni_jepa_bridge_train_step(omni_jepa_bridge_t* b,
                                const float* prev_state,
                                const float* cur_state,
                                uint32_t dim,
                                float* loss_out);

/**
 * @brief Forward-only prediction: z_pred = predictor(state).
 *
 * Copies `state` into the context latent buffer, calls
 * `jepa_predictor_predict`, and copies the predicted embedding back into
 * `out_pred`. Does not change any learning counters.
 *
 * @param b        Bridge handle.
 * @param state    Input context vector. Must have at least `dim` elements.
 * @param dim      Dimensionality of both input and output. Must equal
 *                 the bridge's `state_dim`.
 * @param out_pred Caller-allocated output buffer (at least `dim` floats).
 * @return 0 on success, -1 on NULL inputs, dim mismatch, or predictor error.
 */
int omni_jepa_bridge_predict(omni_jepa_bridge_t* b,
                             const float* state,
                             uint32_t dim,
                             float* out_pred);

/**
 * @brief Total number of successful training steps observed by the bridge.
 *
 * @param b Bridge handle (may be NULL → returns 0).
 * @return Cumulative training-step count.
 */
uint32_t omni_jepa_bridge_n_steps(const omni_jepa_bridge_t* b);

/**
 * @brief Most recent training-step loss reported by the predictor.
 *
 * @param b Bridge handle (may be NULL → returns 0.0f).
 * @return Last step loss, or 0.0f if no successful step has occurred yet.
 */
float omni_jepa_bridge_last_loss(const omni_jepa_bridge_t* b);

/**
 * @brief Self-driving tick: internally maintains a rolling prev embed.
 *        First call stores prev; later calls train on (prev → cur) then
 *        roll prev forward. Accepts dim != state_dim via silent
 *        truncation/zero-pad so generic brain-state vectors work.
 *
 * Call once per brain step with the current state vector to drive
 * continuous training without threading prev through call sites.
 *
 * @return 0 on successful training step, -1 otherwise (no prev yet, any
 *         sub-call failure). Counter only advances on real training.
 */
int omni_jepa_bridge_tick_from_vec(omni_jepa_bridge_t* b,
                                    const float* cur,
                                    uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_JEPA_BRIDGE_H */
