/**
 * @file nimcp_thousand_brains_integration.h
 * @brief Thousand Brains Integration Hub — Full System Integration
 * @version 1.0.0
 * @date 2026-03-17
 *
 * WHAT: Central integration manager that wires Hawkins' Thousand Brains features
 *       (reference frames, column voting, dendritic sequences) to all brain systems.
 * WHY:  The TB components must connect bidirectionally to ~15 modules: entorhinal
 *       grid cells, hippocampus, predictive coding, dendritic computation, feature
 *       hypercolumns, attention gain, oscillations, sparse coding, temporal dynamics,
 *       cortical hierarchy, global workspace, spatial reasoning, parietal, thalamic
 *       relay, visual cortex, imagination, theory of mind, and FEP bridges.
 * HOW:  Single integration hub manages all connections. Called once per brain cycle,
 *       it runs integration steps in dependency order.
 *
 * INTEGRATION MAP:
 * ================
 *
 *   TIER 1 — Cortical Column (direct, same substrate):
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │  Entorhinal Grid Cells ──→ Reference Frames (phase offsets)     │
 *   │  Feature Hypercolumns  ──→ Reference Frames (feature vectors)   │
 *   │  Predictive Coding     ←→ Dendritic Sequences (PE ↔ surprise)   │
 *   │  Cortical Dendritic    ←→ Dendritic Sequences (BAC mechanism)   │
 *   │  Attention Gain        ←→ Voting (gain modulates vote weight)   │
 *   │  Oscillations          ──→ Voting + Sequences (timing)          │
 *   │  Sparse Coding         ←→ Reference Frames (SDR features)       │
 *   │  Temporal Dynamics     ←→ Dendritic Sequences (timescales)      │
 *   │  Cortical Hierarchy    ←→ Reference Frames (multi-scale)        │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 *   TIER 2 — Brain Regions:
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │  Hippocampus    ←→ All three (place cells, replay, completion)  │
 *   │  Parietal       ←→ Reference Frames (ego↔allo transforms)      │
 *   │  Visual Cortex  ──→ Reference Frames + Voting (features)        │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 *   TIER 3 — Cognitive Modules:
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │  Global Workspace ←─ Voting (consensus broadcast)               │
 *   │  Spatial Reasoning ←→ Reference Frames (transforms)             │
 *   │  Theory of Mind   ←─ Voting + Ref Frames (perspective-taking)   │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 * Based on Hawkins' Thousand Brains theory (Numenta, 2019).
 */

#ifndef NIMCP_THOUSAND_BRAINS_INTEGRATION_H
#define NIMCP_THOUSAND_BRAINS_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Lock Ordering (Thousand Brains subsystem)
 * ============================================================================
 *
 * When multiple TB mutexes must be held simultaneously, acquire in this order
 * to prevent deadlocks:
 *
 *   1. tb_integration_hub->mutex         (outermost — serializes all TB work)
 *   2. column_ref_frame_manager->mutex   (spatial state)
 *   3. column_voting_manager->mutex      (voting state)
 *   4. dendritic_sequence_mgr->mutex     (temporal state)
 *   5. wm_thousand_brains_bridge->base.mutex  (WM bridge)
 *
 * In practice, only the hub mutex is acquired during tb_integration_step().
 * Individual component mutexes are for future use if components are accessed
 * independently from non-TB code paths.
 * ============================================================================ */

/* ============================================================================
 * Forward Declarations — all connected systems (void* to avoid header deps)
 * ============================================================================ */

struct brain_struct;

/* TB components */
typedef struct column_ref_frame_manager column_ref_frame_manager_t;
typedef struct column_voting_manager column_voting_manager_t;
typedef struct dendritic_sequence_mgr dendritic_sequence_mgr_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TB_INTEGRATION_MAX_MODULES      20  /**< Max connected modules */

/* Integration flags (bitmask) */
#define TB_INT_ENTORHINAL              (1u << 0)
#define TB_INT_HIPPOCAMPUS             (1u << 1)
#define TB_INT_PREDICTIVE_CODING       (1u << 2)
#define TB_INT_DENDRITIC_COMPUTE       (1u << 3)
#define TB_INT_FEATURE_HYPERCOLUMNS    (1u << 4)
#define TB_INT_ATTENTION_GAIN          (1u << 5)
#define TB_INT_OSCILLATIONS            (1u << 6)
#define TB_INT_SPARSE_CODING           (1u << 7)
#define TB_INT_TEMPORAL_DYNAMICS       (1u << 8)
#define TB_INT_CORTICAL_HIERARCHY      (1u << 9)
#define TB_INT_GLOBAL_WORKSPACE        (1u << 10)
#define TB_INT_SPATIAL_REASONING       (1u << 11)
#define TB_INT_PARIETAL                (1u << 12)
#define TB_INT_VISUAL_CORTEX           (1u << 13)
#define TB_INT_THEORY_OF_MIND          (1u << 14)
#define TB_INT_ALL                     0xFFFFFFFF

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Configuration for the integration hub.
 */
typedef struct {
    uint32_t enabled_integrations;      /**< Bitmask of TB_INT_* flags */
    float entorhinal_coupling_gain;     /**< Grid cell → ref frame gain (default 1.0) */
    float hippocampal_replay_weight;    /**< Replay → dendritic learning weight (default 0.5) */
    float predictive_error_gain;        /**< Prediction error → surprise scaling (default 1.0) */
    float attention_vote_gain;          /**< Attention → vote weight scaling (default 1.5) */
    float oscillation_phase_tolerance;  /**< Phase coherence tolerance (default 0.3 radians) */
    float workspace_ignition_boost;     /**< Extra boost for consensus → workspace (default 0.2) */
    float spatial_transform_gain;       /**< Spatial reasoning → ref frame gain (default 1.0) */
    float tom_perspective_weight;       /**< ToM perspective → ref frame offset weight (default 0.3) */
} tb_integration_config_t;

/**
 * @brief Statistics for all integrations.
 */
typedef struct {
    /* Per-integration counters */
    uint64_t entorhinal_grid_updates;       /**< Grid cell → ref frame updates */
    uint64_t hippocampal_replay_events;     /**< Replay → dendritic learning events */
    uint64_t predictive_error_exchanges;    /**< PE ↔ surprise exchanges */
    uint64_t dendritic_bac_events;          /**< BAC burst/predict synchronizations */
    uint64_t feature_binding_events;        /**< Hypercolumn → ref frame feature bindings */
    uint64_t attention_modulations;         /**< Attention → vote weight modulations */
    uint64_t oscillation_sync_events;       /**< Oscillation → timing synchronizations */
    uint64_t sparse_coding_events;          /**< SDR ↔ ref frame feature events */
    uint64_t temporal_context_updates;      /**< Temporal dynamics → sequence context */
    uint64_t hierarchy_scale_bindings;      /**< Hierarchy level → ref frame scale bindings */
    uint64_t workspace_broadcasts;          /**< Consensus → workspace broadcasts */
    uint64_t spatial_transform_events;      /**< Spatial reasoning ↔ ref frame transforms */
    uint64_t parietal_coordinate_updates;   /**< Parietal → ref frame coordinate updates */
    uint64_t visual_feature_events;         /**< Visual cortex → feature bindings */
    uint64_t tom_perspective_events;        /**< ToM → ref frame perspective shifts */

    /* Aggregate metrics */
    uint64_t total_steps;                   /**< Total integration steps executed */
    float mean_consensus_strength;          /**< Running mean voting consensus strength */
    float mean_prediction_accuracy;         /**< Running mean dendritic prediction accuracy */
    float mean_grid_confidence;             /**< Running mean grid cell position confidence */
} tb_integration_stats_t;

/**
 * @brief Thousand Brains Integration Hub.
 *
 * Manages all connections between TB components and the rest of the brain.
 * Created once per brain, stepped once per brain cycle.
 */
typedef struct tb_integration_hub {
    /* TB components (owned externally) */
    column_ref_frame_manager_t* ref_frames;
    column_voting_manager_t* voting;
    dendritic_sequence_mgr_t* sequences;

    /* Connected brain systems (all void* to avoid header dependencies) */
    void* entorhinal;          /**< nimcp_entorhinal_t* */
    void* hippocampus;         /**< nimcp_hippocampus_t* */
    void* predictive_coding;   /**< cortical predictive coding system */
    void* dendritic_compute;   /**< cortical_dendritic_t* */
    void* hypercolumns;        /**< feature_hypercolumn_t* array */
    uint32_t num_hypercolumns;
    void* attention_gain;      /**< cortical attention gain system */
    void* oscillations;        /**< cortical oscillation integration */
    void* sparse_coding;       /**< cortical_sparse_coding_system_t* */
    void* temporal_dynamics;   /**< cortical_temporal_system_t* */
    void* cortical_hierarchy;  /**< cortical_hierarchy_t* */
    void* global_workspace;    /**< global_workspace_t (already a pointer) */
    void* spatial_reasoning;   /**< spatial_reasoning_t* */
    void* parietal;            /**< parietal_adapter_t* */
    void* visual_cortex;       /**< visual cortex system */
    void* theory_of_mind;      /**< theory_of_mind_t (already a pointer) */

    /* Configuration */
    tb_integration_config_t config;

    /* State buffers */
    float* grid_population_vector;  /**< Cached grid cell encoding */
    uint32_t grid_vector_dim;
    float* consensus_broadcast_buf; /**< Buffer for workspace broadcast */
    uint32_t broadcast_buf_dim;

    /* Statistics */
    tb_integration_stats_t stats;

    /* Thread safety */
    void* mutex;                    /**< nimcp_mutex_t* */
} tb_integration_hub_t;

/* ============================================================================
 * API
 * ============================================================================ */

/** Set default configuration (all integrations enabled) */
void tb_integration_config_default(tb_integration_config_t* config);

/** Create integration hub */
tb_integration_hub_t* tb_integration_create(const tb_integration_config_t* config);

/** Destroy integration hub */
void tb_integration_destroy(tb_integration_hub_t* hub);

/* ---- Connect TB components ---- */

/** Connect reference frames, voting, and dendritic sequences */
int tb_integration_connect_tb(tb_integration_hub_t* hub,
                               column_ref_frame_manager_t* ref_frames,
                               column_voting_manager_t* voting,
                               dendritic_sequence_mgr_t* sequences);

/* ---- Connect brain systems (all void* to avoid header deps) ---- */

int tb_integration_connect_entorhinal(tb_integration_hub_t* hub, void* entorhinal);
int tb_integration_connect_hippocampus(tb_integration_hub_t* hub, void* hippocampus);
int tb_integration_connect_predictive_coding(tb_integration_hub_t* hub, void* pred_coding);
int tb_integration_connect_dendritic_compute(tb_integration_hub_t* hub, void* dendritic);
int tb_integration_connect_hypercolumns(tb_integration_hub_t* hub, void* hypercolumns, uint32_t count);
int tb_integration_connect_attention_gain(tb_integration_hub_t* hub, void* attention);
int tb_integration_connect_oscillations(tb_integration_hub_t* hub, void* oscillations);
int tb_integration_connect_sparse_coding(tb_integration_hub_t* hub, void* sparse);
int tb_integration_connect_temporal_dynamics(tb_integration_hub_t* hub, void* temporal);
int tb_integration_connect_cortical_hierarchy(tb_integration_hub_t* hub, void* hierarchy);
int tb_integration_connect_global_workspace(tb_integration_hub_t* hub, void* workspace);
int tb_integration_connect_spatial_reasoning(tb_integration_hub_t* hub, void* spatial);
int tb_integration_connect_parietal(tb_integration_hub_t* hub, void* parietal);
int tb_integration_connect_visual_cortex(tb_integration_hub_t* hub, void* visual);
int tb_integration_connect_theory_of_mind(tb_integration_hub_t* hub, void* tom);

/**
 * @brief Wire all connections from a brain struct automatically.
 *
 * Inspects brain fields and connects all available subsystems.
 * @return Number of successful connections, -1 on error
 */
int tb_integration_wire_from_brain(tb_integration_hub_t* hub,
                                    struct brain_struct* brain);

/* ---- Core step ---- */

/**
 * @brief Full integration step — runs all enabled integrations in order.
 *
 * Call this once per brain cycle. Execution order:
 *   1. Entorhinal grid cells → reference frame locations
 *   2. Feature hypercolumns → reference frame features
 *   3. Cortical hierarchy → multi-scale reference frame binding
 *   4. Sparse coding ↔ reference frame SDR features
 *   5. Visual cortex → voting feature evidence
 *   6. Attention gain → voting weight modulation
 *   7. Oscillations → voting round timing + sequence stepping
 *   8. Predictive coding ↔ dendritic sequences (PE ↔ surprise)
 *   9. Cortical dendritic ↔ dendritic sequences (BAC sync)
 *  10. Temporal dynamics ↔ dendritic sequences (timescale context)
 *  11. Hippocampus ↔ all three (place cells, replay, completion)
 *  12. Parietal ↔ reference frames (coordinate transforms)
 *  13. Spatial reasoning ↔ reference frames (mental rotation)
 *  14. Voting consensus → global workspace broadcast
 *  15. Theory of mind ← voting + ref frames (perspective)
 *
 * @return 0 on success, -1 on error
 */
int tb_integration_step(tb_integration_hub_t* hub);

/* ---- Individual integration functions (for fine-grained control) ---- */

int tb_int_entorhinal_to_ref_frames(tb_integration_hub_t* hub);
int tb_int_hippocampus_sync(tb_integration_hub_t* hub);
int tb_int_predictive_coding_sync(tb_integration_hub_t* hub);
int tb_int_dendritic_compute_sync(tb_integration_hub_t* hub);
int tb_int_hypercolumns_to_ref_frames(tb_integration_hub_t* hub);
int tb_int_attention_to_voting(tb_integration_hub_t* hub);
int tb_int_oscillation_timing(tb_integration_hub_t* hub);
int tb_int_sparse_coding_sync(tb_integration_hub_t* hub);
int tb_int_temporal_to_sequences(tb_integration_hub_t* hub);
int tb_int_hierarchy_to_ref_frames(tb_integration_hub_t* hub);
int tb_int_voting_to_workspace(tb_integration_hub_t* hub);
int tb_int_spatial_reasoning_sync(tb_integration_hub_t* hub);
int tb_int_parietal_to_ref_frames(tb_integration_hub_t* hub);
int tb_int_visual_to_features(tb_integration_hub_t* hub);
int tb_int_theory_of_mind_sync(tb_integration_hub_t* hub);

/* ---- Query ---- */

int tb_integration_get_stats(const tb_integration_hub_t* hub,
                              tb_integration_stats_t* stats);

/** Get number of active integrations (based on what's connected + enabled) */
uint32_t tb_integration_get_active_count(const tb_integration_hub_t* hub);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THOUSAND_BRAINS_INTEGRATION_H */
