/**
 * @file nimcp_temporal_replay.h
 * @brief Temporal Replay - Hippocampal-Inspired Forward/Backward Replay
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Temporal sequence storage and replay in forward/backward order
 * WHY:  Enable experience replay for learning and backward inference
 * HOW:  Circular buffer with priority-weighted sampling and bidirectional replay
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * HIPPOCAMPAL REPLAY (Foster & Wilson, 2006):
 * -------------------------------------------
 * The hippocampus replays sequential experiences during rest/sleep:
 *
 *   1. FORWARD REPLAY: During awake rest
 *      Replays recent experiences in original order
 *      Consolidates memories, strengthens sequences
 *
 *   2. BACKWARD REPLAY: During sharp-wave ripples
 *      Replays sequences in reverse order
 *      Enables credit assignment, planning
 *
 *   3. PREPLAY: During planning
 *      Generates novel sequences for prospection
 *
 * PRIORITIZED REPLAY (Schaul et al., 2015):
 * -----------------------------------------
 * Sample transitions proportional to TD-error:
 *
 *   P(i) ∝ |δ_i|^α + ε
 *
 * Where δ_i is the temporal difference error.
 *
 * SEQUENCE COMPRESSION:
 * ---------------------
 * Replay occurs faster than real-time (10-20x compression)
 * Enables rapid consolidation of extended experiences.
 *
 * INTEGRATION POINTS:
 * -------------------
 * - JEPA bidirectional: Backward direction for retrodiction
 * - Hopfield: Patterns as memory traces
 * - Training: Experience replay buffer
 * - Sleep: Consolidation during simulated sleep
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TEMPORAL_REPLAY_H
#define NIMCP_TEMPORAL_REPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/context/nimcp_gpu_context.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for temporal replay */
#define BIO_MODULE_TEMPORAL_REPLAY              0x0E40

/** @brief Default buffer capacity */
#define REPLAY_DEFAULT_CAPACITY                 10000

/** @brief Maximum buffer capacity */
#define REPLAY_MAX_CAPACITY                     1000000

/** @brief Default sequence length for replay */
#define REPLAY_DEFAULT_SEQUENCE_LENGTH          32

/** @brief Priority exponent α */
#define REPLAY_DEFAULT_PRIORITY_ALPHA           0.6f

/** @brief Importance sampling exponent β */
#define REPLAY_DEFAULT_IS_BETA                  0.4f

/** @brief Minimum priority ε */
#define REPLAY_MIN_PRIORITY                     1e-6f

/** @brief Default compression ratio (real-time / replay-time) */
#define REPLAY_DEFAULT_COMPRESSION              10.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Replay mode
 */
typedef enum {
    REPLAY_MODE_FORWARD = 0,        /**< Forward replay (original order) */
    REPLAY_MODE_BACKWARD,           /**< Backward replay (reverse order) */
    REPLAY_MODE_RANDOM,             /**< Random access (no sequence) */
    REPLAY_MODE_PRIORITY,           /**< Priority-weighted sampling */
    REPLAY_MODE_INTERLEAVED         /**< Alternate forward/backward */
} replay_mode_t;

/**
 * @brief Sequence state
 */
typedef enum {
    REPLAY_SEQ_IDLE = 0,            /**< No active replay */
    REPLAY_SEQ_FORWARD,             /**< Forward replay in progress */
    REPLAY_SEQ_BACKWARD,            /**< Backward replay in progress */
    REPLAY_SEQ_PAUSED               /**< Replay paused */
} replay_seq_state_t;

/**
 * @brief GPU acceleration mode
 */
typedef enum {
    REPLAY_GPU_DISABLED = 0,        /**< CPU only */
    REPLAY_GPU_AUTO,                /**< Auto-select */
    REPLAY_GPU_PREFERRED,           /**< Prefer GPU if available */
    REPLAY_GPU_REQUIRED             /**< Require GPU */
} replay_gpu_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Single transition/state in buffer
 */
typedef struct {
    float* state;                   /**< State representation [state_dim] */
    float* action;                  /**< Action taken (optional) [action_dim] */
    float* next_state;              /**< Next state (optional) [state_dim] */
    float reward;                   /**< Reward received */
    float td_error;                 /**< Temporal difference error */
    uint64_t timestamp;             /**< Original timestamp */
    uint32_t sequence_id;           /**< ID of parent sequence */
    uint32_t position_in_sequence;  /**< Position within sequence */
    bool terminal;                  /**< Is this a terminal state? */
} replay_transition_t;

/**
 * @brief Replay sequence
 */
typedef struct {
    uint32_t* transition_indices;   /**< Indices into transition buffer */
    uint32_t length;                /**< Number of transitions */
    uint32_t sequence_id;           /**< Unique sequence identifier */
    uint64_t start_timestamp;       /**< Start time */
    uint64_t end_timestamp;         /**< End time */
    float total_reward;             /**< Cumulative reward */
    float avg_priority;             /**< Average priority of transitions */
} replay_sequence_t;

/**
 * @brief Sampled batch
 */
typedef struct {
    float* states;                  /**< States [batch_size × state_dim] */
    float* actions;                 /**< Actions [batch_size × action_dim] */
    float* next_states;             /**< Next states [batch_size × state_dim] */
    float* rewards;                 /**< Rewards [batch_size] */
    float* is_weights;              /**< Importance sampling weights [batch_size] */
    uint32_t* indices;              /**< Buffer indices [batch_size] */
    uint32_t batch_size;            /**< Number of samples */
    bool is_sequence;               /**< Is this a sequence or random samples? */
} replay_batch_t;

/**
 * @brief Sweep result (forward or backward)
 */
typedef struct {
    float** states;                 /**< Sequence of states [length × state_dim] */
    uint64_t* timestamps;           /**< Original timestamps [length] */
    float* rewards;                 /**< Rewards [length] */
    uint32_t length;                /**< Sequence length */
    replay_mode_t mode;             /**< Replay mode used */
    uint64_t replay_duration_us;    /**< Replay duration */
    float compression_ratio;        /**< Actual compression ratio */
} replay_sweep_result_t;

/**
 * @brief Temporal replay configuration
 */
typedef struct {
    /* Buffer dimensions */
    uint32_t capacity;              /**< Maximum transitions to store */
    uint32_t state_dim;             /**< State dimension */
    uint32_t action_dim;            /**< Action dimension (0 if none) */
    uint32_t max_sequence_length;   /**< Maximum sequence length */

    /* Replay parameters */
    replay_mode_t default_mode;     /**< Default replay mode */
    float priority_alpha;           /**< Priority exponent α */
    float is_beta;                  /**< Importance sampling β */
    float compression_ratio;        /**< Time compression ratio */

    /* Priority tree */
    bool use_priority_tree;         /**< Use sum tree for priority */
    float priority_decay;           /**< Priority decay over time */

    /* GPU configuration */
    replay_gpu_mode_t gpu_mode;     /**< GPU acceleration mode */
    uint32_t min_batch_for_gpu;     /**< Min batch size for GPU */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */

    /* Memory management */
    bool store_next_states;         /**< Store next states (doubles memory) */
    bool store_actions;             /**< Store actions */
} replay_config_t;

/**
 * @brief Temporal replay statistics
 */
typedef struct {
    uint32_t transitions_stored;    /**< Current transition count */
    uint32_t sequences_stored;      /**< Current sequence count */
    uint64_t total_samples;         /**< Total samples drawn */
    uint64_t forward_sweeps;        /**< Forward sweep count */
    uint64_t backward_sweeps;       /**< Backward sweep count */
    float avg_priority;             /**< Average priority */
    float max_priority;             /**< Maximum priority */
    float avg_sequence_length;      /**< Average sequence length */
    uint64_t gpu_samples;           /**< GPU sample count */
    uint64_t cpu_samples;           /**< CPU sample count */
    float capacity_used;            /**< Capacity utilization [0,1] */
} replay_stats_t;

/**
 * @brief Temporal replay buffer
 */
typedef struct temporal_replay {
    /* Configuration */
    replay_config_t config;

    /* Circular buffer */
    replay_transition_t* transitions; /**< Transition storage */
    uint32_t head;                  /**< Write position */
    uint32_t count;                 /**< Current count */
    uint32_t next_transition_id;    /**< Next transition ID */

    /* Sequence tracking */
    replay_sequence_t* sequences;   /**< Active sequences */
    uint32_t num_sequences;         /**< Number of sequences */
    uint32_t max_sequences;         /**< Maximum sequences */
    uint32_t current_sequence_id;   /**< Current recording sequence */
    bool recording_sequence;        /**< Currently recording? */

    /* Priority tree (for efficient sampling) */
    float* priority_tree;           /**< Sum tree for priorities */
    float* min_tree;                /**< Min tree for IS weights */
    float total_priority;           /**< Sum of all priorities */

    /* Replay state */
    replay_seq_state_t seq_state;   /**< Current sequence state */
    uint32_t replay_position;       /**< Current replay position */
    uint32_t replay_sequence_idx;   /**< Sequence being replayed */

    /* GPU resources */
#ifdef NIMCP_ENABLE_CUDA
    struct nimcp_gpu_context_s* gpu_ctx;
    float* states_device;           /**< States on GPU */
    float* priorities_device;       /**< Priorities on GPU */
    bool gpu_initialized;
#endif

    /* Statistics */
    replay_stats_t stats;

    /* Thread safety */
    void* mutex;
} temporal_replay_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int replay_default_config(replay_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
int replay_validate_config(const replay_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create temporal replay buffer
 *
 * WHAT: Initialize hippocampal-inspired replay system
 * WHY:  Enable experience replay for learning and backward inference
 * HOW:  Allocate circular buffer, priority tree, GPU resources
 *
 * @param config Configuration (NULL for defaults)
 * @return New replay buffer or NULL on failure
 */
temporal_replay_t* temporal_replay_create(const replay_config_t* config);

/**
 * @brief Destroy replay buffer
 *
 * @param replay Replay buffer to destroy (NULL safe)
 */
void temporal_replay_destroy(temporal_replay_t* replay);

/**
 * @brief Clear all stored transitions
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_clear(temporal_replay_t* replay);

/* ============================================================================
 * Storage API
 * ============================================================================ */

/**
 * @brief Store a transition
 *
 * WHAT: Add state transition to buffer
 * WHY:  Build experience memory for replay
 * HOW:  Add to circular buffer, update priority tree
 *
 * @param replay Replay buffer
 * @param state Current state [state_dim]
 * @param action Action taken [action_dim] (can be NULL)
 * @param next_state Next state [state_dim] (can be NULL)
 * @param reward Reward received
 * @param terminal Is this terminal?
 * @param priority Initial priority (0 for max priority)
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_store(temporal_replay_t* replay,
                           const float* state,
                           const float* action,
                           const float* next_state,
                           float reward,
                           bool terminal,
                           float priority);

/**
 * @brief Start recording a sequence
 *
 * @param replay Replay buffer
 * @return Sequence ID, or UINT32_MAX on error
 */
uint32_t temporal_replay_start_sequence(temporal_replay_t* replay);

/**
 * @brief End current sequence recording
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_end_sequence(temporal_replay_t* replay);

/**
 * @brief Update priority for a transition
 *
 * @param replay Replay buffer
 * @param index Transition index
 * @param priority New priority
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_update_priority(temporal_replay_t* replay,
                                     uint32_t index,
                                     float priority);

/**
 * @brief Update priorities in batch
 *
 * @param replay Replay buffer
 * @param indices Transition indices [batch_size]
 * @param priorities New priorities [batch_size]
 * @param batch_size Number of updates
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_update_priorities(temporal_replay_t* replay,
                                       const uint32_t* indices,
                                       const float* priorities,
                                       uint32_t batch_size);

/* ============================================================================
 * Sampling API
 * ============================================================================ */

/**
 * @brief Sample a batch of transitions
 *
 * WHAT: Draw samples from replay buffer
 * WHY:  Training data for learning
 * HOW:  Priority-weighted or uniform sampling
 *
 * @param replay Replay buffer
 * @param mode Sampling mode
 * @param batch_size Number of samples
 * @param batch Output batch
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_sample(temporal_replay_t* replay,
                            replay_mode_t mode,
                            uint32_t batch_size,
                            replay_batch_t* batch);

/**
 * @brief Sample a sequence
 *
 * @param replay Replay buffer
 * @param sequence_length Desired sequence length
 * @param batch Output batch (is_sequence will be true)
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_sample_sequence(temporal_replay_t* replay,
                                     uint32_t sequence_length,
                                     replay_batch_t* batch);

/* ============================================================================
 * Replay Sweep API
 * ============================================================================ */

/**
 * @brief Forward sweep (replay in original order)
 *
 * WHAT: Replay sequence forward from start index
 * WHY:  Consolidate memories, reinforce sequences
 * HOW:  Iterate through stored transitions
 *
 * @param replay Replay buffer
 * @param start_idx Starting transition index
 * @param length Number of transitions to replay
 * @param result Output sweep result
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_forward_sweep(temporal_replay_t* replay,
                                   uint32_t start_idx,
                                   uint32_t length,
                                   replay_sweep_result_t* result);

/**
 * @brief Backward sweep (replay in reverse order)
 *
 * WHAT: Replay sequence backward from end index
 * WHY:  Credit assignment, backward inference
 * HOW:  Reverse iterate through stored transitions
 *
 * @param replay Replay buffer
 * @param end_idx Ending transition index
 * @param length Number of transitions to replay
 * @param result Output sweep result
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_backward_sweep(temporal_replay_t* replay,
                                    uint32_t end_idx,
                                    uint32_t length,
                                    replay_sweep_result_t* result);

/**
 * @brief Replay a stored sequence
 *
 * @param replay Replay buffer
 * @param sequence_id Sequence to replay
 * @param mode Replay mode (forward or backward)
 * @param result Output sweep result
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_replay_sequence(temporal_replay_t* replay,
                                     uint32_t sequence_id,
                                     replay_mode_t mode,
                                     replay_sweep_result_t* result);

/**
 * @brief Get next state in current replay
 *
 * @param replay Replay buffer
 * @param state Output state [state_dim]
 * @param timestamp Output timestamp
 * @return NIMCP_SUCCESS on success, NIMCP_ERROR_COMPLETE when done
 */
int temporal_replay_next(temporal_replay_t* replay,
                          float* state,
                          uint64_t* timestamp);

/**
 * @brief Pause current replay
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_pause(temporal_replay_t* replay);

/**
 * @brief Resume paused replay
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_resume(temporal_replay_t* replay);

/**
 * @brief Stop current replay
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_stop(temporal_replay_t* replay);

/* ============================================================================
 * GPU API
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA
/**
 * @brief Initialize GPU acceleration
 *
 * @param replay Replay buffer
 * @param gpu_ctx GPU context (NULL for auto-create)
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_init_gpu(temporal_replay_t* replay,
                              struct nimcp_gpu_context_s* gpu_ctx);

/**
 * @brief Sync buffer to GPU
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_sync_to_gpu(temporal_replay_t* replay);

/**
 * @brief Check if GPU is available
 *
 * @param replay Replay buffer
 * @return true if GPU initialized
 */
bool temporal_replay_has_gpu(const temporal_replay_t* replay);
#endif

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param replay Replay buffer
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_get_stats(const temporal_replay_t* replay,
                               replay_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_reset_stats(temporal_replay_t* replay);

/**
 * @brief Get current transition count
 *
 * @param replay Replay buffer
 * @return Transition count
 */
uint32_t temporal_replay_count(const temporal_replay_t* replay);

/**
 * @brief Get capacity
 *
 * @param replay Replay buffer
 * @return Capacity
 */
uint32_t temporal_replay_capacity(const temporal_replay_t* replay);

/**
 * @brief Check if buffer is full
 *
 * @param replay Replay buffer
 * @return true if at capacity
 */
bool temporal_replay_is_full(const temporal_replay_t* replay);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_connect_bio_async(temporal_replay_t* replay);

/**
 * @brief Disconnect from bio-async router
 *
 * @param replay Replay buffer
 * @return NIMCP_SUCCESS on success
 */
int temporal_replay_disconnect_bio_async(temporal_replay_t* replay);

/* ============================================================================
 * Result Management API
 * ============================================================================ */

/**
 * @brief Create replay batch
 *
 * @param batch_size Batch size
 * @param state_dim State dimension
 * @param action_dim Action dimension
 * @return New batch or NULL
 */
replay_batch_t* replay_batch_create(uint32_t batch_size,
                                     uint32_t state_dim,
                                     uint32_t action_dim);

/**
 * @brief Destroy replay batch
 *
 * @param batch Batch to destroy (NULL safe)
 */
void replay_batch_destroy(replay_batch_t* batch);

/**
 * @brief Create sweep result
 *
 * @param max_length Maximum sequence length
 * @param state_dim State dimension
 * @return New result or NULL
 */
replay_sweep_result_t* replay_sweep_result_create(uint32_t max_length,
                                                   uint32_t state_dim);

/**
 * @brief Destroy sweep result
 *
 * @param result Result to destroy (NULL safe)
 */
void replay_sweep_result_destroy(replay_sweep_result_t* result);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert replay mode to string
 *
 * @param mode Replay mode
 * @return Human-readable string
 */
const char* replay_mode_to_string(replay_mode_t mode);

/**
 * @brief Convert sequence state to string
 *
 * @param state Sequence state
 * @return Human-readable string
 */
const char* replay_seq_state_to_string(replay_seq_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_REPLAY_H */
