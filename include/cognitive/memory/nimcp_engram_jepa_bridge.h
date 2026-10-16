/**
 * @file nimcp_engram_jepa_bridge.h
 * @brief JEPA bridge for the engram memory system — predictive coding over
 *        memory-trace transitions in embedding space.
 *
 * WHAT: A thin bridge that wraps a JEPA predictor + two reusable latent
 *       buffers and uses them to model transitions between successive
 *       engram embeddings: given a recently observed engram embedding,
 *       predict the embedding of the NEXT recalled engram.
 *
 * WHY:  Engram recall is sequential — one memory trace tends to cue the
 *       next (systems-level replay, pattern completion, hippocampal
 *       chaining). Learning this transition structure in latent space
 *       gives the memory system a lightweight "next-trace predictor"
 *       that can drive cue completion, replay priors, and novelty
 *       signals without reconstructing raw engram state.
 *
 * HOW:  Caller streams engram embeddings in with
 *       engram_jepa_bridge_record(). The bridge keeps the previous
 *       embedding and, whenever a new one arrives, runs one JEPA
 *       train_step on (prev → current). At any time,
 *       engram_jepa_bridge_predict_next() runs a forward pass to return
 *       the predicted next embedding for an arbitrary embed. The bridge
 *       is agnostic about how engram embeddings are produced — it just
 *       operates on raw float vectors of a caller-chosen dimension
 *       (typically 64 or 128).
 *
 * DESIGN:
 *   - Opaque type; internals in the .c file.
 *   - Single shared predictor (not per-engram) — the transition family
 *     we want to learn is shared across traces.
 *   - Light MLP: input_dim = output_dim = hidden_dim = embed_dim,
 *     num_layers = 1, layer norm on, FEP off (this bridge is standalone).
 *   - All heap allocations go through nimcp_calloc / nimcp_free.
 *
 * Modeled after Phase 4m of the octopus cognitive module (see
 * src/cognitive/octopus/nimcp_octopus.c for the integration template).
 */

#ifndef NIMCP_ENGRAM_JEPA_BRIDGE_H
#define NIMCP_ENGRAM_JEPA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle for the engram-JEPA bridge.
 *
 * Defined in nimcp_engram_jepa_bridge.c. Callers should treat this as
 * a black box and only touch it through the API below.
 */
typedef struct engram_jepa_bridge_s engram_jepa_bridge_t;

/**
 * @brief Create a new engram-JEPA bridge.
 *
 * Allocates the internal JEPA predictor (input_dim = output_dim =
 * hidden_dim = embed_dim, num_layers = 1, lr = 1e-3, weight_decay = 1e-5,
 * layer norm on, FEP off), two reusable jepa_latent_t buffers
 * (context + target), and a prev-embedding buffer of length embed_dim.
 *
 * @param embed_dim Dimension of the engram embedding vectors this
 *                  bridge will operate on. Typically 64 or 128.
 *                  Must be > 0.
 * @return Newly allocated bridge, or NULL on invalid argument or
 *         allocation failure. On failure, no partial state is left
 *         behind.
 */
engram_jepa_bridge_t* engram_jepa_bridge_create(uint32_t embed_dim);

/**
 * @brief Destroy an engram-JEPA bridge and free all owned resources.
 *
 * Safe to pass NULL.
 *
 * @param b Bridge to destroy, or NULL.
 */
void engram_jepa_bridge_destroy(engram_jepa_bridge_t* b);

/**
 * @brief Record a new engram embedding and, if a previous embedding
 *        exists, run one JEPA training step on the (prev → current)
 *        transition.
 *
 * After this call, the provided embed becomes the new "prev" for the
 * next call. On the very first call there is nothing to train on, so
 * only the prev buffer is updated.
 *
 * @param b     Bridge (non-NULL).
 * @param embed Engram embedding vector (non-NULL, length >= dim).
 * @param dim   Length of embed. Must equal the embed_dim the bridge
 *              was created with.
 * @return 0 on success, -1 on invalid arguments or internal failure
 *         (e.g. dim mismatch, JEPA call failure).
 */
int engram_jepa_bridge_record(engram_jepa_bridge_t* b,
                              const float* embed,
                              uint32_t dim);

/**
 * @brief Predict the NEXT engram embedding given a current one.
 *
 * Runs a pure forward pass through the JEPA predictor. No training,
 * no state mutation beyond the reusable internal latent buffers.
 *
 * @param b        Bridge (non-NULL).
 * @param embed    Current engram embedding (non-NULL, length >= dim).
 * @param dim      Length of embed. Must equal the bridge's embed_dim.
 * @param out_pred Output buffer of length >= embed_dim; receives the
 *                 predicted next embedding.
 * @return 0 on success, -1 on invalid arguments or internal failure.
 */
int engram_jepa_bridge_predict_next(engram_jepa_bridge_t* b,
                                    const float* embed,
                                    uint32_t dim,
                                    float* out_pred);

/**
 * @brief Number of completed JEPA training steps.
 *
 * Increments by one each time engram_jepa_bridge_record() successfully
 * trains on a (prev → current) pair. NULL-safe (returns 0).
 *
 * @param b Bridge or NULL.
 * @return Step count, or 0 if b is NULL.
 */
uint32_t engram_jepa_bridge_n_steps(const engram_jepa_bridge_t* b);

/**
 * @brief Last observed JEPA training loss.
 *
 * Updated each time a training step succeeds. Zero if no step has
 * been taken yet. NULL-safe (returns 0.0f).
 *
 * @param b Bridge or NULL.
 * @return Most recent training loss, or 0.0f if b is NULL.
 */
float engram_jepa_bridge_last_loss(const engram_jepa_bridge_t* b);

/**
 * @brief Self-driving tick: same semantics as record() but accepts any
 *        dim via silent truncation / zero-pad against the bridge's
 *        embed_dim. Lets the orchestrator drive this bridge with any
 *        brain-state vector without enforcing a dim match.
 */
int engram_jepa_bridge_tick_from_vec(engram_jepa_bridge_t* b,
                                     const float* vec,
                                     uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENGRAM_JEPA_BRIDGE_H */
