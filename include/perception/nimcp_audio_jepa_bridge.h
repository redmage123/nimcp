/**
 * @file nimcp_audio_jepa_bridge.h
 * @brief Audio JEPA Bridge - environmental audio latent-space predictive coding
 *
 * WHAT: Bridge between the audio cortex and a JEPA predictor for environmental
 *       (non-speech) audio. Parallel to nimcp_speech_jepa_bridge but operates
 *       on mel/MFCC features rather than phoneme frames.
 * WHY:  Self-supervised temporal prediction of environmental audio gives the
 *       audio cortex a cheap "what comes next" signal — useful for scene
 *       change detection, surprise, and downstream consolidation.
 * HOW:  Each training step reads the audio cortex's current training state,
 *       packs top-16 mel + top-16 MFCC channels into a small embedding, and
 *       trains a 1-layer MLP JEPA predictor on (prev_embed -> cur_embed)
 *       transitions.
 *
 * DESIGN NOTES:
 * -------------
 * - Opaque type. Internal struct lives in the .c file.
 * - Non-owning reference to audio_cortex_t (caller owns the cortex).
 * - Owning reference to jepa_predictor_t + 2 jepa_latent_t + prev_embed buf.
 * - No brain integration, no bio-async, no CMakeLists edits — purely
 *   self-contained module that a future caller can wire in.
 * - Typical embed_dim = 32 (16 mel + 16 MFCC, tanh-squashed on the MFCC half,
 *   max-abs-normalized on the mel half).
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Non-primary auditory cortex (belt/parabelt in primates, A2 in rodents) is
 * thought to implement forward models of environmental sound streams, firing
 * strongly at unexpected transitions and suppressing at predicted ones. A
 * JEPA-style latent predictor is a minimal computational stand-in for that
 * predictive-coding circuit.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AUDIO_JEPA_BRIDGE_H
#define NIMCP_AUDIO_JEPA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#include "perception/nimcp_audio_cortex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque audio JEPA bridge handle
 */
typedef struct audio_jepa_bridge_s audio_jepa_bridge_t;

/**
 * @brief Create an audio JEPA bridge.
 *
 * Stores a non-owning reference to @p ac. Allocates a JEPA predictor
 * (input/output/hidden = embed_dim, num_layers=1, lr=1e-3, wd=1e-5,
 * layer-norm on, FEP off), two jepa_latent_t buffers, and a float[embed_dim]
 * buffer to cache the previous embedding.
 *
 * @param ac         Audio cortex to pull training state from (non-owning).
 * @param embed_dim  Embedding dimension. Typical value: 32.
 * @return Bridge handle, or NULL on allocation failure or invalid args.
 */
audio_jepa_bridge_t* audio_jepa_bridge_create(audio_cortex_t* ac,
                                              uint32_t embed_dim);

/**
 * @brief Destroy an audio JEPA bridge. NULL-safe.
 *
 * Frees the JEPA predictor, both latents, the prev_embed buffer, and the
 * bridge struct itself. Does NOT destroy the referenced audio_cortex_t.
 *
 * @param b Bridge to destroy (may be NULL).
 */
void audio_jepa_bridge_destroy(audio_jepa_bridge_t* b);

/**
 * @brief Run one JEPA training step.
 *
 * Fetches the current audio training state from the cortex, packs it into an
 * embed_dim vector, and — if a previous embedding exists — trains the JEPA
 * predictor on (prev -> cur). The current embedding is then saved as the new
 * prev for the next call.
 *
 * @param b         Bridge.
 * @param loss_out  Optional out-param; receives the JEPA loss when training
 *                  actually happened, or 0 when only bootstrapping prev.
 * @return 0 on success (including the first-call bootstrap path),
 *         -1 if args are NULL or the cortex has no valid training state yet.
 */
int audio_jepa_bridge_train_step(audio_jepa_bridge_t* b, float* loss_out);

/**
 * @brief Number of successful training steps performed.
 *
 * The first call (prev-bootstrap only, no JEPA update) does NOT count.
 *
 * @param b Bridge (may be NULL -> returns 0).
 */
uint32_t audio_jepa_bridge_n_steps(const audio_jepa_bridge_t* b);

/**
 * @brief Loss from the most recent actual training step (not bootstrap).
 *
 * @param b Bridge (may be NULL -> returns 0.0f).
 */
float audio_jepa_bridge_last_loss(const audio_jepa_bridge_t* b);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIO_JEPA_BRIDGE_H */
