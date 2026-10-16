/**
 * @file nimcp_consolidation_jepa_bridge.h
 * @brief JEPA bridge for the memory consolidation system — predictive
 *        coding over consolidation-event transitions in embedding space.
 *
 * WHAT: A thin bridge that wraps a JEPA predictor + two reusable latent
 *       buffers and uses them to model transitions between the memory
 *       state BEFORE a consolidation event (e.g. replay / systems-level
 *       transfer / sharp-wave ripple) and the memory state AFTER it.
 *       Given a pre-consolidation embedding, predict the post-
 *       consolidation embedding.
 *
 * WHY:  Consolidation reshapes memory traces in structured, learnable
 *       ways (abstraction, interference reduction, pattern separation,
 *       cortical integration). Learning this pre→post mapping in latent
 *       space gives the memory system a lightweight "consolidation
 *       outcome predictor" that can be used to (a) forecast the result
 *       of consolidation before committing to it, (b) score the quality
 *       of an observed consolidation event against the model's
 *       expectation, and (c) provide a prior for selective replay.
 *
 * HOW:  Caller streams (pre, post) pairs in with
 *       consolidation_jepa_bridge_record(). Each call runs exactly one
 *       JEPA train_step on (pre → post). There is no internal prev
 *       buffer: consolidation events are intrinsically pre/post pairs
 *       and the caller supplies both. At any time,
 *       consolidation_jepa_bridge_predict_outcome() runs a forward pass
 *       to return the predicted post-consolidation embedding for an
 *       arbitrary pre-consolidation embed.
 *       consolidation_jepa_bridge_quality() compares an observed post
 *       against the model's prediction and returns a bounded score.
 *
 * DESIGN:
 *   - Opaque type; internals in the .c file.
 *   - Single shared predictor (not per-event) — the consolidation
 *     transformation family we want to learn is shared across events.
 *   - Light MLP: input_dim = output_dim = hidden_dim = embed_dim,
 *     num_layers = 1, layer norm on, FEP off (this bridge is standalone).
 *   - All heap allocations go through nimcp_calloc / nimcp_free.
 *
 * Modeled after nimcp_engram_jepa_bridge (which operates on successive
 * engram embeddings). The consolidation variant differs only in that
 * (pre, post) pairs are provided explicitly per call rather than
 * reconstructed from a rolling prev buffer.
 */

#ifndef NIMCP_CONSOLIDATION_JEPA_BRIDGE_H
#define NIMCP_CONSOLIDATION_JEPA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle for the consolidation-JEPA bridge.
 *
 * Defined in nimcp_consolidation_jepa_bridge.c. Callers should treat
 * this as a black box and only touch it through the API below.
 */
typedef struct consolidation_jepa_bridge_s consolidation_jepa_bridge_t;

/**
 * @brief Create a new consolidation-JEPA bridge.
 *
 * Allocates the internal JEPA predictor (input_dim = output_dim =
 * hidden_dim = embed_dim, num_layers = 1, lr = 1e-3, weight_decay = 1e-5,
 * layer norm on, FEP off) and two reusable jepa_latent_t buffers
 * (context + target).
 *
 * @param embed_dim Dimension of the consolidation-event embedding
 *                  vectors this bridge will operate on. Typically 64
 *                  or 128. Must be > 0.
 * @return Newly allocated bridge, or NULL on invalid argument or
 *         allocation failure. On failure, no partial state is left
 *         behind.
 */
consolidation_jepa_bridge_t* consolidation_jepa_bridge_create(uint32_t embed_dim);

/**
 * @brief Destroy a consolidation-JEPA bridge and free all owned
 *        resources.
 *
 * Safe to pass NULL.
 *
 * @param b Bridge to destroy, or NULL.
 */
void consolidation_jepa_bridge_destroy(consolidation_jepa_bridge_t* b);

/**
 * @brief Record a consolidation event and run one JEPA training step
 *        on the (pre → post) transition.
 *
 * Unlike the engram variant, both endpoints are supplied per call —
 * the bridge does not buffer a "previous" embedding.
 *
 * @param b          Bridge (non-NULL).
 * @param pre_embed  Pre-consolidation embedding (non-NULL, length >= dim).
 * @param post_embed Post-consolidation embedding (non-NULL, length >= dim).
 * @param dim        Length of both embeddings. Must equal the embed_dim
 *                   the bridge was created with.
 * @param loss_out   Optional output — if non-NULL, receives the training
 *                   step loss on success. Undefined on failure.
 * @return 0 on success, -1 on invalid arguments or internal failure
 *         (e.g. dim mismatch, JEPA call failure).
 */
int consolidation_jepa_bridge_record(consolidation_jepa_bridge_t* b,
                                     const float* pre_embed,
                                     const float* post_embed,
                                     uint32_t dim,
                                     float* loss_out);

/**
 * @brief Predict the POST-consolidation embedding given a PRE-
 *        consolidation one.
 *
 * Runs a pure forward pass through the JEPA predictor. No training,
 * no state mutation beyond the reusable internal latent buffers.
 *
 * @param b         Bridge (non-NULL).
 * @param pre_embed Pre-consolidation embedding (non-NULL, length >= dim).
 * @param dim       Length of pre_embed. Must equal the bridge's embed_dim.
 * @param out_pred  Output buffer of length >= embed_dim; receives the
 *                  predicted post-consolidation embedding.
 * @return 0 on success, -1 on invalid arguments or internal failure.
 */
int consolidation_jepa_bridge_predict_outcome(consolidation_jepa_bridge_t* b,
                                              const float* pre_embed,
                                              uint32_t dim,
                                              float* out_pred);

/**
 * @brief Score the quality of an observed consolidation event.
 *
 * Internally runs predict_outcome(pre_embed), computes the mean-
 * squared-error between the JEPA prediction and the observed
 * post-consolidation embedding, and returns
 *
 *     quality = 1.0f / (1.0f + mse)
 *
 * Range: (0, 1]. Values close to 1 indicate the observed outcome
 * matches the bridge's learned expectation; values close to 0
 * indicate a surprising / anomalous consolidation event.
 *
 * @param b             Bridge (non-NULL).
 * @param pre_embed     Pre-consolidation embedding (non-NULL, length >= dim).
 * @param observed_post Observed post-consolidation embedding (non-NULL,
 *                      length >= dim).
 * @param dim           Length of both embeddings. Must equal the
 *                      bridge's embed_dim.
 * @return Quality in (0, 1] on success, 0.0f on any error (invalid
 *         arguments, dim mismatch, internal failure).
 */
float consolidation_jepa_bridge_quality(consolidation_jepa_bridge_t* b,
                                        const float* pre_embed,
                                        const float* observed_post,
                                        uint32_t dim);

/**
 * @brief Number of completed JEPA training steps.
 *
 * Increments by one each time consolidation_jepa_bridge_record()
 * successfully trains on a (pre → post) pair. NULL-safe (returns 0).
 *
 * @param b Bridge or NULL.
 * @return Step count, or 0 if b is NULL.
 */
uint32_t consolidation_jepa_bridge_n_steps(const consolidation_jepa_bridge_t* b);

/**
 * @brief Last observed JEPA training loss.
 *
 * Updated each time a training step succeeds. Zero if no step has
 * been taken yet. NULL-safe (returns 0.0f).
 *
 * @param b Bridge or NULL.
 * @return Most recent training loss, or 0.0f if b is NULL.
 */
float consolidation_jepa_bridge_last_loss(const consolidation_jepa_bridge_t* b);

/**
 * @brief Self-driving tick: internal prev snapshot + (prev → cur) training.
 *        Accepts any dim (truncation/zero-pad against embed_dim). Used by
 *        the brain-level JEPA orchestrator to drive this bridge from a
 *        single call site with whatever state vector is available.
 */
int consolidation_jepa_bridge_tick_from_vec(consolidation_jepa_bridge_t* b,
                                             const float* vec,
                                             uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONSOLIDATION_JEPA_BRIDGE_H */
