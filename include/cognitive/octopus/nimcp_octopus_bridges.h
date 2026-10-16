/**
 * @file nimcp_octopus_bridges.h
 * @brief Phase 2a bridges that wire the octopus module to peer subsystems.
 *
 * The octopus module exposes six hook slots (ethics / swarm / world / fep /
 * bio / immune) via octopus_set_*_hook(). This header declares a single
 * install function that populates all of them with wrappers around the
 * corresponding brain subsystems.
 *
 * SOLID notes:
 *  - SRP: each hook is a thin wrapper, one responsibility
 *  - OCP: adding a new hook means adding a new wrapper fn, no changes
 *    to the octopus core module
 *  - DIP: octopus core depends on the hook signatures, not these wrappers
 *
 * Called from the brain factory's octopus init after the octopus itself
 * is created and brain->{ethics,bio_router,...} subsystems are available.
 * Order-sensitive: bridges must install AFTER their peer subsystems are up.
 */
#ifndef NIMCP_OCTOPUS_BRIDGES_H
#define NIMCP_OCTOPUS_BRIDGES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Install all octopus hooks on the given brain's octopus module.
 *
 * Safe to call multiple times (idempotent). Safe if any peer subsystem is
 * NULL — that hook is just left unbound (octopus treats NULL hooks as no-op).
 *
 * @param brain Brain whose octopus module should be wired up.
 * @return true if at least one hook was bound; false on invalid input.
 */
bool nimcp_octopus_install_bridges(brain_t brain);

/**
 * @brief Tear down any bio_router registration + hook state.
 *
 * Safe to call on a brain that never had bridges installed.
 * Must be called BEFORE the bridge state is freed; lifecycle.c handles
 * this ordering (call uninstall, then nimcp_free the state).
 */
void nimcp_octopus_uninstall_bridges(brain_t brain);

/**
 * @brief Phase 4b: sample current occipital/visual cortex activity into a
 *        fixed-length feature vector suitable for feeding to octopus_explore().
 *
 * Packs a compact 64-channel summary of current visual state:
 *   [0..7]   V1 orientation histogram (normalized)
 *   [8..15]  Per-area feature-count density (V1..V5, padded)
 *   [16..23] Top-8 visual feature strengths (any area)
 *   [24..31] Top-8 motion-vector magnitudes from V5/MT
 *   [32..47] Top-8 color-percept hues + saturations from V4 (interleaved)
 *   [48..55] Global motion vector (dx/dy) and processing-time proxies
 *   [56..63] Padding / reserved
 *
 * Non-populated channels are zeroed. Out-of-range out_dim > 64 is clamped.
 *
 * @param brain   Brain instance (must have non-NULL brain->occipital).
 * @param out_vec Caller-provided buffer of length >= out_dim.
 * @param out_dim Target number of channels (clamped to [1, 64]).
 * @return Number of channels written (0 if occipital unavailable).
 */
uint32_t nimcp_octopus_sample_occipital_vec(brain_t brain,
                                            float* out_vec,
                                            uint32_t out_dim);

/**
 * @brief Phase 4b: convenience — sample occipital features and call
 *        octopus_explore() on the packed vision vector. Increments the
 *        bridge's vision_samples counter.
 *
 * @param brain Brain instance. Must have brain->octopus and brain->occipital.
 * @return 0 on success; -1 if occipital or octopus unavailable.
 */
int nimcp_octopus_explore_from_occipital(brain_t brain);

/**
 * @brief Phase 4c: sample current audio cortex (A1) activity into a
 *        fixed-length feature vector suitable for octopus_explore().
 *
 * Reads the cortex's cached training-state snapshot (last forward pass):
 *   [ 0 .. 15]  Mel filterbank features (truncated / zero-padded)
 *   [16 .. 31]  MFCC coefficients (truncated / zero-padded)
 *   [32 .. 47]  Reserved / zero (future spectrum buckets)
 *   [48]        Audio quality [0,1]
 *   [49]        Speech salience [0,1]
 *   [50]        Temporal coherence [0,1]
 *   [51]        log1p(frames_processed) normalized
 *   [52]        log1p(memories_stored) normalized
 *   [53]        tanh(avg_processing_time_ms)
 *   [54 .. 63]  Reserved (zero)
 *
 * Returns 0 if audio cortex is unavailable or no valid state exists yet.
 *
 * @param brain   Brain instance.
 * @param out_vec Caller-provided buffer of length >= out_dim.
 * @param out_dim Target number of channels (clamped to [1, 64]).
 * @return Number of channels written; 0 on unavailable/invalid state.
 */
uint32_t nimcp_octopus_sample_audio_cortex_vec(brain_t brain,
                                                float* out_vec,
                                                uint32_t out_dim);

/**
 * @brief Phase 4c: sample audio cortex + call octopus_explore().
 *
 * @param brain Brain instance. Must have brain->octopus and brain->audio_cortex.
 * @return 0 on success; -1 if audio or octopus unavailable.
 */
int nimcp_octopus_explore_from_audio_cortex(brain_t brain);

/**
 * @brief Phase 4d: sample current somatosensory cortex (S1/S2) state into
 *        a fixed-length feature vector suitable for octopus_explore().
 *
 * This is the strongest biological fit — the octopus arm module is modeled
 * on cephalopod peripheral cognition, and somatosensory input IS arm skin-
 * receptor input (touch/pain/proprioception/temperature).
 *
 *   [ 0 .. 15]  Per-segment pain level (first 16 segments)
 *   [16 .. 31]  Per-segment temperature (normalized temp_sensation_t / 6)
 *   [32 .. 47]  Per-segment position magnitude (L2 of xyz)
 *   [48]        Total pain (clamp01)
 *   [49]        Max-segment pain intensity
 *   [50]        log1p(num_active_segments) normalized
 *   [51 .. 63]  Reserved
 *
 * @param brain   Brain instance (must have non-NULL brain->somatosensory).
 * @param out_vec Caller-provided buffer of length >= out_dim.
 * @param out_dim Target number of channels (clamped to [1, 64]).
 * @return Number of channels written; 0 on unavailable state.
 */
uint32_t nimcp_octopus_sample_somatosensory_vec(brain_t brain,
                                                 float* out_vec,
                                                 uint32_t out_dim);

/**
 * @brief Phase 4d: sample somatosensory + call octopus_explore().
 *
 * @param brain Brain instance. Must have brain->octopus and brain->somatosensory.
 * @return 0 on success; -1 if somatosensory or octopus unavailable.
 */
int nimcp_octopus_explore_from_somatosensory(brain_t brain);

/**
 * @brief Phase 4e: sample SNN spike-activity into a fixed-length feature
 *        vector suitable for octopus_explore().
 *
 * The octopus arms consume subsymbolic spiking dynamics from the 1.8M-neuron
 * SNN. Per-population firing rates act as fast population-code features;
 * network-level stats (sparsity, synchrony, health) give arms awareness of
 * the substrate's state of wellness.
 *
 *   [ 0 .. 31]  Per-population firing rate (rate_hz/50, clamp01); first 32 pops
 *   [32 .. 47]  Reserved per-population (padding / future pops)
 *   [48]        mean_firing_rate / 50 clamp01
 *   [49]        max_firing_rate / 200 clamp01
 *   [50]        sparsity (already [0,1])
 *   [51]        synchrony (already [0,1])
 *   [52]        health enum / 6
 *   [53]        log1p(silent_neurons) / log1p(1e6)
 *   [54]        log1p(hyperactive_neurons) / log1p(1e6)
 *   [55]        log1p(total_spikes) / log1p(1e9)
 *   [56 .. 63]  Reserved
 *
 * @param brain   Brain instance (must have non-NULL brain->snn_network).
 * @param out_vec Caller-provided buffer of length >= out_dim.
 * @param out_dim Target number of channels (clamped to [1, 64]).
 * @return Number of channels written; 0 on unavailable state.
 */
uint32_t nimcp_octopus_sample_snn_vec(brain_t brain,
                                       float* out_vec,
                                       uint32_t out_dim);

/**
 * @brief Phase 4e: sample SNN + call octopus_explore().
 */
int nimcp_octopus_explore_from_snn(brain_t brain);

/**
 * @brief Phase 4f: sample global neuromodulator concentrations into a
 *        fixed-length feature vector for octopus_explore().
 *
 * Biologically correct integration — neuromodulators (DA/5-HT/ACh/NE/GABA/
 * GLU) bathe the whole brain and should gate arm-level exploration too.
 *
 *   [0] dopamine
 *   [1] serotonin
 *   [2] acetylcholine
 *   [3] norepinephrine
 *   [4] gaba
 *   [5] glutamate
 *   [6 .. 63] Reserved
 *
 * @param brain   Brain instance (must have neuromodulator_system).
 * @param out_vec Caller-provided buffer of length >= out_dim.
 * @param out_dim Target number of channels (clamped to [1, 64]).
 * @return Number of channels written; 0 on unavailable state.
 */
uint32_t nimcp_octopus_sample_neuromod_vec(brain_t brain,
                                            float* out_vec,
                                            uint32_t out_dim);

/**
 * @brief Phase 4f: sample neuromodulators + call octopus_explore().
 */
int nimcp_octopus_explore_from_neuromod(brain_t brain);

/**
 * @brief Phase 4g: sample peer cognitive modules (Portia + Dragonfly) into
 *        a feature vector. Completes the delegation/attention/adaptation
 *        triad the octopus was designed around.
 *
 *   [ 0 ..  9]  Dragonfly mode, tracking flags, TSDN direction/elevation/
 *               magnitude, angular velocity, time-to-intercept
 *   [10 .. 15]  Reserved
 *   [16 .. 23]  Portia tier/power/thermal/degradation + CPU/memory/battery/temp
 *   [24 .. 63]  Reserved
 *
 * Returns 0 if neither peer is available. If only one is available, its
 * block is populated and the other stays zero.
 */
uint32_t nimcp_octopus_sample_peers_vec(brain_t brain,
                                         float* out_vec,
                                         uint32_t out_dim);

/**
 * @brief Phase 4g: sample peers + call octopus_explore().
 */
int nimcp_octopus_explore_from_peers(brain_t brain);

/**
 * @brief Phase 5A: drive the octopus per brain tick.
 *
 * This is THE integration that turns the octopus module from a passive
 * statue into an always-on peripheral cognition layer. Called from
 * brain_decide and brain_learn_vector so every inference and training
 * step exercises the arms.
 *
 * Per call:
 *   - octopus_explore(brain->octopus, features, num_features)
 *     (arm forwards run, arm LNN state advances, hooks fire)
 *   - octopus_integrate(...) to produce the aggregated latent
 *     (world/FEP/bio hooks fire)
 *   - On cadence: octopus_train_step() if training is enabled
 *   - On cadence: octopus_ng_regularize() if NG is enabled
 *
 * Safe to call on any brain — becomes a no-op when brain->octopus is NULL
 * or features is NULL/empty. Never throws.
 *
 * @param brain        Brain instance (may be NULL — no-op).
 * @param features     Input feature vector (may be NULL — no-op).
 * @param num_features Length of features.
 */
void nimcp_octopus_tick(brain_t brain,
                        const float* features,
                        uint32_t num_features);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCTOPUS_BRIDGES_H */
