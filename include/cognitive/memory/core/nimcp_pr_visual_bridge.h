/**
 * @file nimcp_pr_visual_bridge.h
 * @brief Prime Resonant Visual Bridge - Integrates visual processing with PR memory
 *
 * This bridge connects the visual processing pipeline (visual cortex, retina, FEP)
 * with the Prime Resonant memory system. It computes prime signatures from visual
 * features, maps visual attributes to quaternion states, and enables resonance-based
 * visual memory retrieval.
 *
 * Key Features:
 * - Feature → Prime Signature mapping (64 bins per feature dimension)
 * - Quaternion state from visual attributes (w=consolidation, x=emotion, y=salience, z=accessibility)
 * - Theta-gamma phase-gated encoding and retrieval
 * - Entanglement graph for visual memory associations
 * - FEP prediction error integration for active inference
 *
 * @version 1.0.0
 * @date 2025-01-09
 */

#ifndef NIMCP_PR_VISUAL_BRIDGE_H
#define NIMCP_PR_VISUAL_BRIDGE_H

#include "nimcp_pr_memory_node.h"
#include "nimcp_prime_signature.h"
#include "nimcp_quaternion.h"
#include "nimcp_theta_gamma.h"
#include "nimcp_entanglement.h"
#include "nimcp_resonance.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_retina.h"
#include "perception/nimcp_visual_cortex_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"

/* Forward declaration for resonance engine */
struct resonance_engine_struct;
typedef struct resonance_engine_struct resonance_engine_t;

/* Forward declaration for health agent (B25 Upgrade) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Configuration
 * ============================================================================ */

/** Number of feature bins for prime signature computation */
#define PR_VISUAL_FEATURE_BINS          64

/** Number of prime factors in visual signature */
#define PR_VISUAL_PRIME_COUNT           64

/** Maximum visual memories in entanglement graph */
#define PR_VISUAL_MAX_MEMORIES          4096

/** Default theta-gamma coupling strength */
#define PR_VISUAL_DEFAULT_TG_COUPLING   0.7f

/** Default emotion weight for quaternion.x */
#define PR_VISUAL_DEFAULT_EMOTION_WT    0.3f

/** Default salience weight for quaternion.y */
#define PR_VISUAL_DEFAULT_SALIENCE_WT   0.4f

/** Default accessibility weight for quaternion.z */
#define PR_VISUAL_DEFAULT_ACCESS_WT     0.3f

/** Default consolidation decay rate */
#define PR_VISUAL_DEFAULT_CONSOL_DECAY  0.01f

/** Default prediction error threshold for memory update */
#define PR_VISUAL_DEFAULT_PE_THRESHOLD  0.1f

/** Default resonance threshold for retrieval */
#define PR_VISUAL_DEFAULT_RES_THRESHOLD 0.5f

/** Maximum retrieval results */
#define PR_VISUAL_MAX_RETRIEVAL         32

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/**
 * @enum pr_visual_bridge_error_t
 * @brief Error codes specific to the PR Visual Bridge
 */
typedef enum {
    PR_VISUAL_OK = 0,                       /**< Success */
    PR_VISUAL_ERROR_NULL_PARAM = -1,        /**< Null parameter provided */
    PR_VISUAL_ERROR_INVALID_CONFIG = -2,    /**< Invalid configuration */
    PR_VISUAL_ERROR_NOT_INITIALIZED = -3,   /**< Bridge not initialized */
    PR_VISUAL_ERROR_NO_VISUAL_CORTEX = -4,  /**< Visual cortex not connected */
    PR_VISUAL_ERROR_NO_FEP_BRIDGE = -5,     /**< FEP bridge not connected */
    PR_VISUAL_ERROR_NO_RETINA = -6,         /**< Retina not connected */
    PR_VISUAL_ERROR_MEMORY_FULL = -7,       /**< Visual memory capacity exceeded */
    PR_VISUAL_ERROR_ENCODING_FAILED = -8,   /**< Memory encoding failed */
    PR_VISUAL_ERROR_RETRIEVAL_FAILED = -9,  /**< Memory retrieval failed */
    PR_VISUAL_ERROR_PHASE_MISMATCH = -10,   /**< Theta-gamma phase window mismatch */
    PR_VISUAL_ERROR_SIGNATURE_FAILED = -11, /**< Prime signature computation failed */
    PR_VISUAL_ERROR_QUATERNION_FAILED = -12,/**< Quaternion computation failed */
    PR_VISUAL_ERROR_ENTANGLE_FAILED = -13,  /**< Entanglement operation failed */
    PR_VISUAL_ERROR_ALLOCATION = -14,       /**< Memory allocation failed */
    PR_VISUAL_ERROR_MUTEX = -15,            /**< Mutex operation failed */
    PR_VISUAL_ERROR_UNKNOWN = -99           /**< Unknown error */
} pr_visual_bridge_error_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct pr_visual_bridge_t pr_visual_bridge_t;
typedef struct pr_visual_bridge_config_t pr_visual_bridge_config_t;
typedef struct pr_visual_bridge_stats_t pr_visual_bridge_stats_t;
typedef struct pr_visual_feature_vector_t pr_visual_feature_vector_t;
typedef struct pr_visual_retrieval_result_t pr_visual_retrieval_result_t;

/* ============================================================================
 * Feature Vector Structure
 * ============================================================================ */

/**
 * @struct pr_visual_feature_vector_t
 * @brief Extracted visual features for prime signature computation
 */
struct pr_visual_feature_vector_t {
    /** Color histogram bins (normalized 0-1) */
    float color_histogram[PR_VISUAL_FEATURE_BINS];

    /** Edge orientation bins */
    float edge_orientations[PR_VISUAL_FEATURE_BINS];

    /** Spatial frequency bins */
    float spatial_frequencies[PR_VISUAL_FEATURE_BINS];

    /** Texture energy bins */
    float texture_energy[PR_VISUAL_FEATURE_BINS];

    /** Multispectral features from retina (UV, NIR, thermal) */
    float multispectral[PR_VISUAL_FEATURE_BINS];

    /** Attention-weighted saliency map bins */
    float saliency_bins[PR_VISUAL_FEATURE_BINS];

    /** Total feature count */
    uint32_t feature_count;

    /** Timestamp of extraction */
    uint64_t timestamp_ns;
};

/* ============================================================================
 * Retrieval Result Structure
 * ============================================================================ */

/**
 * @struct pr_visual_retrieval_result_t
 * @brief Result from visual memory retrieval
 */
struct pr_visual_retrieval_result_t {
    /** Retrieved memory node */
    pr_memory_node_t* memory_node;

    /** Resonance score (0-1) */
    float resonance_score;

    /** Jaccard similarity component */
    float jaccard_component;

    /** Phase alignment component */
    float phase_component;

    /** Quaternion similarity component */
    float quaternion_component;

    /** Kuramoto synchronization component */
    float kuramoto_component;

    /** Prediction error relative to query */
    float prediction_error;
};

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @struct pr_visual_bridge_config_t
 * @brief Configuration for the PR Visual Bridge
 */
struct pr_visual_bridge_config_t {
    /** Maximum visual memories to store */
    uint32_t max_memories;

    /** Theta-gamma coupling strength (0-1) */
    float theta_gamma_coupling;

    /** Emotion weight for quaternion.x computation */
    float emotion_weight;

    /** Salience weight for quaternion.y computation */
    float salience_weight;

    /** Accessibility weight for quaternion.z computation */
    float accessibility_weight;

    /** Consolidation decay rate per update */
    float consolidation_decay;

    /** Prediction error threshold for memory update */
    float pe_threshold;

    /** Resonance threshold for retrieval */
    float resonance_threshold;

    /** Enable automatic entanglement creation */
    bool auto_entangle;

    /** Enable FEP-driven active inference */
    bool enable_active_inference;

    /** Enable theta-gamma phase gating */
    bool enable_phase_gating;

    /** Prime signature configuration */
    prime_sig_config_t sig_config;

    /** Resonance scoring configuration */
    resonance_config_t resonance_config;
};

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @struct pr_visual_bridge_stats_t
 * @brief Runtime statistics for the PR Visual Bridge
 */
struct pr_visual_bridge_stats_t {
    /** Total frames processed */
    uint64_t frames_processed;

    /** Total memories encoded */
    uint64_t memories_encoded;

    /** Total retrieval queries */
    uint64_t retrieval_queries;

    /** Successful retrievals */
    uint64_t successful_retrievals;

    /** Phase-gated encode operations */
    uint64_t phase_gated_encodes;

    /** Phase-gated retrieve operations */
    uint64_t phase_gated_retrieves;

    /** FEP updates applied */
    uint64_t fep_updates;

    /** Entanglement edges created */
    uint64_t entangle_edges_created;

    /** Average resonance score */
    float avg_resonance_score;

    /** Average prediction error */
    float avg_prediction_error;

    /** Current memory count */
    uint32_t current_memory_count;

    /** Current entanglement edge count */
    uint32_t current_edge_count;

    /** Processing time statistics (ns) */
    uint64_t total_processing_time_ns;
    uint64_t max_processing_time_ns;
    uint64_t min_processing_time_ns;
};

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @struct pr_visual_bridge_t
 * @brief Prime Resonant Visual Bridge main structure
 */
struct pr_visual_bridge_t {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /** Connected visual cortex */
    visual_cortex_t* visual_cortex;

    /** Connected FEP bridge */
    visual_cortex_fep_bridge_t* fep_bridge;

    /** Connected retina */
    retina_t* retina;

    /** Current visual memory being processed */
    pr_memory_node_t* current_visual_memory;

    /** Visual memory entanglement graph */
    entangle_graph_t visual_entanglement;

    /** Prime signature configuration */
    prime_sig_config_t sig_config;

    /** Current visual quaternion state */
    nimcp_quaternion_t current_visual_quat;

    /** Theta-gamma manager */
    theta_gamma_manager_t* theta_gamma;

    /** Bridge configuration */
    pr_visual_bridge_config_t config;

    /** Runtime statistics */
    pr_visual_bridge_stats_t stats;

    /** Memory node pool */
    pr_memory_node_t** memory_pool;
    uint32_t memory_pool_size;
    uint32_t memory_pool_count;

    /** Current feature vector */
    pr_visual_feature_vector_t current_features;

    /** Current prime signature */
    prime_signature_t current_signature;

    /** Resonance engine */
    resonance_engine_t* resonance_engine;

    /** Initialization flag */
    bool initialized;

    /** Last error code */
    pr_visual_bridge_error_t last_error;

    /** Instance-level health agent (B25 Upgrade) */
    nimcp_health_agent_t* health_agent;
};

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration for PR Visual Bridge
 * @return Default configuration structure
 */
NIMCP_EXPORT pr_visual_bridge_config_t pr_visual_bridge_default_config(void);

/**
 * @brief Create a new PR Visual Bridge
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
NIMCP_EXPORT pr_visual_bridge_t* pr_visual_bridge_create(
    const pr_visual_bridge_config_t* config);

/**
 * @brief Destroy a PR Visual Bridge
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void pr_visual_bridge_destroy(pr_visual_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge instance
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_reset(
    pr_visual_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect visual cortex to the bridge
 * @param bridge Bridge instance
 * @param cortex Visual cortex to connect
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_connect_visual_cortex(
    pr_visual_bridge_t* bridge,
    visual_cortex_t* cortex);

/**
 * @brief Connect FEP bridge for active inference
 * @param bridge Bridge instance
 * @param fep_bridge FEP bridge to connect
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_connect_fep_bridge(
    pr_visual_bridge_t* bridge,
    visual_cortex_fep_bridge_t* fep_bridge);

/**
 * @brief Connect retina for multispectral features
 * @param bridge Bridge instance
 * @param retina Retina to connect
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_connect_retina(
    pr_visual_bridge_t* bridge,
    retina_t* retina);

/**
 * @brief Connect theta-gamma manager for phase gating
 * @param bridge Bridge instance
 * @param tg_manager Theta-gamma manager to connect
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_connect_theta_gamma(
    pr_visual_bridge_t* bridge,
    theta_gamma_manager_t* tg_manager);

/* ============================================================================
 * Processing Functions
 * ============================================================================ */

/**
 * @brief Process a visual frame through the bridge
 *
 * Main processing function that:
 * 1. Extracts features from visual cortex output
 * 2. Computes prime signature from features
 * 3. Computes quaternion state from visual attributes
 * 4. Optionally encodes to memory (if in encode phase)
 * 5. Updates from FEP prediction error
 *
 * @param bridge Bridge instance
 * @param frame_id Frame identifier
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_process_frame(
    pr_visual_bridge_t* bridge,
    uint64_t frame_id);

/**
 * @brief Extract features from current visual cortex state
 * @param bridge Bridge instance
 * @param features Output feature vector
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_extract_features(
    pr_visual_bridge_t* bridge,
    pr_visual_feature_vector_t* features);

/* ============================================================================
 * Prime Signature Functions
 * ============================================================================ */

/**
 * @brief Compute prime signature from visual features
 *
 * Maps visual feature bins to prime factors:
 * 1. Quantize each feature to 64 bins
 * 2. Hash bin indices to prime factor selection
 * 3. Set exponents based on feature intensity
 *
 * @param bridge Bridge instance
 * @param features Input feature vector
 * @param signature Output prime signature
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_compute_visual_prime_sig(
    pr_visual_bridge_t* bridge,
    const pr_visual_feature_vector_t* features,
    prime_signature_t* signature);

/**
 * @brief Compute signature similarity between two visual signatures
 * @param bridge Bridge instance
 * @param sig1 First signature
 * @param sig2 Second signature
 * @param similarity Output similarity score (0-1)
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_signature_similarity(
    pr_visual_bridge_t* bridge,
    const prime_signature_t* sig1,
    const prime_signature_t* sig2,
    float* similarity);

/* ============================================================================
 * Quaternion Functions
 * ============================================================================ */

/**
 * @brief Compute quaternion state from visual attributes
 *
 * Maps visual processing outputs to quaternion components:
 * - w (consolidation): Based on repeated viewing / memory strength
 * - x (emotion): Color warmth, contrast, aesthetic features
 * - y (salience): Attention map intensity, gaze targets
 * - z (accessibility): Novelty score, prediction error
 *
 * @param bridge Bridge instance
 * @param quat Output quaternion
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_compute_visual_quaternion(
    pr_visual_bridge_t* bridge,
    nimcp_quaternion_t* quat);

/**
 * @brief Apply attention map to salience (quaternion.y)
 * @param bridge Bridge instance
 * @param attention_map Attention map from visual cortex
 * @param quat Quaternion to update (modifies y component)
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_apply_attention_to_salience(
    pr_visual_bridge_t* bridge,
    const attention_map_t* attention_map,
    nimcp_quaternion_t* quat);

/**
 * @brief Apply novelty to accessibility (quaternion.z)
 * @param bridge Bridge instance
 * @param novelty Novelty score (0-1)
 * @param quat Quaternion to update (modifies z component)
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_apply_novelty_to_accessibility(
    pr_visual_bridge_t* bridge,
    float novelty,
    nimcp_quaternion_t* quat);

/**
 * @brief Apply color warmth to emotion (quaternion.x)
 * @param bridge Bridge instance
 * @param warmth Color warmth score (0-1, warm to cool)
 * @param quat Quaternion to update (modifies x component)
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_apply_warmth_to_emotion(
    pr_visual_bridge_t* bridge,
    float warmth,
    nimcp_quaternion_t* quat);

/* ============================================================================
 * Memory Encoding Functions
 * ============================================================================ */

/**
 * @brief Encode current visual state to memory
 *
 * Creates a new PR memory node from current visual processing:
 * 1. Uses current prime signature
 * 2. Uses current quaternion state
 * 3. Respects theta-gamma phase gating if enabled
 * 4. Creates entanglement edges to similar memories
 *
 * @param bridge Bridge instance
 * @param memory_node Output memory node (if provided, uses this instead of allocating)
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_encode_to_memory(
    pr_visual_bridge_t* bridge,
    pr_memory_node_t** memory_node);

/**
 * @brief Encode with explicit signature and quaternion
 * @param bridge Bridge instance
 * @param signature Prime signature to encode
 * @param quat Quaternion state to encode
 * @param memory_node Output memory node
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_encode_explicit(
    pr_visual_bridge_t* bridge,
    const prime_signature_t* signature,
    const nimcp_quaternion_t* quat,
    pr_memory_node_t** memory_node);

/* ============================================================================
 * Memory Retrieval Functions
 * ============================================================================ */

/**
 * @brief Retrieve similar visual memories using resonance
 *
 * Performs resonance-based retrieval:
 * 1. Computes resonance scores against memory pool
 * 2. Respects theta-gamma phase gating if enabled
 * 3. Returns top matches above threshold
 *
 * @param bridge Bridge instance
 * @param query_signature Signature to query with
 * @param query_quat Quaternion state for query
 * @param results Output array of retrieval results
 * @param max_results Maximum results to return
 * @param result_count Actual number of results
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_retrieve_similar_visual(
    pr_visual_bridge_t* bridge,
    const prime_signature_t* query_signature,
    const nimcp_quaternion_t* query_quat,
    pr_visual_retrieval_result_t* results,
    uint32_t max_results,
    uint32_t* result_count);

/**
 * @brief Retrieve using current visual state as query
 * @param bridge Bridge instance
 * @param results Output array of retrieval results
 * @param max_results Maximum results to return
 * @param result_count Actual number of results
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_retrieve_current(
    pr_visual_bridge_t* bridge,
    pr_visual_retrieval_result_t* results,
    uint32_t max_results,
    uint32_t* result_count);

/* ============================================================================
 * FEP Integration Functions
 * ============================================================================ */

/**
 * @brief Update visual memory from FEP prediction error
 *
 * Uses prediction error from FEP bridge to:
 * 1. Modulate quaternion accessibility (z)
 * 2. Trigger memory consolidation on low PE
 * 3. Trigger memory update on high PE
 *
 * @param bridge Bridge instance
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_update_from_fep(
    pr_visual_bridge_t* bridge);

/**
 * @brief Get prediction error for a specific memory
 * @param bridge Bridge instance
 * @param memory_node Memory to check
 * @param prediction_error Output prediction error
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_memory_pe(
    pr_visual_bridge_t* bridge,
    const pr_memory_node_t* memory_node,
    float* prediction_error);

/* ============================================================================
 * Entanglement Functions
 * ============================================================================ */

/**
 * @brief Get the visual memory entanglement graph
 * @param bridge Bridge instance
 * @return Entanglement graph or NULL if not available
 */
NIMCP_EXPORT entangle_graph_t pr_visual_bridge_get_visual_entanglement(
    pr_visual_bridge_t* bridge);

/**
 * @brief Create entanglement between visual memories
 * @param bridge Bridge instance
 * @param node1 First memory node
 * @param node2 Second memory node
 * @param edge_type Type of entanglement edge
 * @param strength Edge strength (0-1)
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_entangle_memories(
    pr_visual_bridge_t* bridge,
    pr_memory_node_t* node1,
    pr_memory_node_t* node2,
    entangle_edge_type_t edge_type,
    float strength);

/**
 * @brief Auto-entangle current memory with similar memories
 * @param bridge Bridge instance
 * @param similarity_threshold Minimum similarity for entanglement
 * @param edges_created Output number of edges created
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_auto_entangle(
    pr_visual_bridge_t* bridge,
    float similarity_threshold,
    uint32_t* edges_created);

/* ============================================================================
 * Theta-Gamma Functions
 * ============================================================================ */

/**
 * @brief Check if currently in encode phase
 * @param bridge Bridge instance
 * @return true if in encode phase (0-90 degrees)
 */
NIMCP_EXPORT bool pr_visual_bridge_in_encode_phase(pr_visual_bridge_t* bridge);

/**
 * @brief Check if currently in retrieve phase
 * @param bridge Bridge instance
 * @return true if in retrieve phase (180-270 degrees)
 */
NIMCP_EXPORT bool pr_visual_bridge_in_retrieve_phase(pr_visual_bridge_t* bridge);

/**
 * @brief Get current theta phase (0-360 degrees)
 * @param bridge Bridge instance
 * @return Current phase in degrees
 */
NIMCP_EXPORT float pr_visual_bridge_get_theta_phase(pr_visual_bridge_t* bridge);

/**
 * @brief Force phase advance (for testing)
 * @param bridge Bridge instance
 * @param phase_degrees Phase to set (0-360)
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_set_theta_phase(
    pr_visual_bridge_t* bridge,
    float phase_degrees);

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_stats(
    pr_visual_bridge_t* bridge,
    pr_visual_bridge_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge instance
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_reset_stats(
    pr_visual_bridge_t* bridge);

/**
 * @brief Get last error code
 * @param bridge Bridge instance
 * @return Last error code
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_last_error(
    pr_visual_bridge_t* bridge);

/**
 * @brief Get error string for error code
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_visual_bridge_error_string(
    pr_visual_bridge_error_t error);

/**
 * @brief Check if bridge is fully connected
 * @param bridge Bridge instance
 * @return true if visual cortex, FEP bridge, and retina are connected
 */
NIMCP_EXPORT bool pr_visual_bridge_is_connected(pr_visual_bridge_t* bridge);

/**
 * @brief Get current visual quaternion
 * @param bridge Bridge instance
 * @param quat Output quaternion
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_current_quaternion(
    pr_visual_bridge_t* bridge,
    nimcp_quaternion_t* quat);

/**
 * @brief Get current prime signature
 * @param bridge Bridge instance
 * @param signature Output signature
 * @return PR_VISUAL_OK on success, error code otherwise
 */
NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_current_signature(
    pr_visual_bridge_t* bridge,
    prime_signature_t* signature);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_VISUAL_BRIDGE_H */
