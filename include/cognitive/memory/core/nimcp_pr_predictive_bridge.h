//=============================================================================
// nimcp_pr_predictive_bridge.h - FEP/Predictive Processing Bridge for PR Memory
//=============================================================================
/**
 * @file nimcp_pr_predictive_bridge.h
 * @brief Integration bridge between Free Energy Principle and Prime Resonant Memory
 *
 * WHAT: Bidirectional bridge connecting FEP/predictive processing with PR memory
 * WHY:  Memory shapes predictions; prediction errors modulate memory formation
 * HOW:  Precision-weighted PE from sensory FEP bridges drives memory updates,
 *       high-resonance memories generate sensory predictions
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Predictive Processing ↔ Memory Architecture:
 *   +-----------------------------------------------------------------------+
 *   |  The brain minimizes prediction error by:                             |
 *   |  1. Updating predictions (perceptual inference)                       |
 *   |  2. Updating world model (learning/memory)                            |
 *   |  3. Acting to change sensory input (active inference)                 |
 *   |                                                                       |
 *   |  Memory's role in predictive processing:                              |
 *   |  - Past experiences generate current predictions                       |
 *   |  - Prediction errors drive memory consolidation/reconsolidation       |
 *   |  - Precision determines whether PE updates memory or is ignored       |
 *   +-----------------------------------------------------------------------+
 *
 *   PE → Memory Modulation:
 *   +-----------------------------------------------------------------------+
 *   |  Prediction Error Level | Memory Response                             |
 *   |-------------------------|---------------------------------------------|
 *   |  Low PE + High Resonance| Consolidation - strengthen existing pattern |
 *   |  High PE + High Resonance| Reconsolidation - update existing memory   |
 *   |  High PE + Low Resonance| New encoding - create new memory            |
 *   |  Low PE + Low Resonance | No action - not surprising, not relevant    |
 *   +-----------------------------------------------------------------------+
 *
 *   Resonance → Prediction Coupling:
 *   +-----------------------------------------------------------------------+
 *   |  High resonance memories generate confident predictions               |
 *   |  - Resonance score → prediction confidence                            |
 *   |  - Quaternion.w (consolidation) → prediction precision                |
 *   |  - Similar memories produce similar predictions                        |
 *   |                                                                       |
 *   |  Expected PE = f(1/resonance)                                         |
 *   |  - High resonance → expect low PE (prediction should match)           |
 *   |  - Low resonance → expect high PE (uncertain prediction)              |
 *   +-----------------------------------------------------------------------+
 *
 *   Active Inference Actions:
 *   +-----------------------------------------------------------------------+
 *   |  Action Type    | Purpose                       | PE Reduction        |
 *   |-----------------|-------------------------------|---------------------|
 *   |  SACCADE        | Move eyes to reduce visual PE | Sample high-PE area |
 *   |  ORIENT_AUDIO   | Turn head toward sound        | Localize audio PE   |
 *   |  ARTICULATE     | Produce speech to test        | Verify speech model |
 *   |  QUERY_MEMORY   | Retrieve related memories     | Pattern completion  |
 *   |  ATTEND         | Focus attention on modality   | Increase precision  |
 *   +-----------------------------------------------------------------------+
 *
 *   Precision Weighting:
 *   +-----------------------------------------------------------------------+
 *   |  Each sensory modality has precision (inverse variance):              |
 *   |  - High precision = reliable signal, strong PE influence              |
 *   |  - Low precision = noisy signal, weak PE influence                    |
 *   |                                                                       |
 *   |  Combined PE = Σ(precision_i * PE_i) / Σ(precision_i)                 |
 *   |                                                                       |
 *   |  Precision updated based on PE history:                               |
 *   |  - Consistent PE → increase precision                                 |
 *   |  - Variable PE → decrease precision                                   |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - PE collection: ~50us (from all FEP bridges)
 * - Combined PE: ~10us (precision-weighted sum)
 * - Prediction generation: ~200us (depends on memory count)
 * - Memory update: ~100us per memory
 * - Action selection: ~50us
 *
 * MEMORY:
 * - pr_predictive_bridge_t: ~1KB base
 * - Free energy history: 64 * 4 = 256 bytes
 * - Pending actions: variable (max 16 actions)
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via bridge mutex
 * - FEP bridge connections require external synchronization
 *
 * INTEGRATION:
 * - Perception: Visual, Audio, Speech FEP bridges
 * - Memory: PR Memory nodes, resonance scoring
 * - Cognitive: Free energy computation, active inference
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_PREDICTIVE_BRIDGE_H
#define NIMCP_PR_PREDICTIVE_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Core dependencies
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_resonance.h"
#include "cognitive/memory/core/nimcp_quaternion.h"

// FEP bridge dependencies
#include "perception/nimcp_visual_cortex_fep_bridge.h"
#include "perception/nimcp_audio_cortex_fep_bridge.h"
#include "perception/nimcp_speech_cortex_fep_bridge.h"

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

/** Maximum number of prediction sources (high-resonance memories) */
#define PR_PRED_MAX_PREDICTION_SOURCES     64

/** Maximum number of pending active inference actions */
#define PR_PRED_MAX_PENDING_ACTIONS        16

/** Free energy history buffer size */
#define PR_PRED_FREE_ENERGY_HISTORY_SIZE   64

/** Default PE threshold for memory reconsolidation */
#define PR_PRED_DEFAULT_PE_THRESHOLD_UPDATE    2.0f

/** Default PE threshold for new memory creation */
#define PR_PRED_DEFAULT_PE_THRESHOLD_NEW       5.0f

/** Default resonance-prediction weight */
#define PR_PRED_DEFAULT_RESONANCE_WEIGHT       0.7f

/** Default precision for visual modality */
#define PR_PRED_DEFAULT_VISUAL_PRECISION       1.0f

/** Default precision for audio modality */
#define PR_PRED_DEFAULT_AUDIO_PRECISION        1.0f

/** Default precision for speech modality */
#define PR_PRED_DEFAULT_SPEECH_PRECISION       1.0f

/** Minimum precision value */
#define PR_PRED_MIN_PRECISION                  0.1f

/** Maximum precision value */
#define PR_PRED_MAX_PRECISION                  10.0f

/** Precision adaptation rate (per update) */
#define PR_PRED_PRECISION_ADAPTATION_RATE      0.05f

/** Consolidation strength boost on low PE */
#define PR_PRED_CONSOLIDATION_BOOST            0.1f

/** Reconsolidation window duration (ms) */
#define PR_PRED_RECONSOLIDATION_WINDOW_MS      5000

/** Action selection temperature for softmax */
#define PR_PRED_ACTION_TEMPERATURE             1.0f

/** Epsilon for floating-point comparisons */
#define PR_PRED_EPSILON                        1e-6f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Active inference action types
 *
 * Actions that can be taken to reduce prediction error through
 * active sampling of the environment or internal processing.
 */
typedef enum {
    PR_ACTION_NONE = 0,           /**< No action */
    PR_ACTION_SACCADE,            /**< Eye movement to high-PE region */
    PR_ACTION_ORIENT_AUDIO,       /**< Orient toward sound source */
    PR_ACTION_ARTICULATE,         /**< Produce speech to test model */
    PR_ACTION_QUERY_MEMORY,       /**< Retrieve related memories */
    PR_ACTION_ATTEND_VISUAL,      /**< Increase visual attention/precision */
    PR_ACTION_ATTEND_AUDIO,       /**< Increase audio attention/precision */
    PR_ACTION_ATTEND_SPEECH,      /**< Increase speech attention/precision */
    PR_ACTION_SUPPRESS,           /**< Suppress prediction (accept PE) */
    PR_ACTION_COUNT               /**< Number of action types */
} pr_action_type_t;

/**
 * @brief Memory update modes triggered by PE
 */
typedef enum {
    PR_MEM_UPDATE_NONE = 0,       /**< No update needed */
    PR_MEM_UPDATE_CONSOLIDATE,    /**< Strengthen existing memory (low PE) */
    PR_MEM_UPDATE_RECONSOLIDATE,  /**< Update existing memory (high PE + high res) */
    PR_MEM_UPDATE_ENCODE_NEW      /**< Create new memory (high PE + low res) */
} pr_mem_update_mode_t;

/**
 * @brief Error codes for predictive bridge operations
 */
typedef enum {
    PR_PRED_SUCCESS = 0,                   /**< Operation succeeded */
    PR_PRED_ERROR_NULL_POINTER = -1,       /**< NULL pointer argument */
    PR_PRED_ERROR_INVALID_CONFIG = -2,     /**< Invalid configuration */
    PR_PRED_ERROR_NO_MEMORY = -3,          /**< Memory allocation failed */
    PR_PRED_ERROR_NOT_CONNECTED = -4,      /**< Required bridge not connected */
    PR_PRED_ERROR_FEP_FAILED = -5,         /**< FEP operation failed */
    PR_PRED_ERROR_PR_FAILED = -6,          /**< PR memory operation failed */
    PR_PRED_ERROR_COMPUTE_FAILED = -7,     /**< Computation failed */
    PR_PRED_ERROR_BUFFER_FULL = -8,        /**< Action buffer full */
    PR_PRED_ERROR_INVALID_ACTION = -9,     /**< Invalid action type */
    PR_PRED_ERROR_RECONSOLIDATION = -10    /**< Reconsolidation failed */
} pr_pred_error_t;

//=============================================================================
// Forward Declarations
//=============================================================================

/* Forward declarations for PR perception bridges (may not exist yet) */
typedef struct pr_visual_bridge_struct pr_visual_bridge_t;
typedef struct pr_audio_bridge_struct pr_audio_bridge_t;
typedef struct pr_speech_bridge_struct pr_speech_bridge_t;
typedef struct pr_omni_bridge_struct pr_omni_bridge_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Active inference action
 *
 * WHAT: Represents an action to reduce prediction error
 * WHY:  Active inference: minimize expected free energy through action
 * HOW:  Action type + expected benefit + parameters
 */
typedef struct {
    pr_action_type_t type;            /**< Action type */
    float expected_pe_reduction;       /**< Expected PE reduction [0, inf) */
    float priority;                    /**< Action priority (higher = more urgent) */
    float action_params[8];            /**< Action-specific parameters */
    uint64_t created_time_ms;          /**< When action was generated */
    bool executed;                     /**< Whether action has been executed */
} pr_active_inference_action_t;

/**
 * @brief Prediction source information
 *
 * WHAT: A memory contributing to sensory predictions
 * WHY:  Track which memories are generating predictions
 * HOW:  Memory ID + resonance score + contribution weight
 */
typedef struct {
    uint64_t node_id;                  /**< Source memory node ID */
    float resonance_score;             /**< Resonance with current context */
    float contribution_weight;         /**< Weight in prediction generation */
    nimcp_quaternion_t state;          /**< Memory's quaternion state */
} pr_prediction_source_t;

/**
 * @brief Reconsolidation window state
 *
 * WHAT: State of a memory in reconsolidation
 * WHY:  High PE triggers reconsolidation window where memory is labile
 * HOW:  Track memory, window timing, accumulated updates
 */
typedef struct {
    uint64_t node_id;                  /**< Memory being reconsolidated */
    uint64_t window_start_ms;          /**< When window opened */
    uint64_t window_duration_ms;       /**< How long window is open */
    float accumulated_pe;              /**< PE accumulated during window */
    float update_magnitude;            /**< How much to update memory */
    bool active;                       /**< Whether window is currently active */
} pr_reconsolidation_window_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Parameters controlling predictive bridge behavior
 * WHY:  Different contexts may need different PE/memory coupling
 * HOW:  Thresholds, weights, and feature flags
 */
typedef struct {
    /* PE thresholds */
    float pe_threshold_update;         /**< PE above this triggers reconsolidation */
    float pe_threshold_new;            /**< PE above this creates new memory */

    /* Initial precision values */
    float initial_visual_precision;    /**< Starting visual precision */
    float initial_audio_precision;     /**< Starting audio precision */
    float initial_speech_precision;    /**< Starting speech precision */

    /* Coupling parameters */
    float resonance_prediction_weight; /**< Resonance → prediction confidence */
    float consolidation_boost;         /**< Strength boost on low PE */
    float reconsolidation_duration_ms; /**< How long reconsolidation window stays open */

    /* Precision adaptation */
    float precision_adaptation_rate;   /**< How fast precision adapts */
    bool enable_precision_adaptation;  /**< Auto-adapt precision from PE variance */

    /* Feature enables */
    bool enable_visual;                /**< Process visual FEP */
    bool enable_audio;                 /**< Process audio FEP */
    bool enable_speech;                /**< Process speech FEP */
    bool enable_active_inference;      /**< Generate active inference actions */
    bool enable_reconsolidation;       /**< Allow memory reconsolidation */
    bool track_free_energy;            /**< Track free energy history */

    /* Action selection */
    float action_temperature;          /**< Softmax temperature for action selection */
    size_t max_pending_actions;        /**< Maximum queued actions */
} pr_predictive_bridge_config_t;

/**
 * @brief Prediction error state
 *
 * WHAT: Current PE values from all modalities
 * WHY:  Central tracking of sensory prediction errors
 * HOW:  Per-modality PE + combined PE + history
 */
typedef struct {
    /* Per-modality PE */
    float visual_pe;                   /**< Visual prediction error */
    float audio_pe;                    /**< Audio prediction error */
    float speech_pe;                   /**< Speech prediction error */

    /* Combined PE */
    float combined_pe;                 /**< Precision-weighted sum */

    /* Precision weights */
    float visual_precision;            /**< Visual precision (inverse variance) */
    float audio_precision;             /**< Audio precision */
    float speech_precision;            /**< Speech precision */

    /* PE history for precision adaptation */
    float visual_pe_variance;          /**< Variance of recent visual PE */
    float audio_pe_variance;           /**< Variance of recent audio PE */
    float speech_pe_variance;          /**< Variance of recent speech PE */

    /* Timestamps */
    uint64_t last_visual_update_ms;    /**< Last visual PE update */
    uint64_t last_audio_update_ms;     /**< Last audio PE update */
    uint64_t last_speech_update_ms;    /**< Last speech PE update */
} pr_pred_error_state_t;

/**
 * @brief Memory update state
 *
 * WHAT: Track memory operations triggered by PE
 * WHY:  Monitor how PE is affecting memory system
 * HOW:  Counts and recent update info
 */
typedef struct {
    uint64_t consolidations;           /**< Total consolidation events */
    uint64_t reconsolidations;         /**< Total reconsolidation events */
    uint64_t new_encodings;            /**< Total new memory encodings */
    pr_mem_update_mode_t last_mode;    /**< Most recent update mode */
    uint64_t last_updated_node_id;     /**< Most recently updated node */
    uint64_t last_update_time_ms;      /**< When last update occurred */
} pr_mem_update_state_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the bridge
 * WHY:  Monitor bridge health and performance
 * HOW:  Counters and averages
 */
typedef struct {
    /* Update counts */
    uint64_t total_updates;            /**< Total update cycles */
    uint64_t pe_collections;           /**< Times PE was collected */
    uint64_t predictions_generated;    /**< Predictions generated */
    uint64_t actions_generated;        /**< Active inference actions created */
    uint64_t actions_executed;         /**< Actions that were executed */

    /* PE statistics */
    double avg_combined_pe;            /**< Running average combined PE */
    float max_combined_pe;             /**< Peak combined PE seen */
    float min_combined_pe;             /**< Minimum combined PE */

    /* Free energy */
    double avg_free_energy;            /**< Running average free energy */
    float current_free_energy;         /**< Current free energy value */

    /* Memory operations */
    uint64_t memories_strengthened;    /**< Memories consolidated */
    uint64_t memories_updated;         /**< Memories reconsolidated */
    uint64_t memories_created;         /**< New memories from PE */

    /* Timing */
    double avg_update_time_us;         /**< Average update time */
    uint64_t last_reset_time_ms;       /**< When stats were reset */
} pr_predictive_bridge_stats_t;

/**
 * @brief Prime Resonant Predictive Bridge
 *
 * WHAT: Integrates FEP/predictive processing with PR memory system
 * WHY:  Memory shapes predictions; prediction errors modulate memory
 * HOW:  Collects PE from FEP bridges, generates predictions from memories,
 *       triggers memory consolidation/reconsolidation/encoding based on PE
 */
/* Forward declaration for health agent (Phase 8) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

typedef struct {
    bridge_base_t base;                /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    pr_predictive_bridge_config_t config;

    /* Connected FEP bridges */
    visual_cortex_fep_bridge_t* visual_fep;
    audio_cortex_fep_bridge_t* audio_fep;
    speech_cortex_fep_bridge_t* speech_fep;

    /* Connected PR bridges (optional) */
    pr_visual_bridge_t* pr_visual;
    pr_audio_bridge_t* pr_audio;
    pr_speech_bridge_t* pr_speech;
    pr_omni_bridge_t* pr_omni;

    /* PR memory manager */
    pr_node_manager_t node_manager;

    /* Prediction error state */
    pr_pred_error_state_t pe_state;

    /* Memory-based predictions */
    pr_prediction_source_t prediction_sources[PR_PRED_MAX_PREDICTION_SOURCES];
    size_t num_prediction_sources;

    /* Reconsolidation windows */
    pr_reconsolidation_window_t* reconsolidation_windows;
    size_t num_reconsolidation_windows;
    size_t max_reconsolidation_windows;

    /* Free energy tracking */
    float current_free_energy;
    float free_energy_history[PR_PRED_FREE_ENERGY_HISTORY_SIZE];
    size_t history_idx;
    size_t history_count;

    /* Active inference actions */
    pr_active_inference_action_t pending_actions[PR_PRED_MAX_PENDING_ACTIONS];
    size_t num_pending_actions;

    /* Memory update state */
    pr_mem_update_state_t mem_update_state;

    /* Statistics */
    pr_predictive_bridge_stats_t stats;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;

} pr_predictive_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets biologically plausible parameters
 *
 * @return Default configuration
 *
 * Performance: ~5ns
 *
 * Default values:
 * - pe_threshold_update: 2.0
 * - pe_threshold_new: 5.0
 * - All precisions: 1.0
 * - resonance_prediction_weight: 0.7
 * - All features enabled
 */
NIMCP_EXPORT pr_predictive_bridge_config_t pr_predictive_bridge_config_default(void);

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
 * - Thresholds must be > 0
 * - Precisions must be in [PR_PRED_MIN_PRECISION, PR_PRED_MAX_PRECISION]
 * - Weights must be in [0, 1]
 */
NIMCP_EXPORT bool pr_predictive_bridge_config_validate(
    const pr_predictive_bridge_config_t* config
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create predictive bridge
 *
 * WHAT: Allocates and initializes bridge
 * WHY:  Entry point for FEP-PR integration
 * HOW:  Allocates memory, initializes state, applies config
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 *
 * Performance: ~100us
 * Memory: ~2KB base + action/reconsolidation buffers
 */
NIMCP_EXPORT pr_predictive_bridge_t* pr_predictive_bridge_create(
    const pr_predictive_bridge_config_t* config
);

/**
 * @brief Destroy predictive bridge
 *
 * WHAT: Releases all bridge resources
 * WHY:  Clean shutdown
 * HOW:  Disconnects bridges, frees buffers
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: ~10us
 */
NIMCP_EXPORT void pr_predictive_bridge_destroy(pr_predictive_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Clears PE state, actions, statistics while preserving connections
 * WHY:  Allow fresh start without reconnecting
 *
 * @param bridge Bridge to reset
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_reset(
    pr_predictive_bridge_t* bridge
);

//=============================================================================
// Connection Functions - FEP Bridges
//=============================================================================

/**
 * @brief Connect visual FEP bridge
 *
 * WHAT: Link to visual cortex FEP bridge for visual PE
 * WHY:  Enable visual prediction error collection
 *
 * @param bridge Predictive bridge
 * @param visual_fep Visual FEP bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_connect_visual_fep(
    pr_predictive_bridge_t* bridge,
    visual_cortex_fep_bridge_t* visual_fep
);

/**
 * @brief Connect audio FEP bridge
 *
 * WHAT: Link to audio cortex FEP bridge for audio PE
 * WHY:  Enable auditory prediction error collection
 *
 * @param bridge Predictive bridge
 * @param audio_fep Audio FEP bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_connect_audio_fep(
    pr_predictive_bridge_t* bridge,
    audio_cortex_fep_bridge_t* audio_fep
);

/**
 * @brief Connect speech FEP bridge
 *
 * WHAT: Link to speech cortex FEP bridge for speech PE
 * WHY:  Enable speech/phoneme prediction error collection
 *
 * @param bridge Predictive bridge
 * @param speech_fep Speech FEP bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_connect_speech_fep(
    pr_predictive_bridge_t* bridge,
    speech_cortex_fep_bridge_t* speech_fep
);

//=============================================================================
// Connection Functions - PR Bridges
//=============================================================================

/**
 * @brief Connect PR visual bridge
 *
 * @param bridge Predictive bridge
 * @param pr_visual PR visual bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_connect_pr_visual(
    pr_predictive_bridge_t* bridge,
    pr_visual_bridge_t* pr_visual
);

/**
 * @brief Connect PR audio bridge
 *
 * @param bridge Predictive bridge
 * @param pr_audio PR audio bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_connect_pr_audio(
    pr_predictive_bridge_t* bridge,
    pr_audio_bridge_t* pr_audio
);

/**
 * @brief Connect PR speech bridge
 *
 * @param bridge Predictive bridge
 * @param pr_speech PR speech bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_connect_pr_speech(
    pr_predictive_bridge_t* bridge,
    pr_speech_bridge_t* pr_speech
);

/**
 * @brief Connect PR omni bridge (multimodal)
 *
 * @param bridge Predictive bridge
 * @param pr_omni PR omni bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_connect_pr_omni(
    pr_predictive_bridge_t* bridge,
    pr_omni_bridge_t* pr_omni
);

/**
 * @brief Connect PR node manager
 *
 * WHAT: Link to PR memory node manager
 * WHY:  Enable memory creation and access
 *
 * @param bridge Predictive bridge
 * @param node_manager PR node manager
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_connect_node_manager(
    pr_predictive_bridge_t* bridge,
    pr_node_manager_t node_manager
);

//=============================================================================
// Main Update Functions
//=============================================================================

/**
 * @brief Main update cycle
 *
 * WHAT: Primary update function - collect PE, update memories, generate actions
 * WHY:  Core predictive processing loop
 * HOW:
 *   1. Collect PE from all connected FEP bridges
 *   2. Compute precision-weighted combined PE
 *   3. Update precision estimates from PE variance
 *   4. Determine memory update mode (consolidate/reconsolidate/encode)
 *   5. Update memories accordingly
 *   6. Generate predictions from high-resonance memories
 *   7. Compute free energy
 *   8. Generate active inference actions if enabled
 *
 * @param bridge Predictive bridge
 * @param delta_ms Time since last update
 * @return PR_PRED_SUCCESS or error code
 *
 * Performance: ~500us typical
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_update(
    pr_predictive_bridge_t* bridge,
    uint64_t delta_ms
);

//=============================================================================
// Prediction Error Functions
//=============================================================================

/**
 * @brief Collect PE from all connected FEP bridges
 *
 * WHAT: Gather current prediction errors from sensory modalities
 * WHY:  First step in predictive processing cycle
 * HOW:  Query each connected FEP bridge for current PE
 *
 * @param bridge Predictive bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_collect_pe(
    pr_predictive_bridge_t* bridge
);

/**
 * @brief Compute precision-weighted combined PE
 *
 * WHAT: Combine modality-specific PEs using precision weights
 * WHY:  Single PE value for memory decisions
 * HOW:  Weighted average: Σ(precision_i * PE_i) / Σ(precision_i)
 *
 * @param bridge Predictive bridge
 * @param combined_pe Output combined PE value
 * @return PR_PRED_SUCCESS or error code
 *
 * Performance: ~10us
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_compute_combined_pe(
    pr_predictive_bridge_t* bridge,
    float* combined_pe
);

/**
 * @brief Update precision estimates from PE variance
 *
 * WHAT: Adapt precision weights based on PE reliability
 * WHY:  Reliable modalities should have higher precision
 * HOW:  precision = 1 / (variance + epsilon)
 *
 * @param bridge Predictive bridge
 * @return PR_PRED_SUCCESS or error code
 *
 * Performance: ~20us
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_update_precision(
    pr_predictive_bridge_t* bridge
);

/**
 * @brief Set precision manually for a modality
 *
 * WHAT: Override automatic precision for a modality
 * WHY:  Allow external control of modality weighting
 *
 * @param bridge Predictive bridge
 * @param visual_precision Visual precision (or NaN to keep)
 * @param audio_precision Audio precision (or NaN to keep)
 * @param speech_precision Speech precision (or NaN to keep)
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_set_precision(
    pr_predictive_bridge_t* bridge,
    float visual_precision,
    float audio_precision,
    float speech_precision
);

//=============================================================================
// Prediction Generation Functions
//=============================================================================

/**
 * @brief Generate predictions from high-resonance memories
 *
 * WHAT: Use memories to generate sensory predictions
 * WHY:  Predictions compared to input yield PE
 * HOW:  Find high-resonance memories, weight their contributions
 *
 * @param bridge Predictive bridge
 * @param query Current context for resonance scoring
 * @return PR_PRED_SUCCESS or error code
 *
 * Performance: ~200us (depends on memory count)
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_generate_predictions(
    pr_predictive_bridge_t* bridge,
    const resonance_query_t* query
);

/**
 * @brief Add memory as prediction source
 *
 * WHAT: Register a memory as contributing to predictions
 * WHY:  Manual control over prediction sources
 *
 * @param bridge Predictive bridge
 * @param node Memory node to add
 * @param resonance_score How strongly this memory contributes
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_add_prediction_source(
    pr_predictive_bridge_t* bridge,
    const pr_memory_node_t* node,
    float resonance_score
);

/**
 * @brief Clear all prediction sources
 *
 * @param bridge Predictive bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_clear_prediction_sources(
    pr_predictive_bridge_t* bridge
);

/**
 * @brief Convert resonance score to prediction confidence
 *
 * WHAT: Map resonance → how confident the prediction should be
 * WHY:  High resonance = confident prediction = expect low PE
 * HOW:  confidence = resonance_prediction_weight * resonance
 *
 * @param bridge Predictive bridge
 * @param resonance Resonance score [0, 1]
 * @return Prediction confidence [0, 1]
 */
NIMCP_EXPORT float pr_predictive_bridge_resonance_to_confidence(
    const pr_predictive_bridge_t* bridge,
    float resonance
);

//=============================================================================
// Memory Update Functions
//=============================================================================

/**
 * @brief Update memories based on current PE
 *
 * WHAT: Trigger appropriate memory operation based on PE level
 * WHY:  PE modulates memory consolidation/reconsolidation/encoding
 * HOW:  Determine mode from PE + resonance, execute update
 *
 * @param bridge Predictive bridge
 * @return PR_PRED_SUCCESS or error code
 *
 * PE → Memory update logic:
 * - Low PE + high resonance → consolidation (strengthen)
 * - High PE + high resonance → reconsolidation (update)
 * - High PE + low resonance → new encoding
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_update_memories_from_pe(
    pr_predictive_bridge_t* bridge
);

/**
 * @brief Trigger reconsolidation for a memory
 *
 * WHAT: Open reconsolidation window for a memory
 * WHY:  High PE for a predicted memory = memory needs updating
 * HOW:  Mark memory as labile, start window timer
 *
 * @param bridge Predictive bridge
 * @param node_id ID of memory to reconsolidate
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_trigger_reconsolidation(
    pr_predictive_bridge_t* bridge,
    uint64_t node_id
);

/**
 * @brief Close reconsolidation window and apply updates
 *
 * WHAT: End reconsolidation window, commit accumulated changes
 * WHY:  Reconsolidation window has time limit
 *
 * @param bridge Predictive bridge
 * @param node_id ID of memory
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_close_reconsolidation(
    pr_predictive_bridge_t* bridge,
    uint64_t node_id
);

/**
 * @brief Strengthen memory (consolidation)
 *
 * WHAT: Boost quaternion.w (consolidation) for a memory
 * WHY:  Low PE confirms memory accuracy → strengthen
 * HOW:  Increase w by consolidation_boost amount
 *
 * @param bridge Predictive bridge
 * @param node Memory node to strengthen
 * @param boost Amount to increase consolidation [0, 1]
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_strengthen_memory(
    pr_predictive_bridge_t* bridge,
    pr_memory_node_t* node,
    float boost
);

/**
 * @brief Create new memory from high PE context
 *
 * WHAT: Encode new memory when PE is high but no matching memory exists
 * WHY:  Novel experience requires new memory
 *
 * @param bridge Predictive bridge
 * @param data Data to encode
 * @param data_size Size of data
 * @param initial_state Initial quaternion state
 * @param new_node Output: created node (can be NULL)
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_encode_new_memory(
    pr_predictive_bridge_t* bridge,
    const void* data,
    size_t data_size,
    nimcp_quaternion_t initial_state,
    pr_memory_node_t** new_node
);

//=============================================================================
// Free Energy Functions
//=============================================================================

/**
 * @brief Compute variational free energy
 *
 * WHAT: Calculate current free energy (complexity + accuracy)
 * WHY:  Free energy is what the brain minimizes
 * HOW:  F = -log P(o|m) + KL[q(s)||p(s|m)]
 *
 * Simplified: F ≈ PE + complexity_term
 *
 * @param bridge Predictive bridge
 * @param free_energy Output free energy value
 * @return PR_PRED_SUCCESS or error code
 *
 * Performance: ~30us
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_compute_free_energy(
    pr_predictive_bridge_t* bridge,
    float* free_energy
);

/**
 * @brief Get free energy history
 *
 * WHAT: Retrieve recent free energy values
 * WHY:  Track free energy trends over time
 *
 * @param bridge Predictive bridge
 * @param history Output buffer (must be >= PR_PRED_FREE_ENERGY_HISTORY_SIZE)
 * @param count Output: number of values in history
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_get_free_energy_history(
    const pr_predictive_bridge_t* bridge,
    float* history,
    size_t* count
);

//=============================================================================
// Active Inference Functions
//=============================================================================

/**
 * @brief Select best action to reduce PE
 *
 * WHAT: Choose action with highest expected PE reduction
 * WHY:  Active inference: act to minimize expected free energy
 * HOW:  Softmax over expected PE reductions
 *
 * @param bridge Predictive bridge
 * @param selected_action Output selected action
 * @return PR_PRED_SUCCESS or error code
 *
 * Performance: ~50us
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_select_action(
    pr_predictive_bridge_t* bridge,
    pr_active_inference_action_t* selected_action
);

/**
 * @brief Generate candidate actions based on current PE
 *
 * WHAT: Create set of possible actions to reduce PE
 * WHY:  Need candidates before selection
 * HOW:  Based on which modalities have high PE
 *
 * @param bridge Predictive bridge
 * @return Number of actions generated, or -1 on error
 */
NIMCP_EXPORT int pr_predictive_bridge_generate_actions(
    pr_predictive_bridge_t* bridge
);

/**
 * @brief Execute a pending action
 *
 * WHAT: Mark action as executed, apply effects
 * WHY:  Track action execution
 *
 * @param bridge Predictive bridge
 * @param action_idx Index of action to execute
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_execute_action(
    pr_predictive_bridge_t* bridge,
    size_t action_idx
);

/**
 * @brief Get pending actions
 *
 * @param bridge Predictive bridge
 * @param actions Output buffer for actions
 * @param max_actions Maximum actions to retrieve
 * @return Number of actions copied, or -1 on error
 */
NIMCP_EXPORT int pr_predictive_bridge_get_pending_actions(
    const pr_predictive_bridge_t* bridge,
    pr_active_inference_action_t* actions,
    size_t max_actions
);

/**
 * @brief Clear all pending actions
 *
 * @param bridge Predictive bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_clear_actions(
    pr_predictive_bridge_t* bridge
);

//=============================================================================
// State and Statistics Functions
//=============================================================================

/**
 * @brief Get current PE state
 *
 * @param bridge Predictive bridge
 * @param pe_state Output PE state
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_get_pe_state(
    const pr_predictive_bridge_t* bridge,
    pr_pred_error_state_t* pe_state
);

/**
 * @brief Get memory update state
 *
 * @param bridge Predictive bridge
 * @param mem_state Output memory update state
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_get_mem_update_state(
    const pr_predictive_bridge_t* bridge,
    pr_mem_update_state_t* mem_state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Predictive bridge
 * @param stats Output statistics
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_get_stats(
    const pr_predictive_bridge_t* bridge,
    pr_predictive_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Predictive bridge
 * @return PR_PRED_SUCCESS or error code
 */
NIMCP_EXPORT pr_pred_error_t pr_predictive_bridge_reset_stats(
    pr_predictive_bridge_t* bridge
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
NIMCP_EXPORT const char* pr_pred_error_string(pr_pred_error_t error);

/**
 * @brief Get action type name
 *
 * @param action_type Action type
 * @return Human-readable action name
 */
NIMCP_EXPORT const char* pr_pred_action_name(pr_action_type_t action_type);

/**
 * @brief Get memory update mode name
 *
 * @param mode Update mode
 * @return Human-readable mode name
 */
NIMCP_EXPORT const char* pr_pred_update_mode_name(pr_mem_update_mode_t mode);

/**
 * @brief Print bridge state summary (debug)
 *
 * @param bridge Predictive bridge
 */
NIMCP_EXPORT void pr_predictive_bridge_print_state(
    const pr_predictive_bridge_t* bridge
);

/**
 * @brief Check if bridge is ready for update
 *
 * WHAT: Verify at least one FEP bridge is connected
 * WHY:  Need PE source to do anything useful
 *
 * @param bridge Predictive bridge
 * @return true if ready, false otherwise
 */
NIMCP_EXPORT bool pr_predictive_bridge_is_ready(
    const pr_predictive_bridge_t* bridge
);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_pred_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_PREDICTIVE_BRIDGE_H
