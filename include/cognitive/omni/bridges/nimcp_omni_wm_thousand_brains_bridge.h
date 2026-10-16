/**
 * @file nimcp_omni_wm_thousand_brains_bridge.h
 * @brief World Model Thousand Brains Bridge - Hawkins Cortical Column Integration
 * @version 1.0.0
 * @date 2026-03-17
 *
 * WHAT: Bidirectional bridge connecting Omnidirectional World Model (RSSM) with
 *       Hawkins' Thousand Brains cortical column systems: reference frames,
 *       column voting, and dendritic sequence prediction.
 * WHY:  The world model needs grounded spatial representations (reference frames),
 *       multi-column consensus (voting), and temporal prediction (dendritic sequences).
 *       Conversely, cortical columns need top-down predictions from the world model.
 * HOW:  Reference frames supply object-centric spatial state to WM;
 *       column voting consensus feeds recognized objects into WM state;
 *       dendritic sequence predictions provide temporal context;
 *       WM top-down predictions drive cortical column expectations.
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * THOUSAND BRAINS THEORY (Hawkins, 2019):
 * ----------------------------------------
 * Every cortical column learns complete models of objects. Each column:
 *   1. Has a REFERENCE FRAME from entorhinal grid cells
 *   2. Builds object models as "features at locations"
 *   3. VOTES with neighbors via lateral connections
 *   4. Reaches CONSENSUS on what object is being sensed
 *
 * WORLD MODEL INTEGRATION:
 * -------------------------
 * The world model operates in latent state space. This bridge maps between:
 *
 *   CORTICAL COLUMNS (concrete, multi-column)
 *       ↕ Reference frames, voting, sequences
 *   WORLD MODEL (abstract, unified state)
 *
 * DATA FLOW:
 * ----------
 *   Columns → WM:
 *     - Reference frame locations → spatial component of WM state
 *     - Voting consensus (object_id, confidence) → object component of WM state
 *     - Dendritic predictions → temporal expectations for WM forward model
 *     - Surprise rate → prediction error signal for WM learning
 *
 *   WM → Columns:
 *     - WM predicted state → top-down feature expectations for columns
 *     - WM spatial predictions → movement priors for reference frames
 *     - WM object predictions → hypothesis priors for voting
 *     - WM prediction error → modulate dendritic learning rates
 *
 * CLOSED LOOP:
 * ------------
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │            THOUSAND BRAINS ↔ WORLD MODEL LOOP                    │
 *   │                                                                  │
 *   │   Sensory Input → Columns → Reference Frames → WM State Update  │
 *   │        ↑                                           ↓             │
 *   │   Column Predictions ← WM Forward Predict ← WM Dynamics         │
 *   │                                                                  │
 *   │   Column Voting → Consensus → WM Object State → Counterfactual  │
 *   │                                                                  │
 *   │   Dendritic Seq → Surprise Rate → WM Learning Rate Modulation   │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E6E
 *   Message Range: 0x6E00-0x6EFF
 */

#ifndef NIMCP_OMNI_WM_THOUSAND_BRAINS_BRIDGE_H
#define NIMCP_OMNI_WM_THOUSAND_BRAINS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"

/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* World Model (from nimcp_omni_world_model.h) */
typedef struct omni_world_model omni_world_model_t;

/* Thousand Brains components (from cortical_columns/) */
typedef struct column_ref_frame_manager column_ref_frame_manager_t;
typedef struct column_voting_manager column_voting_manager_t;
typedef struct dendritic_sequence_mgr dendritic_sequence_mgr_t;

/* Global Workspace (for consensus broadcast) — already typedef'd as pointer in nimcp_brain.h */
#ifndef GLOBAL_WORKSPACE_T_DEFINED
#define GLOBAL_WORKSPACE_T_DEFINED
typedef struct global_workspace_struct* global_workspace_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Thousand Brains Bridge */
#define BIO_MODULE_WM_THOUSAND_BRAINS_BRIDGE    0x0E6E

/** Message types */
#define BIO_MSG_WM_TB_SPATIAL_UPDATE            0x6E01  /**< Ref frame → WM spatial state */
#define BIO_MSG_WM_TB_CONSENSUS_UPDATE          0x6E02  /**< Voting consensus → WM object state */
#define BIO_MSG_WM_TB_SEQUENCE_PREDICTION       0x6E03  /**< Dendritic prediction → WM temporal */
#define BIO_MSG_WM_TB_TOPDOWN_EXPECTATION       0x6E04  /**< WM → column top-down predictions */
#define BIO_MSG_WM_TB_SURPRISE_SIGNAL           0x6E05  /**< Surprise rate → WM learning mod */
#define BIO_MSG_WM_TB_MOVEMENT_PRIOR            0x6E06  /**< WM → ref frame movement prediction */

/** Maximum dimensions for bridge state vectors */
#define WM_TB_MAX_SPATIAL_DIM       16   /**< Spatial state from reference frames */
#define WM_TB_MAX_OBJECT_DIM        32   /**< Object identity embedding */
#define WM_TB_MAX_TEMPORAL_DIM      32   /**< Temporal prediction state */
#define WM_TB_MAX_COLUMNS           128  /**< Max columns tracked by bridge */
#define WM_TB_MAX_FEATURE_DIM       32   /**< Feature vector from ref frame recall */

/** Update rate defaults */
#define WM_TB_DEFAULT_UPDATE_INTERVAL_MS    50   /**< 20 Hz default */
#define WM_TB_DEFAULT_SPATIAL_WEIGHT        0.3f /**< Weight of spatial in WM state */
#define WM_TB_DEFAULT_OBJECT_WEIGHT         0.4f /**< Weight of object in WM state */
#define WM_TB_DEFAULT_TEMPORAL_WEIGHT       0.3f /**< Weight of temporal in WM state */
#define WM_TB_DEFAULT_TOPDOWN_GAIN          0.5f /**< WM → column prediction gain */
#define WM_TB_DEFAULT_SURPRISE_LR_SCALE     2.0f /**< How much surprise boosts WM LR */

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Spatial state extracted from reference frames for world model.
 *
 * Aggregates location encodings across multiple columns into a single
 * spatial state vector that the world model can use.
 */
typedef struct {
    float location[3];                               /**< Mean location across frames */
    float orientation;                                /**< Mean orientation */
    float encoding[WM_TB_MAX_SPATIAL_DIM];           /**< Compressed spatial encoding */
    uint32_t encoding_dim;                            /**< Active encoding dimensions */
    float confidence;                                 /**< Spatial state confidence */
    uint32_t num_active_frames;                       /**< How many frames contributed */
} wm_tb_spatial_state_t;

/**
 * @brief Object state from column voting consensus.
 */
typedef struct {
    uint32_t object_id;                               /**< Consensus object ID */
    float confidence;                                  /**< Consensus confidence [0,1] */
    float agreement_ratio;                             /**< Column agreement ratio */
    float object_embedding[WM_TB_MAX_OBJECT_DIM];    /**< Object identity embedding */
    uint32_t embedding_dim;                            /**< Active embedding dimensions */
    uint32_t rounds_to_consensus;                      /**< Voting rounds needed */
    bool has_consensus;                                /**< Whether consensus was reached */
} wm_tb_object_state_t;

/**
 * @brief Temporal prediction state from dendritic sequences.
 */
typedef struct {
    float prediction_accuracy;                         /**< Sequence prediction accuracy */
    float surprise_rate;                               /**< Burst/surprise rate */
    uint32_t predicted_cells[WM_TB_MAX_TEMPORAL_DIM]; /**< Currently predicted cell IDs */
    uint32_t num_predicted;                            /**< Number of predicted cells */
    float temporal_encoding[WM_TB_MAX_TEMPORAL_DIM];  /**< Compressed temporal state */
    uint32_t encoding_dim;                             /**< Active temporal dimensions */
} wm_tb_temporal_state_t;

/**
 * @brief Top-down expectations from world model to cortical columns.
 */
typedef struct {
    float expected_feature[WM_TB_MAX_FEATURE_DIM];    /**< Expected feature at location */
    uint32_t feature_dim;                              /**< Feature dimensionality */
    float expected_movement[3];                        /**< Predicted next movement */
    uint32_t expected_object_id;                       /**< Predicted object ID */
    float prediction_confidence;                       /**< WM prediction confidence */
    float learning_rate_modulation;                    /**< Modulated LR for sequences */
} wm_tb_topdown_t;

/**
 * @brief Combined Thousand Brains state for world model integration.
 *
 * This is the full state vector that gets mapped into the WM latent space.
 */
typedef struct {
    wm_tb_spatial_state_t spatial;
    wm_tb_object_state_t object;
    wm_tb_temporal_state_t temporal;
    double timestamp;
} wm_tb_combined_state_t;

/**
 * @brief Configuration for the Thousand Brains bridge.
 */
typedef struct {
    /* Weights for combining TB state into WM state */
    float spatial_weight;               /**< Weight for spatial component */
    float object_weight;                /**< Weight for object component */
    float temporal_weight;              /**< Weight for temporal component */

    /* Top-down modulation */
    float topdown_gain;                 /**< Gain for WM → column predictions */
    float surprise_lr_scale;            /**< Surprise-driven LR modulation scale */

    /* Update timing */
    uint32_t update_interval_ms;        /**< Minimum ms between updates */

    /* Feature flags */
    bool enable_spatial_integration;    /**< Feed ref frame spatial to WM */
    bool enable_voting_integration;     /**< Feed voting consensus to WM */
    bool enable_sequence_integration;   /**< Feed dendritic predictions to WM */
    bool enable_topdown_predictions;    /**< Send WM predictions to columns */
    bool enable_surprise_modulation;    /**< Surprise modulates WM learning */
    bool enable_movement_priors;        /**< WM predicts movements for ref frames */
} wm_tb_bridge_config_t;

/**
 * @brief Statistics for the Thousand Brains bridge.
 */
typedef struct {
    uint64_t spatial_updates;           /**< Spatial state → WM updates */
    uint64_t consensus_updates;         /**< Voting consensus → WM updates */
    uint64_t temporal_updates;          /**< Temporal prediction → WM updates */
    uint64_t topdown_predictions;       /**< WM → column predictions sent */
    uint64_t movement_priors_sent;      /**< WM → ref frame movement priors */
    uint64_t surprise_modulations;      /**< Surprise-driven LR modulations */
    float mean_consensus_confidence;    /**< Average voting confidence */
    float mean_prediction_accuracy;     /**< Average dendritic prediction accuracy */
    float mean_surprise_rate;           /**< Average surprise rate */
    float mean_spatial_confidence;      /**< Average spatial state confidence */
} wm_tb_bridge_stats_t;

/**
 * @brief Thousand Brains ↔ World Model bridge instance.
 */
typedef struct wm_thousand_brains_bridge {
    bridge_base_t base;                 /**< MUST be first member */

    /* Connected systems */
    omni_world_model_t* world_model;
    column_ref_frame_manager_t* ref_frames;
    column_voting_manager_t* voting;
    dendritic_sequence_mgr_t* sequences;
    global_workspace_t* workspace;

    /* Configuration */
    wm_tb_bridge_config_t config;

    /* Current state */
    wm_tb_combined_state_t current_state;
    wm_tb_topdown_t topdown;

    /* Mapping buffers: TB state → WM state vector */
    float* wm_state_buffer;             /**< [state_dim] mapped WM state */
    uint32_t wm_state_dim;              /**< WM state dimensionality */

    /* Statistics */
    wm_tb_bridge_stats_t stats;

    /* Health agent */
    nimcp_health_agent_t* health_agent;
} wm_thousand_brains_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

/** Get default configuration */
void wm_tb_bridge_config_default(wm_tb_bridge_config_t* config);

/** Create bridge */
wm_thousand_brains_bridge_t* wm_tb_bridge_create(const wm_tb_bridge_config_t* config);

/** Destroy bridge */
void wm_tb_bridge_destroy(wm_thousand_brains_bridge_t* bridge);

/* ---- System connections ---- */

/** Connect to world model */
nimcp_error_t wm_tb_bridge_connect_world_model(wm_thousand_brains_bridge_t* bridge,
                                                omni_world_model_t* wm);

/** Connect to reference frame manager */
nimcp_error_t wm_tb_bridge_connect_ref_frames(wm_thousand_brains_bridge_t* bridge,
                                               column_ref_frame_manager_t* ref_frames);

/** Connect to column voting manager */
nimcp_error_t wm_tb_bridge_connect_voting(wm_thousand_brains_bridge_t* bridge,
                                           column_voting_manager_t* voting);

/** Connect to dendritic sequence manager */
nimcp_error_t wm_tb_bridge_connect_sequences(wm_thousand_brains_bridge_t* bridge,
                                              dendritic_sequence_mgr_t* sequences);

/** Connect to global workspace (for consensus broadcast relay) */
nimcp_error_t wm_tb_bridge_connect_workspace(wm_thousand_brains_bridge_t* bridge,
                                              global_workspace_t* workspace);

/* ---- Core update cycle ---- */

/**
 * @brief Full bridge step: gather TB state → update WM → generate top-down.
 *
 * Call this each brain cycle. It:
 *   1. Gathers spatial state from reference frames
 *   2. Gathers object state from column voting
 *   3. Gathers temporal state from dendritic sequences
 *   4. Maps combined TB state into WM state vector
 *   5. Updates world model with new state
 *   6. Gets WM forward prediction
 *   7. Sends top-down expectations back to columns
 *
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t wm_tb_bridge_step(wm_thousand_brains_bridge_t* bridge);

/* ---- Individual update phases (for fine-grained control) ---- */

/** Phase 1: Gather spatial state from reference frames → WM */
nimcp_error_t wm_tb_bridge_update_spatial(wm_thousand_brains_bridge_t* bridge);

/** Phase 2: Gather voting consensus → WM */
nimcp_error_t wm_tb_bridge_update_consensus(wm_thousand_brains_bridge_t* bridge);

/** Phase 3: Gather dendritic predictions → WM */
nimcp_error_t wm_tb_bridge_update_temporal(wm_thousand_brains_bridge_t* bridge);

/** Phase 4: Map combined TB state into WM state and push update */
nimcp_error_t wm_tb_bridge_push_to_world_model(wm_thousand_brains_bridge_t* bridge);

/** Phase 5: Get WM prediction and send top-down to columns */
nimcp_error_t wm_tb_bridge_generate_topdown(wm_thousand_brains_bridge_t* bridge);

/** Phase 6: Modulate dendritic learning based on surprise */
nimcp_error_t wm_tb_bridge_modulate_surprise(wm_thousand_brains_bridge_t* bridge);

/* ---- Query API ---- */

/** Get current combined TB state */
nimcp_error_t wm_tb_bridge_get_state(const wm_thousand_brains_bridge_t* bridge,
                                      wm_tb_combined_state_t* state);

/** Get current top-down expectations */
nimcp_error_t wm_tb_bridge_get_topdown(const wm_thousand_brains_bridge_t* bridge,
                                        wm_tb_topdown_t* topdown);

/** Get bridge statistics */
nimcp_error_t wm_tb_bridge_get_stats(const wm_thousand_brains_bridge_t* bridge,
                                      wm_tb_bridge_stats_t* stats);

/* ---- Health & mesh ---- */

/** Set instance health agent */
void wm_tb_bridge_set_instance_health_agent(wm_thousand_brains_bridge_t* bridge,
                                             nimcp_health_agent_t* agent);

/** Register with mesh */
nimcp_error_t wm_tb_bridge_mesh_register(void* registry);

/** Unregister from mesh */
void wm_tb_bridge_mesh_unregister(void);

/* ---- Training hooks ---- */

/** Notify bridge that training is beginning */
int wm_tb_bridge_training_begin(wm_thousand_brains_bridge_t* bridge);

/** Notify bridge that training is ending */
int wm_tb_bridge_training_end(wm_thousand_brains_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_THOUSAND_BRAINS_BRIDGE_H */
