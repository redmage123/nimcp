/**
 * @file nimcp_neuromod_jepa_bridge.h
 * @brief JEPA latent-space predictor bridge for the neuromodulator system.
 *
 * WHAT: Wraps a small JEPA predictor (6-dim context -> 6-dim target) that
 *       learns the one-step transition dynamics of the 6 global
 *       neuromodulator levels (DA, 5-HT, ACh, NE, GABA, GLU). Each
 *       train_step call reads the current 6-dim level vector from the
 *       neuromodulator system, compares against the previously stored
 *       snapshot, trains the JEPA predictor on the (prev -> current) pair,
 *       and then stores current as the new prev.
 *
 * WHY:  Predictive coding on the neuromodulator channel gives the brain a
 *       cheap, always-on self-model of its own chemical trajectory.
 *       Surprise in this latent is a natural salience signal and can be
 *       fed into higher-level control without touching the neuromodulator
 *       system itself.
 *
 * HOW:  Non-owning pointer to a neuromodulator_system_t. A shared
 *       jepa_predictor_t plus two 6-dim jepa_latent_t buffers (one for
 *       context, one for target) are reused across every training step.
 *       All allocation goes through nimcp_calloc / nimcp_free.
 *
 * DESIGN:
 * - Opaque handle; internal layout is not exposed.
 * - Channel slotting is stable: [0]=DA, [1]=5-HT, [2]=ACh, [3]=NE,
 *   [4]=GABA, [5]=GLU -- matches the first six NEUROMOD_* enum values.
 * - No modification of the neuromodulator system -- this is read-only on
 *   the modulator side. Levels are clamped to [0,1] defensively before
 *   being fed into the JEPA latents.
 * - Does not own the neuromodulator system; caller is responsible for
 *   destroying both (bridge first, then system).
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_JEPA_BRIDGE_H
#define NIMCP_NEUROMOD_JEPA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dimensionality of the neuromod->JEPA latent (6 channels).
 */
#define NIMCP_NEUROMOD_JEPA_DIM 6

/**
 * @brief Opaque bridge handle.
 */
typedef struct neuromod_jepa_bridge_s neuromod_jepa_bridge_t;

/**
 * @brief Create a neuromod->JEPA bridge.
 *
 * WHAT: Allocates the bridge struct, the JEPA predictor (6/6/6, 1 hidden
 *       layer, lr=1e-3, wd=1e-5, layer-norm on, FEP off), and two 6-dim
 *       JEPA latent buffers.
 * WHY:  One-shot setup so train_step is zero-alloc on the hot path.
 *
 * @param sys Non-owning neuromodulator system handle. If NULL, returns NULL.
 * @return New bridge on success, NULL on any allocation failure. On
 *         partial-failure the function tears down everything it allocated
 *         before returning NULL; no leaks.
 */
neuromod_jepa_bridge_t* neuromod_jepa_bridge_create(neuromodulator_system_t sys);

/**
 * @brief Destroy the bridge and free all owned resources.
 *
 * WHAT: Destroys the JEPA predictor, both latent buffers, and the bridge
 *       struct itself. The neuromodulator system is NOT touched (non-owning).
 *
 * @param b Bridge to destroy (NULL-safe).
 */
void neuromod_jepa_bridge_destroy(neuromod_jepa_bridge_t* b);

/**
 * @brief Run one JEPA training step on the 6-dim neuromodulator trajectory.
 *
 * WHAT: Reads the current 6-dim level vector (DA, 5-HT, ACh, NE, GABA, GLU),
 *       clamps each to [0,1], and if a previous snapshot exists trains the
 *       JEPA predictor on (prev -> current). Then stores current as the new
 *       prev for the next call.
 *
 * WHY:  Continuous predictive coding on the modulator trajectory -- surprise
 *       in this latent is a cheap self-model signal.
 *
 * @param b Bridge.
 * @param loss_out Optional out-param; set to the step loss on success, or
 *                 0.0f if no training step was performed (first call).
 * @return 0 on success (a training step ran), -1 if no prev snapshot yet
 *         (first call -- snapshot captured but no training) or if the
 *         system handle is missing.
 */
int neuromod_jepa_bridge_train_step(neuromod_jepa_bridge_t* b, float* loss_out);

/**
 * @brief Total number of successful training steps performed.
 *
 * @param b Bridge.
 * @return Step count (0 if b is NULL).
 */
uint32_t neuromod_jepa_bridge_n_steps(const neuromod_jepa_bridge_t* b);

/**
 * @brief Loss from the most recent successful training step.
 *
 * @param b Bridge.
 * @return Loss value, or 0.0f if no step has run yet or b is NULL.
 */
float neuromod_jepa_bridge_last_loss(const neuromod_jepa_bridge_t* b);

#ifdef __cplusplus
}
#endif

#endif  /* NIMCP_NEUROMOD_JEPA_BRIDGE_H */
