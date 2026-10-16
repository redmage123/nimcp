/**
 * @file nimcp_portia_sensor_fusion_immune_bridge.h
 * @brief Portia Sensor Fusion-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and Portia sensor fusion
 * WHY:  Biological evidence shows sensory overload triggers stress responses and inflammation;
 *       immune activation (fever/inflammation) impairs sensory processing and integration.
 * HOW:  Cytokines reduce sensor weights and fusion confidence; sensory overload/conflicts
 *       trigger immune stress responses.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SENSOR FUSION PATHWAYS:
 * -------------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair multisensory integration
 *    - Reduce sensory precision and discrimination
 *    - Lower sensor confidence weights
 *    - Increase sensory noise susceptibility
 *    - Reference: Brydon et al. (2009) "Peripheral inflammation is associated with
 *      altered substantia nigra activity and psychomotor slowing"
 *
 * 2. Fever and Inflammation:
 *    - Reduced sensory acuity across modalities
 *    - Impaired cross-modal binding
 *    - Lower fusion confidence
 *    - Increased sensory prediction errors
 *    - Reference: Harrison et al. (2015) "Inflammation causes mood changes through
 *      alterations in subgenual cingulate activity and mesolimbic connectivity"
 *
 * 3. Cytokine Storm:
 *    - Severe sensory processing impairment
 *    - Hallucinations and distorted perception
 *    - Complete fusion breakdown
 *    - Sensory fragmentation
 *
 * SENSOR FUSION → IMMUNE PATHWAYS:
 * -------------------------------
 * 1. Sensory Overload:
 *    - Excessive sensor inputs → stress response
 *    - Triggers inflammatory cascade
 *    - Activates defensive immune state
 *    - Reference: Sensory overload activates HPA axis and sympathetic nervous system
 *
 * 2. Sensor Conflicts:
 *    - Cross-modal conflicts → increased uncertainty
 *    - Triggers threat detection (prediction error)
 *    - Low-level immune activation
 *    - Enhanced immune vigilance
 *
 * 3. Sensor Degradation:
 *    - Chronic low-quality inputs → chronic stress
 *    - Sustained low-level inflammation
 *    - Metabolic immune suppression
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              PORTIA SENSOR FUSION-IMMUNE BRIDGE                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 IMMUNE → SENSOR FUSION                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.2 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.15│         │                                       │  ║
 * ║   │   │ TNF-α → -0.3 │         ├──→ Sensor Weight Reduction            │  ║
 * ║   │   │              │         │    Confidence Impairment              │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   SENSOR FUSION SYSTEM          │                             │  ║
 * ║   │   │  - Reduced sensor weights       │                             │  ║
 * ║   │   │  - Lower fusion confidence      │                             │  ║
 * ║   │   │  - Increased noise tolerance    │                             │  ║
 * ║   │   │  - Outlier rejection threshold↑ │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -10% weights  │                                     │  ║
 * ║   │   │ REGIONAL → -25% weights  │                                     │  ║
 * ║   │   │ SYSTEMIC → -50% weights  │                                     │  ║
 * ║   │   │ STORM    → -80% weights  │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 SENSOR FUSION → IMMUNE                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │SENSOR OVERLOAD│──→ IL-6 Release (Stress)                       │  ║
 * ║   │   │HIGH CONFLICTS │──→ IL-1β Release (Prediction Error)            │  ║
 * ║   │   │LOW CONFIDENCE │──→ Immune Vigilance Boost                      │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │SENSOR DROPOUT │──→ Metabolic Suppression                       │  ║
 * ║   │   │DEGRADATION    │──→ Chronic Low-Level Inflammation              │  ║
 * ║   │   └──────────────┘                                                 │  ║
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

#ifndef NIMCP_PORTIA_SENSOR_FUSION_IMMUNE_BRIDGE_H
#define NIMCP_PORTIA_SENSOR_FUSION_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "portia/nimcp_portia_sensor_fusion.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine sensor weight impact factors */
#define CYTOKINE_IL1_SENSOR_WEIGHT_IMPACT    -0.2f   /**< IL-1β → sensor weight loss */
#define CYTOKINE_IL6_SENSOR_WEIGHT_IMPACT    -0.15f  /**< IL-6 → sensor weight loss */
#define CYTOKINE_TNF_SENSOR_WEIGHT_IMPACT    -0.3f   /**< TNF-α → strong weight loss */
#define CYTOKINE_IFN_GAMMA_SENSOR_IMPACT     -0.1f   /**< IFN-γ → mild weight loss */
#define CYTOKINE_IL10_SENSOR_RECOVERY        0.15f   /**< IL-10 → recovery boost */

/* Inflammation sensor weight reduction */
#define INFLAMMATION_NONE_SENSOR_FACTOR      1.0f    /**< No reduction */
#define INFLAMMATION_LOCAL_SENSOR_FACTOR     0.9f    /**< -10% weights */
#define INFLAMMATION_REGIONAL_SENSOR_FACTOR  0.75f   /**< -25% weights */
#define INFLAMMATION_SYSTEMIC_SENSOR_FACTOR  0.5f    /**< -50% weights */
#define INFLAMMATION_STORM_SENSOR_FACTOR     0.2f    /**< -80% weights */

/* Inflammation fusion confidence reduction */
#define INFLAMMATION_CONFIDENCE_BASE         0.05f   /**< Base reduction */
#define INFLAMMATION_CONFIDENCE_PER_LEVEL    0.1f    /**< Per inflammation level */

/* Sensor-triggered immune thresholds */
#define SENSOR_OVERLOAD_THRESHOLD            0.8f    /**< High sensor count → stress */
#define SENSOR_CONFLICT_THRESHOLD            0.3f    /**< High conflict → immune alert */
#define SENSOR_DROPOUT_THRESHOLD             0.4f    /**< Low active sensors → suppression */
#define SENSOR_CONFIDENCE_LOW_THRESHOLD      0.5f    /**< Low confidence → vigilance */

/* Sensor immune response rates */
#define SENSOR_OVERLOAD_IL6_RELEASE          0.2f    /**< Overload → IL-6 */
#define SENSOR_CONFLICT_IL1_RELEASE          0.15f   /**< Conflict → IL-1β */
#define SENSOR_DROPOUT_METABOLIC_SUPPRESSION 0.25f   /**< Dropout → suppression */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine sensor fusion effects
 *
 * How cytokine levels impair sensor fusion
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_weight_reduction;          /**< IL-1β sensor weight loss */
    float il6_weight_reduction;          /**< IL-6 sensor weight loss */
    float tnf_weight_reduction;          /**< TNF-α sensor weight loss */
    float ifn_gamma_weight_reduction;    /**< IFN-γ sensor weight loss */

    /* Anti-inflammatory effects */
    float il10_recovery_boost;           /**< IL-10 recovery boost */

    /* Aggregate effects */
    float total_weight_factor;           /**< Combined weight multiplier [0-1] */
    float confidence_reduction;          /**< Fusion confidence loss [0-1] */
    float noise_tolerance_increase;      /**< Noise threshold increase [0-1] */
    float outlier_threshold_increase;    /**< Outlier rejection threshold↑ */
} cytokine_sensor_effects_t;

/**
 * @brief Inflammation sensor fusion state
 *
 * How chronic inflammation affects sensor fusion
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;     /**< How long inflamed */
    bool is_chronic;                     /**< >= threshold */

    /* Sensor fusion impacts */
    float weight_factor;                 /**< Overall weight multiplier [0-1] */
    float confidence_reduction;          /**< Fusion confidence loss [0-1] */
    float acuity_loss;                   /**< Sensory acuity loss [0-1] */
    float cross_modal_binding_deficit;   /**< Binding impairment [0-1] */
    float prediction_error_increase;     /**< Prediction error↑ [0-1] */

    /* Sensor processing */
    float sensory_fragmentation;         /**< Fragmented perception [0-1] */
    float hallucination_risk;            /**< Hallucination risk (storm) [0-1] */
} inflammation_sensor_state_t;

/**
 * @brief Sensor-driven immune modulation
 *
 * How sensor fusion state affects immune function
 */
typedef struct {
    /* Sensor fusion state */
    uint32_t active_sensor_count;        /**< Number of active sensors */
    float fusion_confidence;             /**< Current fusion confidence [0-1] */
    float sensor_conflict_level;         /**< Cross-modal conflict [0-1] */
    float sensor_quality_avg;            /**< Average sensor quality [0-1] */

    /* Immune effects */
    float overload_stress_level;         /**< Overload stress [0-1] */
    float conflict_immune_activation;    /**< Conflict → immune [0-1] */
    float dropout_metabolic_suppression; /**< Dropout → suppression [0-1] */

    /* Cytokine releases */
    float il6_release_from_overload;     /**< IL-6 from overload */
    float il1_release_from_conflict;     /**< IL-1β from conflict */
    bool chronic_degradation_inflammation; /**< Chronic low-grade inflammation */
} sensor_immune_modulation_t;

/**
 * @brief Sensor fusion-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_sensor_impairment;
    bool enable_inflammation_sensor_reduction;
    bool enable_sensor_overload_immune_trigger;
    bool enable_sensor_conflict_immune_boost;
    bool enable_sensor_dropout_suppression;

    /* Sensitivity tuning */
    float cytokine_sensitivity;          /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;      /**< Inflammation effect multiplier [0.5-2.0] */
    float sensor_immune_sensitivity;     /**< Sensor→immune multiplier [0.5-2.0] */

    /* Thresholds */
    float overload_threshold;            /**< Sensor overload threshold [0.6-0.9] */
    float conflict_threshold;            /**< Conflict threshold [0.2-0.5] */
    float dropout_threshold;             /**< Dropout threshold [0.3-0.6] */
    float confidence_low_threshold;      /**< Low confidence threshold [0.4-0.7] */
} portia_sensor_fusion_immune_config_t;

/**
 * @brief Complete sensor fusion-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    portia_fusion_ctx_t* sensor_fusion;

    /* Configuration */
    portia_sensor_fusion_immune_config_t config;

    /* Current state */
    cytokine_sensor_effects_t cytokine_effects;
    inflammation_sensor_state_t inflammation_state;
    sensor_immune_modulation_t sensor_modulation;

    /* Timing */
    uint64_t last_update_time;
    float overload_accumulator;          /**< Accumulated overload time */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t overload_events;
    uint32_t conflict_immune_triggers;
    uint32_t dropout_suppressions;

    } portia_sensor_fusion_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int portia_sensor_fusion_immune_default_config(portia_sensor_fusion_immune_config_t* config);

/**
 * @brief Create sensor fusion-immune bridge
 *
 * WHAT: Initialize bidirectional sensor fusion-immune integration
 * WHY:  Enable realistic immune-sensory coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param sensor_fusion Sensor fusion system
 * @return New bridge or NULL on failure
 */
portia_sensor_fusion_immune_bridge_t* portia_sensor_fusion_immune_create(
    const portia_sensor_fusion_immune_config_t* config,
    brain_immune_system_t* immune_system,
    portia_fusion_ctx_t* sensor_fusion
);

/**
 * @brief Destroy sensor fusion-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void portia_sensor_fusion_immune_destroy(portia_sensor_fusion_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Sensor Fusion API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to sensor fusion
 *
 * WHAT: Reduce sensor weights and confidence based on cytokines
 * WHY:  Pro-inflammatory cytokines impair sensory processing
 * HOW:  Query immune system cytokines, adjust sensor weights
 *
 * @param bridge Sensor fusion-immune bridge
 * @return 0 on success
 */
int portia_sensor_fusion_immune_apply_cytokine_effects(
    portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Apply inflammation effects to sensor fusion
 *
 * WHAT: Reduce sensor weights and fusion confidence from inflammation
 * WHY:  Inflammation reduces sensory acuity and integration
 * HOW:  Check inflammation level/duration, adjust fusion parameters
 *
 * @param bridge Sensor fusion-immune bridge
 * @return 0 on success
 */
int portia_sensor_fusion_immune_apply_inflammation_effects(
    portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Compute sensor weight factor from immune state
 *
 * WHAT: Calculate sensor weight multiplier given immune status
 * WHY:  Inflammation reduces sensory precision
 * HOW:  Map inflammation level to weight factor [0-1]
 *
 * @param bridge Sensor fusion-immune bridge
 * @return Weight factor [0-1] (1.0 = normal, 0.0 = complete impairment)
 */
float portia_sensor_fusion_immune_compute_weight_factor(
    const portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Compute fusion confidence reduction from inflammation
 *
 * WHAT: Calculate how much inflammation reduces fusion confidence
 * WHY:  Inflammation increases sensory uncertainty
 * HOW:  Map inflammation level to confidence reduction
 *
 * @param bridge Sensor fusion-immune bridge
 * @return Confidence reduction [0-1] (0 = no reduction, 1 = complete loss)
 */
float portia_sensor_fusion_immune_compute_confidence_reduction(
    const portia_sensor_fusion_immune_bridge_t* bridge
);

/* ============================================================================
 * Sensor Fusion → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from sensor overload
 *
 * WHAT: Activate stress immune response from excessive sensor inputs
 * WHY:  Sensory overload activates stress/inflammatory cascade
 * HOW:  Check active sensor count, release IL-6 if overloaded
 *
 * @param bridge Sensor fusion-immune bridge
 * @return 0 on success
 */
int portia_sensor_fusion_immune_trigger_overload_response(
    portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Boost immune vigilance from sensor conflicts
 *
 * WHAT: Enhance immune surveillance from cross-modal conflicts
 * WHY:  Sensory conflicts indicate prediction errors (potential threats)
 * HOW:  Detect conflicts, release IL-1β, boost immune vigilance
 *
 * @param bridge Sensor fusion-immune bridge
 * @return 0 on success
 */
int portia_sensor_fusion_immune_boost_from_conflicts(
    portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Trigger metabolic suppression from sensor dropout
 *
 * WHAT: Suppress immune activity from low sensor quality/count
 * WHY:  Chronic poor sensory input → metabolic conservation
 * HOW:  Detect low active sensors, reduce immune metabolic allocation
 *
 * @param bridge Sensor fusion-immune bridge
 * @return 0 on success
 */
int portia_sensor_fusion_immune_trigger_dropout_suppression(
    portia_sensor_fusion_immune_bridge_t* bridge
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update sensor fusion-immune bridge (both directions)
 *
 * WHAT: Process all sensor fusion-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from sensors, adjust parameters
 *
 * @param bridge Sensor fusion-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int portia_sensor_fusion_immune_update(
    portia_sensor_fusion_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine sensor effects
 *
 * @param bridge Sensor fusion-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int portia_sensor_fusion_immune_get_cytokine_effects(
    const portia_sensor_fusion_immune_bridge_t* bridge,
    cytokine_sensor_effects_t* effects
);

/**
 * @brief Get current inflammation sensor state
 *
 * @param bridge Sensor fusion-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int portia_sensor_fusion_immune_get_inflammation_state(
    const portia_sensor_fusion_immune_bridge_t* bridge,
    inflammation_sensor_state_t* state
);

/**
 * @brief Check if experiencing sensor impairment from inflammation
 *
 * WHAT: Determine if inflammation causing significant sensor degradation
 * WHY:  Detect clinically significant sensory effects
 * HOW:  Check weight reduction threshold
 *
 * @param bridge Sensor fusion-immune bridge
 * @return true if significant impairment (>25% weight loss)
 */
bool portia_sensor_fusion_immune_has_sensor_impairment(
    const portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Get current sensor weight factor
 *
 * @param bridge Sensor fusion-immune bridge
 * @return Weight factor [0-1]
 */
float portia_sensor_fusion_immune_get_weight_factor(
    const portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Get current fusion confidence reduction
 *
 * @param bridge Sensor fusion-immune bridge
 * @return Confidence reduction [0-1]
 */
float portia_sensor_fusion_immune_get_confidence_reduction(
    const portia_sensor_fusion_immune_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_PORTIA_SENSOR
 *
 * @param bridge Sensor fusion-immune bridge
 * @return 0 on success, -1 on error
 */
int portia_sensor_fusion_immune_connect_bio_async(
    portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Sensor fusion-immune bridge
 * @return 0 on success
 */
int portia_sensor_fusion_immune_disconnect_bio_async(
    portia_sensor_fusion_immune_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Sensor fusion-immune bridge
 * @return true if connected
 */
bool portia_sensor_fusion_immune_is_bio_async_connected(
    const portia_sensor_fusion_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_SENSOR_FUSION_IMMUNE_BRIDGE_H */
