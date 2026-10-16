/**
 * @file nimcp_vae_hippocampus_bridge.h
 * @brief Bridge between VAE and Hippocampus for Memory Encoding
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE latent representations with hippocampal memory
 *
 * WHY:  VAE provides ideal memory encoding for hippocampus:
 *       - Compressed latent codes as episode representations
 *       - Pattern separation via latent space sparsification
 *       - Pattern completion via decoder reconstruction
 *       - Uncertainty estimation for memory confidence
 *       - Generative recall for imagination/dreaming
 *
 * HOW:  Bridge maps between VAE latent space and hippocampal operations:
 *       - Encode sensory input → VAE latent → hippocampus episode
 *       - Retrieve episode → VAE latent → decode to reconstruction
 *       - Pattern separate in latent space → store in DG
 *       - Pattern complete from CA3 → sample from VAE latent
 *
 * THEORETICAL BASIS:
 * ==================
 * The hippocampus performs memory encoding and retrieval through:
 * - DG: Pattern separation (orthogonalization of similar inputs)
 * - CA3: Pattern completion (reconstruction from partial cues)
 * - CA1: Temporal binding and output to cortex
 *
 * VAE latent space provides:
 * - Compressed representation ideal for episodic storage
 * - Generative capability for memory reconstruction
 * - Uncertainty quantification for confidence estimation
 * - Disentangled features (with beta-VAE) for attribute binding
 *
 * Integration mapping:
 *   VAE Component      | Hippocampus Role
 *   -------------------|------------------
 *   Encoder            | Perceptual binding
 *   Latent z           | Episode representation
 *   Latent variance    | Memory uncertainty
 *   Decoder            | Memory reconstruction
 *   Reconstruction err | Novelty signal
 *   KL divergence      | Pattern typicality
 *
 * BIO_MODULE: 0x1F14 (VAE-Hippocampus Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_HIPPOCAMPUS_BRIDGE_H
#define NIMCP_VAE_HIPPOCAMPUS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bridge version */
#define VAE_HIPPO_BRIDGE_VERSION        "1.0.0"

/** Bio-async module ID */
#define BIO_MODULE_VAE_HIPPO_BRIDGE     0x1F14

/** Maximum cue dimension for retrieval */
#define VAE_HIPPO_MAX_CUE_DIM           1024

/** Maximum similar episodes to return */
#define VAE_HIPPO_MAX_SIMILAR           64

/** Default similarity threshold */
#define VAE_HIPPO_DEFAULT_SIMILARITY    0.7f

/** Default novelty threshold for pattern separation */
#define VAE_HIPPO_NOVELTY_THRESHOLD     0.5f

/** Error code range (32450-32459) */
#define NIMCP_ERROR_VAE_HIPPO_BASE          32450
#define NIMCP_ERROR_VAE_HIPPO_NULL          32451
#define NIMCP_ERROR_VAE_HIPPO_NOT_CONNECTED 32452
#define NIMCP_ERROR_VAE_HIPPO_ENCODE_FAILED 32453
#define NIMCP_ERROR_VAE_HIPPO_DECODE_FAILED 32454
#define NIMCP_ERROR_VAE_HIPPO_NO_MEMORY     32455
#define NIMCP_ERROR_VAE_HIPPO_EPISODE_FAIL  32456
#define NIMCP_ERROR_VAE_HIPPO_PATTERN_FAIL  32457
#define NIMCP_ERROR_VAE_HIPPO_DIM_MISMATCH  32458
#define NIMCP_ERROR_VAE_HIPPO_SYNC_FAILED   32459

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Memory encoding mode
 */
typedef enum {
    VAE_HIPPO_ENCODE_STANDARD = 0,   /**< Standard VAE encoding */
    VAE_HIPPO_ENCODE_SPARSE,          /**< Enforce sparsity (DG-like) */
    VAE_HIPPO_ENCODE_DISENTANGLED,    /**< High-beta disentangled */
    VAE_HIPPO_ENCODE_HIERARCHICAL     /**< Multi-level encoding */
} vae_hippo_encode_mode_t;

/**
 * @brief Memory retrieval mode
 */
typedef enum {
    VAE_HIPPO_RETRIEVE_EXACT = 0,    /**< Exact latent match */
    VAE_HIPPO_RETRIEVE_SIMILAR,       /**< Similarity-based search */
    VAE_HIPPO_RETRIEVE_COMPLETE,      /**< Pattern completion from partial */
    VAE_HIPPO_RETRIEVE_GENERATIVE     /**< Generative sampling around cue */
} vae_hippo_retrieve_mode_t;

/**
 * @brief Pattern operation type
 */
typedef enum {
    VAE_HIPPO_PATTERN_SEPARATE = 0,  /**< Pattern separation (DG) */
    VAE_HIPPO_PATTERN_COMPLETE,       /**< Pattern completion (CA3) */
    VAE_HIPPO_PATTERN_BIND            /**< Temporal binding (CA1) */
} vae_hippo_pattern_op_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_HIPPO_STATE_DISCONNECTED = 0,
    VAE_HIPPO_STATE_CONNECTED,
    VAE_HIPPO_STATE_ENCODING,
    VAE_HIPPO_STATE_RETRIEVING,
    VAE_HIPPO_STATE_SEPARATING,
    VAE_HIPPO_STATE_COMPLETING,
    VAE_HIPPO_STATE_ERROR
} vae_hippo_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Memory encoding configuration
 */
typedef struct {
    vae_hippo_encode_mode_t mode;    /**< Encoding mode */
    float sparsity_target;            /**< Target sparsity for sparse mode */
    float beta_override;              /**< Override VAE beta (0 = use VAE config) */
    bool include_variance;            /**< Store variance with episode */
    bool compute_novelty;             /**< Compute novelty score during encoding */
    bool enable_tagging;              /**< Enable emotional/contextual tagging */
} vae_hippo_encode_config_t;

/**
 * @brief Memory retrieval configuration
 */
typedef struct {
    vae_hippo_retrieve_mode_t mode;  /**< Retrieval mode */
    float similarity_threshold;       /**< Minimum similarity for matches */
    uint32_t max_candidates;          /**< Maximum candidates to consider */
    bool sample_on_retrieve;          /**< Sample from latent vs use mean */
    float temperature;                /**< Sampling temperature for generative */
    bool return_confidence;           /**< Return confidence scores */
} vae_hippo_retrieve_config_t;

/**
 * @brief Pattern operation configuration
 */
typedef struct {
    float separation_ratio;           /**< Expansion ratio for separation */
    float completion_threshold;       /**< Confidence threshold for completion */
    uint32_t max_iterations;          /**< Max iterations for completion */
    bool use_attractor_dynamics;      /**< Enable attractor network dynamics */
    float noise_level;                /**< Noise for robust completion */
} vae_hippo_pattern_config_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_hippo_encode_config_t encode;    /**< Encoding configuration */
    vae_hippo_retrieve_config_t retrieve; /**< Retrieval configuration */
    vae_hippo_pattern_config_t pattern;   /**< Pattern operation config */

    /* Dimension mapping */
    uint32_t what_dim;                /**< Dimension for 'what' content */
    uint32_t where_dim;               /**< Dimension for 'where' content */
    uint32_t when_dim;                /**< Dimension for 'when' content */

    /* Integration options */
    bool sync_on_encode;              /**< Sync hippocampus state on encode */
    bool sync_on_retrieve;            /**< Sync hippocampus state on retrieve */
    bool enable_replay_integration;   /**< Integrate with replay system */
    bool enable_place_cell_binding;   /**< Bind episodes to place cells */

    /* Logging */
    bool enable_logging;              /**< Enable detailed logging */
    bool log_latent_stats;            /**< Log latent space statistics */
} vae_hippo_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Episode encoding result
 */
typedef struct {
    uint32_t episode_id;              /**< Assigned episode ID */
    float novelty_score;              /**< Novelty (0 = familiar, 1 = novel) */
    float encoding_strength;          /**< Encoding strength */
    float* latent_code;               /**< Copy of latent representation */
    float* latent_variance;           /**< Latent variance (if stored) */
    uint32_t latent_dim;              /**< Latent dimension */
    bool pattern_separated;           /**< Was pattern separation applied */
    uint32_t active_dimensions;       /**< Number of active latent dims */
} vae_hippo_encode_result_t;

/**
 * @brief Episode retrieval result
 */
typedef struct {
    uint32_t episode_id;              /**< Retrieved episode ID */
    float similarity;                 /**< Similarity to cue (0-1) */
    float confidence;                 /**< Retrieval confidence */
    float* reconstruction;            /**< Reconstructed content */
    uint32_t reconstruction_dim;      /**< Reconstruction dimension */
    float reconstruction_error;       /**< Reconstruction error */
    bool is_pattern_completed;        /**< Was pattern completion used */
    float completion_confidence;      /**< Confidence if completed */
} vae_hippo_retrieve_result_t;

/**
 * @brief Similar episodes result
 */
typedef struct {
    uint32_t* episode_ids;            /**< Array of episode IDs */
    float* similarities;              /**< Corresponding similarity scores */
    uint32_t num_found;               /**< Number of similar episodes */
    uint32_t capacity;                /**< Allocated capacity */
} vae_hippo_similar_result_t;

/**
 * @brief Pattern operation result
 */
typedef struct {
    vae_hippo_pattern_op_t operation; /**< Operation performed */
    float* output_pattern;            /**< Resulting pattern */
    uint32_t output_dim;              /**< Output dimension */
    float confidence;                 /**< Operation confidence */
    uint32_t iterations_used;         /**< Iterations for completion */
    bool converged;                   /**< Did completion converge */
    float sparsity_achieved;          /**< Actual sparsity (for separation) */
} vae_hippo_pattern_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Operation counts */
    uint64_t total_encodes;
    uint64_t total_retrieves;
    uint64_t total_separations;
    uint64_t total_completions;
    uint64_t successful_encodes;
    uint64_t successful_retrieves;
    uint64_t failed_operations;

    /* Quality metrics */
    float avg_encoding_strength;
    float avg_retrieval_similarity;
    float avg_novelty_score;
    float avg_completion_confidence;
    float avg_reconstruction_error;

    /* Latent space metrics */
    float avg_latent_sparsity;
    float avg_latent_variance;
    uint32_t avg_active_dimensions;

    /* Performance */
    float avg_encode_latency_us;
    float avg_retrieve_latency_us;
    float avg_pattern_latency_us;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_operation_us;
} vae_hippo_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief VAE-Hippocampus bridge instance
 */
typedef struct vae_hippo_bridge {
    vae_hippo_bridge_config_t config;
    vae_system_t* vae;
    void* hippocampus;                /**< nimcp_hippocampus_t* (avoid circular include) */
    vae_hippo_bridge_state_t state;
    bool is_initialized;

    /* Dimension info */
    uint32_t vae_input_dim;
    uint32_t vae_latent_dim;
    uint32_t vae_output_dim;

    /* Working buffers */
    float* encode_buffer;
    float* decode_buffer;
    float* latent_buffer;
    float* variance_buffer;

    /* Statistics */
    vae_hippo_bridge_stats_t stats;

    /* Latent space baseline for novelty */
    float* latent_mean_baseline;
    float* latent_var_baseline;
    uint64_t baseline_samples;

    /* Timing */
    uint64_t creation_time_us;
} vae_hippo_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 */
int vae_hippo_bridge_default_config(vae_hippo_bridge_config_t* config);

/**
 * @brief Create VAE-Hippocampus bridge
 */
vae_hippo_bridge_t* vae_hippo_bridge_create(const vae_hippo_bridge_config_t* config);

/**
 * @brief Destroy VAE-Hippocampus bridge
 */
void vae_hippo_bridge_destroy(vae_hippo_bridge_t* bridge);

/**
 * @brief Connect VAE to bridge
 */
int vae_hippo_bridge_connect_vae(vae_hippo_bridge_t* bridge, vae_system_t* vae);

/**
 * @brief Connect hippocampus to bridge
 */
int vae_hippo_bridge_connect_hippo(vae_hippo_bridge_t* bridge, void* hippocampus);

/**
 * @brief Disconnect systems
 */
int vae_hippo_bridge_disconnect(vae_hippo_bridge_t* bridge);

/**
 * @brief Check if fully connected
 */
bool vae_hippo_bridge_is_connected(const vae_hippo_bridge_t* bridge);

/* ============================================================================
 * Memory Encoding API
 * ============================================================================ */

/**
 * @brief Encode sensory input as hippocampal episode via VAE
 *
 * Encodes input through VAE, stores latent as episode representation
 * in hippocampus with what/where/when binding.
 *
 * @param bridge Bridge instance
 * @param input Raw sensory input
 * @param input_dim Input dimension
 * @param where Spatial context (can be NULL)
 * @param where_dim Spatial dimension
 * @param when Temporal context (can be NULL)
 * @param when_dim Temporal dimension
 * @param emotional_valence Emotional valence [-1, 1]
 * @param emotional_arousal Emotional arousal [0, 1]
 * @param result Output encoding result
 * @return 0 on success, error code on failure
 */
int vae_hippo_encode_episode(vae_hippo_bridge_t* bridge,
                              const float* input, uint32_t input_dim,
                              const float* where, uint32_t where_dim,
                              const float* when, uint32_t when_dim,
                              float emotional_valence,
                              float emotional_arousal,
                              vae_hippo_encode_result_t* result);

/**
 * @brief Encode tensor as episode
 */
int vae_hippo_encode_tensor(vae_hippo_bridge_t* bridge,
                             const nimcp_tensor_t* input,
                             const nimcp_tensor_t* context,
                             vae_hippo_encode_result_t* result);

/**
 * @brief Encode with custom configuration
 */
int vae_hippo_encode_with_config(vae_hippo_bridge_t* bridge,
                                  const float* input, uint32_t input_dim,
                                  const vae_hippo_encode_config_t* encode_config,
                                  vae_hippo_encode_result_t* result);

/**
 * @brief Batch encode multiple episodes
 */
int vae_hippo_encode_batch(vae_hippo_bridge_t* bridge,
                            const float* inputs, uint32_t num_inputs,
                            uint32_t input_dim,
                            vae_hippo_encode_result_t* results);

/* ============================================================================
 * Memory Retrieval API
 * ============================================================================ */

/**
 * @brief Retrieve episode using cue via VAE latent matching
 *
 * Encodes cue through VAE, finds matching episode in hippocampus,
 * decodes episode latent to reconstruct memory.
 *
 * @param bridge Bridge instance
 * @param cue Retrieval cue
 * @param cue_dim Cue dimension
 * @param result Output retrieval result
 * @return 0 on success, error code on failure
 */
int vae_hippo_retrieve(vae_hippo_bridge_t* bridge,
                        const float* cue, uint32_t cue_dim,
                        vae_hippo_retrieve_result_t* result);

/**
 * @brief Retrieve by episode ID
 */
int vae_hippo_retrieve_by_id(vae_hippo_bridge_t* bridge,
                              uint32_t episode_id,
                              vae_hippo_retrieve_result_t* result);

/**
 * @brief Retrieve with custom configuration
 */
int vae_hippo_retrieve_with_config(vae_hippo_bridge_t* bridge,
                                    const float* cue, uint32_t cue_dim,
                                    const vae_hippo_retrieve_config_t* config,
                                    vae_hippo_retrieve_result_t* result);

/**
 * @brief Find similar episodes by latent similarity
 */
int vae_hippo_find_similar(vae_hippo_bridge_t* bridge,
                            const float* cue, uint32_t cue_dim,
                            float similarity_threshold,
                            uint32_t max_results,
                            vae_hippo_similar_result_t* result);

/**
 * @brief Retrieve episode and reconstruct to tensor
 */
int vae_hippo_retrieve_tensor(vae_hippo_bridge_t* bridge,
                               const nimcp_tensor_t* cue,
                               nimcp_tensor_t* reconstruction);

/* ============================================================================
 * Pattern Operations API
 * ============================================================================ */

/**
 * @brief Perform pattern separation in VAE latent space (DG-like)
 *
 * Takes input pattern, encodes to latent, applies sparsification
 * to orthogonalize representation from existing memories.
 *
 * @param bridge Bridge instance
 * @param input Input pattern
 * @param input_dim Input dimension
 * @param result Output pattern result
 * @return 0 on success, error code on failure
 */
int vae_hippo_pattern_separate(vae_hippo_bridge_t* bridge,
                                const float* input, uint32_t input_dim,
                                vae_hippo_pattern_result_t* result);

/**
 * @brief Perform pattern completion from partial cue (CA3-like)
 *
 * Takes partial cue, encodes incomplete latent, iteratively
 * completes using VAE decoder/encoder feedback and hippocampal
 * attractor dynamics.
 *
 * @param bridge Bridge instance
 * @param partial_cue Partial input cue
 * @param cue_dim Cue dimension
 * @param mask Binary mask (1 = present, 0 = missing)
 * @param result Output pattern result
 * @return 0 on success, error code on failure
 */
int vae_hippo_pattern_complete(vae_hippo_bridge_t* bridge,
                                const float* partial_cue, uint32_t cue_dim,
                                const float* mask,
                                vae_hippo_pattern_result_t* result);

/**
 * @brief Bind multiple features into unified episode representation
 */
int vae_hippo_pattern_bind(vae_hippo_bridge_t* bridge,
                            const float* what, uint32_t what_dim,
                            const float* where, uint32_t where_dim,
                            const float* when, uint32_t when_dim,
                            vae_hippo_pattern_result_t* result);

/* ============================================================================
 * Generative Memory API
 * ============================================================================ */

/**
 * @brief Generate memory-like samples from learned distribution
 *
 * Samples from VAE prior conditioned on hippocampal memory statistics
 * to generate plausible but novel memory-like content.
 *
 * @param bridge Bridge instance
 * @param num_samples Number of samples to generate
 * @param samples Output samples array
 * @param sample_dim Expected sample dimension
 * @return 0 on success, error code on failure
 */
int vae_hippo_generate_memories(vae_hippo_bridge_t* bridge,
                                 uint32_t num_samples,
                                 float* samples,
                                 uint32_t sample_dim);

/**
 * @brief Interpolate between two episodes in latent space
 */
int vae_hippo_interpolate_episodes(vae_hippo_bridge_t* bridge,
                                    uint32_t episode_id_1,
                                    uint32_t episode_id_2,
                                    uint32_t num_steps,
                                    float* interpolations,
                                    uint32_t step_dim);

/**
 * @brief Generate variation of existing episode
 */
int vae_hippo_generate_variation(vae_hippo_bridge_t* bridge,
                                  uint32_t episode_id,
                                  float variation_strength,
                                  float* variation,
                                  uint32_t output_dim);

/* ============================================================================
 * Novelty and Familiarity API
 * ============================================================================ */

/**
 * @brief Compute novelty score for input
 *
 * Uses VAE reconstruction error and latent distance from baseline
 * to estimate how novel the input is compared to stored memories.
 *
 * @param bridge Bridge instance
 * @param input Input to assess
 * @param input_dim Input dimension
 * @param novelty_score Output novelty (0 = very familiar, 1 = very novel)
 * @return 0 on success, error code on failure
 */
int vae_hippo_compute_novelty(vae_hippo_bridge_t* bridge,
                               const float* input, uint32_t input_dim,
                               float* novelty_score);

/**
 * @brief Compute familiarity score (recognition without retrieval)
 */
int vae_hippo_compute_familiarity(vae_hippo_bridge_t* bridge,
                                   const float* input, uint32_t input_dim,
                                   float* familiarity_score);

/**
 * @brief Update baseline statistics for novelty computation
 */
int vae_hippo_update_baseline(vae_hippo_bridge_t* bridge,
                               const float* input, uint32_t input_dim);

/* ============================================================================
 * Replay Integration API
 * ============================================================================ */

/**
 * @brief Get episode for replay (decodes latent for replay system)
 */
int vae_hippo_get_replay_content(vae_hippo_bridge_t* bridge,
                                  uint32_t episode_id,
                                  float* content,
                                  uint32_t max_dim,
                                  uint32_t* actual_dim);

/**
 * @brief Process replay event (re-encode after replay consolidation)
 */
int vae_hippo_process_replay(vae_hippo_bridge_t* bridge,
                              const uint32_t* episode_ids,
                              uint32_t num_episodes,
                              float compression_factor);

/* ============================================================================
 * Synchronization API
 * ============================================================================ */

/**
 * @brief Sync latent states with hippocampus
 */
int vae_hippo_sync(vae_hippo_bridge_t* bridge);

/**
 * @brief Export hippocampus patterns to VAE training
 */
int vae_hippo_export_for_training(vae_hippo_bridge_t* bridge,
                                   float* patterns,
                                   uint32_t max_patterns,
                                   uint32_t pattern_dim,
                                   uint32_t* num_exported);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge state
 */
vae_hippo_bridge_state_t vae_hippo_bridge_get_state(const vae_hippo_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 */
int vae_hippo_bridge_get_stats(const vae_hippo_bridge_t* bridge,
                                vae_hippo_bridge_stats_t* stats);

/**
 * @brief Get latent dimension
 */
uint32_t vae_hippo_get_latent_dim(const vae_hippo_bridge_t* bridge);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Free encode result resources
 */
void vae_hippo_encode_result_free(vae_hippo_encode_result_t* result);

/**
 * @brief Free retrieve result resources
 */
void vae_hippo_retrieve_result_free(vae_hippo_retrieve_result_t* result);

/**
 * @brief Free similar result resources
 */
void vae_hippo_similar_result_free(vae_hippo_similar_result_t* result);

/**
 * @brief Free pattern result resources
 */
void vae_hippo_pattern_result_free(vae_hippo_pattern_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_HIPPOCAMPUS_BRIDGE_H */
