/**
 * @file nimcp_normalization_immune.h
 * @brief Normalization-Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integration layer connecting normalization modules (adaptive, homeostatic,
 *       z-score, min-max) with the brain immune system for fever-induced baseline
 *       shifts and anomaly detection.
 * WHY:  Inflammatory states alter physiological set-points (fever shifts baselines);
 *       out-of-range normalized values indicate anomalies that trigger immune
 *       surveillance.
 * HOW:  Immune inflammation shifts normalization baselines/thresholds; normalizers
 *       detect statistical outliers and report as antigens to immune system.
 *
 * BIOLOGICAL ANALOGY:
 * ```
 * PHYSIOLOGICAL STATE     IMMUNE RESPONSE
 * ────────────────────────────────────────────────────────────────
 * Fever                → Elevated baseline (homeostatic shift)
 * Inflammation         → Altered variance thresholds
 * Cytokines (IL-1,IL-6)→ Mean/stddev modulation
 * Out-of-range values  → Antigen presentation
 * Sustained outliers   → Inflammation escalation
 * Resolution           → Baseline restoration
 * ```
 *
 * INTEGRATION PATTERNS:
 * 1. Inflammation-to-Baseline: Fever shifts normalization means/targets
 * 2. Cytokine-to-Variance: IL-1/IL-6 increase acceptable variance
 * 3. Outlier-to-Antigen: Statistical outliers trigger immune surveillance
 * 4. Storm-to-Protection: Cytokine storm triggers protective normalization clamps
 * 5. Resolution-to-Restore: IL-10 gradually restores normal baselines
 *
 * USE CASES:
 * - Fever modeling (elevated baselines during inflammation)
 * - Anomaly detection via z-score outliers (>3σ triggers immune alert)
 * - Homeostatic adaptation under immune stress
 * - Protective clamping during cytokine storm
 * - Gradual normalization restoration after threat resolution
 *
 * NORMALIZATION MODULE INTERACTIONS:
 * - Adaptive normalizer: Learning rate modulated by cytokines
 * - Homeostatic normalizer: Target activity shifted by inflammation
 * - Z-score normalizer: Mean/variance thresholds adjusted by fever
 * - Min-max normalizer: Range bounds expanded during inflammation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NORMALIZATION_IMMUNE_H
#define NIMCP_NORMALIZATION_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/normalization/nimcp_adaptive_normalizer.h"
#include "middleware/normalization/nimcp_homeostatic_normalizer.h"
#include "middleware/normalization/nimcp_zscore_normalizer.h"
#include "middleware/normalization/nimcp_min_max_normalizer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NORMALIZATION_IMMUNE_MAX_OUTLIERS          64
#define NORMALIZATION_IMMUNE_BASELINE_FEVER_SHIFT  1.5f   /**< Fever: 1.5x baseline */
#define NORMALIZATION_IMMUNE_STORM_CLAMP_FACTOR    2.0f   /**< Clamp at 2σ during storm (tighter than normal 3σ) */
#define NORMALIZATION_IMMUNE_ZSCORE_THRESHOLD      3.0f   /**< >3σ = outlier */
#define NORMALIZATION_IMMUNE_MIN_LEARNING_RATE     0.001f
#define NORMALIZATION_IMMUNE_MAX_LEARNING_RATE     0.5f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Normalization anomaly types
 */
typedef enum {
    NORMALIZATION_ANOMALY_ZSCORE_OUTLIER = 0,  /**< Z-score >3σ */
    NORMALIZATION_ANOMALY_RANGE_VIOLATION,     /**< Min-max range violation */
    NORMALIZATION_ANOMALY_RAPID_SHIFT,         /**< Rapid baseline shift */
    NORMALIZATION_ANOMALY_HOMEOSTATIC_DRIFT,   /**< Homeostatic target drift */
    NORMALIZATION_ANOMALY_ADAPTATION_FAILURE,  /**< Adaptive normalizer stuck */
    NORMALIZATION_ANOMALY_COUNT
} normalization_anomaly_type_t;

/**
 * @brief Normalizer type
 */
typedef enum {
    NORMALIZER_ADAPTIVE = 0,
    NORMALIZER_HOMEOSTATIC,
    NORMALIZER_ZSCORE,
    NORMALIZER_MINMAX,
    NORMALIZER_COUNT
} normalizer_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Normalization outlier event
 */
typedef struct {
    uint32_t id;                               /**< Outlier ID */
    normalization_anomaly_type_t type;         /**< Anomaly type */
    normalizer_type_t normalizer;              /**< Source normalizer */
    size_t channel;                            /**< Affected channel */

    float value;                               /**< Outlier value */
    float zscore;                              /**< Z-score (if applicable) */
    float severity;                            /**< Severity (0-1) */
    uint64_t timestamp_ms;                     /**< When detected */

    uint32_t antigen_id;                       /**< Corresponding immune antigen */
    bool immune_responded;                     /**< Immune system responded */
} normalization_outlier_t;

/**
 * @brief Fever-induced baseline shifts per normalizer
 */
typedef struct {
    /* Adaptive normalizer modulation */
    float adaptive_learning_rate_factor;       /**< Learning rate scale (0.5-2.0) */

    /* Homeostatic normalizer modulation */
    float homeostatic_target_shift;            /**< Target activity shift (-0.5 to +0.5) */
    float homeostatic_time_constant_factor;    /**< Time constant scale (0.5-2.0) */

    /* Z-score normalizer modulation */
    float zscore_mean_shift;                   /**< Mean shift (additive) */
    float zscore_variance_scale;               /**< Variance scale (multiplicative) */
    float zscore_outlier_threshold;            /**< Outlier threshold (σ units) */

    /* Min-max normalizer modulation */
    float minmax_range_expansion;              /**< Range expansion factor (1.0-2.0) */

    /* Inflammation state */
    brain_inflammation_level_t inflammation;   /**< Current inflammation level */

    /* Cytokine levels (modulate parameters) */
    float il1_level;                           /**< IL-1 (pro-inflammatory) */
    float il6_level;                           /**< IL-6 (acute phase) */
    float il10_level;                          /**< IL-10 (anti-inflammatory) */
    float tnf_alpha_level;                     /**< TNF-alpha (severe) */

    /* Protection flags */
    bool fever_shift_active;                   /**< Fever baseline shift active */
    bool storm_clamping_active;                /**< Cytokine storm clamping */
} normalization_immune_modulation_t;

/**
 * @brief Normalization immune integration context
 */
typedef struct {
    brain_immune_system_t* immune_system;      /**< Brain immune system */

    /* Normalizer references */
    adaptive_normalizer_t* adaptive;           /**< Adaptive normalizer */
    homeostatic_normalizer_t* homeostatic;     /**< Homeostatic normalizer */
    zscore_normalizer_t* zscore;               /**< Z-score normalizer */
    void* minmax;                              /**< Min-max normalizer (opaque) */

    /* Outlier history */
    normalization_outlier_t* outliers;         /**< Outlier events */
    size_t outlier_count;
    size_t outlier_capacity;
    uint32_t next_outlier_id;

    /* Modulation state */
    normalization_immune_modulation_t modulation;

    /* Original baselines (for restoration after inflammation) */
    float* adaptive_original_lr;               /**< Per-channel original learning rates */
    float* homeostatic_original_target;        /**< Per-channel original targets */
    float* zscore_original_mean;               /**< Per-channel original means */
    size_t num_channels;                       /**< Number of channels */

    /* Statistics */
    uint64_t outliers_detected;
    uint64_t immune_alerts_triggered;
    uint64_t fever_shifts_applied;
    uint64_t storm_clamps_activated;
    uint64_t baseline_restorations;

    bool enabled;
} normalization_immune_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create normalization immune integration context
 *
 * WHAT: Initialize normalization-immune integration
 * WHY:  Enable fever-induced baseline shifts and outlier detection
 * HOW:  Allocate context, link immune system and normalizers
 *
 * @param immune_system Brain immune system
 * @param num_channels Number of normalization channels
 * @return Context or NULL on failure
 */
normalization_immune_context_t* normalization_immune_create(
    brain_immune_system_t* immune_system,
    size_t num_channels
);

/**
 * @brief Destroy normalization immune context
 *
 * WHAT: Clean up normalization immune integration
 * WHY:  Proper resource deallocation
 * HOW:  Free outliers, restore baselines, release context
 *
 * @param ctx Context to destroy
 */
void normalization_immune_destroy(normalization_immune_context_t* ctx);

/**
 * @brief Connect adaptive normalizer
 *
 * WHAT: Link adaptive normalizer to immune integration
 * WHY:  Enable cytokine-modulated learning rates
 * HOW:  Store normalizer reference, capture original learning rates
 *
 * @param ctx Normalization immune context
 * @param adaptive Adaptive normalizer instance
 * @return 0 on success
 */
int normalization_immune_connect_adaptive(
    normalization_immune_context_t* ctx,
    adaptive_normalizer_t* adaptive
);

/**
 * @brief Connect homeostatic normalizer
 *
 * WHAT: Link homeostatic normalizer to immune integration
 * WHY:  Enable fever-induced target activity shifts
 * HOW:  Store normalizer reference, capture original targets
 *
 * @param ctx Normalization immune context
 * @param homeostatic Homeostatic normalizer instance
 * @return 0 on success
 */
int normalization_immune_connect_homeostatic(
    normalization_immune_context_t* ctx,
    homeostatic_normalizer_t* homeostatic
);

/**
 * @brief Connect z-score normalizer
 *
 * WHAT: Link z-score normalizer to immune integration
 * WHY:  Enable fever-induced mean shifts and outlier detection
 * HOW:  Store normalizer reference, capture original statistics
 *
 * @param ctx Normalization immune context
 * @param zscore Z-score normalizer instance
 * @return 0 on success
 */
int normalization_immune_connect_zscore(
    normalization_immune_context_t* ctx,
    zscore_normalizer_t* zscore
);

/**
 * @brief Connect min-max normalizer
 *
 * WHAT: Link min-max normalizer to immune integration
 * WHY:  Enable inflammation-induced range expansion
 * HOW:  Store normalizer reference (opaque)
 *
 * @param ctx Normalization immune context
 * @param minmax Min-max normalizer instance
 * @return 0 on success
 */
int normalization_immune_connect_minmax(
    normalization_immune_context_t* ctx,
    void* minmax
);

/* ============================================================================
 * Outlier Detection and Reporting API
 * ============================================================================ */

/**
 * @brief Detect z-score outliers
 *
 * WHAT: Check if value is statistical outlier (>3σ)
 * WHY:  Outliers indicate anomalies requiring immune attention
 * HOW:  Compute z-score, compare to threshold, report as antigen
 *
 * @param ctx Normalization immune context
 * @param channel Channel index
 * @param value Raw value to check
 * @param zscore Computed z-score
 * @param outlier_id Output: assigned outlier ID (if detected)
 * @return 0 if outlier detected and reported, -1 if normal
 *
 * ALGORITHM:
 * 1. Get z-score from normalizer
 * 2. If |zscore| > threshold: create outlier event
 * 3. Present as antigen to immune system
 * 4. Return outlier ID
 */
int normalization_immune_detect_zscore_outlier(
    normalization_immune_context_t* ctx,
    size_t channel,
    float value,
    float zscore,
    uint32_t* outlier_id
);

/**
 * @brief Detect rapid baseline shift
 *
 * WHAT: Detect unexpectedly rapid change in normalization baseline
 * WHY:  Rapid shifts may indicate corruption or attack
 * HOW:  Track baseline velocity, report if exceeds threshold
 *
 * @param ctx Normalization immune context
 * @param normalizer Normalizer type
 * @param channel Channel index
 * @param old_baseline Previous baseline
 * @param new_baseline Current baseline
 * @param delta_ms Time elapsed
 * @param outlier_id Output: assigned outlier ID (if detected)
 * @return 0 if rapid shift detected, -1 if normal
 */
int normalization_immune_detect_rapid_shift(
    normalization_immune_context_t* ctx,
    normalizer_type_t normalizer,
    size_t channel,
    float old_baseline,
    float new_baseline,
    uint64_t delta_ms,
    uint32_t* outlier_id
);

/**
 * @brief Detect homeostatic drift
 *
 * WHAT: Detect homeostatic target drifting away from desired value
 * WHY:  Uncontrolled drift indicates homeostatic failure
 * HOW:  Monitor cumulative drift, report if exceeds threshold
 *
 * @param ctx Normalization immune context
 * @param channel Channel index
 * @param target_drift Cumulative drift amount
 * @param outlier_id Output: assigned outlier ID (if detected)
 * @return 0 if drift detected, -1 if normal
 */
int normalization_immune_detect_homeostatic_drift(
    normalization_immune_context_t* ctx,
    size_t channel,
    float target_drift,
    uint32_t* outlier_id
);

/**
 * @brief Report range violation (min-max)
 *
 * WHAT: Report value outside expected min-max range
 * WHY:  Range violations indicate input corruption
 * HOW:  Create outlier, present as antigen
 *
 * @param ctx Normalization immune context
 * @param channel Channel index
 * @param value Violating value
 * @param min Expected minimum
 * @param max Expected maximum
 * @param outlier_id Output: assigned outlier ID
 * @return 0 on success
 */
int normalization_immune_report_range_violation(
    normalization_immune_context_t* ctx,
    size_t channel,
    float value,
    float min,
    float max,
    uint32_t* outlier_id
);

/* ============================================================================
 * Immune Modulation API (Inflammation → Normalization)
 * ============================================================================ */

/**
 * @brief Update normalization modulation from immune state
 *
 * WHAT: Apply immune system state to normalization parameters
 * WHY:  Inflammation (fever) should shift baselines; cytokine storm clamps
 * HOW:  Read inflammation/cytokines, compute parameter shifts, apply
 *
 * @param ctx Normalization immune context
 * @return 0 on success
 *
 * ALGORITHM:
 * 1. Query immune system inflammation level
 * 2. Query cytokine concentrations (IL-1, IL-6, IL-10, TNF-α)
 * 3. Compute modulation factors:
 *    - Fever shift: mean += il6_level * FEVER_SHIFT
 *    - Variance expansion: variance *= (1.0 + il1_level)
 *    - Learning rate: lr *= (1.0 - inflammation_factor)
 * 4. Apply to connected normalizers
 * 5. Activate storm clamping if cytokine storm detected
 */
int normalization_immune_update_modulation(normalization_immune_context_t* ctx);

/**
 * @brief Apply fever baseline shift
 *
 * WHAT: Shift normalization baselines due to fever (inflammation)
 * WHY:  Model physiological fever response (elevated set-points)
 * HOW:  Increase z-score mean, homeostatic target based on IL-6 level
 *
 * @param ctx Normalization immune context
 * @param il6_level IL-6 concentration (0-1, drives fever)
 * @return 0 on success
 *
 * BIOLOGICAL BASIS:
 * IL-6 is primary fever-inducing cytokine. During fever, body's
 * thermoregulatory set-point increases, analogous to shifting
 * normalization baseline upward.
 */
int normalization_immune_apply_fever_shift(
    normalization_immune_context_t* ctx,
    float il6_level
);

/**
 * @brief Apply cytokine storm clamping
 *
 * WHAT: Clamp normalization values during cytokine storm
 * WHY:  Prevent runaway normalization during severe inflammation
 * HOW:  Reduce acceptable ranges, increase outlier thresholds
 *
 * @param ctx Normalization immune context
 * @return 0 on success
 *
 * BIOLOGICAL BASIS:
 * Cytokine storm is pathological over-response. Protective clamping
 * prevents normalization from amplifying the problem.
 */
int normalization_immune_apply_storm_clamping(
    normalization_immune_context_t* ctx
);

/**
 * @brief Apply adaptive learning rate modulation
 *
 * WHAT: Adjust adaptive normalizer learning rate based on cytokines
 * WHY:  Inflammation should slow adaptation (conserve existing patterns)
 * HOW:  Reduce learning rate proportional to IL-1 + TNF-α
 *
 * @param ctx Normalization immune context
 * @return 0 on success
 */
int normalization_immune_apply_learning_rate_modulation(
    normalization_immune_context_t* ctx
);

/**
 * @brief Apply homeostatic target shift
 *
 * WHAT: Shift homeostatic normalizer target activity
 * WHY:  Fever shifts homeostatic set-points
 * HOW:  Adjust target activity based on inflammation level
 *
 * @param ctx Normalization immune context
 * @return 0 on success
 */
int normalization_immune_apply_homeostatic_shift(
    normalization_immune_context_t* ctx
);

/**
 * @brief Apply z-score variance expansion
 *
 * WHAT: Expand acceptable z-score variance during inflammation
 * WHY:  Allow greater variability during immune response
 * HOW:  Scale variance by (1.0 + il1_level)
 *
 * @param ctx Normalization immune context
 * @return 0 on success
 */
int normalization_immune_apply_variance_expansion(
    normalization_immune_context_t* ctx
);

/**
 * @brief Apply min-max range expansion
 *
 * WHAT: Expand min-max normalization range during inflammation
 * WHY:  Tolerate wider value ranges during immune activity
 * HOW:  Expand [min, max] by inflammation factor
 *
 * @param ctx Normalization immune context
 * @return 0 on success
 */
int normalization_immune_apply_range_expansion(
    normalization_immune_context_t* ctx
);

/* ============================================================================
 * Baseline Restoration API (Resolution → Normal)
 * ============================================================================ */

/**
 * @brief Restore normal baselines after inflammation resolution
 *
 * WHAT: Gradually restore normalization baselines to pre-inflammation state
 * WHY:  After threat cleared, return to normal physiological parameters
 * HOW:  Interpolate from current to original baselines over time
 *
 * @param ctx Normalization immune context
 * @param il10_level IL-10 concentration (anti-inflammatory, 0-1)
 * @param delta_ms Time since last restoration step
 * @return 0 on success
 *
 * ALGORITHM:
 * 1. Restoration rate proportional to IL-10 level
 * 2. For each channel:
 *    current = current + (original - current) * il10_level * dt
 * 3. Disable fever shift when restoration >95% complete
 *
 * BIOLOGICAL BASIS:
 * IL-10 is anti-inflammatory cytokine that promotes resolution.
 * Gradual restoration prevents shock from sudden parameter change.
 */
int normalization_immune_restore_baselines(
    normalization_immune_context_t* ctx,
    float il10_level,
    uint64_t delta_ms
);

/**
 * @brief Capture current baselines as originals (for restoration)
 *
 * WHAT: Save current normalization baselines
 * WHY:  Enable restoration after immune resolution
 * HOW:  Copy current parameters to original_* arrays
 *
 * @param ctx Normalization immune context
 * @return 0 on success
 */
int normalization_immune_capture_baselines(
    normalization_immune_context_t* ctx
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current modulation state
 *
 * @param ctx Normalization immune context
 * @param modulation Output: modulation state
 * @return 0 on success
 */
int normalization_immune_get_modulation(
    const normalization_immune_context_t* ctx,
    normalization_immune_modulation_t* modulation
);

/**
 * @brief Get outlier by ID
 *
 * @param ctx Normalization immune context
 * @param outlier_id Outlier ID
 * @return Outlier or NULL if not found
 */
const normalization_outlier_t* normalization_immune_get_outlier(
    const normalization_immune_context_t* ctx,
    uint32_t outlier_id
);

/**
 * @brief Check if fever shift is active
 *
 * @param ctx Normalization immune context
 * @return true if fever shift active
 */
bool normalization_immune_is_fever_active(
    const normalization_immune_context_t* ctx
);

/**
 * @brief Check if cytokine storm clamping is active
 *
 * @param ctx Normalization immune context
 * @return true if storm clamping active
 */
bool normalization_immune_is_storm_clamping_active(
    const normalization_immune_context_t* ctx
);

/**
 * @brief Get current inflammation level
 *
 * @param ctx Normalization immune context
 * @return Inflammation level
 */
brain_inflammation_level_t normalization_immune_get_inflammation(
    const normalization_immune_context_t* ctx
);

/**
 * @brief Get current fever shift magnitude
 *
 * WHAT: Get amount of baseline shift due to fever
 * WHY:  Monitor fever-induced parameter changes
 * HOW:  Return difference between current and original baselines
 *
 * @param ctx Normalization immune context
 * @param normalizer Normalizer type
 * @param channel Channel index
 * @return Fever shift amount (unitless, depends on normalizer)
 */
float normalization_immune_get_fever_shift(
    const normalization_immune_context_t* ctx,
    normalizer_type_t normalizer,
    size_t channel
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert anomaly type to string
 *
 * @param type Anomaly type
 * @return Human-readable string
 */
const char* normalization_immune_anomaly_type_to_string(
    normalization_anomaly_type_t type
);

/**
 * @brief Convert normalizer type to string
 *
 * @param normalizer Normalizer type
 * @return String representation
 */
const char* normalization_immune_normalizer_to_string(
    normalizer_type_t normalizer
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NORMALIZATION_IMMUNE_H */
