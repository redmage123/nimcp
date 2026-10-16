/**
 * @file nimcp_world_model_cognitive_integration.h
 * @brief Wires world model simulation engines to ALL relevant brain systems
 *
 * WHAT: Unified integration layer connecting the 36 simulation engines to
 *       subcortical structures (cerebellum, basal ganglia, medulla),
 *       cognitive modules (ToM, imagination, curiosity, causal reasoning,
 *       counterfactual, ethics, planning, attention, working memory),
 *       and the FEP/JEPA/prime resonance loop.
 *
 * WHY:  The world model is useless in isolation. It must inform action
 *       selection (BG), error correction (cerebellum), arousal (medulla),
 *       social prediction (ToM), hypothetical reasoning (imagination),
 *       exploration (curiosity), and moral judgment (ethics).
 *
 * HOW:  Callback-based integration — registers handlers for each brain
 *       system that fires on world model events (prediction error,
 *       surprise, state change). No tight coupling to brain internals.
 *
 * DATA FLOW:
 *   World Model Bridge (predict-verify-surprise)
 *     ↓ prediction error
 *     ├→ Cerebellum: broadcast_error() for fast motor correction
 *     ├→ Cerebellum: update_forward_model() from simulation data
 *     ├→ Basal Ganglia: reward = -error (actions that reduce error are good)
 *     ├→ Medulla: arousal scales with surprise (high error → alert)
 *     ├→ FEP: precision-weighted free energy update
 *     ├→ JEPA: latent prediction training
 *     ├→ Prime Resonance: surprising events stored
 *     ├→ Curiosity: high-error domains flagged for exploration
 *     ├→ Imagination: counterfactual scenarios via simulation re-run
 *     ├→ Theory of Mind: agent behavior prediction errors
 *     ├→ Working Memory: current world state snapshot
 *     ├→ Attention: precision-driven salience map
 *     ├→ Ethics: consequence prediction for moral reasoning
 *     └→ Executive: planning via multi-step simulation rollout
 */

#ifndef NIMCP_WORLD_MODEL_COGNITIVE_INTEGRATION_H
#define NIMCP_WORLD_MODEL_COGNITIVE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_world_model_bridges.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Integration Targets (which systems to wire)
 * ============================================================================ */

typedef enum {
    WMCI_TARGET_CEREBELLUM      = (1 << 0),
    WMCI_TARGET_BASAL_GANGLIA   = (1 << 1),
    WMCI_TARGET_MEDULLA         = (1 << 2),
    WMCI_TARGET_FEP             = (1 << 3),
    WMCI_TARGET_JEPA            = (1 << 4),
    WMCI_TARGET_PRIME_RESONANCE = (1 << 5),
    WMCI_TARGET_CURIOSITY       = (1 << 6),
    WMCI_TARGET_IMAGINATION     = (1 << 7),
    WMCI_TARGET_THEORY_OF_MIND  = (1 << 8),
    WMCI_TARGET_WORKING_MEMORY  = (1 << 9),
    WMCI_TARGET_ATTENTION       = (1 << 10),
    WMCI_TARGET_ETHICS          = (1 << 11),
    WMCI_TARGET_EXECUTIVE       = (1 << 12),
    WMCI_TARGET_CAUSAL          = (1 << 13),
    WMCI_TARGET_COUNTERFACTUAL  = (1 << 14),
    WMCI_TARGET_ALL             = 0x7FFF,
} wmci_target_flags_t;

/* ============================================================================
 * Integration Context — holds pointers to all brain systems
 * ============================================================================ */

typedef struct {
    /* World model bridge (source of prediction errors) */
    world_model_bridge_t*   bridge;

    /* Subcortical */
    void*   cerebellum;         /* cerebellum_adapter_t* */
    void*   basal_ganglia;      /* basal_ganglia_t* */
    void*   medulla;            /* medulla_t */

    /* Core cognitive */
    void*   fep;                /* fep_system_t* */
    void*   jepa;               /* jepa_predictor_t* */
    void*   curiosity;          /* curiosity_system_t* */
    void*   imagination;        /* imagination_engine_t* */
    void*   theory_of_mind;     /* tom_system_t* */
    void*   working_memory;     /* working_memory_t* */
    void*   attention;          /* attention_system_t* */
    void*   ethics;             /* ethics_module_t* */
    void*   executive;          /* executive_system_t* */
    void*   causal;             /* causal_reasoning_t* */
    void*   counterfactual;     /* counterfactual_imagination_t* */

    /* Which systems are wired */
    uint32_t    active_targets;
    uint32_t    total_events_dispatched;
    bool        initialized;
} wmci_context_t;

/* ============================================================================
 * API
 * ============================================================================ */

/** Create integration context */
wmci_context_t* wmci_create(world_model_bridge_t* bridge);

/** Destroy integration context */
void wmci_destroy(wmci_context_t* ctx);

/** Wire a brain system to receive world model events */
void wmci_connect(wmci_context_t* ctx, wmci_target_flags_t target, void* system);

/**
 * @brief Auto-wire all available systems from a brain struct
 *
 * Scans brain for cerebellum, BG, medulla, FEP, JEPA, curiosity,
 * imagination, ToM, working memory, attention, ethics, executive,
 * and registers callbacks for each one found.
 */
struct brain_struct;
int wmci_auto_wire_from_brain(wmci_context_t* ctx, struct brain_struct* brain);

/**
 * @brief Dispatch a world model event to all connected systems
 *
 * Called automatically by the bridge's surprise callback. Fans out
 * the event to cerebellum (error), BG (reward), medulla (arousal),
 * curiosity (exploration signal), etc.
 */
void wmci_dispatch_surprise(wmci_context_t* ctx,
                              const wmb_surprise_event_t* event);

/**
 * @brief Dispatch a prediction error update (every step, not just surprises)
 *
 * Sends per-domain errors to cerebellum, FEP, attention, working memory.
 */
void wmci_dispatch_prediction_error(wmci_context_t* ctx,
                                      float physics_error,
                                      float chemistry_error,
                                      float biology_error);

/**
 * @brief Dispatch a replay event during consolidation
 */
void wmci_dispatch_replay(wmci_context_t* ctx,
                            const float* latent_before,
                            const float* latent_after,
                            float surprise_score);

/** Get count of active integration targets */
uint32_t wmci_active_count(const wmci_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORLD_MODEL_COGNITIVE_INTEGRATION_H */
