//=============================================================================
// nimcp_pr_attention_bridge.h - Prime Resonant Attention Bridge
//=============================================================================
/**
 * @file nimcp_pr_attention_bridge.h
 * @brief Attention integration bridge for Prime Resonant memory system
 *
 * WHAT: Bidirectional bridge between attention mechanisms and PR memory
 * WHY:  Attention modulates memory encoding/retrieval; memories guide attention
 * HOW:  Fuses bottom-up (feature) and top-down (memory) attention signals,
 *       updates quaternion salience, applies resonance-based attention boost
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Attention-Memory Integration:
 *   +-----------------------------------------------------------------------+
 *   |  BOTTOM-UP (Feature-Driven) ATTENTION:                                |
 *   |  - Visual saliency from visual cortex                                 |
 *   |  - Auditory saliency from audio cortex                                |
 *   |  - Cross-modal integration via omni-sensory bridge                    |
 *   |  - Novelty detection (features unlike stored memories)                |
 *   |                                                                       |
 *   |  TOP-DOWN (Memory-Guided) ATTENTION:                                  |
 *   |  - Attended memories boost attention to similar features              |
 *   |  - Task goals (stored as memories) direct attention                   |
 *   |  - Expectations (from memory) create attention templates              |
 *   |  - High resonance memories capture attention                          |
 *   |                                                                       |
 *   |  UNIFIED ATTENTION = alpha * bottom_up + (1-alpha) * top_down         |
 *   +-----------------------------------------------------------------------+
 *
 *   Attention -> Quaternion Mapping:
 *   +-----------------------------------------------------------------------+
 *   |  Attention directly modulates quaternion.y (salience component):      |
 *   |                                                                       |
 *   |  High attention -> high quat.y -> stronger memory encoding            |
 *   |  Low attention  -> low quat.y  -> weaker encoding, faster decay       |
 *   |                                                                       |
 *   |  This creates the "attentional gate" for memory:                      |
 *   |  - Attended items encoded strongly (flashbulb memory effect)          |
 *   |  - Unattended items encoded weakly (inattentional blindness)          |
 *   +-----------------------------------------------------------------------+
 *
 *   Resonance-Attention Coupling:
 *   +-----------------------------------------------------------------------+
 *   |  Memory resonance affects attention allocation:                       |
 *   |                                                                       |
 *   |  1. FAMILIARITY EFFECT:                                               |
 *   |     High resonance (similar to stored memories) -> attention boost    |
 *   |     "I recognize this!" captures attention                            |
 *   |                                                                       |
 *   |  2. NOVELTY EFFECT:                                                   |
 *   |     Low resonance (unlike anything stored) -> attention boost         |
 *   |     "What is this new thing?" also captures attention                 |
 *   |                                                                       |
 *   |  3. ATTENTION CURVE:                                                  |
 *   |     U-shaped: high for very familiar AND very novel                   |
 *   |     Low for intermediate familiarity (habituated)                     |
 *   +-----------------------------------------------------------------------+
 *
 *   Theta-Gamma Attention Rhythms:
 *   +-----------------------------------------------------------------------+
 *   |  Attention modulates neural oscillations:                             |
 *   |                                                                       |
 *   |  ALPHA SUPPRESSION (8-12 Hz):                                         |
 *   |  - Attention suppresses alpha in attended regions                     |
 *   |  - Release from inhibition allows processing                          |
 *   |                                                                       |
 *   |  GAMMA ENHANCEMENT (30-100 Hz):                                       |
 *   |  - Attention increases gamma power in attended regions                |
 *   |  - Gamma carries attention-selected information                       |
 *   |                                                                       |
 *   |  THETA PHASE:                                                         |
 *   |  - Attention peaks align with gamma bursts during theta               |
 *   |  - Phase-locked attention gating                                      |
 *   +-----------------------------------------------------------------------+
 *
 *   Inhibition of Return (IOR):
 *   +-----------------------------------------------------------------------+
 *   |  Spatial attention mechanism to prevent re-attending:                 |
 *   |                                                                       |
 *   |  1. After attending a location, that location is inhibited            |
 *   |  2. IOR map tracks recently attended locations/memories               |
 *   |  3. Inhibition decays over time (~300-500ms in biology)               |
 *   |  4. Encourages exploration of new locations/memories                  |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Bridge update: ~100us (full attention computation)
 * - Bottom-up computation: ~30us
 * - Top-down computation: ~50us (depends on num_attended_memories)
 * - Quaternion update: ~5us
 * - IOR update: ~10us
 *
 * MEMORY:
 * - pr_attention_bridge_t: ~512 bytes base + attention maps
 * - Attention maps: spatial_width * spatial_height * temporal_depth * 4 bytes
 * - Memory attention: num_attended * 8 bytes
 *
 * THREAD SAFETY:
 * - Bridge maintains internal mutex for state updates
 * - Perception bridge connections require external synchronization
 * - Quaternion updates are atomic where possible
 *
 * INTEGRATION:
 * - Visual/Audio/Speech cortex bridges for bottom-up attention
 * - PR Memory nodes for top-down attention
 * - Theta-gamma system for oscillatory gating
 * - Resonance engine for memory-attention coupling
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_ATTENTION_BRIDGE_H
#define NIMCP_PR_ATTENTION_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_resonance.h"
#include "cognitive/memory/core/nimcp_theta_gamma.h"
#include "constants/nimcp_math_constants.h"

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

/** Default spatial width for attention map */
#define PR_ATTN_DEFAULT_SPATIAL_WIDTH       64

/** Default spatial height for attention map */
#define PR_ATTN_DEFAULT_SPATIAL_HEIGHT      64

/** Default temporal depth for attention map */
#define PR_ATTN_DEFAULT_TEMPORAL_DEPTH      16

/** Maximum number of attended memories at once */
#define PR_ATTN_MAX_ATTENDED_MEMORIES       256

/** Default bottom-up/top-down balance (alpha in fusion) */
#define PR_ATTN_DEFAULT_BU_TD_BALANCE       0.6f

/** Default resonance attention gain */
#define PR_ATTN_DEFAULT_RESONANCE_GAIN      0.3f

/** Default novelty attention gain */
#define PR_ATTN_DEFAULT_NOVELTY_GAIN        0.4f

/** Default IOR decay time constant (ms) */
#define PR_ATTN_DEFAULT_IOR_DECAY_MS        400.0f

/** Default IOR inhibition strength */
#define PR_ATTN_DEFAULT_IOR_STRENGTH        0.5f

/** Default alpha suppression baseline */
#define PR_ATTN_DEFAULT_ALPHA_SUPPRESSION   0.3f

/** Default gamma enhancement baseline */
#define PR_ATTN_DEFAULT_GAMMA_ENHANCEMENT   0.5f

/** Epsilon for floating-point comparisons */
#define PR_ATTN_EPSILON                     1e-6f

/** Pi constant */

//=============================================================================
// Forward Declarations
//=============================================================================

/* Forward declaration for perception bridges (to avoid circular deps) */
typedef struct pr_visual_bridge_struct* pr_visual_bridge_t;
typedef struct pr_audio_bridge_struct* pr_audio_bridge_t;
typedef struct pr_speech_bridge_struct* pr_speech_bridge_t;
typedef struct pr_omni_bridge_struct* pr_omni_bridge_t;

/* Forward declaration for health agent (Phase 8) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Attention source type
 */
typedef enum {
    PR_ATTN_SOURCE_VISUAL = 0,       /**< Visual cortex salience */
    PR_ATTN_SOURCE_AUDIO,            /**< Audio cortex salience */
    PR_ATTN_SOURCE_SPEECH,           /**< Speech perception salience */
    PR_ATTN_SOURCE_OMNI,             /**< Omni-sensory integration */
    PR_ATTN_SOURCE_MEMORY,           /**< Top-down memory guidance */
    PR_ATTN_SOURCE_COUNT             /**< Number of source types */
} pr_attn_source_t;

/**
 * @brief Attention fusion mode
 */
typedef enum {
    PR_ATTN_FUSE_WEIGHTED = 0,       /**< Weighted average fusion */
    PR_ATTN_FUSE_MAX,                /**< Maximum pooling */
    PR_ATTN_FUSE_MULTIPLY,           /**< Multiplicative (AND-like) */
    PR_ATTN_FUSE_BIASED_COMPETITION  /**< Biased competition model */
} pr_attn_fusion_mode_t;

/**
 * @brief Attention-resonance coupling mode
 */
typedef enum {
    PR_ATTN_RESONANCE_LINEAR = 0,    /**< Linear boost with resonance */
    PR_ATTN_RESONANCE_U_SHAPED,      /**< U-shaped (familiar & novel boost) */
    PR_ATTN_RESONANCE_INVERTED_U,    /**< Inverted-U (intermediate peak) */
    PR_ATTN_RESONANCE_ADAPTIVE       /**< Context-adaptive coupling */
} pr_attn_resonance_mode_t;

/**
 * @brief Error codes for attention bridge operations
 */
typedef enum {
    PR_ATTN_SUCCESS = 0,                 /**< Operation succeeded */
    PR_ATTN_ERROR_NULL_POINTER = -1,     /**< NULL pointer argument */
    PR_ATTN_ERROR_INVALID_CONFIG = -2,   /**< Invalid configuration */
    PR_ATTN_ERROR_NO_MEMORY = -3,        /**< Memory allocation failed */
    PR_ATTN_ERROR_INVALID_STATE = -4,    /**< Invalid bridge state */
    PR_ATTN_ERROR_NOT_CONNECTED = -5,    /**< Required bridge not connected */
    PR_ATTN_ERROR_CAPACITY_EXCEEDED = -6,/**< Max attended memories exceeded */
    PR_ATTN_ERROR_INVALID_COORDS = -7,   /**< Invalid spatial coordinates */
    PR_ATTN_ERROR_UPDATE_FAILED = -8,    /**< Update operation failed */
    PR_ATTN_ERROR_THETA_GAMMA = -9       /**< Theta-gamma operation failed */
} pr_attn_error_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Attention bridge configuration
 *
 * WHAT: Parameters controlling attention bridge behavior
 * WHY:  Different applications need different attention characteristics
 * HOW:  Set at creation time, some modifiable afterward
 */
typedef struct {
    /* Spatial attention map dimensions */
    size_t spatial_width;            /**< Width of attention map */
    size_t spatial_height;           /**< Height of attention map */
    size_t temporal_depth;           /**< Temporal buffer depth */

    /* Bottom-up / top-down balance */
    float bu_td_balance;             /**< Alpha: 1.0=all BU, 0.0=all TD */
    pr_attn_fusion_mode_t fusion_mode; /**< How to fuse attention sources */

    /* Source weights (for weighted fusion) */
    float visual_weight;             /**< Weight for visual attention */
    float audio_weight;              /**< Weight for audio attention */
    float speech_weight;             /**< Weight for speech attention */
    float omni_weight;               /**< Weight for omni-sensory */
    float memory_weight;             /**< Weight for memory-guided */

    /* Resonance-attention coupling */
    pr_attn_resonance_mode_t resonance_mode; /**< Resonance coupling mode */
    float resonance_attention_gain;  /**< Resonance -> attention boost */
    float novelty_attention_gain;    /**< Novelty -> attention boost */
    float familiarity_threshold;     /**< Resonance threshold for "familiar" */
    float novelty_threshold;         /**< Resonance threshold for "novel" */

    /* Inhibition of Return */
    bool enable_ior;                 /**< Enable inhibition of return */
    float ior_decay_ms;              /**< IOR decay time constant (ms) */
    float ior_strength;              /**< IOR inhibition strength [0, 1] */
    float ior_radius;                /**< Spatial radius of IOR (pixels) */

    /* Oscillatory modulation */
    bool enable_theta_gamma;         /**< Enable theta-gamma coupling */
    float alpha_suppression_base;    /**< Base alpha suppression level */
    float gamma_enhancement_base;    /**< Base gamma enhancement level */

    /* Memory attention */
    size_t max_attended_memories;    /**< Max memories to track */
    float memory_attention_decay;    /**< Decay rate for memory attention */

    /* Integration flags */
    bool enable_quaternion_update;   /**< Update quat.y from attention */
    bool track_statistics;           /**< Enable statistics tracking */
} pr_attention_bridge_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Attention peak location
 */
typedef struct {
    size_t x;                        /**< Spatial X coordinate */
    size_t y;                        /**< Spatial Y coordinate */
    size_t t;                        /**< Temporal coordinate */
    float attention_value;           /**< Attention value at peak */
    pr_attn_source_t dominant_source;/**< Which source dominates here */
} pr_attn_peak_t;

/**
 * @brief Inhibition of Return state
 */
typedef struct {
    float* ior_map;                  /**< IOR inhibition map */
    size_t width;                    /**< Map width */
    size_t height;                   /**< Map height */
    uint64_t* last_attended_ms;      /**< When each location was attended */
    float decay_tau_ms;              /**< Decay time constant */
    float strength;                  /**< Inhibition strength */
} pr_attn_ior_state_t;

/**
 * @brief Memory attention entry
 */
typedef struct {
    pr_memory_node_t* node;          /**< Attended memory node */
    float attention_weight;          /**< Current attention weight [0, 1] */
    float resonance_score;           /**< Resonance with current percept */
    uint64_t last_attended_ms;       /**< When last attended */
    uint32_t attend_count;           /**< Times attended this session */
} pr_attn_memory_entry_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Operation counts */
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t bottom_up_computations; /**< Bottom-up attention computations */
    uint64_t top_down_computations;  /**< Top-down attention computations */
    uint64_t quaternion_updates;     /**< Quaternion salience updates */
    uint64_t ior_applications;       /**< IOR inhibition applications */

    /* Attention metrics */
    float mean_attention;            /**< Running mean attention level */
    float max_attention_seen;        /**< Maximum attention observed */
    float mean_resonance_boost;      /**< Mean resonance attention boost */
    float mean_novelty_boost;        /**< Mean novelty attention boost */

    /* Memory attention */
    uint64_t memories_attended;      /**< Total memories attended */
    float mean_memory_attention;     /**< Mean memory attention weight */

    /* Oscillatory metrics */
    float mean_alpha_suppression;    /**< Mean alpha suppression level */
    float mean_gamma_enhancement;    /**< Mean gamma enhancement level */

    /* Timing */
    double avg_update_time_us;       /**< Average update time (us) */
    uint64_t last_reset_time_ms;     /**< When stats were last reset */
} pr_attn_bridge_stats_t;

//=============================================================================
// Main Bridge Structure
//=============================================================================

/**
 * @brief Prime Resonant Attention Bridge
 *
 * WHAT: Central attention integration point for PR memory system
 * WHY:  Coordinates bottom-up and top-down attention for memory
 * HOW:  Maintains attention maps, manages memory attention, updates quaternions
 *
 * Memory layout: ~512 bytes base + variable attention maps
 */
typedef struct {
    /*-------------------------------------------------------------------------
     * Connected Perception Bridges
     *-----------------------------------------------------------------------*/
    pr_visual_bridge_t visual_bridge;   /**< Visual cortex bridge */
    pr_audio_bridge_t audio_bridge;     /**< Audio cortex bridge */
    pr_speech_bridge_t speech_bridge;   /**< Speech cortex bridge */
    pr_omni_bridge_t omni_bridge;       /**< Omni-sensory bridge */

    /*-------------------------------------------------------------------------
     * Attention State
     *-----------------------------------------------------------------------*/
    float* unified_attention_map;       /**< Unified attention [W*H*T] */
    float* bottom_up_map;               /**< Bottom-up attention [W*H*T] */
    float* top_down_map;                /**< Top-down attention [W*H*T] */
    size_t spatial_width;               /**< Attention map width */
    size_t spatial_height;              /**< Attention map height */
    size_t temporal_depth;              /**< Temporal buffer depth */

    /*-------------------------------------------------------------------------
     * Memory Attention
     *-----------------------------------------------------------------------*/
    pr_attn_memory_entry_t* attended_memories; /**< Currently attended memories */
    size_t num_attended;                /**< Number of attended memories */
    size_t max_attended;                /**< Maximum capacity */

    /*-------------------------------------------------------------------------
     * Resonance-Guided Attention
     *-----------------------------------------------------------------------*/
    float resonance_attention_gain;     /**< Resonance -> attention gain */
    resonance_config_t resonance_config;/**< Resonance computation config */

    /*-------------------------------------------------------------------------
     * Oscillatory Modulation
     *-----------------------------------------------------------------------*/
    theta_gamma_manager_t theta_gamma;  /**< Theta-gamma coupling manager */
    float alpha_suppression;            /**< Current alpha suppression [0, 1] */
    float gamma_enhancement;            /**< Current gamma enhancement */

    /*-------------------------------------------------------------------------
     * Inhibition of Return
     *-----------------------------------------------------------------------*/
    pr_attn_ior_state_t ior_state;      /**< IOR state */

    /*-------------------------------------------------------------------------
     * Configuration and Statistics
     *-----------------------------------------------------------------------*/
    pr_attention_bridge_config_t config;/**< Current configuration */
    pr_attn_bridge_stats_t stats;       /**< Operational statistics */

    /*-------------------------------------------------------------------------
     * Internal State
     *-----------------------------------------------------------------------*/
    uint64_t last_update_ms;            /**< Last update timestamp */
    bool initialized;                   /**< Bridge initialized flag */

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
} pr_attention_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default attention bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets biologically realistic parameters
 *
 * @return Default configuration with:
 *         - 64x64 spatial attention map
 *         - 16 temporal frames
 *         - 0.6 bottom-up / top-down balance
 *         - U-shaped resonance coupling
 *         - IOR enabled with 400ms decay
 *         - Theta-gamma coupling enabled
 *
 * Performance: ~5ns
 *
 * Example:
 *   pr_attention_bridge_config_t config = pr_attention_bridge_config_default();
 *   config.spatial_width = 128;
 *   pr_attention_bridge_t* bridge = pr_attention_bridge_create(&config);
 */
NIMCP_EXPORT pr_attention_bridge_config_t pr_attention_bridge_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - Spatial dimensions must be > 0
 * - Temporal depth must be > 0
 * - Weights must be >= 0
 * - Thresholds must be in [0, 1]
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool pr_attention_bridge_config_validate(
    const pr_attention_bridge_config_t* config
);

//=============================================================================
// Bridge Lifecycle Functions
//=============================================================================

/**
 * @brief Create attention bridge
 *
 * WHAT: Creates and initializes attention bridge
 * WHY:  Central entry point for attention-memory integration
 * HOW:  Allocates bridge, initializes maps, sets up IOR and theta-gamma
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge or NULL on error
 *
 * Performance: ~100us (allocation + initialization)
 * Memory: ~512 bytes + spatial_width * spatial_height * temporal_depth * 12 bytes
 *
 * Example:
 *   pr_attention_bridge_config_t config = pr_attention_bridge_config_default();
 *   pr_attention_bridge_t* bridge = pr_attention_bridge_create(&config);
 *   if (!bridge) {
 *       fprintf(stderr, "Failed: %s\n", pr_attn_get_last_error());
 *   }
 */
NIMCP_EXPORT pr_attention_bridge_t* pr_attention_bridge_create(
    const pr_attention_bridge_config_t* config
);

/**
 * @brief Destroy attention bridge
 *
 * WHAT: Frees all bridge resources
 * WHY:  Clean shutdown and resource release
 * HOW:  Frees maps, IOR state, memory entries
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: ~10us
 */
NIMCP_EXPORT void pr_attention_bridge_destroy(pr_attention_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clears attention maps, IOR, memory attention
 * WHY:  Start fresh attention state
 * HOW:  Zeros all maps, resets statistics
 *
 * @param bridge Attention bridge
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: ~50us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_reset(
    pr_attention_bridge_t* bridge
);

//=============================================================================
// Bridge Connection Functions
//=============================================================================

/**
 * @brief Connect perception bridges
 *
 * WHAT: Links perception bridges for bottom-up attention
 * WHY:  Enable multi-modal attention integration
 * HOW:  Stores bridge pointers for attention queries
 *
 * @param bridge Attention bridge
 * @param visual Visual cortex bridge (can be NULL)
 * @param audio Audio cortex bridge (can be NULL)
 * @param speech Speech cortex bridge (can be NULL)
 * @param omni Omni-sensory bridge (can be NULL)
 * @return PR_ATTN_SUCCESS or error code
 *
 * At least one perception bridge should be connected for meaningful
 * bottom-up attention computation.
 *
 * Performance: ~100ns
 *
 * Example:
 *   pr_attention_bridge_connect_bridges(attn_bridge,
 *       visual_bridge, audio_bridge, NULL, omni_bridge);
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_connect_bridges(
    pr_attention_bridge_t* bridge,
    pr_visual_bridge_t visual,
    pr_audio_bridge_t audio,
    pr_speech_bridge_t speech,
    pr_omni_bridge_t omni
);

/**
 * @brief Connect theta-gamma manager
 *
 * WHAT: Links external theta-gamma manager for oscillatory modulation
 * WHY:  Enable phase-locked attention gating
 * HOW:  Stores manager pointer for phase queries
 *
 * @param bridge Attention bridge
 * @param theta_gamma Theta-gamma manager (can be NULL to use internal)
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_connect_theta_gamma(
    pr_attention_bridge_t* bridge,
    theta_gamma_manager_t theta_gamma
);

//=============================================================================
// Main Update Functions
//=============================================================================

/**
 * @brief Main attention update
 *
 * WHAT: Computes unified attention from all sources
 * WHY:  Primary attention computation entry point
 * HOW:  Computes BU + TD, fuses, applies IOR, updates oscillators
 *
 * @param bridge Attention bridge
 * @param dt_ms Time delta since last update (ms)
 * @return PR_ATTN_SUCCESS or error code
 *
 * This function:
 * 1. Queries connected perception bridges for bottom-up salience
 * 2. Computes top-down attention from attended memories
 * 3. Fuses bottom-up and top-down using configured method
 * 4. Applies inhibition of return
 * 5. Updates alpha suppression and gamma enhancement
 * 6. Updates attended memory weights
 *
 * Performance: ~100us
 *
 * Example:
 *   // Update at 100Hz
 *   pr_attention_bridge_update(bridge, 10.0f);
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_update(
    pr_attention_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Compute bottom-up attention
 *
 * WHAT: Computes feature-driven attention from perception
 * WHY:  Salient features automatically attract attention
 * HOW:  Queries perception bridges, combines salience maps
 *
 * @param bridge Attention bridge
 * @return PR_ATTN_SUCCESS or error code
 *
 * Bottom-up attention sources:
 * - Visual: Edge density, color contrast, motion
 * - Audio: Loudness, spectral novelty, onsets
 * - Speech: Phoneme salience, prosodic emphasis
 * - Omni: Cross-modal integration
 *
 * Performance: ~30us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_compute_bottom_up(
    pr_attention_bridge_t* bridge
);

/**
 * @brief Compute top-down attention
 *
 * WHAT: Computes memory-guided attention
 * WHY:  Task goals and expectations direct attention
 * HOW:  Uses attended memories to create attention templates
 *
 * @param bridge Attention bridge
 * @return PR_ATTN_SUCCESS or error code
 *
 * Top-down attention mechanism:
 * 1. For each attended memory with high weight:
 *    - Generate feature template from memory content
 *    - Boost attention to matching features
 * 2. Weight templates by memory attention weight
 * 3. Combine weighted templates
 *
 * Performance: ~50us (depends on num_attended)
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_compute_top_down(
    pr_attention_bridge_t* bridge
);

/**
 * @brief Fuse bottom-up and top-down attention
 *
 * WHAT: Combines BU and TD attention into unified map
 * WHY:  Final attention is combination of both sources
 * HOW:  Uses configured fusion mode (weighted, max, multiply, etc.)
 *
 * @param bridge Attention bridge
 * @return PR_ATTN_SUCCESS or error code
 *
 * Fusion modes:
 * - WEIGHTED: unified = alpha * BU + (1-alpha) * TD
 * - MAX: unified = max(BU, TD)
 * - MULTIPLY: unified = BU * TD (AND-like, both must be high)
 * - BIASED_COMPETITION: TD biases BU competition
 *
 * Performance: ~10us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_fuse_attention(
    pr_attention_bridge_t* bridge
);

//=============================================================================
// Resonance-Attention Functions
//=============================================================================

/**
 * @brief Apply resonance-based attention boost
 *
 * WHAT: Boosts attention based on memory resonance
 * WHY:  Familiar and novel items capture attention
 * HOW:  Computes resonance with memories, applies boost
 *
 * @param bridge Attention bridge
 * @param query Query for resonance computation
 * @param boost_output Output attention boost value [0, 1]
 * @return PR_ATTN_SUCCESS or error code
 *
 * Resonance modes:
 * - LINEAR: boost = resonance * gain
 * - U_SHAPED: boost = f(|resonance - 0.5|) (high for familiar AND novel)
 * - INVERTED_U: boost peaks at intermediate resonance
 * - ADAPTIVE: context-dependent mode selection
 *
 * Performance: ~20us
 *
 * Example:
 *   resonance_query_t query = {.signature = &current_sig, ...};
 *   float boost;
 *   pr_attention_bridge_apply_resonance_boost(bridge, &query, &boost);
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_apply_resonance_boost(
    pr_attention_bridge_t* bridge,
    const resonance_query_t* query,
    float* boost_output
);

/**
 * @brief Compute familiarity from resonance
 *
 * WHAT: Converts resonance score to familiarity measure
 * WHY:  Familiarity affects attention allocation
 * HOW:  Normalized resonance across attended memories
 *
 * @param bridge Attention bridge
 * @param query Query for resonance computation
 * @return Familiarity value [0, 1] or -1.0f on error
 *
 * Performance: ~15us
 */
NIMCP_EXPORT float pr_attention_bridge_compute_familiarity(
    pr_attention_bridge_t* bridge,
    const resonance_query_t* query
);

/**
 * @brief Compute novelty from resonance
 *
 * WHAT: Computes novelty as inverse of max resonance
 * WHY:  Novel stimuli capture attention
 * HOW:  novelty = 1 - max_resonance_to_any_memory
 *
 * @param bridge Attention bridge
 * @param query Query for resonance computation
 * @return Novelty value [0, 1] or -1.0f on error
 *
 * Performance: ~15us
 */
NIMCP_EXPORT float pr_attention_bridge_compute_novelty(
    pr_attention_bridge_t* bridge,
    const resonance_query_t* query
);

//=============================================================================
// Quaternion Update Functions
//=============================================================================

/**
 * @brief Update quaternion salience from attention
 *
 * WHAT: Sets quaternion.y (salience) based on attention level
 * WHY:  Attention gates memory encoding strength
 * HOW:  Maps attention value to quaternion y component
 *
 * @param bridge Attention bridge
 * @param node Memory node to update
 * @param spatial_x Spatial X coordinate (for spatial attention)
 * @param spatial_y Spatial Y coordinate (for spatial attention)
 * @return PR_ATTN_SUCCESS or error code
 *
 * The salience update:
 * - High attention -> high quat.y -> strong encoding
 * - Low attention -> low quat.y -> weak encoding
 *
 * Performance: ~5us
 *
 * Example:
 *   // Update node salience based on where it was perceived
 *   pr_attention_bridge_update_quaternion_salience(bridge, node, x, y);
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_update_quaternion_salience(
    pr_attention_bridge_t* bridge,
    pr_memory_node_t* node,
    size_t spatial_x,
    size_t spatial_y
);

/**
 * @brief Modulate memory encoding by attention
 *
 * WHAT: Gates memory encoding strength by attention level
 * WHY:  Attended items encoded more strongly
 * HOW:  Scales encoding parameters by attention
 *
 * @param bridge Attention bridge
 * @param node Memory node being encoded
 * @param base_strength Base encoding strength [0, 1]
 * @param modulated_strength Output modulated strength
 * @return PR_ATTN_SUCCESS or error code
 *
 * Formula: modulated = base * (attention^gamma)
 * where gamma controls attention influence
 *
 * Performance: ~5us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_modulate_encoding(
    pr_attention_bridge_t* bridge,
    const pr_memory_node_t* node,
    float base_strength,
    float* modulated_strength
);

//=============================================================================
// Attention Query Functions
//=============================================================================

/**
 * @brief Get attention peak location
 *
 * WHAT: Finds spatial location of maximum attention
 * WHY:  Identify current focus of attention
 * HOW:  Argmax over unified attention map
 *
 * @param bridge Attention bridge
 * @param peak Output peak information
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: ~5us
 *
 * Example:
 *   pr_attn_peak_t peak;
 *   pr_attention_bridge_get_attention_peak(bridge, &peak);
 *   printf("Attention peak at (%zu, %zu) = %.3f\n",
 *          peak.x, peak.y, peak.attention_value);
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_get_attention_peak(
    const pr_attention_bridge_t* bridge,
    pr_attn_peak_t* peak
);

/**
 * @brief Get attention value at location
 *
 * WHAT: Queries attention at specific coordinates
 * WHY:  Check attention level at point of interest
 * HOW:  Direct map lookup
 *
 * @param bridge Attention bridge
 * @param x Spatial X coordinate
 * @param y Spatial Y coordinate
 * @param t Temporal coordinate (0 for current)
 * @return Attention value [0, 1] or -1.0f on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT float pr_attention_bridge_get_attention_at(
    const pr_attention_bridge_t* bridge,
    size_t x,
    size_t y,
    size_t t
);

/**
 * @brief Get top-K attention locations
 *
 * WHAT: Finds K locations with highest attention
 * WHY:  Multi-focus attention (multiple attended locations)
 * HOW:  Partial sort for top-K
 *
 * @param bridge Attention bridge
 * @param k Number of peaks to find
 * @param peaks Output array of peaks (size >= k)
 * @return Number of peaks found (<= k) or -1 on error
 *
 * Performance: O(W*H + k*log(k))
 */
NIMCP_EXPORT int pr_attention_bridge_get_top_k_peaks(
    const pr_attention_bridge_t* bridge,
    size_t k,
    pr_attn_peak_t* peaks
);

/**
 * @brief Get mean attention level
 *
 * WHAT: Computes mean attention across map
 * WHY:  Monitor overall attention/arousal level
 * HOW:  Sum / (W*H*T)
 *
 * @param bridge Attention bridge
 * @return Mean attention [0, 1] or -1.0f on error
 *
 * Performance: ~10us
 */
NIMCP_EXPORT float pr_attention_bridge_get_mean_attention(
    const pr_attention_bridge_t* bridge
);

//=============================================================================
// Inhibition of Return Functions
//=============================================================================

/**
 * @brief Apply inhibition of return
 *
 * WHAT: Inhibits recently attended locations
 * WHY:  Prevents re-attending same locations
 * HOW:  Applies IOR map to attention map
 *
 * @param bridge Attention bridge
 * @return PR_ATTN_SUCCESS or error code
 *
 * IOR mechanism:
 * 1. Track when each location was last attended
 * 2. Apply decaying inhibition to recently attended
 * 3. Inhibition = strength * exp(-t / tau)
 *
 * Performance: ~10us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_apply_ior(
    pr_attention_bridge_t* bridge
);

/**
 * @brief Update IOR map for attended location
 *
 * WHAT: Marks location as attended for IOR
 * WHY:  Will be inhibited on subsequent frames
 * HOW:  Sets IOR map value and timestamp
 *
 * @param bridge Attention bridge
 * @param x Spatial X coordinate
 * @param y Spatial Y coordinate
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: ~1us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_mark_attended(
    pr_attention_bridge_t* bridge,
    size_t x,
    size_t y
);

/**
 * @brief Decay IOR map
 *
 * WHAT: Applies temporal decay to IOR inhibition
 * WHY:  IOR wears off over time
 * HOW:  Multiplies by exp(-dt/tau) for each location
 *
 * @param bridge Attention bridge
 * @param dt_ms Time delta (ms)
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: ~5us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_decay_ior(
    pr_attention_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Clear IOR map
 *
 * WHAT: Resets all IOR inhibition
 * WHY:  Allow re-attending all locations
 * HOW:  Zeros IOR map
 *
 * @param bridge Attention bridge
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: ~1us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_clear_ior(
    pr_attention_bridge_t* bridge
);

//=============================================================================
// Oscillatory Modulation Functions
//=============================================================================

/**
 * @brief Compute alpha suppression from attention
 *
 * WHAT: Computes alpha band suppression level
 * WHY:  Attention suppresses alpha in attended regions
 * HOW:  Inverse of mean attention in focused region
 *
 * @param bridge Attention bridge
 * @return Alpha suppression [0, 1] or -1.0f on error
 *
 * Alpha suppression interpretation:
 * - 0 = no suppression (alpha intact, low attention)
 * - 1 = full suppression (alpha suppressed, high attention)
 *
 * Performance: ~5us
 */
NIMCP_EXPORT float pr_attention_bridge_compute_alpha_suppression(
    pr_attention_bridge_t* bridge
);

/**
 * @brief Get current gamma enhancement
 *
 * WHAT: Returns attention-related gamma enhancement
 * WHY:  Attention increases gamma in attended regions
 * HOW:  Proportional to attention level
 *
 * @param bridge Attention bridge
 * @return Gamma enhancement value or -1.0f on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT float pr_attention_bridge_get_gamma_enhancement(
    const pr_attention_bridge_t* bridge
);

/**
 * @brief Modulate attention by theta phase
 *
 * WHAT: Adjusts attention based on theta oscillation phase
 * WHY:  Attention peaks at specific theta phases
 * HOW:  Queries theta-gamma manager, scales attention
 *
 * @param bridge Attention bridge
 * @return PR_ATTN_SUCCESS or error code
 *
 * Theta-phase modulation:
 * - Encoding phase (trough): High attention for encoding
 * - Retrieval phase (peak): Lower encoding attention
 *
 * Performance: ~5us
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_modulate_by_theta(
    pr_attention_bridge_t* bridge
);

//=============================================================================
// Memory Attention Functions
//=============================================================================

/**
 * @brief Add memory to attended set
 *
 * WHAT: Adds memory node to tracked attended memories
 * WHY:  Memory guides top-down attention
 * HOW:  Stores reference with initial attention weight
 *
 * @param bridge Attention bridge
 * @param node Memory node to attend
 * @param initial_weight Initial attention weight [0, 1]
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: ~1us
 *
 * Example:
 *   // Attend to retrieved memory
 *   pr_attention_bridge_attend_memory(bridge, retrieved_node, 0.8f);
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_attend_memory(
    pr_attention_bridge_t* bridge,
    pr_memory_node_t* node,
    float initial_weight
);

/**
 * @brief Remove memory from attended set
 *
 * WHAT: Removes memory from tracked attended memories
 * WHY:  Memory no longer guides attention
 * HOW:  Removes entry from attended list
 *
 * @param bridge Attention bridge
 * @param node Memory node to remove
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: O(num_attended)
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_unattend_memory(
    pr_attention_bridge_t* bridge,
    pr_memory_node_t* node
);

/**
 * @brief Update memory attention weight
 *
 * WHAT: Modifies attention weight for attended memory
 * WHY:  Attention to memories changes over time
 * HOW:  Direct weight update
 *
 * @param bridge Attention bridge
 * @param node Memory node
 * @param new_weight New attention weight [0, 1]
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: O(num_attended)
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_update_memory_attention(
    pr_attention_bridge_t* bridge,
    pr_memory_node_t* node,
    float new_weight
);

/**
 * @brief Get memory attention weight
 *
 * WHAT: Returns current attention weight for memory
 * WHY:  Query how much attention a memory receives
 * HOW:  Lookup in attended list
 *
 * @param bridge Attention bridge
 * @param node Memory node
 * @return Attention weight [0, 1] or -1.0f if not attended
 *
 * Performance: O(num_attended)
 */
NIMCP_EXPORT float pr_attention_bridge_get_memory_attention(
    const pr_attention_bridge_t* bridge,
    const pr_memory_node_t* node
);

/**
 * @brief Decay all memory attention weights
 *
 * WHAT: Applies temporal decay to memory attention
 * WHY:  Attention to memories fades over time
 * HOW:  Multiplies all weights by decay factor
 *
 * @param bridge Attention bridge
 * @param decay_factor Decay multiplier [0, 1]
 * @return PR_ATTN_SUCCESS or error code
 *
 * Performance: O(num_attended)
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_decay_memory_attention(
    pr_attention_bridge_t* bridge,
    float decay_factor
);

/**
 * @brief Get most attended memory
 *
 * WHAT: Returns memory with highest attention weight
 * WHY:  Identify current focus of memory attention
 * HOW:  Argmax over attended memories
 *
 * @param bridge Attention bridge
 * @return Most attended memory or NULL if none
 *
 * Performance: O(num_attended)
 */
NIMCP_EXPORT pr_memory_node_t* pr_attention_bridge_get_most_attended_memory(
    const pr_attention_bridge_t* bridge
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Attention bridge
 * @param stats Output statistics structure
 * @return PR_ATTN_SUCCESS or error code
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_get_stats(
    const pr_attention_bridge_t* bridge,
    pr_attn_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Attention bridge
 * @return PR_ATTN_SUCCESS or error code
 */
NIMCP_EXPORT pr_attn_error_t pr_attention_bridge_reset_stats(
    pr_attention_bridge_t* bridge
);

/**
 * @brief Print bridge state (debug)
 *
 * @param bridge Attention bridge
 */
NIMCP_EXPORT void pr_attention_bridge_print_state(
    const pr_attention_bridge_t* bridge
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_attn_error_string(pr_attn_error_t error);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* pr_attn_get_last_error(void);

/**
 * @brief Get source name as string
 *
 * @param source Attention source type
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_attn_source_name(pr_attn_source_t source);

/**
 * @brief Get fusion mode name as string
 *
 * @param mode Fusion mode
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_attn_fusion_mode_name(pr_attn_fusion_mode_t mode);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_attn_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_ATTENTION_BRIDGE_H
