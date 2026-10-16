/**
 * @file nimcp_introspection_substrate_bridge.h
 * @brief Bridge between neural substrate and introspection module
 *
 * WHAT: Bidirectional integration between neural substrate metabolic state
 *       and introspection capabilities (consciousness metrics, metacognition,
 *       self-awareness, uncertainty estimation)
 *
 * WHY: Introspection is metabolically expensive, requiring sustained prefrontal-
 *      medial cortex engagement. ATP depletion impairs self-awareness depth,
 *      metacognitive accuracy, and internal state monitoring. This bridge
 *      models how metabolic constraints shape metacognitive capacity.
 *
 * HOW: Monitors substrate ATP levels, fatigue state, and metabolic stress to
 *      compute effects on introspection parameters. Provides modulation of
 *      self-awareness depth, metacognitive accuracy, monitoring capacity, and
 *      uncertainty estimation based on metabolic availability.
 *
 * BIOLOGICAL BASIS:
 * - Introspection requires sustained activity in:
 *   * Dorsomedial prefrontal cortex (dmPFC) - self-referential processing
 *   * Anterior cingulate cortex (ACC) - performance monitoring
 *   * Posterior cingulate cortex (PCC) - self-awareness
 *   * Insula - interoceptive awareness
 *
 * - ATP depletion effects:
 *   * Reduced self-awareness depth (diminished sense of self)
 *   * Impaired metacognitive accuracy (poor self-assessment)
 *   * Limited monitoring capacity (decreased introspective access)
 *   * Degraded uncertainty estimation (overconfident/underconfident)
 *
 * - Fatigue progression:
 *   * Mild: Subtle reduction in metacognitive precision
 *   * Moderate: Noticeable impairment in self-monitoring
 *   * Severe: Loss of metacognitive insight, poor self-assessment
 *   * Critical: Minimal introspective capacity, impaired self-awareness
 *
 * @author NIMCP Development Team
 * @date 2024-12
 */

#ifndef NIMCP_INTROSPECTION_SUBSTRATE_BRIDGE_H
#define NIMCP_INTROSPECTION_SUBSTRATE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


/* Forward declaration for introspection system (opaque pointer) */
typedef struct nimcp_introspection nimcp_introspection_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * Bio-async module ID for introspection substrate bridge
 * Range: 0x1200-0x12FF (Substrate bridges)
 */
#define BIO_MODULE_SUBSTRATE_INTROSPECTION 0x1207

/**
 * ATP thresholds for introspection function
 *
 * Introspection is metabolically demanding due to sustained prefrontal-medial
 * cortex activity. Different levels of self-awareness and metacognition require
 * different ATP availability.
 */
#define INTROSPECTION_SUBSTRATE_ATP_CRITICAL_THRESHOLD 0.15f  /**< Below: Minimal introspection */
#define INTROSPECTION_SUBSTRATE_ATP_LOW_THRESHOLD 0.30f       /**< Below: Impaired metacognition */
#define INTROSPECTION_SUBSTRATE_ATP_MODERATE_THRESHOLD 0.50f  /**< Below: Reduced self-awareness */
#define INTROSPECTION_SUBSTRATE_ATP_OPTIMAL_THRESHOLD 0.70f   /**< Above: Full introspective capacity */

/**
 * Fatigue sensitivity factors
 *
 * How much different introspection components degrade with fatigue.
 * Higher values = more sensitive to metabolic depletion.
 */
#define INTROSPECTION_SUBSTRATE_SELF_AWARENESS_FATIGUE_FACTOR 0.7f    /**< Self-awareness depth */
#define INTROSPECTION_SUBSTRATE_METACOGNITION_FATIGUE_FACTOR 0.8f      /**< Metacognitive accuracy */
#define INTROSPECTION_SUBSTRATE_MONITORING_FATIGUE_FACTOR 0.6f         /**< Internal monitoring */
#define INTROSPECTION_SUBSTRATE_UNCERTAINTY_FATIGUE_FACTOR 0.5f        /**< Uncertainty estimation */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * Substrate effects on introspection parameters
 *
 * WHAT: Computed effects of metabolic state on introspection capabilities
 *
 * WHY: Introspection quality depends on sustained metabolic support for
 *      prefrontal-medial cortex networks. ATP depletion degrades metacognitive
 *      accuracy and self-awareness depth.
 *
 * HOW: Values computed from ATP levels, fatigue state, and metabolic stress.
 *      Applied to introspection module to modulate consciousness metrics,
 *      temporal pattern detection, and uncertainty estimation.
 */
typedef struct {
    /* Core introspection effects */
    float self_awareness_depth;      /**< [0-1] Depth of self-reflective processing */
    float metacognitive_accuracy;    /**< [0-1] Accuracy of self-assessment and monitoring */
    float monitoring_capacity;       /**< [0-1] Capacity for internal state monitoring */
    float uncertainty_estimation;    /**< [0-1] Quality of epistemic uncertainty tracking */

    /* State flags */
    bool is_impaired;                /**< True if introspection is significantly impaired */
} introspection_substrate_effects_t;

/**
 * Configuration for introspection substrate bridge
 *
 * WHAT: Parameters controlling how substrate affects introspection
 *
 * WHY: Different introspection tasks have different metabolic demands and
 *      sensitivities to ATP depletion. Configuration allows tuning of these
 *      relationships.
 *
 * HOW: Enable/disable specific substrate effects and set sensitivity factors
 *      for how strongly metabolic state influences introspection parameters.
 */
typedef struct {
    /* Feature enables */
    bool enable_atp_modulation;           /**< Enable ATP-based modulation */
    bool enable_fatigue_effects;          /**< Enable fatigue-based degradation */
    bool enable_metabolic_monitoring;     /**< Enable metabolic stress tracking */
    bool enable_bio_async;                /**< Enable bio-async messaging */

    /* Sensitivity factors [0-1] */
    float atp_sensitivity;                /**< How strongly ATP affects introspection */
    float fatigue_sensitivity;            /**< How strongly fatigue affects introspection */
    float recovery_rate;                  /**< How quickly introspection recovers */

    /* Thresholds */
    float impairment_threshold;           /**< Below this, introspection is impaired */
    float critical_threshold;             /**< Below this, introspection is critical */
} introspection_substrate_config_t;

/**
 * Statistics for introspection substrate bridge
 *
 * WHAT: Tracking metrics for substrate-introspection interactions
 *
 * WHY: Monitor metabolic impact on metacognition over time, detect patterns
 *      of impairment, and track recovery dynamics.
 *
 * HOW: Accumulate counts and compute running statistics during bridge updates.
 */
typedef struct {
    /* Update tracking */
    uint64_t update_count;                /**< Number of substrate updates processed */
    uint64_t impairment_count;            /**< Number of times introspection was impaired */
    uint64_t critical_count;              /**< Number of times introspection was critical */

    /* Metabolic tracking */
    float min_atp_observed;               /**< Minimum ATP level observed */
    float max_atp_observed;               /**< Maximum ATP level observed */
    float avg_atp;                        /**< Running average ATP level */

    /* Effect tracking */
    float min_self_awareness;             /**< Minimum self-awareness depth observed */
    float max_self_awareness;             /**< Maximum self-awareness depth observed */
    float avg_metacognitive_accuracy;     /**< Average metacognitive accuracy */

    /* Recovery tracking */
    uint64_t recovery_count;              /**< Number of recovery events */
    float avg_recovery_time;              /**< Average time to recover from impairment */
} introspection_substrate_stats_t;

/**
 * Introspection substrate bridge
 *
 * WHAT: Complete bridge between neural substrate and introspection module
 *
 * WHY: Integrates metabolic constraints with metacognitive processing to model
 *      how energy availability shapes self-awareness, metacognitive accuracy,
 *      and uncertainty estimation.
 *
 * HOW: Monitors substrate state, computes effects on introspection parameters,
 *      applies modulation to introspection module, and tracks statistics.
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Component pointers */
    neural_substrate_t* substrate;        /**< Neural substrate being monitored */
    nimcp_introspection_t* introspection; /**< Introspection module being modulated */

    /* Configuration and state */
    introspection_substrate_config_t config;  /**< Bridge configuration */
    introspection_substrate_effects_t effects; /**< Current substrate effects */
    introspection_substrate_stats_t stats;     /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;

    } introspection_substrate_bridge_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * Initialize default configuration
 *
 * WHAT: Sets default parameters for introspection substrate bridge
 * WHY: Provides sensible defaults based on biological constraints
 * HOW: Initializes all config fields with empirically-tuned values
 *
 * @param config Configuration to initialize
 */
void introspection_substrate_default_config(introspection_substrate_config_t* config);

/**
 * Create introspection substrate bridge
 *
 * WHAT: Allocates and initializes bridge between substrate and introspection
 * WHY: Establishes bidirectional integration for metabolic-metacognitive coupling
 * HOW: Creates bridge struct, connects components, initializes bio-async if enabled
 *
 * @param config Bridge configuration (if NULL, uses defaults)
 * @param substrate Neural substrate to monitor
 * @param introspection Introspection module to modulate
 * @return Initialized bridge, or NULL on failure
 *
 * BIOLOGICAL BASIS: Prefrontal-medial cortex introspection networks require
 *                   sustained metabolic support. Bridge models this dependency.
 */
introspection_substrate_bridge_t* introspection_substrate_bridge_create(
    const introspection_substrate_config_t* config,
    neural_substrate_t* substrate,
    nimcp_introspection_t* introspection
);

/**
 * Destroy introspection substrate bridge
 *
 * WHAT: Cleans up and deallocates bridge
 * WHY: Prevents memory leaks and disconnects components
 * HOW: Disconnects bio-async, destroys mutex, frees memory
 *
 * @param bridge Bridge to destroy
 */
void introspection_substrate_bridge_destroy(introspection_substrate_bridge_t* bridge);

/**
 * Connect bridge to bio-async router
 *
 * WHAT: Registers bridge as bio-async module for inter-module messaging
 * WHY: Enables communication with other substrate bridges and modules
 * HOW: Registers with BIO_MODULE_SUBSTRATE_INTROSPECTION ID
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative on failure
 */
int introspection_substrate_connect_bio_async(introspection_substrate_bridge_t* bridge);

/**
 * Disconnect bridge from bio-async router
 *
 * WHAT: Unregisters bridge from bio-async system
 * WHY: Clean shutdown and resource cleanup
 * HOW: Unregisters module and clears bio-async state
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative on failure
 */
int introspection_substrate_disconnect_bio_async(introspection_substrate_bridge_t* bridge);

/**
 * Check if bridge is connected to bio-async
 *
 * WHAT: Query bio-async connection status
 * WHY: Verify communication capability before sending messages
 * HOW: Checks bio_async_enabled flag
 *
 * @param bridge Bridge to query
 * @return True if connected, false otherwise
 */
bool introspection_substrate_is_bio_async_connected(const introspection_substrate_bridge_t* bridge);

/**
 * Update substrate effects on introspection
 *
 * WHAT: Computes current substrate effects on introspection parameters
 * WHY: Keeps introspection modulation synchronized with metabolic state
 * HOW: Reads substrate ATP, fatigue, stress; computes effects; updates stats
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative on failure
 *
 * BIOLOGICAL BASIS:
 * - ATP depletion reduces self-awareness depth exponentially
 * - Fatigue impairs metacognitive accuracy and monitoring capacity
 * - Metabolic stress degrades uncertainty estimation quality
 */
int introspection_substrate_update(introspection_substrate_bridge_t* bridge);

/**
 * Get current self-awareness depth
 *
 * WHAT: Retrieves metabolically-modulated self-awareness depth
 * WHY: Self-awareness requires sustained prefrontal-medial cortex activity
 * HOW: Returns current effect value from bridge state
 *
 * @param bridge Bridge to query
 * @return Self-awareness depth [0-1], or 0.0 on failure
 *
 * BIOLOGICAL BASIS: dmPFC and PCC activity (self-referential processing)
 *                   degrades with ATP depletion, reducing introspective depth.
 */
float introspection_substrate_get_self_awareness_depth(
    const introspection_substrate_bridge_t* bridge
);

/**
 * Get current metacognitive accuracy
 *
 * WHAT: Retrieves metabolically-modulated metacognitive accuracy
 * WHY: Accurate self-assessment requires sustained ACC monitoring activity
 * HOW: Returns current effect value from bridge state
 *
 * @param bridge Bridge to query
 * @return Metacognitive accuracy [0-1], or 0.0 on failure
 *
 * BIOLOGICAL BASIS: Anterior cingulate cortex (performance monitoring) is
 *                   highly sensitive to metabolic depletion, impairing
 *                   metacognitive accuracy early in fatigue progression.
 */
float introspection_substrate_get_metacognitive_accuracy(
    const introspection_substrate_bridge_t* bridge
);

/**
 * Get current monitoring capacity
 *
 * WHAT: Retrieves metabolically-modulated internal state monitoring capacity
 * WHY: Monitoring internal states requires sustained interoceptive processing
 * HOW: Returns current effect value from bridge state
 *
 * @param bridge Bridge to query
 * @return Monitoring capacity [0-1], or 0.0 on failure
 *
 * BIOLOGICAL BASIS: Insula (interoceptive awareness) and ACC (state monitoring)
 *                   degrade with fatigue, limiting introspective access to
 *                   internal states.
 */
float introspection_substrate_get_monitoring_capacity(
    const introspection_substrate_bridge_t* bridge
);

/**
 * Get current uncertainty estimation quality
 *
 * WHAT: Retrieves metabolically-modulated uncertainty estimation quality
 * WHY: Epistemic uncertainty tracking requires sustained metacognitive processing
 * HOW: Returns current effect value from bridge state
 *
 * @param bridge Bridge to query
 * @return Uncertainty estimation quality [0-1], or 0.0 on failure
 *
 * BIOLOGICAL BASIS: Fatigue impairs calibration of confidence judgments,
 *                   leading to overconfidence (low ATP) or underconfidence
 *                   (moderate depletion) in uncertainty estimation.
 */
float introspection_substrate_get_uncertainty_estimation(
    const introspection_substrate_bridge_t* bridge
);

/**
 * Get current substrate effects
 *
 * WHAT: Retrieves all substrate effects on introspection
 * WHY: Provides complete view of metabolic modulation
 * HOW: Returns copy of effects struct
 *
 * @param bridge Bridge to query
 * @param effects Output buffer for effects (must be non-NULL)
 * @return 0 on success, negative on failure
 */
int introspection_substrate_get_effects(
    const introspection_substrate_bridge_t* bridge,
    introspection_substrate_effects_t* effects
);

/**
 * Check if introspection is impaired
 *
 * WHAT: Determines if metabolic state has significantly impaired introspection
 * WHY: Critical state requires intervention or reduced introspective demands
 * HOW: Checks if self-awareness or metacognition below impairment threshold
 *
 * @param bridge Bridge to query
 * @return True if introspection is impaired, false otherwise
 *
 * BIOLOGICAL BASIS: Severe ATP depletion causes loss of metacognitive insight,
 *                   diminished self-awareness, and impaired introspective access.
 */
bool introspection_substrate_is_impaired(introspection_substrate_bridge_t* bridge);

/**
 * Get bridge statistics
 *
 * WHAT: Retrieves accumulated statistics about substrate-introspection interactions
 * WHY: Monitor metabolic impact on metacognition, detect patterns, track recovery
 * HOW: Returns copy of stats struct
 *
 * @param bridge Bridge to query
 * @param stats Output buffer for statistics (must be non-NULL)
 * @return 0 on success, negative on failure
 */
int introspection_substrate_get_stats(
    const introspection_substrate_bridge_t* bridge,
    introspection_substrate_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTROSPECTION_SUBSTRATE_BRIDGE_H */
