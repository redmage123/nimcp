/**
 * @file nimcp_omni_wm_memory_bridge.h
 * @brief World Model Memory Bridge - Integration with Hippocampus, Engram, and Consolidation Systems
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with memory systems
 * WHY:  Enable memory-informed world modeling and world-model-driven memory encoding
 * HOW:  Hippocampal replay trains world model; world model predictions guide memory encoding
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * COMPLEMENTARY LEARNING SYSTEMS (McClelland et al., 1995):
 * ---------------------------------------------------------
 * The hippocampus rapidly encodes episodic memories, while the cortex slowly
 * learns statistical regularities. The world model bridges these systems:
 *
 *   Hippocampus -> Replay -> World Model Training -> Better Predictions
 *   World Model -> Predictions -> Memory Encoding -> Episodic Context
 *
 * MEMORY-AUGMENTED WORLD MODELS:
 * ------------------------------
 * Following Graves et al. (2016) Neural Turing Machines and Hafner et al.
 * DreamerV3, the world model benefits from episodic memory:
 *
 *   1. REPLAY-BASED TRAINING: Sharp-wave ripple sequences train RSSM dynamics
 *   2. EPISODIC CONTEXT: Engrams provide rich context for state prediction
 *   3. CONSOLIDATION SYNC: Systems consolidation aligns WM and cortical knowledge
 *
 * DATA FLOW:
 * ----------
 *   Hippocampus -> WM: Replay sequences for temporal dynamics training
 *   WM -> Hippocampus: Predicted sequences for error-driven encoding
 *   Engram -> WM: Episodic context for context-dependent prediction
 *   WM -> Engram: World state snapshots for memory encoding
 *   Consolidation -> WM: Offline training signals during sleep
 *   WM -> Consolidation: Semantic features for cortical transfer
 *
 * INTEGRATION POINTS:
 * -------------------
 *   - Hippocampus (nimcp_hippocampus.h): DG, CA3, CA1, replay
 *   - Engram System (nimcp_engram.h): Episodic encoding/retrieval
 *   - Systems Consolidation (nimcp_systems_consolidation.h): Sleep transfer
 *   - World Model (nimcp_omni_world_model.h): RSSM, predictions
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E6A
 *   Message Range: 0x6A00-0x6AFF
 */

#ifndef NIMCP_OMNI_WM_MEMORY_BRIDGE_H
#define NIMCP_OMNI_WM_MEMORY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_MEMORY_* message types */
#include "cognitive/memory/nimcp_engram.h"  /* engram_system_t, memory_engram_t */
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

/* Hippocampus (from nimcp_hippocampus.h) */
typedef struct nimcp_hippocampus nimcp_hippocampus_t;
struct nimcp_episode;
struct nimcp_ripple_event;

/* Engram System types included from nimcp_engram.h above:
 *   - engram_system_t
 *   - memory_engram_t
 */

/* Systems Consolidation (from nimcp_systems_consolidation.h) */
typedef struct systems_consolidation_system systems_consolidation_system_t;
typedef struct cortical_memory_node cortical_memory_node_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Memory Bridge */
#define BIO_MODULE_WM_MEMORY_BRIDGE         0x0E6A

/** Maximum replay sequence length for training */
#define WM_MEMORY_MAX_REPLAY_LENGTH         64

/** Maximum engram context dimension */
#define WM_MEMORY_MAX_CONTEXT_DIM           256

/** Maximum episodes tracked per update */
#define WM_MEMORY_MAX_EPISODES_PER_UPDATE   16

/** Default training batch size from replay */
#define WM_MEMORY_DEFAULT_REPLAY_BATCH      32

/** Default encoding threshold for WM state snapshots */
#define WM_MEMORY_DEFAULT_ENCODING_THRESHOLD 0.5f

/* ============================================================================
 * Bio-Async Message Types (0x6A00-0x6AFF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_MEMORY_REPLAY_SEQ (0x6A00): Hippocampal replay sequence
 *   - BIO_MSG_WM_MEMORY_ENGRAM_ENCODE: Encode WM state as engram
 *   - BIO_MSG_WM_MEMORY_ENGRAM_RETRIEVE: Retrieve episodic context
 *   - BIO_MSG_WM_MEMORY_CONSOLIDATION: Consolidation signal
 *   - BIO_MSG_WM_MEMORY_HIPPOCAMPAL_PRED: WM prediction to hippocampus
 * ============================================================================ */

/** @brief Message type alias for Memory bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_memory_msg_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Memory Bridge configuration
 *
 * WHAT: Parameters controlling WM-Memory integration
 * WHY:  Tune replay training, encoding thresholds, and consolidation sync
 * HOW:  Configurable learning rates, batch sizes, and modulation strengths
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;                 /**< Enable bidirectional modulation */
    float sensitivity;                      /**< General sensitivity [0.5-2.0] */

    /* Replay Training Settings */
    bool enable_replay_training;            /**< Train WM from hippocampal replay */
    uint32_t replay_batch_size;             /**< Batch size for replay training */
    float replay_learning_rate;             /**< Learning rate for replay-based updates */
    float replay_priority_decay;            /**< Decay for prioritized replay [0-1] */
    bool use_reverse_replay;                /**< Include reverse replay sequences */
    float replay_compression_factor;        /**< Temporal compression (15x biological) */

    /* Engram Integration Settings */
    bool enable_engram_encoding;            /**< Encode WM states as engrams */
    float encoding_threshold;               /**< Min prediction error for encoding */
    float emotional_boost_factor;           /**< Emotional salience boost [1.0-3.0] */
    bool enable_context_retrieval;          /**< Use engrams for context prediction */
    uint32_t max_context_engrams;           /**< Max engrams to retrieve for context */

    /* Consolidation Settings */
    bool enable_consolidation_sync;         /**< Sync with sleep consolidation */
    float consolidation_learning_rate;      /**< Learning rate during consolidation */
    bool enable_semantic_extraction;        /**< Extract semantics for cortex */
    float semantic_abstraction_level;       /**< Abstraction level [0-1] */

    /* Hippocampus Settings */
    bool enable_pattern_completion;         /**< Use CA3 pattern completion for WM */
    bool enable_pattern_separation;         /**< Use DG pattern separation */
    float completion_threshold;             /**< Similarity for pattern completion */
    float separation_threshold;             /**< Difference for pattern separation */

    /* Bio-async Settings */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
} omni_wm_memory_bridge_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from World Model to Memory Systems
 *
 * WHAT: WM predictions and states flowing to memory systems
 * WHY:  Guide memory encoding and provide prediction-based context
 * HOW:  State snapshots, prediction errors, semantic features
 */
typedef struct {
    /* State Snapshot for Encoding */
    float* wm_state_snapshot;               /**< Current WM state for encoding */
    uint32_t state_dim;                     /**< Dimensionality of state */
    float state_uncertainty;                /**< Uncertainty of current state */
    double snapshot_timestamp;              /**< When snapshot was taken */

    /* Prediction Information */
    float* predicted_next_state;            /**< Forward prediction for hippocampus */
    uint32_t prediction_horizon;            /**< Steps ahead predicted */
    float prediction_confidence;            /**< Confidence in prediction */
    float prediction_error_magnitude;       /**< Current PE magnitude */

    /* Semantic Features for Consolidation */
    float* semantic_features;               /**< Abstracted features for cortex */
    uint32_t semantic_dim;                  /**< Semantic feature dimension */
    float semantic_novelty;                 /**< How novel are these features */

    /* Counterfactual Information */
    bool has_counterfactual;                /**< Counterfactual result available */
    float counterfactual_divergence;        /**< How different from actual */
    float counterfactual_value;             /**< Expected value of CF trajectory */

    /* Encoding Guidance */
    float encoding_priority;                /**< Suggested encoding priority */
    bool should_encode;                     /**< Should this be encoded as engram */
    uint32_t suggested_memory_type;         /**< Suggested engram type */
} omni_wm_to_memory_effects_t;

/**
 * @brief Effects from Memory Systems to World Model
 *
 * WHAT: Memory-derived information flowing to world model
 * WHY:  Provide training data, context, and consolidation signals
 * HOW:  Replay sequences, episodic context, consolidation state
 */
typedef struct {
    /* Replay Sequence for Training */
    float** replay_states;                  /**< Sequence of states from replay */
    float** replay_actions;                 /**< Sequence of actions from replay */
    float* replay_rewards;                  /**< Sequence of rewards */
    uint32_t replay_length;                 /**< Length of replay sequence */
    bool is_reverse_replay;                 /**< Is this reverse replay */
    float replay_priority;                  /**< Priority weight for training */
    uint64_t replay_episode_id;             /**< Source episode ID */

    /* Episodic Context */
    float* episodic_context;                /**< Retrieved episodic context */
    uint32_t context_dim;                   /**< Context dimensionality */
    float context_match_confidence;         /**< How well context matches */
    uint32_t num_context_engrams;           /**< Number of engrams used */
    uint64_t* context_engram_ids;           /**< IDs of context engrams */

    /* Pattern Completion Result */
    float* completed_pattern;               /**< CA3 pattern completion result */
    uint32_t completed_dim;                 /**< Completed pattern dimension */
    float completion_confidence;            /**< Pattern completion confidence */

    /* Pattern Separation Result */
    float* separated_pattern;               /**< DG separated pattern */
    uint32_t separated_dim;                 /**< Separated pattern dimension */
    float separation_strength;              /**< How different from stored */

    /* Consolidation State */
    bool is_consolidating;                  /**< Currently in consolidation */
    float consolidation_progress;           /**< Progress [0-1] */
    float sleep_stage;                      /**< Current sleep stage (0=awake, 1=SWS, 2=REM) */

    /* Hippocampal State */
    float theta_phase;                      /**< Current theta phase (radians) */
    float theta_power;                      /**< Theta oscillation power */
    float gamma_power;                      /**< Gamma oscillation power */
    bool ripple_active;                     /**< Sharp-wave ripple in progress */
} memory_to_omni_wm_effects_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Memory Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and error metrics
 */
typedef struct {
    /* Replay Training Statistics */
    uint64_t replay_sequences_received;     /**< Total replay sequences received */
    uint64_t replay_training_updates;       /**< Training updates from replay */
    float mean_replay_training_loss;        /**< Average training loss */
    float mean_replay_prediction_error;     /**< Average PE on replay data */

    /* Engram Statistics */
    uint64_t engrams_encoded;               /**< Total WM states encoded */
    uint64_t engram_retrievals;             /**< Total context retrievals */
    float mean_encoding_strength;           /**< Average encoding strength */
    float mean_retrieval_confidence;        /**< Average retrieval confidence */

    /* Consolidation Statistics */
    uint64_t consolidation_cycles;          /**< Consolidation cycles participated */
    uint64_t semantic_transfers;            /**< Semantic features transferred */
    float mean_consolidation_learning;      /**< Learning per consolidation */

    /* Pattern Operations Statistics */
    uint64_t pattern_completions;           /**< Pattern completion operations */
    uint64_t pattern_separations;           /**< Pattern separation operations */
    float mean_completion_accuracy;         /**< Completion accuracy */
    float mean_separation_strength;         /**< Separation distinctiveness */

    /* Timing Statistics */
    uint64_t total_updates;                 /**< Total update cycles */
    double total_processing_time_ms;        /**< Total processing time */
    double mean_update_time_ms;             /**< Average update duration */
    uint64_t last_update_time_us;           /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;                  /**< Total errors encountered */
    uint64_t errors_replay;                 /**< Replay-related errors */
    uint64_t errors_encoding;               /**< Encoding-related errors */
    uint64_t errors_consolidation;          /**< Consolidation-related errors */
} omni_wm_memory_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Memory Bridge
 *
 * WHAT: Main bridge structure connecting WM with memory systems
 * WHY:  Orchestrates bidirectional information flow
 * HOW:  Maintains connections, effects, and state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_memory_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    omni_wm_memory_bridge_config_t config;  /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;        /**< World model (RSSM) */
    nimcp_hippocampus_t* hippocampus;       /**< Hippocampus system */
    engram_system_t* engram_system;         /**< Engram memory system */
    systems_consolidation_system_t* consolidation; /**< Systems consolidation */

    /* Bidirectional Effects */
    omni_wm_to_memory_effects_t wm_to_memory; /**< Effects: WM -> Memory */
    memory_to_omni_wm_effects_t memory_to_wm; /**< Effects: Memory -> WM */

    /* Internal State */
    bool is_sleeping;                       /**< Currently in sleep state */
    float current_sleep_stage;              /**< Current sleep stage */
    uint64_t current_episode_id;            /**< Current episode being processed */
    bool replay_in_progress;                /**< Replay sequence active */

    /* Replay Buffer */
    float** replay_buffer_states;           /**< Buffer for replay states */
    float** replay_buffer_actions;          /**< Buffer for replay actions */
    float* replay_buffer_rewards;           /**< Buffer for replay rewards */
    uint32_t replay_buffer_size;            /**< Current buffer occupancy */
    uint32_t replay_buffer_capacity;        /**< Buffer capacity */

    /* Context Cache */
    float* context_cache;                   /**< Cached episodic context */
    uint32_t context_cache_dim;             /**< Context cache dimension */
    bool context_cache_valid;               /**< Is cache valid */
    uint64_t context_cache_time;            /**< Cache timestamp */

    /* Statistics */
    omni_wm_memory_bridge_stats_t stats;    /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;      /**< Per-instance health agent */
} omni_wm_memory_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Sets all config fields to defaults
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_default_config(
    omni_wm_memory_bridge_config_t* config);

/**
 * @brief Create World Model Memory Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_memory_bridge_t* omni_wm_memory_bridge_create(
    const omni_wm_memory_bridge_config_t* config);

/**
 * @brief Destroy World Model Memory Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_memory_bridge_destroy(omni_wm_memory_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, reset stats, preserve config
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_reset(omni_wm_memory_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all memory systems to bridge
 *
 * WHAT: Establish connections to WM, hippocampus, engram, and consolidation
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param hippocampus Hippocampus system - optional
 * @param engram_system Engram system - optional
 * @param consolidation Consolidation system - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_connect(
    omni_wm_memory_bridge_t* bridge,
    omni_world_model_t* world_model,
    nimcp_hippocampus_t* hippocampus,
    engram_system_t* engram_system,
    systems_consolidation_system_t* consolidation);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_connect_world_model(
    omni_wm_memory_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect hippocampus
 *
 * @param bridge Bridge instance
 * @param hippocampus Hippocampus to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_connect_hippocampus(
    omni_wm_memory_bridge_t* bridge,
    nimcp_hippocampus_t* hippocampus);

/**
 * @brief Connect engram system
 *
 * @param bridge Bridge instance
 * @param engram_system Engram system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_connect_engram(
    omni_wm_memory_bridge_t* bridge,
    engram_system_t* engram_system);

/**
 * @brief Connect consolidation system
 *
 * @param bridge Bridge instance
 * @param consolidation Consolidation system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_connect_consolidation(
    omni_wm_memory_bridge_t* bridge,
    systems_consolidation_system_t* consolidation);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_memory_bridge_is_connected(const omni_wm_memory_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM and memory systems
 * HOW:  Gather memory effects, compute WM effects, apply both
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_update(
    omni_wm_memory_bridge_t* bridge,
    float dt);

/**
 * @brief Set sleep state
 *
 * WHAT: Notify bridge of sleep/wake transition
 * WHY:  Consolidation and replay behavior differ by sleep state
 * HOW:  Update internal state, adjust parameters
 *
 * @param bridge Bridge instance
 * @param is_sleeping true if entering sleep
 * @param sleep_stage Sleep stage (0=awake, 1=SWS, 2=REM)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_set_sleep_state(
    omni_wm_memory_bridge_t* bridge,
    bool is_sleeping,
    float sleep_stage);

/* ============================================================================
 * Replay Training API
 * ============================================================================ */

/**
 * @brief Train world model from hippocampal replay sequence
 *
 * WHAT: Use replay sequence to train RSSM dynamics
 * WHY:  Offline training from experience replay
 * HOW:  Extract transitions, compute gradients, update weights
 *
 * @param bridge Bridge instance
 * @param states State sequence from replay
 * @param actions Action sequence from replay
 * @param rewards Reward sequence from replay
 * @param length Sequence length
 * @param is_reverse True if reverse replay
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_train_from_replay(
    omni_wm_memory_bridge_t* bridge,
    const float** states,
    const float** actions,
    const float* rewards,
    uint32_t length,
    bool is_reverse);

/**
 * @brief Process incoming ripple event
 *
 * WHAT: Handle sharp-wave ripple from hippocampus
 * WHY:  Trigger replay-based training during ripple
 * HOW:  Extract sequence, train WM, update stats
 *
 * @param bridge Bridge instance
 * @param ripple Ripple event from hippocampus
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_on_ripple(
    omni_wm_memory_bridge_t* bridge,
    const struct nimcp_ripple_event* ripple);

/* ============================================================================
 * Engram API
 * ============================================================================ */

/**
 * @brief Encode current WM state as engram
 *
 * WHAT: Create memory engram from world model state
 * WHY:  Store significant states as episodic memories
 * HOW:  Extract state, compute features, create engram
 *
 * @param bridge Bridge instance
 * @param emotional_tag Emotional context for encoding
 * @param force_encode Encode even if below threshold
 * @param engram_id_out Output: created engram ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_encode_engram(
    omni_wm_memory_bridge_t* bridge,
    float emotional_tag,
    bool force_encode,
    uint64_t* engram_id_out);

/**
 * @brief Retrieve episodic context for current state
 *
 * WHAT: Get relevant episodic memories for context
 * WHY:  Enrich WM predictions with episodic information
 * HOW:  Query engrams by similarity, fuse contexts
 *
 * @param bridge Bridge instance
 * @param context_out Output: episodic context vector (pre-allocated)
 * @param context_dim Size of context buffer
 * @param confidence_out Output: retrieval confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_retrieve_episodic_context(
    omni_wm_memory_bridge_t* bridge,
    float* context_out,
    uint32_t context_dim,
    float* confidence_out);

/* ============================================================================
 * Pattern Operations API
 * ============================================================================ */

/**
 * @brief Request pattern completion from CA3
 *
 * WHAT: Complete partial pattern using hippocampal CA3
 * WHY:  Recover full state from partial observation
 * HOW:  Send partial to CA3, retrieve completed pattern
 *
 * @param bridge Bridge instance
 * @param partial_pattern Partial pattern to complete
 * @param partial_dim Dimension of partial pattern
 * @param completed_out Output: completed pattern (pre-allocated)
 * @param completed_dim Size of output buffer
 * @param confidence_out Output: completion confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_pattern_complete(
    omni_wm_memory_bridge_t* bridge,
    const float* partial_pattern,
    uint32_t partial_dim,
    float* completed_out,
    uint32_t completed_dim,
    float* confidence_out);

/**
 * @brief Request pattern separation from DG
 *
 * WHAT: Separate similar patterns using dentate gyrus
 * WHY:  Distinguish similar states for encoding
 * HOW:  Send to DG, retrieve orthogonalized pattern
 *
 * @param bridge Bridge instance
 * @param input_pattern Input pattern to separate
 * @param input_dim Dimension of input
 * @param separated_out Output: separated pattern (pre-allocated)
 * @param separated_dim Size of output buffer
 * @param separation_strength_out Output: separation strength
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_pattern_separate(
    omni_wm_memory_bridge_t* bridge,
    const float* input_pattern,
    uint32_t input_dim,
    float* separated_out,
    uint32_t separated_dim,
    float* separation_strength_out);

/* ============================================================================
 * Consolidation API
 * ============================================================================ */

/**
 * @brief Extract semantic features for cortical transfer
 *
 * WHAT: Abstract semantic features from WM state
 * WHY:  Support systems consolidation hippocampus -> cortex
 * HOW:  Compute abstract representation, send to consolidation
 *
 * @param bridge Bridge instance
 * @param features_out Output: semantic features (pre-allocated)
 * @param features_dim Size of output buffer
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_extract_semantics(
    omni_wm_memory_bridge_t* bridge,
    float* features_out,
    uint32_t features_dim);

/**
 * @brief Sync with consolidation cycle
 *
 * WHAT: Participate in systems consolidation cycle
 * WHY:  Coordinate WM updates with sleep consolidation
 * HOW:  Receive consolidation signals, apply learning
 *
 * @param bridge Bridge instance
 * @param consolidation_signal Signal from consolidation system
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_consolidation_sync(
    omni_wm_memory_bridge_t* bridge,
    float consolidation_signal);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from WM to memory
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_memory_effects_t* omni_wm_memory_bridge_get_wm_effects(
    const omni_wm_memory_bridge_t* bridge);

/**
 * @brief Get current effects from memory to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const memory_to_omni_wm_effects_t* omni_wm_memory_bridge_get_memory_effects(
    const omni_wm_memory_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_get_stats(
    const omni_wm_memory_bridge_t* bridge,
    omni_wm_memory_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_reset_stats(
    omni_wm_memory_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_connect_bio_async(
    omni_wm_memory_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_memory_bridge_disconnect_bio_async(
    omni_wm_memory_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_memory_bridge_is_bio_async_connected(
    const omni_wm_memory_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_memory_msg_type_to_string(omni_wm_memory_msg_type_t msg_type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_memory_bridge_validate_config(
    const omni_wm_memory_bridge_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_MEMORY_BRIDGE_H */
