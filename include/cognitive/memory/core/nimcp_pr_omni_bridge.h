//=============================================================================
// nimcp_pr_omni_bridge.h - Prime Resonant Omni-Sensory Bridge
//=============================================================================
/**
 * @file nimcp_pr_omni_bridge.h
 * @brief Bridge integrating cross-modal perception with Prime Resonant memory
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Integrates omni-sensory perception (visual, audio, speech) with PR memory
 * WHY:  Multimodal experiences form unified memories; cross-modal binding creates
 *       richer, more accessible memory traces through multiple retrieval cues
 * HOW:  Fuses modal prime signatures, SLERP-blends quaternions, Kuramoto-couples
 *       oscillators, creates entangled multimodal memory nodes
 *
 * NEUROSCIENCE FOUNDATION:
 * =============================================================================
 *
 * Multimodal Memory Integration:
 * +--------------------------------------------------------------------------+
 * |  Memories are rarely unimodal. Seeing a friend's face while hearing      |
 * |  their voice creates a unified memory trace that can be retrieved by     |
 * |  either cue. This "binding" occurs through:                              |
 * |                                                                          |
 * |  1. CROSS-MODAL PRIME SIGNATURE FUSION                                   |
 * |     - Each modality generates prime factors from its content             |
 * |     - Bound modalities share primes (intersection weighted by binding)   |
 * |     - Weakly bound modalities have more separate primes                  |
 * |     - Result: Unified signature retrievable by any modal cue             |
 * |                                                                          |
 * |  2. QUATERNION STATE BLENDING (SLERP)                                    |
 * |     - Each modality has its own quaternion state                         |
 * |     - Unified quaternion = SLERP blend with adaptive weights             |
 * |     - w: max consolidation (strongest modal encoding wins)               |
 * |     - x: weighted average emotion                                        |
 * |     - y: max salience (most salient modality dominates)                  |
 * |     - z: geometric mean accessibility                                    |
 * |                                                                          |
 * |  3. KURAMOTO OSCILLATOR COUPLING                                         |
 * |     - Each modality is an oscillator in the Kuramoto system              |
 * |     - Cross-modal binding = high phase coherence                         |
 * |     - Binding strength modulates coupling strength                       |
 * |     - Synchronized modalities communicate effectively                    |
 * |                                                                          |
 * |  4. THETA-GAMMA PHASE GATING                                             |
 * |     - Multimodal encoding occurs during theta trough                     |
 * |     - Cross-modal retrieval optimal at theta peak                        |
 * |     - Gamma bursts carry modal-specific information                      |
 * +--------------------------------------------------------------------------+
 *
 * Cross-Modal Binding Strength:
 * +--------------------------------------------------------------------------+
 * |  Binding strength [0, 1] indicates how tightly two modalities are        |
 * |  associated in the current memory:                                       |
 * |                                                                          |
 * |  0.0 = Independent (seeing a face, hearing unrelated sound)              |
 * |  0.5 = Weak binding (same event, not semantically linked)                |
 * |  1.0 = Strong binding (face + voice of same person speaking)             |
 * |                                                                          |
 * |  Computed from:                                                          |
 * |  - Temporal coincidence (omni-sensory bridge provides)                   |
 * |  - Semantic coherence (prediction error minimization)                    |
 * |  - Attention weights (what was attended together)                        |
 * |  - Prediction success (correct cross-modal predictions)                  |
 * +--------------------------------------------------------------------------+
 *
 * ARCHITECTURE:
 * =============================================================================
 *
 *   +------------------+     +------------------+     +------------------+
 *   |  PR Visual       |     |  PR Audio        |     |  PR Speech       |
 *   |  Bridge          |     |  Bridge          |     |  Bridge          |
 *   +--------+---------+     +--------+---------+     +--------+---------+
 *            |                        |                        |
 *            v                        v                        v
 *   +--------+---------+     +--------+---------+     +--------+---------+
 *   | Visual Signature |     | Audio Signature  |     | Speech Signature |
 *   | Visual Quaternion|     | Audio Quaternion |     | Speech Quaternion|
 *   +--------+---------+     +--------+---------+     +--------+---------+
 *            |                        |                        |
 *            +------------------------+------------------------+
 *                                     |
 *                                     v
 *                     +---------------+----------------+
 *                     |     PR OMNI BRIDGE             |
 *                     |                                |
 *                     | - Compute binding strengths    |
 *                     | - Fuse prime signatures        |
 *                     | - SLERP quaternions            |
 *                     | - Kuramoto coupling            |
 *                     | - Create multimodal memory     |
 *                     +---------------+----------------+
 *                                     |
 *                                     v
 *                     +---------------+----------------+
 *                     |  Unified Multimodal Memory     |
 *                     |  - Combined prime signature    |
 *                     |  - Blended quaternion state    |
 *                     |  - Entanglement graph          |
 *                     +--------------------------------+
 *
 * PERFORMANCE:
 * =============================================================================
 * - Update cycle: ~200us (signature fusion + quaternion blend + Kuramoto step)
 * - Memory fusion: ~100us (create multimodal node + entanglement)
 * - Retrieval: ~150us (cross-modal resonance search)
 * - Binding computation: ~50us per modality pair
 *
 * MEMORY:
 * =============================================================================
 * - pr_omni_bridge_t: ~512 bytes (config + state + pointers)
 * - Per multimodal memory: ~200 bytes (node) + entanglement overhead
 * - Kuramoto oscillators: 3 oscillators * ~20 bytes = 60 bytes
 *
 * INTEGRATION:
 * =============================================================================
 * - Core: Omni-sensory bridge (cross-modal binding, predictions)
 * - Core: PR Visual/Audio/Speech bridges (modal memory representations)
 * - Core: Prime signature, quaternion, entanglement systems
 * - Middleware: Theta-gamma gating, Kuramoto synchronization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_OMNI_BRIDGE_H
#define NIMCP_PR_OMNI_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include order matters - avoid duplicate type definitions */
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_resonance.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_kuramoto.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "perception/nimcp_omni_sensory_bridge.h"
#include "constants/nimcp_math_constants.h"

/* Note: theta_gamma.h has its own quaternion and kuramoto forward declarations
 * that conflict with the full definitions. We use forward declaration instead. */

/* Forward declare theta_gamma_manager_t to avoid including conflicting header */
typedef struct theta_gamma_manager_internal* theta_gamma_manager_t;

/* Forward declare theta-gamma functions we need */
extern float theta_gamma_get_encode_strength(const theta_gamma_manager_t manager);
extern float theta_gamma_get_retrieve_strength(const theta_gamma_manager_t manager);
extern bool theta_gamma_can_encode(const theta_gamma_manager_t manager);

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Number of sensory modalities (visual, audio, speech) */
#define PR_OMNI_NUM_MODALITIES          3

/** Index for visual modality */
#define PR_OMNI_MODALITY_VISUAL         0

/** Index for audio modality */
#define PR_OMNI_MODALITY_AUDIO          1

/** Index for speech modality */
#define PR_OMNI_MODALITY_SPEECH         2

/** Default binding strength threshold for fusion */
#define PR_OMNI_BINDING_THRESHOLD       0.3f

/** Default coherence threshold for cross-modal sync */
#define PR_OMNI_COHERENCE_THRESHOLD     0.5f

/** Default Kuramoto coupling strength for modal oscillators */
#define PR_OMNI_KURAMOTO_COUPLING       0.6f

/** Maximum number of multimodal memories to track */
#define PR_OMNI_MAX_MULTIMODAL_MEMORIES 1024

/** Default signature fusion mode: weighted union */
#define PR_OMNI_FUSION_WEIGHTED_UNION   0

/** Signature fusion mode: intersection weighted by binding */
#define PR_OMNI_FUSION_BINDING_INTERSECT 1

/** Pi constant */

/** Epsilon for floating-point comparisons */
#define PR_OMNI_EPSILON                 1e-6f

//=============================================================================
// Forward Declarations
//=============================================================================

/** Forward declare modal bridge types */
typedef struct pr_visual_bridge_s pr_visual_bridge_t;
typedef struct pr_audio_bridge_s pr_audio_bridge_t;
typedef struct pr_speech_bridge_s pr_speech_bridge_t;

/* Forward declaration for health agent (Phase 8) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Error codes for PR Omni Bridge operations
 */
typedef enum {
    PR_OMNI_SUCCESS = 0,              /**< Operation succeeded */
    PR_OMNI_ERROR_NULL_POINTER = -1,  /**< NULL pointer argument */
    PR_OMNI_ERROR_INVALID_CONFIG = -2,/**< Invalid configuration */
    PR_OMNI_ERROR_NO_MEMORY = -3,     /**< Memory allocation failed */
    PR_OMNI_ERROR_NOT_CONNECTED = -4, /**< Required bridges not connected */
    PR_OMNI_ERROR_INVALID_MODALITY = -5, /**< Invalid modality index */
    PR_OMNI_ERROR_FUSION_FAILED = -6, /**< Signature fusion failed */
    PR_OMNI_ERROR_QUATERNION_FAILED = -7, /**< Quaternion blend failed */
    PR_OMNI_ERROR_MEMORY_FAILED = -8, /**< Memory node operation failed */
    PR_OMNI_ERROR_KURAMOTO_FAILED = -9, /**< Kuramoto operation failed */
    PR_OMNI_ERROR_RETRIEVAL_FAILED = -10, /**< Cross-modal retrieval failed */
    PR_OMNI_ERROR_PHASE_BLOCKED = -11 /**< Operation blocked by theta phase */
} pr_omni_error_t;

/**
 * @brief Modality dominance for retrieval
 */
typedef enum {
    PR_OMNI_DOMINANT_NONE = 0,        /**< No single dominant modality */
    PR_OMNI_DOMINANT_VISUAL,          /**< Visual is most salient */
    PR_OMNI_DOMINANT_AUDIO,           /**< Audio is most salient */
    PR_OMNI_DOMINANT_SPEECH           /**< Speech is most salient */
} pr_omni_dominant_modality_t;

/**
 * @brief Signature fusion strategy
 */
typedef enum {
    PR_OMNI_FUSION_UNION = 0,         /**< Union of all modal primes */
    PR_OMNI_FUSION_INTERSECTION,      /**< Intersection (shared primes only) */
    PR_OMNI_FUSION_WEIGHTED,          /**< Weighted by binding strength */
    PR_OMNI_FUSION_DOMINANT           /**< Dominant modality's signature */
} pr_omni_fusion_strategy_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for PR Omni Bridge
 */
typedef struct {
    /* Binding thresholds */
    float binding_threshold;          /**< Min binding for fusion (default 0.3) */
    float coherence_threshold;        /**< Min coherence for sync (default 0.5) */

    /* Signature fusion */
    pr_omni_fusion_strategy_t fusion_strategy; /**< How to fuse signatures */
    float shared_prime_boost;         /**< Boost for primes in multiple modalities */

    /* Quaternion blending */
    float slerp_base_t;               /**< Base interpolation factor (0.5) */
    bool use_adaptive_weights;        /**< Adapt weights based on binding */

    /* Kuramoto coupling */
    float kuramoto_coupling;          /**< Base coupling strength */
    bool enable_adaptive_coupling;    /**< Coupling adapts to binding */

    /* Theta-gamma gating */
    bool enable_phase_gating;         /**< Gate operations by theta phase */
    float encoding_gate_threshold;    /**< Min gate for encoding */
    float retrieval_gate_threshold;   /**< Min gate for retrieval */

    /* Memory creation */
    bool auto_create_memories;        /**< Auto-create multimodal memories */
    float memory_creation_threshold;  /**< Binding threshold for memory creation */

    /* Statistics */
    bool track_statistics;            /**< Enable statistics tracking */
} pr_omni_bridge_config_t;

/**
 * @brief Cross-modal binding state between modality pairs
 */
typedef struct {
    float binding_visual_audio;       /**< Visual-audio binding [0, 1] */
    float binding_visual_speech;      /**< Visual-speech binding [0, 1] */
    float binding_audio_speech;       /**< Audio-speech binding [0, 1] */
    float overall_coherence;          /**< Overall trimodal coherence [0, 1] */

    /* Temporal binding indicators */
    bool visual_audio_bound;          /**< V-A binding above threshold */
    bool visual_speech_bound;         /**< V-S binding above threshold */
    bool audio_speech_bound;          /**< A-S binding above threshold */
    bool fully_bound;                 /**< All three modalities bound */

    /* Kuramoto phase coherences */
    float kuramoto_va_coherence;      /**< Kuramoto V-A phase coherence */
    float kuramoto_vs_coherence;      /**< Kuramoto V-S phase coherence */
    float kuramoto_as_coherence;      /**< Kuramoto A-S phase coherence */
} pr_omni_binding_state_t;

/**
 * @brief Modal weight configuration for quaternion blending
 */
typedef struct {
    float visual_weight;              /**< Weight for visual quaternion */
    float audio_weight;               /**< Weight for audio quaternion */
    float speech_weight;              /**< Weight for speech quaternion */

    /* Per-component weights (advanced) */
    float consolidation_weights[PR_OMNI_NUM_MODALITIES]; /**< w-component weights */
    float emotion_weights[PR_OMNI_NUM_MODALITIES];       /**< x-component weights */
    float salience_weights[PR_OMNI_NUM_MODALITIES];      /**< y-component weights */
    float accessibility_weights[PR_OMNI_NUM_MODALITIES]; /**< z-component weights */
} pr_omni_modal_weights_t;

/**
 * @brief Unified multimodal memory representation
 */
typedef struct {
    pr_memory_node_t* memory_node;    /**< The actual memory node */

    /* Source modality information */
    bool has_visual;                  /**< Visual component present */
    bool has_audio;                   /**< Audio component present */
    bool has_speech;                  /**< Speech component present */

    /* Original modal signatures (before fusion) */
    prime_signature_t* visual_sig;    /**< Original visual signature */
    prime_signature_t* audio_sig;     /**< Original audio signature */
    prime_signature_t* speech_sig;    /**< Original speech signature */

    /* Original modal quaternions (before blend) */
    nimcp_quaternion_t visual_quat;   /**< Original visual quaternion */
    nimcp_quaternion_t audio_quat;    /**< Original audio quaternion */
    nimcp_quaternion_t speech_quat;   /**< Original speech quaternion */

    /* Binding state at creation */
    pr_omni_binding_state_t binding_at_creation;

    /* Timestamps */
    uint64_t creation_time_ns;        /**< When multimodal memory was created */
    uint64_t last_access_time_ns;     /**< Last retrieval time */
    uint32_t access_count;            /**< Number of retrievals */
} pr_omni_multimodal_memory_t;

/**
 * @brief Statistics for PR Omni Bridge operations
 */
typedef struct {
    uint64_t total_updates;           /**< Total update cycles */
    uint64_t signature_fusions;       /**< Signature fusion operations */
    uint64_t quaternion_blends;       /**< Quaternion blend operations */
    uint64_t memories_created;        /**< Multimodal memories created */
    uint64_t cross_modal_retrievals;  /**< Cross-modal retrieval operations */

    /* Binding statistics */
    uint64_t full_bindings;           /**< Times all three modalities bound */
    uint64_t visual_audio_bindings;   /**< V-A binding events */
    uint64_t visual_speech_bindings;  /**< V-S binding events */
    uint64_t audio_speech_bindings;   /**< A-S binding events */

    /* Average metrics */
    float avg_overall_coherence;      /**< Running average coherence */
    float avg_binding_strength;       /**< Running average binding strength */
    float avg_dominant_weight;        /**< Average weight of dominant modality */

    /* Phase gating statistics */
    uint64_t encode_operations;       /**< Operations during encoding phase */
    uint64_t retrieve_operations;     /**< Operations during retrieval phase */
    uint64_t phase_blocked;           /**< Operations blocked by phase */

    /* Timing */
    uint64_t total_update_time_us;    /**< Total time in updates (microseconds) */
    uint64_t total_fusion_time_us;    /**< Total time in fusion (microseconds) */
    uint64_t first_update_ns;         /**< First update timestamp */
    uint64_t last_update_ns;          /**< Last update timestamp */
} pr_omni_bridge_stats_t;

/**
 * @brief Main PR Omni-Sensory Bridge structure
 *
 * Integrates cross-modal perception with Prime Resonant memory through:
 * - Prime signature fusion (combine modal signatures)
 * - Quaternion SLERP blending (unified semantic state)
 * - Kuramoto oscillator coupling (cross-modal synchronization)
 * - Entanglement graph (multimodal associations)
 */
typedef struct {
    /* Connected omni-sensory bridge */
    omni_sensory_bridge_t* omni_bridge; /**< Source of cross-modal binding */

    /* Connected PR perception bridges */
    pr_visual_bridge_t* visual_bridge;  /**< PR visual memory bridge */
    pr_audio_bridge_t* audio_bridge;    /**< PR audio memory bridge */
    pr_speech_bridge_t* speech_bridge;  /**< PR speech memory bridge */

    /* Unified multimodal memory */
    pr_memory_node_t* current_multimodal_memory; /**< Current unified memory */
    entangle_graph_t multimodal_entanglement; /**< Cross-modal associations */

    /* Cross-modal prime signature */
    prime_signature_t unified_signature; /**< Combined signature from all modalities */
    prime_sig_config_t sig_config; /**< Configuration for signature operations */

    /* Unified quaternion (weighted blend of modal quaternions) */
    nimcp_quaternion_t unified_quaternion; /**< Blended semantic state */
    pr_omni_modal_weights_t modal_weights; /**< Weights for blending */

    /* Cross-modal binding state */
    pr_omni_binding_state_t binding_state; /**< Current binding strengths */

    /* Theta-gamma coupling for multimodal integration */
    theta_gamma_manager_t theta_gamma;  /**< Phase gating for encoding/retrieval */

    /* Kuramoto oscillator bank for cross-modal sync */
    kuramoto_system_t* modal_oscillators; /**< One oscillator per modality */
    uint32_t visual_osc_id;             /**< Kuramoto ID for visual */
    uint32_t audio_osc_id;              /**< Kuramoto ID for audio */
    uint32_t speech_osc_id;             /**< Kuramoto ID for speech */

    /* Configuration */
    pr_omni_bridge_config_t config;     /**< Bridge configuration */

    /* Statistics */
    pr_omni_bridge_stats_t stats;       /**< Operational statistics */

    /* Internal state */
    bool bridges_connected;             /**< Are all bridges connected */
    bool initialized;                   /**< Is bridge initialized */
    pr_omni_dominant_modality_t dominant; /**< Current dominant modality */

    /* Thread safety */
    void* mutex;                        /**< Internal mutex */

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
} pr_omni_bridge_t;

/**
 * @brief Cross-modal retrieval query
 */
typedef struct {
    /* Query can be from any modality */
    prime_signature_t* query_signature; /**< Query prime signature */
    nimcp_quaternion_t query_quaternion; /**< Query semantic state */

    /* Which modality is the query from */
    int query_modality;                 /**< 0=visual, 1=audio, 2=speech */

    /* Retrieval parameters */
    size_t max_results;                 /**< Maximum results to return */
    float min_resonance;                /**< Minimum resonance threshold */
    bool prefer_multimodal;             /**< Prefer multimodal memories */
    float cross_modal_boost;            /**< Boost for cross-modal matches */
} pr_omni_retrieval_query_t;

/**
 * @brief Cross-modal retrieval result
 */
typedef struct {
    pr_omni_multimodal_memory_t* memory; /**< Retrieved memory */
    float resonance_score;              /**< Overall resonance */
    float visual_resonance;             /**< Visual component resonance */
    float audio_resonance;              /**< Audio component resonance */
    float speech_resonance;             /**< Speech component resonance */
    float cross_modal_bonus;            /**< Cross-modal retrieval bonus */
    pr_omni_dominant_modality_t matched_via; /**< Which modality matched */
} pr_omni_retrieval_result_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default PR Omni Bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for typical multimodal integration
 * HOW:  Sets balanced parameters for cross-modal binding
 *
 * @return Default configuration structure
 *
 * Default values:
 * - binding_threshold: 0.3
 * - coherence_threshold: 0.5
 * - fusion_strategy: PR_OMNI_FUSION_WEIGHTED
 * - kuramoto_coupling: 0.6
 * - enable_phase_gating: true
 * - auto_create_memories: true
 *
 * EXAMPLE:
 * ```c
 * pr_omni_bridge_config_t config = pr_omni_bridge_config_default();
 * config.binding_threshold = 0.5f;  // More selective binding
 * pr_omni_bridge_t* bridge = pr_omni_bridge_create(&config);
 * ```
 */
NIMCP_EXPORT pr_omni_bridge_config_t pr_omni_bridge_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_omni_bridge_config_validate(const pr_omni_bridge_config_t* config);

/**
 * @brief Get default modal weights
 *
 * @return Default weights (equal 1/3 each)
 */
NIMCP_EXPORT pr_omni_modal_weights_t pr_omni_modal_weights_default(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create PR Omni-Sensory Bridge
 *
 * WHAT: Creates bridge for multimodal memory integration
 * WHY:  Enables unified memories from visual, audio, and speech
 * HOW:  Allocates structures, initializes Kuramoto bank, prepares fusion
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on error
 *
 * Memory: ~512 bytes + Kuramoto system + entanglement graph
 */
NIMCP_EXPORT pr_omni_bridge_t* pr_omni_bridge_create(
    const pr_omni_bridge_config_t* config);

/**
 * @brief Destroy PR Omni Bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void pr_omni_bridge_destroy(pr_omni_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_reset(pr_omni_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect all required bridges
 *
 * WHAT: Links omni-sensory bridge and all modal PR bridges
 * WHY:  Bridge needs access to all modal representations
 * HOW:  Stores pointers, validates connections, sets up Kuramoto
 *
 * @param bridge PR Omni Bridge
 * @param omni Omni-sensory bridge (cross-modal binding source)
 * @param visual PR Visual bridge (optional, can be NULL)
 * @param audio PR Audio bridge (optional, can be NULL)
 * @param speech PR Speech bridge (optional, can be NULL)
 * @return PR_OMNI_SUCCESS or error code
 *
 * NOTE: At least two modal bridges should be connected for meaningful fusion
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_connect_bridges(
    pr_omni_bridge_t* bridge,
    omni_sensory_bridge_t* omni,
    pr_visual_bridge_t* visual,
    pr_audio_bridge_t* audio,
    pr_speech_bridge_t* speech);

/**
 * @brief Connect theta-gamma manager for phase gating
 *
 * @param bridge PR Omni Bridge
 * @param theta_gamma Theta-gamma manager
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_connect_theta_gamma(
    pr_omni_bridge_t* bridge,
    theta_gamma_manager_t theta_gamma);

/**
 * @brief Connect entanglement graph for multimodal associations
 *
 * @param bridge PR Omni Bridge
 * @param entanglement Entanglement graph
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_connect_entanglement(
    pr_omni_bridge_t* bridge,
    entangle_graph_t entanglement);

//=============================================================================
// Main Update Function
//=============================================================================

/**
 * @brief Update bridge - main integration cycle
 *
 * WHAT: Performs complete multimodal integration cycle
 * WHY:  Central function that fuses modalities into unified memory
 * HOW:
 *   1. Get binding strengths from omni-sensory bridge
 *   2. Update Kuramoto oscillators for cross-modal sync
 *   3. Compute unified prime signature (fusion)
 *   4. Compute unified quaternion (SLERP blend)
 *   5. Optionally create multimodal memory node
 *   6. Update entanglement graph
 *
 * @param bridge PR Omni Bridge
 * @return PR_OMNI_SUCCESS or error code
 *
 * Performance: ~200us per update cycle
 *
 * EXAMPLE:
 * ```c
 * // In main loop
 * pr_omni_bridge_update(bridge);
 *
 * // Check if multimodal memory was created
 * if (bridge->current_multimodal_memory) {
 *     // New unified memory available
 * }
 * ```
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_update(pr_omni_bridge_t* bridge);

//=============================================================================
// Prime Signature Functions
//=============================================================================

/**
 * @brief Compute unified prime signature from all modalities
 *
 * WHAT: Fuses modal signatures into unified cross-modal signature
 * WHY:  Enables retrieval by any modal cue
 * HOW:  Strategy-dependent fusion (union, intersection, weighted)
 *
 * Fusion strategies:
 * - UNION: All primes from all modalities (large signature)
 * - INTERSECTION: Only primes shared by bound modalities (small, specific)
 * - WEIGHTED: Union weighted by binding strength (balanced)
 * - DOMINANT: Only dominant modality's signature
 *
 * @param bridge PR Omni Bridge
 * @param unified_sig Output unified signature
 * @return PR_OMNI_SUCCESS or error code
 *
 * Performance: ~50us depending on signature sizes
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_compute_unified_prime_sig(
    pr_omni_bridge_t* bridge,
    prime_signature_t* unified_sig);

/**
 * @brief Fuse two modal signatures with binding weight
 *
 * @param sig1 First modal signature
 * @param sig2 Second modal signature
 * @param binding Binding strength between modalities [0, 1]
 * @param result Output fused signature
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_fuse_signatures(
    const prime_signature_t* sig1,
    const prime_signature_t* sig2,
    float binding,
    prime_signature_t* result);

//=============================================================================
// Quaternion Functions
//=============================================================================

/**
 * @brief Compute unified quaternion from modal quaternions
 *
 * WHAT: SLERP-blends modal quaternions into unified state
 * WHY:  Single semantic state representing multimodal experience
 * HOW:  Weighted SLERP with adaptive component handling
 *
 * Component handling:
 * - w (consolidation): max across modalities (strongest wins)
 * - x (emotion): weighted average
 * - y (salience): max across bound modalities
 * - z (accessibility): geometric mean
 *
 * @param bridge PR Omni Bridge
 * @param unified_quat Output unified quaternion
 * @return PR_OMNI_SUCCESS or error code
 *
 * Performance: ~25us
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_compute_unified_quaternion(
    pr_omni_bridge_t* bridge,
    nimcp_quaternion_t* unified_quat);

/**
 * @brief SLERP blend multiple quaternions with weights
 *
 * @param quaternions Array of quaternions to blend
 * @param weights Array of weights (must sum to 1)
 * @param count Number of quaternions
 * @param result Output blended quaternion
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_slerp_blend(
    const nimcp_quaternion_t* quaternions,
    const float* weights,
    size_t count,
    nimcp_quaternion_t* result);

//=============================================================================
// Memory Functions
//=============================================================================

/**
 * @brief Fuse bound modalities into multimodal memory
 *
 * WHAT: Creates unified memory node from bound modalities
 * WHY:  Stores multimodal experience as single retrievable memory
 * HOW:  Combines signatures, blends quaternions, creates entanglement
 *
 * @param bridge PR Omni Bridge
 * @param memory Output multimodal memory (caller-allocated)
 * @return PR_OMNI_SUCCESS or error code
 *
 * Performance: ~100us
 *
 * Requirements:
 * - At least two modalities must be bound
 * - Binding strength must exceed threshold
 * - Theta phase should be in encoding window (if gating enabled)
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_fuse_memories(
    pr_omni_bridge_t* bridge,
    pr_omni_multimodal_memory_t* memory);

/**
 * @brief Cross-modal resonance retrieval
 *
 * WHAT: Retrieve multimodal memories using any modal cue
 * WHY:  Enables "reminds me of" retrieval across modalities
 * HOW:  Computes resonance against multimodal signature, boosts cross-modal
 *
 * @param bridge PR Omni Bridge
 * @param query Retrieval query
 * @param results Output array of results (caller-allocated)
 * @param num_results Output: number of results returned
 * @return PR_OMNI_SUCCESS or error code
 *
 * Performance: ~150us for typical retrieval
 *
 * EXAMPLE:
 * ```c
 * pr_omni_retrieval_query_t query = {
 *     .query_signature = &visual_sig,
 *     .query_modality = PR_OMNI_MODALITY_VISUAL,
 *     .max_results = 10,
 *     .min_resonance = 0.5f,
 *     .prefer_multimodal = true,
 *     .cross_modal_boost = 0.2f
 * };
 * pr_omni_retrieval_result_t results[10];
 * size_t num_results;
 * pr_omni_bridge_retrieve_multimodal(bridge, &query, results, &num_results);
 * ```
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_retrieve_multimodal(
    pr_omni_bridge_t* bridge,
    const pr_omni_retrieval_query_t* query,
    pr_omni_retrieval_result_t* results,
    size_t* num_results);

//=============================================================================
// Binding Functions
//=============================================================================

/**
 * @brief Compute cross-modal binding strengths
 *
 * WHAT: Calculates pairwise binding between modalities
 * WHY:  Determines how tightly modalities should be fused
 * HOW:  Combines omni-bridge binding + Kuramoto coherence + prediction success
 *
 * @param bridge PR Omni Bridge
 * @param state Output binding state
 * @return PR_OMNI_SUCCESS or error code
 *
 * Performance: ~50us
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_compute_binding_strength(
    pr_omni_bridge_t* bridge,
    pr_omni_binding_state_t* state);

/**
 * @brief Update modal weights based on current state
 *
 * WHAT: Adaptively adjusts weights for quaternion blending
 * WHY:  More salient/bound modalities should have more influence
 * HOW:  Weights from binding strength, attention, prediction error
 *
 * @param bridge PR Omni Bridge
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_update_modal_weights(
    pr_omni_bridge_t* bridge);

/**
 * @brief Get binding strength between two modalities
 *
 * @param bridge PR Omni Bridge
 * @param modality1 First modality (0, 1, or 2)
 * @param modality2 Second modality (0, 1, or 2)
 * @return Binding strength [0, 1], or -1 on error
 */
NIMCP_EXPORT float pr_omni_bridge_get_binding(
    const pr_omni_bridge_t* bridge,
    int modality1,
    int modality2);

//=============================================================================
// Kuramoto Functions
//=============================================================================

/**
 * @brief Synchronize modal oscillators via Kuramoto coupling
 *
 * WHAT: Updates Kuramoto oscillator phases for cross-modal sync
 * WHY:  Synchronized modalities can communicate effectively
 * HOW:  Step Kuramoto system with binding-modulated coupling
 *
 * @param bridge PR Omni Bridge
 * @param dt_ns Time delta in nanoseconds
 * @return PR_OMNI_SUCCESS or error code
 *
 * Performance: ~30us
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_sync_oscillators(
    pr_omni_bridge_t* bridge,
    uint64_t dt_ns);

/**
 * @brief Get Kuramoto phase coherence between modalities
 *
 * @param bridge PR Omni Bridge
 * @param modality1 First modality
 * @param modality2 Second modality
 * @return Phase coherence [-1, 1], or 0 on error
 */
NIMCP_EXPORT float pr_omni_bridge_get_kuramoto_coherence(
    const pr_omni_bridge_t* bridge,
    int modality1,
    int modality2);

/**
 * @brief Set Kuramoto coupling strength between modalities
 *
 * @param bridge PR Omni Bridge
 * @param modality1 First modality
 * @param modality2 Second modality
 * @param coupling Coupling strength
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_set_kuramoto_coupling(
    pr_omni_bridge_t* bridge,
    int modality1,
    int modality2,
    float coupling);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get dominant modality based on current state
 *
 * WHAT: Determines which modality is most salient currently
 * WHY:  For retrieval and attention allocation
 * HOW:  Compares salience, attention weights, prediction errors
 *
 * @param bridge PR Omni Bridge
 * @return Dominant modality enum value
 */
NIMCP_EXPORT pr_omni_dominant_modality_t pr_omni_bridge_get_dominant_modality(
    const pr_omni_bridge_t* bridge);

/**
 * @brief Get current unified signature
 *
 * @param bridge PR Omni Bridge
 * @param sig Output signature (caller-allocated)
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_get_unified_signature(
    const pr_omni_bridge_t* bridge,
    prime_signature_t* sig);

/**
 * @brief Get current unified quaternion
 *
 * @param bridge PR Omni Bridge
 * @return Current unified quaternion
 */
NIMCP_EXPORT nimcp_quaternion_t pr_omni_bridge_get_unified_quaternion(
    const pr_omni_bridge_t* bridge);

/**
 * @brief Get current binding state
 *
 * @param bridge PR Omni Bridge
 * @param state Output binding state
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_get_binding_state(
    const pr_omni_bridge_t* bridge,
    pr_omni_binding_state_t* state);

/**
 * @brief Get modal weights
 *
 * @param bridge PR Omni Bridge
 * @param weights Output weights
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_get_modal_weights(
    const pr_omni_bridge_t* bridge,
    pr_omni_modal_weights_t* weights);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge PR Omni Bridge
 * @param stats Output statistics
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_get_stats(
    const pr_omni_bridge_t* bridge,
    pr_omni_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge PR Omni Bridge
 * @return PR_OMNI_SUCCESS or error code
 */
NIMCP_EXPORT pr_omni_error_t pr_omni_bridge_reset_stats(pr_omni_bridge_t* bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert error code to string
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_omni_bridge_error_string(pr_omni_error_t error);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* pr_omni_bridge_get_last_error(void);

/**
 * @brief Convert dominant modality to string
 *
 * @param dominant Dominant modality enum
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_omni_bridge_dominant_to_string(
    pr_omni_dominant_modality_t dominant);

/**
 * @brief Convert fusion strategy to string
 *
 * @param strategy Fusion strategy enum
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_omni_bridge_fusion_to_string(
    pr_omni_fusion_strategy_t strategy);

/**
 * @brief Print bridge state (debug)
 *
 * @param bridge PR Omni Bridge
 */
NIMCP_EXPORT void pr_omni_bridge_print_state(const pr_omni_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge PR Omni Bridge
 * @return true if all required bridges connected
 */
NIMCP_EXPORT bool pr_omni_bridge_is_connected(const pr_omni_bridge_t* bridge);

/**
 * @brief Get current time in nanoseconds
 *
 * @return Nanoseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_omni_bridge_current_time_ns(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_OMNI_BRIDGE_H */
