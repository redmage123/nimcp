/**
 * @file nimcp_glymphatic.h
 * @brief Glymphatic System - Brain Waste Clearance Module
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Glymphatic waste clearance system modeling
 * WHY:  The glymphatic system clears metabolic waste (beta-amyloid, tau, etc.)
 *       from the brain during sleep via CSF-driven perivascular drainage.
 *       Impaired clearance is linked to neurodegeneration.
 * HOW:  Model CSF flow, AQP4-mediated clearance, sleep-state modulation,
 *       and waste accumulation/removal dynamics.
 *
 * KEY CONCEPTS:
 * - CSF Flow: Cerebrospinal fluid drives waste clearance through perivascular channels
 * - AQP4 (Aquaporin-4): Water channels on astrocytic endfeet that facilitate CSF exchange
 * - Sleep Modulation: Interstitial space expands ~60% during NREM sleep, boosting clearance
 * - Waste Markers: Beta-amyloid, tau protein, and generic metabolic waste tracked separately
 * - State Machine: INACTIVE -> PRIMING -> ACTIVE -> FLUSHING lifecycle
 *
 * BIOLOGICAL BASIS:
 * - Discovered by Nedergaard lab (2012), the glymphatic system is a brain-wide
 *   waste clearance pathway dependent on astrocytic AQP4 water channels
 * - Clearance is ~10-20x more efficient during NREM sleep than wakefulness
 * - Norepinephrine (via LC) suppresses clearance during wake; its reduction
 *   during sleep allows interstitial space expansion
 * - Impaired glymphatic function is implicated in Alzheimer's, Parkinson's,
 *   and traumatic brain injury
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GLYMPHATIC_H
#define NIMCP_GLYMPHATIC_H

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
 * Sleep State Constants (self-contained, no external enum dependency)
 *===========================================================================*/

#define GLYM_SLEEP_AWAKE     0
#define GLYM_SLEEP_LIGHT     1
#define GLYM_SLEEP_DEEP_NREM 2
#define GLYM_SLEEP_REM       3

/*=============================================================================
 * System Constants
 *===========================================================================*/

/** Magic number for validity checks */
#define GLYM_MAGIC               0x6C7940A7U

/** Default base clearance rate (fraction per second) */
#define GLYM_DEFAULT_BASE_CLEARANCE   0.001f

/** Default waste generation rate (per second while awake) */
#define GLYM_DEFAULT_WASTE_GEN_RATE   0.005f

/** Default AQP4 expression level */
#define GLYM_DEFAULT_AQP4_EXPRESSION  0.7f

/** NREM clearance multiplier (biological: ~10-20x) */
#define GLYM_DEFAULT_NREM_MULTIPLIER  15.0f

/** REM clearance multiplier (intermediate) */
#define GLYM_DEFAULT_REM_MULTIPLIER   3.0f

/** Awake clearance multiplier (minimal) */
#define GLYM_DEFAULT_AWAKE_MULTIPLIER 0.1f

/** Waste alert threshold (fraction; triggers immune notification) */
#define GLYM_DEFAULT_WASTE_ALERT      0.75f

/** Interstitial space expansion during NREM (60%) */
#define GLYM_ISV_NREM_EXPANSION       1.6f

/** Priming warmup duration (seconds) */
#define GLYM_PRIMING_DURATION_S       30.0f

/** Flushing threshold (waste below this -> FLUSHING state) */
#define GLYM_FLUSHING_THRESHOLD       0.1f

/** Beta-amyloid half-life relative multiplier */
#define GLYM_BETA_AMYLOID_HALFLIFE    1.0f

/** Tau protein half-life relative multiplier (slower clearance) */
#define GLYM_TAU_HALFLIFE             1.5f

/** Generic metabolic waste half-life relative multiplier (fastest clearance) */
#define GLYM_METABOLIC_HALFLIFE       0.7f

/** Healthy fractal dimension for vasculature */
#define GLYM_HEALTHY_FRACTAL_DIM      1.7f

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Glymphatic system operational state
 */
typedef enum {
    GLYM_INACTIVE = 0,   /**< System idle (awake, minimal clearance) */
    GLYM_PRIMING,        /**< Warming up after sleep onset detected */
    GLYM_ACTIVE,         /**< Full clearance active (deep sleep) */
    GLYM_FLUSHING        /**< Final flush, waste nearly cleared */
} glymphatic_state_t;

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct glymphatic_config  glymphatic_config_t;
typedef struct glymphatic_system  glymphatic_system_t;

/* Thread-layer mutex (nimcp_mutex_t* from utils/thread/nimcp_thread.h)
 * Declared as void* to avoid header dependency in public API. */

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Glymphatic system configuration
 */
struct glymphatic_config {
    float base_clearance_rate;        /**< Base clearance rate (fraction/s) */
    float waste_generation_rate;      /**< Waste accumulation rate while awake (fraction/s) */
    float aqp4_expression;            /**< AQP4 channel expression (0.0-1.0) */
    float nrem_clearance_multiplier;  /**< Clearance multiplier during NREM sleep */
    float rem_clearance_multiplier;   /**< Clearance multiplier during REM sleep */
    float awake_clearance_multiplier; /**< Clearance multiplier while awake */
    float waste_alert_threshold;      /**< Threshold for immune/inflammation alert */
};

/**
 * @brief Main glymphatic system structure
 */
struct glymphatic_system {
    uint32_t magic;                   /**< Validity marker: GLYM_MAGIC */

    glymphatic_config_t config;       /**< Configuration */
    glymphatic_state_t state;         /**< Current operational state */

    /* Core clearance dynamics */
    float clearance_rate;             /**< Current clearance rate (0.0-1.0), sleep-modulated */
    float waste_accumulation;         /**< Total waste level (0.0-1.0), rises during wake */
    float csf_flow_rate;              /**< CSF flow rate (mL/min equivalent, normalized) */
    float aquaporin4_expression;      /**< AQP4 channel expression (0.0-1.0) */
    float interstitial_space_volume;  /**< Relative volume, expands 60% during sleep */

    /* Individual waste markers */
    float beta_amyloid_level;         /**< Beta-amyloid accumulation (0.0-1.0) */
    float tau_protein_level;          /**< Tau protein accumulation (0.0-1.0) */
    float metabolic_waste_level;      /**< Generic metabolic waste (0.0-1.0) */

    /* Biophysical parameters */
    float fractal_dimension;          /**< Vascular fractal dimension (~1.7 healthy) */
    float quantum_tunneling_rate;     /**< Quantum tunneling contribution (speculative) */

    /* Sleep state tracking */
    uint32_t current_sleep_state;     /**< Uses GLYM_SLEEP_* constants */

    /* Timing */
    uint64_t last_update_us;          /**< Last update timestamp (microseconds) */
    float priming_elapsed_s;          /**< Time spent in PRIMING state */

    /* Synchronization (nimcp_mutex_t* from utils/thread/nimcp_thread.h) */
    void* lock;                       /**< Thread safety mutex */
};

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Get default glymphatic configuration
 * @return Default configuration with biologically-inspired parameters
 */
NIMCP_EXPORT glymphatic_config_t glymphatic_default_config(void);

/**
 * @brief Create a new glymphatic system
 *
 * WHAT: Allocates and initializes the glymphatic waste clearance system
 * WHY:  Provides metabolic waste management for the brain
 * HOW:  Allocates struct, initializes mutex, sets defaults from config
 *
 * @param config Configuration (NULL for defaults)
 * @return New system or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Safe (no shared state during creation)
 */
NIMCP_EXPORT glymphatic_system_t* glymphatic_create(const glymphatic_config_t* config);

/**
 * @brief Destroy a glymphatic system and release all resources
 *
 * @param system System to destroy (NULL-safe)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Safe (final call, no concurrent access expected)
 */
NIMCP_EXPORT void glymphatic_destroy(glymphatic_system_t* system);

/*=============================================================================
 * Core Operations API
 *===========================================================================*/

/**
 * @brief Update glymphatic system for one timestep
 *
 * WHAT: Steps the glymphatic simulation forward by dt_s seconds
 * WHY:  Drives waste accumulation, clearance, state transitions, and CSF flow
 * HOW:  1. Generate waste proportional to wake state
 *       2. Compute clearance rate from sleep state + AQP4
 *       3. Clear waste markers with different half-lives
 *       4. Update state machine (INACTIVE/PRIMING/ACTIVE/FLUSHING)
 *       5. Model interstitial space volume changes
 *
 * @param system The glymphatic system
 * @param dt_s   Time delta in seconds (must be > 0)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Uses internal locking
 */
NIMCP_EXPORT int glymphatic_update(glymphatic_system_t* system, float dt_s);

/**
 * @brief Notify system of sleep state change
 *
 * WHAT: Updates glymphatic response to new sleep/wake state
 * WHY:  Sleep state is the primary modulator of clearance efficiency
 * HOW:  Updates internal sleep state, triggers state machine transitions
 *
 * @param system      The glymphatic system
 * @param sleep_state New sleep state (GLYM_SLEEP_* constant)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Uses internal locking
 */
NIMCP_EXPORT int glymphatic_on_sleep_state_change(glymphatic_system_t* system,
                                                   uint32_t sleep_state);

/**
 * @brief Force an immediate waste flush cycle
 *
 * WHAT: Triggers accelerated waste clearance
 * WHY:  Emergency clearance when waste levels are dangerously high
 * HOW:  Sets state to FLUSHING, boosts clearance rate temporarily
 *
 * @param system The glymphatic system
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Uses internal locking
 */
NIMCP_EXPORT int glymphatic_flush(glymphatic_system_t* system);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Get current clearance rate
 * @param system The glymphatic system
 * @return Clearance rate (0.0-1.0) or -1.0f on error
 */
NIMCP_EXPORT float glymphatic_get_clearance_rate(const glymphatic_system_t* system);

/**
 * @brief Get overall waste accumulation level
 * @param system The glymphatic system
 * @return Waste level (0.0-1.0) or -1.0f on error
 */
NIMCP_EXPORT float glymphatic_get_waste_level(const glymphatic_system_t* system);

/**
 * @brief Get current operational state
 * @param system The glymphatic system
 * @return Current state, or GLYM_INACTIVE on error
 */
NIMCP_EXPORT glymphatic_state_t glymphatic_get_state(const glymphatic_system_t* system);

/**
 * @brief Get CSF flow rate
 * @param system The glymphatic system
 * @return CSF flow rate (normalized) or 0.0f on error
 */
NIMCP_EXPORT float glymphatic_get_csf_flow(const glymphatic_system_t* system);

/**
 * @brief Get beta-amyloid level
 * @param system The glymphatic system
 * @return Level (0.0-1.0) or -1.0f on error
 */
NIMCP_EXPORT float glymphatic_get_beta_amyloid(const glymphatic_system_t* system);

/**
 * @brief Get tau protein level
 * @param system The glymphatic system
 * @return Level (0.0-1.0) or -1.0f on error
 */
NIMCP_EXPORT float glymphatic_get_tau_level(const glymphatic_system_t* system);

/**
 * @brief Get interstitial space volume (relative)
 * @param system The glymphatic system
 * @return Relative volume (1.0 = baseline) or 0.0f on error
 */
NIMCP_EXPORT float glymphatic_get_interstitial_volume(const glymphatic_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GLYMPHATIC_H */
