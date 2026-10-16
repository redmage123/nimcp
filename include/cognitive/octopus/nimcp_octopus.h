/**
 * @file nimcp_octopus.h
 * @brief Octopus cognitive module — distributed peripheral cognition
 *
 * Inspired by cephalopod neuroanatomy: 2/3 of an octopus's neurons live in
 * its arms, not its central brain. Each arm solves problems locally, then
 * reports results up. No top-down plan-then-execute — arms act on local
 * information with loose central coordination.
 *
 * For Athena, this module implements:
 *   - N semi-autonomous "arm agents", each with local state and a small
 *     history buffer (recent inputs + recent decisions)
 *   - Local problem-solving per arm: each arm gets a slice of input and
 *     computes a local response independently
 *   - A central aggregator that integrates arm reports into a coherent
 *     "octopus response"
 *   - Chemosensory-tactile fusion: an arm's somato input is treated as
 *     both texture AND chemical identity (single-channel semantic sense)
 *   - State broadcasting: arm activity states are externally observable
 *     (analogous to octopus skin-color signaling)
 *
 * Role in the wider cognitive architecture:
 *   - Complements Portia (adaptation) and Dragonfly (attention) with
 *     DELEGATION — peripheral agents act first, center integrates after
 *   - Bridges to swarm (octopus-arm ↔ swarm-edge federation)
 *   - Ethics gates each arm decision before aggregation
 *   - World-model ingests arm-level observations as local hypotheses
 *   - FEP uses arm divergence as a surprise/free-energy signal
 *
 * See include/dragonfly/nimcp_dragonfly.h for the peer design pattern.
 */
#ifndef NIMCP_OCTOPUS_H
#define NIMCP_OCTOPUS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default number of arms — biological octopus has 8. More arms = more
 * parallelism but more aggregation overhead. Range [2, 16]. */
#define OCTOPUS_DEFAULT_N_ARMS 8

/* Per-arm history window — recent inputs + decisions retained for the
 * arm's local continuity. Bumped from 16 → 64 in Phase 3b so that
 * fractal_dfa() can actually compute (FRACTAL_MIN_SAMPLES == 64).
 * Memory: 64 × 64 × 4 B = 16 KB/arm × 16 max arms = 256 KB worst case. */
#define OCTOPUS_ARM_HISTORY    64

/* Arm input/output dimension. Matches typical feature-vector slices. */
#define OCTOPUS_ARM_DIM        64

typedef struct octopus_arm_s {
    uint32_t id;
    /* Recent input slices — ring buffer. */
    float    recent_inputs[OCTOPUS_ARM_HISTORY][OCTOPUS_ARM_DIM];
    uint32_t history_head;
    /* Local decision latent — an arm's "opinion" after processing its
     * slice. This is what the central aggregator reads. */
    float    latent[OCTOPUS_ARM_DIM];
    float    confidence;      /**< 0..1; how strongly the arm commits  */
    /* Broadcast state — externally observable arm activity level
     * (analogous to skin color). Any external module can read this. */
    float    broadcast_state; /**< 0..1; 0=quiet, 1=highly active      */
    /* Chemosensory-tactile fusion: for this arm, the most recent somato
     * input is treated as encoding BOTH texture and chemical identity.
     * Downstream modules read `chemo_id` to get a 32-dim fingerprint. */
    float    chemo_id[32];
    /* Local plasticity-like statistic: running variance of this arm's
     * latents. High variance = arm is in an exploratory phase. */
    float    latent_variance;
    /* Phase 3a: Shannon entropy over the arm's softmaxed latent.
     * Higher entropy = more uncertainty in the arm's local opinion.
     * Complements latent_variance with a principled info-theoretic
     * measure (nimcp_entropy from utils/math). */
    float    shannon_entropy;
    /* Phase 3a: DFA exponent of this arm's recent latent norms.
     * ~1.0 = biologically-healthy pink-noise signal; drifts toward 0.5
     * (white) or 1.5 (random walk) indicate pathology. Updated
     * lazily — only when the history ring has been fully populated
     * at least once. 0.0 = not yet computed. */
    float    dfa_exponent;
    /* Arm-local ethics veto state. If nonzero, this arm's last
     * decision was blocked by the ethics gate and should not contribute
     * to aggregation. */
    bool     vetoed;
} octopus_arm_t;

/**
 * @brief Opaque handle — octopus module state.
 * Full struct is defined in nimcp_octopus.c.
 */
typedef struct octopus_system_s octopus_system_t;

/**
 * @brief Statistics snapshot for introspection / monitoring.
 */
typedef struct octopus_stats_s {
    uint32_t n_arms;
    uint32_t n_explorations;         /**< Total explore() calls         */
    uint32_t n_integrations;         /**< Total integrate() calls       */
    uint32_t n_ethics_vetoes;        /**< Arm decisions blocked         */
    uint32_t n_swarm_delegations;    /**< Delegations to swarm edges    */
    uint32_t n_world_model_updates;  /**< World model observations sent */
    float    avg_arm_confidence;     /**< Mean arm.confidence           */
    float    avg_arm_variance;       /**< Mean arm.latent_variance      */
    float    central_coherence;      /**< How aligned arms are [0,1]    */
    /* Phase 3a measurements. */
    float    avg_arm_entropy;        /**< Mean arm.shannon_entropy      */
    float    avg_arm_dfa;            /**< Mean arm.dfa_exponent (>0)    */
    uint32_t n_dfa_computations;     /**< Times DFA has been updated    */
    uint32_t n_pink_noise_injections;/**< Times pink-noise was applied  */
    /* Phase 3b/3c bridge counters — populated by the bridges TU (not
     * octopus core), exposed here so a single stats query covers both. */
    uint64_t bridge_engram_encodings;   /**< Engrams encoded on low-coherence    */
    uint64_t bridge_kg_nodes_added;     /**< KG nodes added on delegations       */
    uint64_t bridge_stress_broadcasts;  /**< Hypothalamus cortisol broadcasts    */
    uint64_t bridge_fear_broadcasts;    /**< Amygdala fear-intensity broadcasts  */
    uint64_t bridge_amygdala_steps;     /**< Times amygdala_step was driven      */
    float    bridge_last_cortisol;      /**< Latest cortisol reading [0,1]       */
    float    bridge_last_fear;          /**< Latest fear intensity [0,1]         */
    uint64_t bridge_wm_updates;         /**< Direct omni_wm_update calls (Phase 4a) */
    uint64_t bridge_fear_conditionings; /**< Veto→amygdala stimulus count (Phase 4a) */
    uint64_t bridge_vision_samples;     /**< Occipital samples injected as arm input (Phase 4b) */
    uint64_t bridge_audio_samples;      /**< Audio cortex samples injected as arm input (Phase 4c) */
    uint64_t bridge_somato_samples;     /**< Somatosensory samples injected as arm input (Phase 4d) */
    uint64_t bridge_snn_samples;        /**< SNN spike-activity samples injected as arm input (Phase 4e) */
    uint64_t bridge_neuromod_samples;   /**< Neuromodulator samples injected as arm input (Phase 4f) */
    uint64_t bridge_peer_samples;       /**< Portia+Dragonfly peer samples injected as arm input (Phase 4g) */
    /* Phase 4h — per-arm LNN temporal integration. */
    uint32_t n_lnn_steps;               /**< Total LNN forward_step calls across all arms */
    bool     lnn_enabled;               /**< true if per-arm LNN cells are live */
    /* Phase 4j — gradient flow on arm LNN cells. */
    uint32_t n_lnn_train_steps;         /**< Total LNN training steps across all arms */
    float    last_lnn_loss;             /**< Most recent training loss (MSE) */
    bool     lnn_training_enabled;      /**< true if gradient descent is live */
    /* Phase 4l — natural-gradient regularization on arm latents. */
    uint32_t n_ng_steps;                /**< Total natural-gradient steps across all arms */
    bool     ng_enabled;                /**< true if NG preconditioner is live */
    /* Phase 4m — JEPA latent-space predictive coding. */
    uint32_t n_jepa_steps;              /**< Total JEPA predictor training steps */
    float    last_jepa_loss;            /**< Most recent JEPA loss (latent-space MSE) */
    bool     jepa_enabled;              /**< true if JEPA latent training is live */
} octopus_stats_t;

/*============================================================================
 * Lifecycle
 *==========================================================================*/

/**
 * @brief Create an octopus cognitive system.
 *
 * @param n_arms Number of arms (clamped to [2,16]; 0 → default 8)
 * @return Opaque handle, or NULL on OOM.
 */
NIMCP_EXPORT octopus_system_t* octopus_create(uint32_t n_arms);

/**
 * @brief Tear down the octopus system. Releases all arm state.
 */
NIMCP_EXPORT void octopus_destroy(octopus_system_t* ctx);

/*============================================================================
 * Core operations
 *==========================================================================*/

/**
 * @brief Distribute an input vector across arms — each arm processes a slice.
 *
 * Slicing strategy: the input is split into overlapping chunks of size
 * OCTOPUS_ARM_DIM. Each arm gets one chunk (ith arm → ith chunk, wrapping
 * around if input is shorter than n_arms × ARM_DIM). This mimics how
 * different octopus arms explore different parts of the environment.
 *
 * Called from training hot path — per-arm compute is cheap (O(ARM_DIM)).
 *
 * @return 0 on success, -1 on invalid args.
 */
NIMCP_EXPORT int octopus_explore(octopus_system_t* ctx,
                                  const float* input, uint32_t input_len);

/**
 * @brief Aggregate arm latents into a coherent central response.
 *
 * Integration rule: confidence-weighted mean of non-vetoed arm latents,
 * with divergence scored as `central_coherence = 1 - var(latents)`.
 * High coherence → all arms agree; low coherence → arms are split.
 *
 * @param out       Output buffer for aggregated latent (size OCTOPUS_ARM_DIM).
 * @param coherence Optional out param for central_coherence (may be NULL).
 * @return 0 on success, -1 on invalid args or all-vetoed.
 */
NIMCP_EXPORT int octopus_integrate(octopus_system_t* ctx,
                                    float* out, float* coherence);

/**
 * @brief Get the most recent snapshot of stats.
 */
NIMCP_EXPORT void octopus_get_stats(const octopus_system_t* ctx,
                                     octopus_stats_t* out);

/*============================================================================
 * Integration hooks — set by bridges (swarm, ethics, world, fep).
 * Each hook is a function pointer that the octopus calls at the relevant
 * moment. Pass NULL to disable. Passing NULL hook is always safe.
 *==========================================================================*/

/** Ethics gate hook: called per arm BEFORE aggregation. Return false to veto
 *  the arm's contribution. Typical impl: call ethics_engine_evaluate_action. */
typedef bool (*octopus_ethics_hook_t)(const octopus_arm_t* arm, void* user);

/** Swarm delegation hook: called when an arm's confidence exceeds threshold
 *  and central decides to delegate the action to a swarm edge. Typical impl:
 *  enqueue a task on the swarm master's edge queue. */
typedef void (*octopus_swarm_hook_t)(const octopus_arm_t* arm, void* user);

/** World-model hook: called once per integrate() with the aggregated latent.
 *  Typical impl: omni_wm_submit_observation(world_model, latent). */
typedef void (*octopus_world_hook_t)(const float* aggregated,
                                      uint32_t len, void* user);

/** FEP hook: called once per integrate() with central_coherence. Coherence
 *  acts as the inverse of free-energy — low coherence = high surprise. */
typedef void (*octopus_fep_hook_t)(float central_coherence, void* user);

/** Bio-async broadcast hook: called on significant events (arm veto,
 *  low coherence, delegation). Typical impl: bio_router broadcast. */
typedef void (*octopus_bio_hook_t)(const char* event, float value, void* user);

/** Immune-system heartbeat hook: called on every explore(). Typical impl:
 *  nimcp_immune_heartbeat("octopus", 0.0f). */
typedef void (*octopus_immune_hook_t)(void* user);

NIMCP_EXPORT void octopus_set_ethics_hook(octopus_system_t* ctx,
                                           octopus_ethics_hook_t fn, void* user);
NIMCP_EXPORT void octopus_set_swarm_hook(octopus_system_t* ctx,
                                          octopus_swarm_hook_t fn, void* user);
NIMCP_EXPORT void octopus_set_world_hook(octopus_system_t* ctx,
                                          octopus_world_hook_t fn, void* user);
NIMCP_EXPORT void octopus_set_fep_hook(octopus_system_t* ctx,
                                        octopus_fep_hook_t fn, void* user);
NIMCP_EXPORT void octopus_set_bio_hook(octopus_system_t* ctx,
                                        octopus_bio_hook_t fn, void* user);
NIMCP_EXPORT void octopus_set_immune_hook(octopus_system_t* ctx,
                                           octopus_immune_hook_t fn, void* user);

/*============================================================================
 * Read-only accessors — other cognitive modules read arm state without
 * needing the private struct.
 *==========================================================================*/

NIMCP_EXPORT uint32_t octopus_get_n_arms(const octopus_system_t* ctx);
NIMCP_EXPORT const octopus_arm_t* octopus_get_arm(
    const octopus_system_t* ctx, uint32_t arm_id);
NIMCP_EXPORT float octopus_get_broadcast_state(
    const octopus_system_t* ctx, uint32_t arm_id);

/**
 * @brief Bridge-stats writer (Phase 3b/3c).
 *
 * Bridges update their Phase 3b/3c counter fields on the octopus's stats
 * struct via this function so a single octopus_get_stats() call surfaces
 * both core and bridge activity. Safe to call from any thread that has
 * exclusive access to ctx (bridges own the per-brain state, so single-
 * writer in practice). All fields optional — pass negative floats / any
 * value for unchanged (the function overwrites unconditionally).
 */
/**
 * @brief Phase 4j: Run one training step on each arm's LNN.
 *
 * Predictive-coding loss: each arm is trained to map its previous input
 * slice to its current input slice. MSE loss, Adam optimizer, adjoint-
 * method gradients (O(1) memory). Runs OUTSIDE of octopus_explore to keep
 * the forward pass clean — call explicitly after an explore batch.
 *
 * Safe if training is disabled, no LNN is live, or no arm has a stored
 * previous slice yet (first call after creation is a no-op).
 *
 * @param ctx       Octopus context.
 * @param loss_out  Optional: receives mean loss across arms (may be NULL).
 * @return 0 on success; -1 if ctx is NULL or training is disabled.
 */
NIMCP_EXPORT int octopus_train_step(octopus_system_t* ctx, float* loss_out);

/**
 * @brief Phase 4j: Enable / disable gradient flow on arm LNNs.
 *
 * When disabled, octopus_train_step() becomes a no-op and the LNN cells
 * continue to run as frozen reservoirs. Safe to toggle mid-run.
 */
NIMCP_EXPORT void octopus_set_training_enabled(octopus_system_t* ctx, bool enabled);

/**
 * @brief Phase 4l: toggle natural-gradient latent regularization.
 *
 * When enabled, octopus_ng_regularize() applies a Fisher-preconditioned
 * step to each non-vetoed arm's latent vector, pulling it slightly toward
 * origin under the information-geometric metric. Complements Phase 4j's
 * Adam-based weight training — NG here is on latent-space state, not on
 * LNN weights (true weight-space NG requires bypassing the internal
 * optimizer in lnn_training_step and is a deeper refactor).
 */
NIMCP_EXPORT void octopus_set_ng_enabled(octopus_system_t* ctx, bool enabled);

/**
 * @brief Phase 4l: apply one natural-gradient step to each arm's latent.
 *
 * Gradient signal: the latent itself (pulls toward zero). This is a
 * regularization pressure; the Fisher metric makes the pull anisotropic
 * along the directions arm latents actually vary, rather than uniformly
 * shrinking all dimensions.
 *
 * @param ctx  Octopus context.
 * @return     0 on success; -1 if ctx is NULL or NG is disabled.
 */
NIMCP_EXPORT int octopus_ng_regularize(octopus_system_t* ctx);

/**
 * @brief Phase 4m: toggle JEPA latent-space predictive coding.
 *
 * When enabled, octopus_train_step (in addition to Phase 4j's Adam MSE
 * training on raw slices) trains a shared JEPA predictor on the
 * embedding-space dynamics of arm latents:
 *
 *   loss_jepa = MSE(jepa_predict(prev_arm_latent), cur_arm_latent)
 *
 * The arm LNN acts as the encoder (raw slice → latent), JEPA learns
 * latent-space transitions. Phase 4j and 4m are additive — arms train
 * on raw MSE AND the JEPA predictor learns semantic dynamics.
 *
 * No-op if JEPA infrastructure isn't live (allocation failed).
 */
NIMCP_EXPORT void octopus_set_jepa_enabled(octopus_system_t* ctx, bool enabled);

NIMCP_EXPORT void octopus_set_bridge_stats(
    octopus_system_t* ctx,
    uint64_t engram_encodings,
    uint64_t kg_nodes_added,
    uint64_t stress_broadcasts,
    uint64_t fear_broadcasts,
    uint64_t amygdala_steps,
    float    last_cortisol,
    float    last_fear,
    uint64_t wm_updates,
    uint64_t fear_conditionings,
    uint64_t vision_samples,
    uint64_t audio_samples,
    uint64_t somato_samples,
    uint64_t snn_samples,
    uint64_t neuromod_samples,
    uint64_t peer_samples);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCTOPUS_H */
