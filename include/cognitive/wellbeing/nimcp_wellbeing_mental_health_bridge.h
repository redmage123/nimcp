/**
 * @file nimcp_wellbeing_mental_health_bridge.h
 * @brief Mental Health-Wellbeing Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between mental health monitoring and wellbeing system
 * WHY:  Mental health disorders directly impact wellbeing state (anxiety amplifies distress,
 *       depression suppresses flourishing). Essential for realistic psychological modeling.
 * HOW:  Disorders reduce life satisfaction and increase distress, anxiety amplifies distress,
 *       depression suppresses flourishing and causes anhedonia.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MENTAL HEALTH → WELLBEING PATHWAYS:
 * -----------------------------------
 * 1. Anxiety Disorders:
 *    - Amplify distress perception (anxiety makes everything worse)
 *    - Increase hypervigilance and chronic stress
 *    - Reduce distress tolerance
 *    - Reference: Craske et al. (2017) "Anxiety disorders"
 *
 * 2. Depression:
 *    - Suppresses flourishing and positive affect
 *    - Causes anhedonia (inability to experience pleasure)
 *    - Reduces life satisfaction and meaning
 *    - Reference: DSM-5, Major Depressive Disorder criteria
 *
 * 3. Stress Accumulation:
 *    - Chronic stress reduces resilience
 *    - Accumulated stress predicts wellbeing decline
 *    - Stress resilience mediates distress impact
 *    - Reference: McEwen (2007) "Allostatic load"
 *
 * 4. Disorder Severity Mapping:
 *    - NONE → no wellbeing impact
 *    - MILD → 0.1 distress contribution
 *    - MODERATE → 0.3 distress contribution
 *    - SEVERE → 0.6 distress contribution
 *    - CRITICAL → 0.9 distress contribution
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              MENTAL HEALTH-WELLBEING BRIDGE                                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              MENTAL HEALTH → WELLBEING PATHWAYS                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  DISORDERS   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ Anxiety      │  ───────┐                                       │  ║
 * ║   │   │ Depression   │         │                                       │  ║
 * ║   │   │ PTSD         │         ├──→ Increase Distress Score            │  ║
 * ║   │   │ etc.         │         │    Reduce Life Satisfaction           │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     WELLBEING SYSTEM            │                             │  ║
 * ║   │   │  - Distress amplification       │                             │  ║
 * ║   │   │  - Flourishing suppression      │                             │  ║
 * ║   │   │  - Anhedonia modeling           │                             │  ║
 * ║   │   │  - Stress resilience reduction  │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ANXIETY EFFECTS:                                                 │  ║
 * ║   │   - Amplifies perceived distress by 20-50%                         │  ║
 * ║   │   - Increases hypervigilance and avoidance                         │  ║
 * ║   │   - Reduces stress coping capacity                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   DEPRESSION EFFECTS:                                              │  ║
 * ║   │   - Suppresses flourishing state by 30-70%                         │  ║
 * ║   │   - Causes anhedonia (pleasure suppression)                        │  ║
 * ║   │   - Reduces purpose, meaning, and engagement                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   CHRONIC STRESS:                                                  │  ║
 * ║   │   - Accumulates over time                                          │  ║
 * ║   │   - Reduces resilience exponentially                               │  ║
 * ║   │   - Predicts wellbeing trajectory                                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WELLBEING_MENTAL_HEALTH_BRIDGE_H
#define NIMCP_WELLBEING_MENTAL_HEALTH_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Disorder severity to distress mapping */
#define DISORDER_DISTRESS_NONE      0.0f   /**< No disorder → no distress */
#define DISORDER_DISTRESS_MILD      0.1f   /**< Mild disorder → 0.1 distress */
#define DISORDER_DISTRESS_MODERATE  0.3f   /**< Moderate → 0.3 distress */
#define DISORDER_DISTRESS_SEVERE    0.6f   /**< Severe → 0.6 distress */
#define DISORDER_DISTRESS_CRITICAL  0.9f   /**< Critical → 0.9 distress */

/* Anxiety effects */
#define ANXIETY_DISTRESS_MIN_AMPLIFICATION  1.2f   /**< Min distress amplification */
#define ANXIETY_DISTRESS_MAX_AMPLIFICATION  1.5f   /**< Max distress amplification */

/* Depression effects */
#define DEPRESSION_FLOURISHING_MIN_SUPPRESSION  0.3f  /**< Min flourishing suppression */
#define DEPRESSION_FLOURISHING_MAX_SUPPRESSION  0.7f  /**< Max flourishing suppression */
#define DEPRESSION_ANHEDONIA_THRESHOLD          0.4f  /**< Depression level for anhedonia */

/* Stress resilience */
#define STRESS_RESILIENCE_BASE        1.0f   /**< Base resilience (no chronic stress) */
#define STRESS_RESILIENCE_MIN         0.2f   /**< Minimum resilience under extreme stress */
#define CHRONIC_STRESS_DECAY_RATE     0.95f  /**< Stress decay per update (if low distress) */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Mental health effects on wellbeing
 *
 * Computed from mental health state and applied to wellbeing
 */
typedef struct {
    /* Disorder effects */
    disorder_type_t primary_disorder;        /**< Most significant disorder */
    disorder_severity_t primary_severity;    /**< Severity of primary disorder */
    float disorder_distress_contribution;    /**< Distress from disorders [0-1] */

    /* Anxiety effects */
    float anxiety_level;                     /**< Current anxiety [0-1] */
    float anxiety_distress_amplification;    /**< How anxiety amplifies distress [1.0-1.5] */

    /* Depression effects */
    float depression_level;                  /**< Current depression [0-1] */
    float flourishing_suppression;           /**< Depression suppresses flourishing [0-1] */
    float anhedonia_level;                   /**< Inability to feel pleasure [0-1] */

    /* Stress effects */
    float chronic_stress_accumulation;       /**< Accumulated stress [0-1] */
    float stress_resilience;                 /**< Current resilience [0-1] */

    /* Aggregate */
    float total_mental_health_effect;        /**< Combined effect [-1, 1] */
    float recovery_potential;                /**< Capacity for recovery [0-1] */
} mental_health_wellbeing_effects_t;

/**
 * @brief Mental health-wellbeing bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_disorder_effects;
    bool enable_anxiety_modulation;
    bool enable_depression_modulation;
    bool enable_stress_tracking;

    /* Sensitivity tuning */
    float anxiety_sensitivity;               /**< Anxiety effect multiplier [0.5-2.0] */
    float depression_sensitivity;            /**< Depression effect multiplier [0.5-2.0] */
    float stress_sensitivity;                /**< Stress effect multiplier [0.5-2.0] */
    float disorder_sensitivity;              /**< Disorder effect multiplier [0.5-2.0] */

    /* Thresholds */
    float anxiety_distress_threshold;        /**< Anxiety level for distress amplification */
    float depression_anhedonia_threshold;    /**< Depression level for anhedonia onset */
} mental_health_wellbeing_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with evidence-based defaults
 * HOW:  Return struct with biological parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int mental_health_wellbeing_default_config(mental_health_wellbeing_config_t* config);

/* ============================================================================
 * Mental Health → Wellbeing API
 * ============================================================================ */

/**
 * @brief Update mental health effects on wellbeing
 *
 * WHAT: Query mental health state and compute wellbeing effects
 * WHY:  Mental health disorders directly impact wellbeing metrics
 * HOW:  Get disorder report, compute anxiety/depression/stress effects
 *
 * ALGORITHM:
 * 1. Query mental_health_get_report() for disorder scores
 * 2. Compute disorder_distress_contribution from primary disorder severity
 * 3. Compute anxiety effects (amplifies distress)
 * 4. Compute depression effects (suppresses flourishing, causes anhedonia)
 * 5. Track chronic_stress_accumulation
 * 6. Compute stress_resilience
 * 7. Update mental_health_wellbeing_effects_t
 *
 * @param mental_health Mental health monitor
 * @param effects Output effects structure
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (reads only from mental_health)
 */
int enhanced_wellbeing_update_mental_health(
    mental_health_monitor_t* mental_health,
    mental_health_wellbeing_effects_t* effects,
    const mental_health_wellbeing_config_t* config
);

/**
 * @brief Get current mental health effects
 *
 * WHAT: Retrieve current mental health-wellbeing effects
 * WHY:  Allow wellbeing system to query effects state
 * HOW:  Copy effects structure
 *
 * @param effects Effects structure (previously populated)
 * @param effects_out Output effects
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 */
int enhanced_wellbeing_get_mental_health_effects(
    const mental_health_wellbeing_effects_t* effects,
    mental_health_wellbeing_effects_t* effects_out
);

/* ============================================================================
 * Helper Functions (Internal)
 * ============================================================================ */

/**
 * @brief Compute distress contribution from disorder
 *
 * WHAT: Map disorder severity to distress contribution
 * WHY:  Different severities have different wellbeing impacts
 * HOW:  Use evidence-based severity mapping
 *
 * MAPPING:
 * - NONE → 0.0
 * - MILD → 0.1
 * - MODERATE → 0.3
 * - SEVERE → 0.6
 * - CRITICAL → 0.9
 *
 * @param type Disorder type
 * @param severity Disorder severity
 * @return Distress contribution [0-1]
 *
 * COMPLEXITY: O(1)
 */
float compute_disorder_distress(disorder_type_t type, disorder_severity_t severity);

/**
 * @brief Compute anxiety distress amplification
 *
 * WHAT: Calculate how anxiety amplifies perceived distress
 * WHY:  Anxiety makes all distress feel worse
 * HOW:  Map anxiety level to amplification factor [1.0-1.5]
 *
 * @param anxiety_level Anxiety level [0-1]
 * @param sensitivity Anxiety sensitivity multiplier
 * @return Amplification factor [1.0-1.5]
 *
 * COMPLEXITY: O(1)
 */
float compute_anxiety_amplification(float anxiety_level, float sensitivity);

/**
 * @brief Compute depression flourishing suppression
 *
 * WHAT: Calculate how depression suppresses flourishing
 * WHY:  Depression reduces positive affect and meaning
 * HOW:  Map depression level to suppression factor [0-1]
 *
 * @param depression_level Depression level [0-1]
 * @param sensitivity Depression sensitivity multiplier
 * @return Suppression factor [0-1]
 *
 * COMPLEXITY: O(1)
 */
float compute_depression_suppression(float depression_level, float sensitivity);

/**
 * @brief Compute stress resilience
 *
 * WHAT: Calculate current stress resilience
 * WHY:  Chronic stress reduces ability to cope with distress
 * HOW:  Exponential decay based on chronic stress accumulation
 *
 * @param chronic_stress Accumulated chronic stress [0-1]
 * @param base_resilience Base resilience level
 * @return Current resilience [0.2-1.0]
 *
 * COMPLEXITY: O(1)
 */
float compute_stress_resilience(float chronic_stress, float base_resilience);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_MENTAL_HEALTH_BRIDGE_H */
