/**
 * @file nimcp_claustrum.h
 * @brief Claustrum Module - Consciousness Integration and Cross-Modal Binding
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Claustrum modeling for consciousness binding and cross-modal integration
 * WHY:  The claustrum is hypothesized to be the "conductor of consciousness" (Crick & Koch)
 * HOW:  Model cross-modal binding, salience detection, attention coordination, and global workspace gating
 *
 * CRICK'S HYPOTHESIS ("The Conductor of Consciousness"):
 * Francis Crick and Christof Koch proposed that the claustrum, a thin sheet of neurons
 * beneath the cortex, orchestrates conscious experience by:
 * 1. Binding disparate sensory modalities into unified percepts
 * 2. Coordinating attention across cortical regions
 * 3. Enabling rapid task switching and state transitions
 * 4. Gating access to the global workspace for conscious processing
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ANATOMICAL FEATURES:
 * - Thin sheet of gray matter between insula and putamen
 * - One of the most densely connected structures in the brain
 * - Bidirectional connections with nearly ALL cortical areas
 * - Contains ~0.25% of total cortical neurons but receives input from 90%+ of cortex
 *
 * KEY CONNECTIONS:
 * - Visual cortex (V1-V4): Visual feature binding
 * - Auditory cortex (A1, belt regions): Auditory stream binding
 * - Somatosensory cortex (S1, S2): Tactile integration
 * - Prefrontal cortex: Executive control, working memory
 * - Cingulate cortex: Salience and attention
 * - Insula: Interoception and embodiment
 *
 * FUNCTIONAL ROLES:
 * 1. Cross-modal Binding:
 *    - Combines visual + auditory + somatosensory into unified percept
 *    - Temporal synchronization of distributed cortical activity
 *    - Feature binding across sensory streams
 *
 * 2. Salience Detection:
 *    - Amplifies salient/relevant stimuli
 *    - Suppresses irrelevant background
 *    - Works with anterior cingulate for attention
 *
 * 3. Attention Coordination:
 *    - Biases cortical processing toward attended stimuli
 *    - Coordinates spatial and feature-based attention
 *    - Enables selective amplification of task-relevant info
 *
 * 4. Task Switching:
 *    - Rapid reconfiguration of cortical networks
 *    - "Reset" signal for cognitive state transitions
 *    - Coordination of default mode vs task-positive networks
 *
 * 5. Global Workspace Access:
 *    - Gating mechanism for conscious awareness
 *    - Determines what reaches global broadcast
 *    - Integration with Global Workspace Theory
 *
 * NEURAL SYNCHRONIZATION:
 * - Gamma oscillations (30-100 Hz): Feature binding
 * - Alpha oscillations (8-12 Hz): Attention gating
 * - Cross-frequency coupling: Integration across timescales
 *
 * MODULE ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                         CLAUSTRUM MODULE                                   |
 * +===========================================================================+
 * |                                                                            |
 * |  INPUT STREAMS                 CORE PROCESSING              OUTPUT         |
 * |  ─────────────                 ───────────────              ──────         |
 * |                                                                            |
 * |  ┌──────────┐                 ┌─────────────────┐                          |
 * |  │ Visual   │────────────────>│                 │                          |
 * |  └──────────┘                 │  Cross-Modal    │       ┌──────────────┐   |
 * |  ┌──────────┐                 │  Binding        │──────>│ Unified      │   |
 * |  │ Auditory │────────────────>│  Engine         │       │ Percept      │   |
 * |  └──────────┘                 │                 │       └──────────────┘   |
 * |  ┌──────────┐                 └─────────────────┘                          |
 * |  │ Somato   │─────────┐                                                    |
 * |  └──────────┘         │       ┌─────────────────┐                          |
 * |  ┌──────────┐         └──────>│                 │       ┌──────────────┐   |
 * |  │ Olfactory│────────────────>│  Salience       │──────>│ Attention    │   |
 * |  └──────────┘                 │  Detection      │       │ Bias         │   |
 * |  ┌──────────┐                 │                 │       └──────────────┘   |
 * |  │ Gustatory│────────────────>└─────────────────┘                          |
 * |  └──────────┘                                                              |
 * |                               ┌─────────────────┐                          |
 * |  ┌──────────┐                 │                 │       ┌──────────────┐   |
 * |  │ Executive│────────────────>│  State          │──────>│ Task         │   |
 * |  │ Goals    │                 │  Controller     │       │ Configuration│   |
 * |  └──────────┘                 │                 │       └──────────────┘   |
 * |                               └─────────────────┘                          |
 * |                                                                            |
 * |                               ┌─────────────────┐       ┌──────────────┐   |
 * |                               │  Synchronizer   │──────>│ Gamma/Alpha  │   |
 * |                               │  (Oscillations) │       │ Modulation   │   |
 * |                               └─────────────────┘       └──────────────┘   |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - Platform tier support
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CLAUSTRUM_H
#define NIMCP_CLAUSTRUM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum number of sensory modalities for binding */
#define CLAUSTRUM_MAX_MODALITIES            8

/** Maximum cortical regions for coordination */
#define CLAUSTRUM_MAX_CORTICAL_REGIONS      32

/** Maximum bound percepts tracked simultaneously */
#define CLAUSTRUM_MAX_BOUND_PERCEPTS        16

/** Gamma oscillation frequency (Hz) for binding */
#define CLAUSTRUM_GAMMA_FREQUENCY_HZ        40.0f

/** Alpha oscillation frequency (Hz) for gating */
#define CLAUSTRUM_ALPHA_FREQUENCY_HZ        10.0f

/** Default binding threshold */
#define CLAUSTRUM_DEFAULT_BINDING_THRESHOLD 0.6f

/** Default salience threshold */
#define CLAUSTRUM_DEFAULT_SALIENCE_THRESHOLD 0.5f

/** Default synchronization window (ms) */
#define CLAUSTRUM_DEFAULT_SYNC_WINDOW_MS    50.0f

/** History buffer size for temporal binding */
#define CLAUSTRUM_HISTORY_SIZE              64

/** Maximum message types for bio-async */
#define CLAUSTRUM_BIO_MSG_COUNT             12

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

typedef enum {
    CLAUSTRUM_OK = 0,
    CLAUSTRUM_ERR_NULL_PTR = -1,
    CLAUSTRUM_ERR_INVALID_PARAM = -2,
    CLAUSTRUM_ERR_NOT_INITIALIZED = -3,
    CLAUSTRUM_ERR_ALREADY_INITIALIZED = -4,
    CLAUSTRUM_ERR_NO_MEMORY = -5,
    CLAUSTRUM_ERR_MODALITY_NOT_FOUND = -6,
    CLAUSTRUM_ERR_CAPACITY_EXCEEDED = -7,
    CLAUSTRUM_ERR_BINDING_FAILED = -8,
    CLAUSTRUM_ERR_SYNC_FAILED = -9,
    CLAUSTRUM_ERR_INVALID_STATE = -10,
    CLAUSTRUM_ERR_SECURITY_VIOLATION = -11
} nimcp_claustrum_error_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Sensory modality types for cross-modal binding
 */
typedef enum {
    CLAUSTRUM_MODALITY_VISUAL = 0,          /**< Visual input stream */
    CLAUSTRUM_MODALITY_AUDITORY,            /**< Auditory input stream */
    CLAUSTRUM_MODALITY_SOMATOSENSORY,       /**< Tactile/proprioceptive */
    CLAUSTRUM_MODALITY_OLFACTORY,           /**< Olfactory input */
    CLAUSTRUM_MODALITY_GUSTATORY,           /**< Gustatory input */
    CLAUSTRUM_MODALITY_INTEROCEPTIVE,       /**< Internal body state */
    CLAUSTRUM_MODALITY_VESTIBULAR,          /**< Balance/spatial */
    CLAUSTRUM_MODALITY_MOTOR_EFFERENCE,     /**< Motor copy */
    CLAUSTRUM_MODALITY_COUNT
} nimcp_claustrum_modality_t;

/**
 * @brief Claustrum operational state
 */
typedef enum {
    CLAUSTRUM_STATE_IDLE = 0,               /**< Ready for processing */
    CLAUSTRUM_STATE_BINDING,                /**< Cross-modal binding active */
    CLAUSTRUM_STATE_SYNCHRONIZING,          /**< Temporal synchronization */
    CLAUSTRUM_STATE_SWITCHING,              /**< Task/state switching */
    CLAUSTRUM_STATE_BROADCASTING,           /**< Global workspace broadcast */
    CLAUSTRUM_STATE_GATING                  /**< Access gating in progress */
} nimcp_claustrum_state_t;

/**
 * @brief Claustrum status for overall health
 */
typedef enum {
    CLAUSTRUM_STATUS_NORMAL = 0,            /**< Normal operation */
    CLAUSTRUM_STATUS_HYPERACTIVE,           /**< Excessive binding */
    CLAUSTRUM_STATUS_HYPOACTIVE,            /**< Insufficient integration */
    CLAUSTRUM_STATUS_DESYNCHRONIZED,        /**< Loss of temporal sync */
    CLAUSTRUM_STATUS_OVERLOADED             /**< Capacity exceeded */
} nimcp_claustrum_status_t;

/**
 * @brief Consciousness level (simplified GWT model)
 */
typedef enum {
    CLAUSTRUM_CONSCIOUSNESS_UNCONSCIOUS = 0,  /**< Below threshold */
    CLAUSTRUM_CONSCIOUSNESS_PRECONSCIOUS,     /**< Available but not accessed */
    CLAUSTRUM_CONSCIOUSNESS_CONSCIOUS,        /**< In global workspace */
    CLAUSTRUM_CONSCIOUSNESS_FOCAL             /**< Center of attention */
} nimcp_claustrum_consciousness_level_t;

/**
 * @brief Brain state for task switching
 */
typedef enum {
    CLAUSTRUM_BRAIN_STATE_DEFAULT = 0,      /**< Default mode network active */
    CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE,    /**< Task-positive network active */
    CLAUSTRUM_BRAIN_STATE_SALIENCE,         /**< Salience network dominant */
    CLAUSTRUM_BRAIN_STATE_TRANSITION        /**< Between states */
} nimcp_claustrum_brain_state_t;

/**
 * @brief Cortical region types for coordination
 */
typedef enum {
    CLAUSTRUM_REGION_PREFRONTAL = 0,        /**< PFC - Executive control */
    CLAUSTRUM_REGION_CINGULATE,             /**< ACC - Conflict monitoring */
    CLAUSTRUM_REGION_INSULA,                /**< Insula - Interoception */
    CLAUSTRUM_REGION_PARIETAL,              /**< Parietal - Attention */
    CLAUSTRUM_REGION_TEMPORAL,              /**< Temporal - Memory */
    CLAUSTRUM_REGION_OCCIPITAL,             /**< Occipital - Vision */
    CLAUSTRUM_REGION_MOTOR,                 /**< Motor cortex */
    CLAUSTRUM_REGION_SOMATOSENSORY,         /**< Somatosensory cortex */
    CLAUSTRUM_REGION_AUDITORY,              /**< Auditory cortex */
    CLAUSTRUM_REGION_HIPPOCAMPUS,           /**< Memory formation */
    CLAUSTRUM_REGION_AMYGDALA,              /**< Emotional tagging */
    CLAUSTRUM_REGION_COUNT
} nimcp_claustrum_region_t;

/*=============================================================================
 * BIO-ASYNC MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief Claustrum bio-async message types
 *
 * WHAT: Message type enumeration for claustrum bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific claustrum function
 */
typedef enum {
    CLAUSTRUM_BIO_MSG_BINDING = 0,          /**< Cross-modal binding result */
    CLAUSTRUM_BIO_MSG_SYNC,                 /**< Synchronization signal */
    CLAUSTRUM_BIO_MSG_SALIENCE,             /**< Salience detection result */
    CLAUSTRUM_BIO_MSG_ATTENTION_BIAS,       /**< Attention modulation signal */
    CLAUSTRUM_BIO_MSG_STATE_SWITCH,         /**< Brain state switch notification */
    CLAUSTRUM_BIO_MSG_WORKSPACE_GATE,       /**< Global workspace gating */
    CLAUSTRUM_BIO_MSG_PERCEPT_BROADCAST,    /**< Unified percept broadcast */
    CLAUSTRUM_BIO_MSG_GAMMA_MODULATION,     /**< Gamma oscillation control */
    CLAUSTRUM_BIO_MSG_ALPHA_MODULATION,     /**< Alpha oscillation control */
    CLAUSTRUM_BIO_MSG_REQUEST_BINDING,      /**< Binding request from module */
    CLAUSTRUM_BIO_MSG_MODALITY_UPDATE,      /**< Sensory modality update */
    CLAUSTRUM_BIO_MSG_CONSCIOUSNESS_CHANGE  /**< Consciousness level change */
} nimcp_claustrum_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define CLAUSTRUM_BIO_SUB_BINDING           (1U << CLAUSTRUM_BIO_MSG_BINDING)
#define CLAUSTRUM_BIO_SUB_SYNC              (1U << CLAUSTRUM_BIO_MSG_SYNC)
#define CLAUSTRUM_BIO_SUB_SALIENCE          (1U << CLAUSTRUM_BIO_MSG_SALIENCE)
#define CLAUSTRUM_BIO_SUB_ATTENTION_BIAS    (1U << CLAUSTRUM_BIO_MSG_ATTENTION_BIAS)
#define CLAUSTRUM_BIO_SUB_STATE_SWITCH      (1U << CLAUSTRUM_BIO_MSG_STATE_SWITCH)
#define CLAUSTRUM_BIO_SUB_WORKSPACE_GATE    (1U << CLAUSTRUM_BIO_MSG_WORKSPACE_GATE)
#define CLAUSTRUM_BIO_SUB_PERCEPT_BROADCAST (1U << CLAUSTRUM_BIO_MSG_PERCEPT_BROADCAST)
#define CLAUSTRUM_BIO_SUB_ALL               (0xFFFFFFFFU)

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct nimcp_claustrum_config_s nimcp_claustrum_config_t;
typedef struct nimcp_claustrum_modality_input_s nimcp_claustrum_modality_input_t;
typedef struct nimcp_claustrum_bound_percept_s nimcp_claustrum_bound_percept_t;
typedef struct nimcp_claustrum_cortical_link_s nimcp_claustrum_cortical_link_t;
typedef struct nimcp_claustrum_oscillator_s nimcp_claustrum_oscillator_t;
typedef struct nimcp_claustrum_metrics_s nimcp_claustrum_metrics_t;
typedef struct nimcp_claustrum_s nimcp_claustrum_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for binding events
 */
typedef void (*nimcp_claustrum_binding_callback_t)(
    nimcp_claustrum_t* claustrum,
    const nimcp_claustrum_bound_percept_t* percept,
    void* user_data
);

/**
 * @brief Callback for state transitions
 */
typedef void (*nimcp_claustrum_state_callback_t)(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_brain_state_t old_state,
    nimcp_claustrum_brain_state_t new_state,
    void* user_data
);

/**
 * @brief Callback for consciousness level changes
 */
typedef void (*nimcp_claustrum_consciousness_callback_t)(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_consciousness_level_t level,
    float strength,
    void* user_data
);

/**
 * @brief Callback for global workspace broadcast
 */
typedef void (*nimcp_claustrum_workspace_callback_t)(
    nimcp_claustrum_t* claustrum,
    const void* content,
    size_t content_size,
    float salience,
    void* user_data
);

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Sensory modality input state
 */
struct nimcp_claustrum_modality_input_s {
    nimcp_claustrum_modality_t type;        /**< Modality type */
    char name[32];                          /**< Modality name */

    /* Current input state */
    float* features;                        /**< Feature vector */
    uint32_t feature_dim;                   /**< Feature dimension */
    float activity_level;                   /**< Overall activity [0,1] */
    float salience;                         /**< Modality salience [0,1] */

    /* Temporal information */
    uint64_t timestamp_us;                  /**< Input timestamp */
    float latency_ms;                       /**< Processing latency */

    /* Binding state */
    bool active;                            /**< Modality currently active */
    bool bound;                             /**< Currently in bound percept */
    uint32_t binding_id;                    /**< ID of current binding */

    /* Synchronization */
    float phase;                            /**< Current oscillation phase */
    float coherence;                        /**< Cross-modal coherence [0,1] */
};

/**
 * @brief Bound unified percept
 */
struct nimcp_claustrum_bound_percept_s {
    uint32_t id;                            /**< Unique percept ID */

    /* Modality composition */
    uint32_t modality_mask;                 /**< Bitmask of included modalities */
    uint32_t num_modalities;                /**< Number of bound modalities */

    /* Percept properties */
    float binding_strength;                 /**< How strongly bound [0,1] */
    float salience;                         /**< Overall salience [0,1] */
    float coherence;                        /**< Temporal coherence [0,1] */
    float stability;                        /**< Percept stability [0,1] */

    /* Consciousness status */
    nimcp_claustrum_consciousness_level_t consciousness_level;
    float access_strength;                  /**< Global workspace access [0,1] */
    bool in_workspace;                      /**< Currently in global workspace */

    /* Timing */
    uint64_t creation_time_us;              /**< When percept formed */
    uint64_t last_update_us;                /**< Last update time */
    float duration_ms;                      /**< How long percept has existed */

    /* Validity */
    bool valid;                             /**< Percept currently valid */
};

/**
 * @brief Cortical region link for coordination
 */
struct nimcp_claustrum_cortical_link_s {
    nimcp_claustrum_region_t region;        /**< Target region */
    char region_name[32];                   /**< Region name */

    /* Connectivity */
    float forward_strength;                 /**< Claustrum -> Region [0,1] */
    float backward_strength;                /**< Region -> Claustrum [0,1] */

    /* Current state */
    float activity;                         /**< Region activity [0,1] */
    float attention_bias;                   /**< Attention modulation [0,1] */
    float synchronization;                  /**< Sync with claustrum [0,1] */

    /* State */
    bool active;
    bool receiving;                         /**< Receiving claustrum output */
    bool sending;                           /**< Sending to claustrum */
};

/**
 * @brief Neural oscillator for synchronization
 */
struct nimcp_claustrum_oscillator_s {
    /* Gamma band (30-100 Hz) - binding */
    float gamma_frequency;                  /**< Current gamma frequency */
    float gamma_amplitude;                  /**< Gamma oscillation amplitude */
    float gamma_phase;                      /**< Current gamma phase */

    /* Alpha band (8-12 Hz) - gating */
    float alpha_frequency;                  /**< Current alpha frequency */
    float alpha_amplitude;                  /**< Alpha oscillation amplitude */
    float alpha_phase;                      /**< Current alpha phase */

    /* Cross-frequency coupling */
    float phase_amplitude_coupling;         /**< Gamma-alpha coupling */

    /* Synchronization metrics */
    float global_coherence;                 /**< Overall oscillatory coherence */
    float binding_coherence;                /**< Coherence for binding */
};

/**
 * @brief Claustrum system configuration
 */
struct nimcp_claustrum_config_s {
    /* Binding parameters */
    float binding_threshold;                /**< Minimum coherence for binding */
    float binding_decay_rate;               /**< How fast bindings decay */
    float temporal_window_ms;               /**< Window for temporal binding */

    /* Salience parameters */
    float salience_threshold;               /**< Minimum salience for attention */
    float salience_gain;                    /**< Gain on salience computation */
    float novelty_weight;                   /**< Weight of novelty in salience */
    float relevance_weight;                 /**< Weight of relevance in salience */

    /* Oscillation parameters */
    float gamma_base_freq;                  /**< Base gamma frequency (Hz) */
    float alpha_base_freq;                  /**< Base alpha frequency (Hz) */
    float oscillation_coupling;             /**< Cross-frequency coupling strength */

    /* Global workspace */
    float workspace_threshold;              /**< Threshold for workspace access */
    float broadcast_duration_ms;            /**< How long broadcasts last */
    bool enable_workspace_gating;           /**< Enable workspace gating */

    /* Task switching */
    float switch_threshold;                 /**< Threshold for state switch */
    float switch_duration_ms;               /**< Duration of switch process */
    bool enable_rapid_switching;            /**< Enable rapid reconfiguration */

    /* Platform tier */
    uint32_t min_tier;                      /**< Minimum platform tier */

    /* Callbacks */
    nimcp_claustrum_binding_callback_t on_binding;
    nimcp_claustrum_state_callback_t on_state_change;
    nimcp_claustrum_consciousness_callback_t on_consciousness_change;
    nimcp_claustrum_workspace_callback_t on_workspace_broadcast;
    void* callback_data;

    /* Integration features */
    bool enable_immune_reporting;           /**< Report to brain immune system */
    bool enable_logging;                    /**< Enable detailed logging */
    bool enable_kg_integration;             /**< Knowledge graph integration */
    bool enable_snn_output;                 /**< Output to SNN layer */
};

/**
 * @brief Claustrum system metrics
 */
struct nimcp_claustrum_metrics_s {
    /* Binding statistics */
    uint64_t total_bindings;                /**< Total binding events */
    uint64_t successful_bindings;           /**< Successful bindings */
    uint64_t failed_bindings;               /**< Failed binding attempts */
    float mean_binding_strength;            /**< Average binding strength */

    /* Salience statistics */
    uint64_t salience_detections;           /**< Salience events detected */
    float mean_salience;                    /**< Average salience level */
    float peak_salience;                    /**< Maximum salience observed */

    /* Consciousness/workspace */
    uint64_t workspace_accesses;            /**< Times content reached workspace */
    uint64_t workspace_broadcasts;          /**< Global broadcasts made */
    float mean_consciousness_level;         /**< Average consciousness */

    /* State switching */
    uint64_t state_switches;                /**< Number of state transitions */
    float mean_switch_duration_ms;          /**< Average switch time */

    /* Synchronization */
    float mean_gamma_coherence;             /**< Average gamma coherence */
    float mean_alpha_power;                 /**< Average alpha power */
    float mean_global_coherence;            /**< Average global sync */

    /* Modality statistics */
    uint64_t modality_updates[CLAUSTRUM_MODALITY_COUNT];
    float modality_activity[CLAUSTRUM_MODALITY_COUNT];

    /* System */
    float total_simulation_time_s;          /**< Total simulated time */
    uint64_t update_count;                  /**< Number of update calls */
    float avg_update_latency_us;            /**< Average update latency */
};

/**
 * @brief Main Claustrum system structure
 */
struct nimcp_claustrum_s {
    /* Configuration */
    nimcp_claustrum_config_t config;

    /* Modality inputs */
    nimcp_claustrum_modality_input_t modalities[CLAUSTRUM_MODALITY_COUNT];
    uint32_t active_modality_mask;          /**< Bitmask of active modalities */

    /* Bound percepts */
    nimcp_claustrum_bound_percept_t percepts[CLAUSTRUM_MAX_BOUND_PERCEPTS];
    uint32_t num_active_percepts;
    uint32_t next_percept_id;

    /* Cortical links */
    nimcp_claustrum_cortical_link_t cortical_links[CLAUSTRUM_REGION_COUNT];
    uint32_t active_region_mask;

    /* Oscillatory system */
    nimcp_claustrum_oscillator_t oscillator;

    /* Current state */
    nimcp_claustrum_state_t state;
    nimcp_claustrum_status_t status;
    nimcp_claustrum_brain_state_t brain_state;

    /* Global workspace state */
    bool workspace_occupied;                /**< Workspace currently occupied */
    uint32_t workspace_percept_id;          /**< ID of percept in workspace */
    float workspace_access_level;           /**< Current access level */

    /* Attention state */
    float global_attention;                 /**< Overall attention level [0,1] */
    float spatial_attention[3];             /**< Spatial attention (x, y, z) */
    float feature_attention[8];             /**< Feature-based attention */

    /* Salience state */
    float global_salience;                  /**< Overall salience [0,1] */
    nimcp_claustrum_modality_t salient_modality; /**< Most salient modality */

    /* Task switching */
    float switch_progress;                  /**< Progress through switch [0,1] */
    nimcp_claustrum_brain_state_t target_state; /**< Target state for switch */

    /* Metrics */
    nimcp_claustrum_metrics_t metrics;

    /* Timing */
    float current_time_ms;                  /**< Simulation time (ms) */
    uint64_t last_update_us;                /**< Last update timestamp */
    uint64_t creation_time_us;              /**< Creation timestamp */

    /* State flags */
    bool initialized;
    bool running;
};

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default claustrum configuration
 *
 * WHAT: Provide sensible defaults for claustrum configuration
 * WHY:  Easy initialization with biologically-realistic parameters
 * HOW:  Return struct with evidence-based timing and thresholds
 *
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_claustrum_config_t nimcp_claustrum_default_config(void);

/**
 * @brief Initialize claustrum system
 *
 * WHAT: Initialize claustrum with given configuration
 * WHY:  Set up all subsystems for consciousness integration
 * HOW:  Allocate resources, initialize modalities, set up oscillators
 *
 * @param claustrum System to initialize
 * @param config Configuration (NULL for defaults)
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_init(
    nimcp_claustrum_t* claustrum,
    const nimcp_claustrum_config_t* config
);

/**
 * @brief Shutdown claustrum system
 *
 * WHAT: Clean shutdown of claustrum
 * WHY:  Proper resource deallocation
 * HOW:  Free modality features, clear state
 *
 * @param claustrum System to shutdown
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_shutdown(
    nimcp_claustrum_t* claustrum
);

/**
 * @brief Reset claustrum to baseline
 *
 * WHAT: Reset claustrum state while preserving configuration
 * WHY:  Allow restart without reinitialization
 * HOW:  Clear percepts, reset oscillators, clear workspace
 *
 * @param claustrum System to reset
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_reset(
    nimcp_claustrum_t* claustrum
);

/*=============================================================================
 * CORE UPDATE API
 *===========================================================================*/

/**
 * @brief Update claustrum system (single timestep)
 *
 * WHAT: Main update loop for claustrum processing
 * WHY:  Advance binding, synchronization, and state evolution
 * HOW:  Update oscillators, process modalities, evaluate bindings
 *
 * @param claustrum System
 * @param dt Time delta (ms)
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_update(
    nimcp_claustrum_t* claustrum,
    float dt
);

/*=============================================================================
 * MODALITY INPUT API
 *===========================================================================*/

/**
 * @brief Update sensory modality input
 *
 * WHAT: Provide new input for a sensory modality
 * WHY:  Feed sensory information for cross-modal binding
 * HOW:  Update modality features, compute salience, trigger binding evaluation
 *
 * @param claustrum System
 * @param modality Which modality to update
 * @param features Feature vector
 * @param feature_dim Feature dimension
 * @param activity Activity level [0,1]
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_update_modality(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_modality_t modality,
    const float* features,
    uint32_t feature_dim,
    float activity
);

/**
 * @brief Set modality salience
 *
 * WHAT: Explicitly set salience for a modality
 * WHY:  Allow external salience computation (e.g., from attention system)
 * HOW:  Update modality salience, trigger re-evaluation
 *
 * @param claustrum System
 * @param modality Which modality
 * @param salience Salience value [0,1]
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_set_modality_salience(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_modality_t modality,
    float salience
);

/**
 * @brief Get modality state
 *
 * WHAT: Retrieve current state of a modality
 * WHY:  Allow inspection of modality processing
 * HOW:  Copy modality state to output
 *
 * @param claustrum System
 * @param modality Which modality
 * @param[out] input Output modality state
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_modality(
    const nimcp_claustrum_t* claustrum,
    nimcp_claustrum_modality_t modality,
    nimcp_claustrum_modality_input_t* input
);

/*=============================================================================
 * CROSS-MODAL BINDING API
 *===========================================================================*/

/**
 * @brief Bind specified modalities into unified percept
 *
 * WHAT: Create bound percept from multiple modalities
 * WHY:  Enable explicit binding requests (vs automatic)
 * HOW:  Check coherence, create percept if threshold met
 *
 * @param claustrum System
 * @param modality_mask Bitmask of modalities to bind
 * @param[out] percept_id ID of created percept (or existing if already bound)
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_bind_modalities(
    nimcp_claustrum_t* claustrum,
    uint32_t modality_mask,
    uint32_t* percept_id
);

/**
 * @brief Get bound percept by ID
 *
 * WHAT: Retrieve bound percept information
 * WHY:  Inspect binding results
 * HOW:  Find percept by ID, copy to output
 *
 * @param claustrum System
 * @param percept_id Percept ID
 * @param[out] percept Output percept structure
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_percept(
    const nimcp_claustrum_t* claustrum,
    uint32_t percept_id,
    nimcp_claustrum_bound_percept_t* percept
);

/**
 * @brief Get strongest current binding
 *
 * WHAT: Find the most strongly bound percept
 * WHY:  Identify dominant unified percept
 * HOW:  Search percepts by binding strength
 *
 * @param claustrum System
 * @param[out] percept_id ID of strongest percept
 * @param[out] strength Binding strength of that percept
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_strongest_binding(
    const nimcp_claustrum_t* claustrum,
    uint32_t* percept_id,
    float* strength
);

/**
 * @brief Release bound percept
 *
 * WHAT: Explicitly unbind a percept
 * WHY:  Allow external control of binding lifetime
 * HOW:  Mark percept as invalid, free resources
 *
 * @param claustrum System
 * @param percept_id Percept to release
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_release_percept(
    nimcp_claustrum_t* claustrum,
    uint32_t percept_id
);

/*=============================================================================
 * SYNCHRONIZATION API
 *===========================================================================*/

/**
 * @brief Synchronize modalities for binding
 *
 * WHAT: Trigger temporal synchronization of active modalities
 * WHY:  Binding requires temporal coherence (Crick's hypothesis)
 * HOW:  Align oscillation phases, compute coherence
 *
 * @param claustrum System
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_synchronize(
    nimcp_claustrum_t* claustrum
);

/**
 * @brief Get current synchronization level
 *
 * WHAT: Query global synchronization coherence
 * WHY:  Monitor binding readiness
 * HOW:  Return oscillator global coherence
 *
 * @param claustrum System
 * @param[out] coherence Global coherence [0,1]
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_sync_level(
    const nimcp_claustrum_t* claustrum,
    float* coherence
);

/**
 * @brief Set gamma oscillation parameters
 *
 * WHAT: Modulate gamma oscillations for binding
 * WHY:  Gamma controls feature binding strength
 * HOW:  Update oscillator parameters
 *
 * @param claustrum System
 * @param frequency Gamma frequency (Hz)
 * @param amplitude Gamma amplitude [0,1]
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_set_gamma(
    nimcp_claustrum_t* claustrum,
    float frequency,
    float amplitude
);

/**
 * @brief Set alpha oscillation parameters
 *
 * WHAT: Modulate alpha oscillations for gating
 * WHY:  Alpha controls attention gating
 * HOW:  Update oscillator parameters
 *
 * @param claustrum System
 * @param frequency Alpha frequency (Hz)
 * @param amplitude Alpha amplitude [0,1]
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_set_alpha(
    nimcp_claustrum_t* claustrum,
    float frequency,
    float amplitude
);

/*=============================================================================
 * SALIENCE AND ATTENTION API
 *===========================================================================*/

/**
 * @brief Detect salience across modalities
 *
 * WHAT: Compute salience map across all modalities
 * WHY:  Identify what should be attended/bound
 * HOW:  Evaluate novelty, relevance, and intensity
 *
 * @param claustrum System
 * @param[out] salience_out Overall salience [0,1]
 * @param[out] salient_modality Most salient modality
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_detect_salience(
    nimcp_claustrum_t* claustrum,
    float* salience_out,
    nimcp_claustrum_modality_t* salient_modality
);

/**
 * @brief Set attention bias for region
 *
 * WHAT: Modulate attention for a cortical region
 * WHY:  Allow top-down attention control
 * HOW:  Update cortical link attention bias
 *
 * @param claustrum System
 * @param region Target region
 * @param bias Attention bias [0,1]
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_set_attention_bias(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_region_t region,
    float bias
);

/**
 * @brief Get current attention state
 *
 * WHAT: Query current attention configuration
 * WHY:  Monitor attention allocation
 * HOW:  Copy attention state to output
 *
 * @param claustrum System
 * @param[out] global_attention Overall attention [0,1]
 * @param[out] spatial_attention Spatial attention (x,y,z) array of 3
 * @param[out] feature_attention Feature attention array of 8
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_attention(
    const nimcp_claustrum_t* claustrum,
    float* global_attention,
    float* spatial_attention,
    float* feature_attention
);

/*=============================================================================
 * GLOBAL WORKSPACE API
 *===========================================================================*/

/**
 * @brief Gate access to global workspace
 *
 * WHAT: Control what enters conscious awareness
 * WHY:  Implement GWT workspace access gating
 * HOW:  Evaluate salience, coherence; allow/deny access
 *
 * @param claustrum System
 * @param percept_id Percept requesting access
 * @param[out] granted Whether access was granted
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_gate_workspace(
    nimcp_claustrum_t* claustrum,
    uint32_t percept_id,
    bool* granted
);

/**
 * @brief Broadcast to global workspace
 *
 * WHAT: Broadcast content to all receiving systems
 * WHY:  Enable conscious awareness propagation
 * HOW:  Trigger callbacks, update workspace state
 *
 * @param claustrum System
 * @param content Content to broadcast
 * @param content_size Size of content
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_broadcast_workspace(
    nimcp_claustrum_t* claustrum,
    const void* content,
    size_t content_size
);

/**
 * @brief Get workspace occupancy
 *
 * WHAT: Check if workspace is currently occupied
 * WHY:  Coordinate workspace access
 * HOW:  Return workspace state
 *
 * @param claustrum System
 * @param[out] occupied Whether workspace is occupied
 * @param[out] percept_id ID of occupying percept (if occupied)
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_workspace_state(
    const nimcp_claustrum_t* claustrum,
    bool* occupied,
    uint32_t* percept_id
);

/*=============================================================================
 * TASK SWITCHING API
 *===========================================================================*/

/**
 * @brief Initiate brain state switch
 *
 * WHAT: Begin transition to new brain state
 * WHY:  Enable task switching (Crick's reset hypothesis)
 * HOW:  Set target state, begin transition process
 *
 * @param claustrum System
 * @param target_state Target brain state
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_switch_state(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_brain_state_t target_state
);

/**
 * @brief Get current brain state
 *
 * WHAT: Query current brain state
 * WHY:  Monitor state for coordination
 * HOW:  Return current brain state enum
 *
 * @param claustrum System
 * @return Current brain state
 */
NIMCP_EXPORT nimcp_claustrum_brain_state_t nimcp_claustrum_get_brain_state(
    const nimcp_claustrum_t* claustrum
);

/**
 * @brief Get state switch progress
 *
 * WHAT: Query progress of ongoing state switch
 * WHY:  Monitor transition for timing-sensitive operations
 * HOW:  Return progress [0,1] (1 = complete)
 *
 * @param claustrum System
 * @param[out] progress Switch progress [0,1]
 * @param[out] target Target state of switch
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_switch_progress(
    const nimcp_claustrum_t* claustrum,
    float* progress,
    nimcp_claustrum_brain_state_t* target
);

/*=============================================================================
 * CORTICAL COORDINATION API
 *===========================================================================*/

/**
 * @brief Update cortical region link
 *
 * WHAT: Set activity level for a cortical region
 * WHY:  Receive cortical input for coordination
 * HOW:  Update cortical link state
 *
 * @param claustrum System
 * @param region Target region
 * @param activity Activity level [0,1]
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_update_cortical_region(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_region_t region,
    float activity
);

/**
 * @brief Get cortical region link state
 *
 * WHAT: Query state of cortical link
 * WHY:  Inspect coordination state
 * HOW:  Copy link state to output
 *
 * @param claustrum System
 * @param region Target region
 * @param[out] link Output link state
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_cortical_link(
    const nimcp_claustrum_t* claustrum,
    nimcp_claustrum_region_t region,
    nimcp_claustrum_cortical_link_t* link
);

/**
 * @brief Set cortical link strengths
 *
 * WHAT: Configure bidirectional link strength
 * WHY:  Model varying connectivity patterns
 * HOW:  Update forward and backward weights
 *
 * @param claustrum System
 * @param region Target region
 * @param forward_strength Forward (claustrum->region) strength
 * @param backward_strength Backward (region->claustrum) strength
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_set_cortical_link_strength(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_region_t region,
    float forward_strength,
    float backward_strength
);

/*=============================================================================
 * STATE AND METRICS API
 *===========================================================================*/

/**
 * @brief Get current state
 *
 * @param claustrum System
 * @return Current operational state
 */
NIMCP_EXPORT nimcp_claustrum_state_t nimcp_claustrum_get_state(
    const nimcp_claustrum_t* claustrum
);

/**
 * @brief Get current status
 *
 * @param claustrum System
 * @return Current health status
 */
NIMCP_EXPORT nimcp_claustrum_status_t nimcp_claustrum_get_status(
    const nimcp_claustrum_t* claustrum
);

/**
 * @brief Get metrics
 *
 * @param claustrum System
 * @param[out] metrics Metrics output
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_get_metrics(
    const nimcp_claustrum_t* claustrum,
    nimcp_claustrum_metrics_t* metrics
);

/**
 * @brief Reset metrics
 *
 * @param claustrum System
 * @return CLAUSTRUM_OK on success
 */
NIMCP_EXPORT nimcp_claustrum_error_t nimcp_claustrum_reset_metrics(
    nimcp_claustrum_t* claustrum
);

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

/**
 * @brief Get error string
 *
 * @param error Error code
 * @return Human-readable string
 */
NIMCP_EXPORT const char* nimcp_claustrum_error_string(
    nimcp_claustrum_error_t error
);

/**
 * @brief Get modality name
 *
 * @param modality Modality type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_claustrum_modality_string(
    nimcp_claustrum_modality_t modality
);

/**
 * @brief Get state name
 *
 * @param state State type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_claustrum_state_string(
    nimcp_claustrum_state_t state
);

/**
 * @brief Get brain state name
 *
 * @param state Brain state type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_claustrum_brain_state_string(
    nimcp_claustrum_brain_state_t state
);

/**
 * @brief Get region name
 *
 * @param region Region type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_claustrum_region_string(
    nimcp_claustrum_region_t region
);

/**
 * @brief Get bio message type name
 *
 * @param msg_type Message type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_claustrum_bio_msg_type_string(
    nimcp_claustrum_bio_msg_type_t msg_type
);

/**
 * @brief Get consciousness level name
 *
 * @param level Consciousness level
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_claustrum_consciousness_string(
    nimcp_claustrum_consciousness_level_t level
);

/**
 * @brief Print claustrum summary to stdout
 *
 * @param claustrum System (NULL-safe)
 */
NIMCP_EXPORT void nimcp_claustrum_print_summary(
    const nimcp_claustrum_t* claustrum
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CLAUSTRUM_H */
