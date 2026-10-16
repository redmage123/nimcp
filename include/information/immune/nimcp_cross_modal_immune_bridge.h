/**
 * @file nimcp_cross_modal_immune_bridge.h
 * @brief Cross-Modal Integration-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and cross-modal information integration
 * WHY:  Biological evidence shows inflammation impairs multi-sensory binding; sickness behavior
 *       disrupts cross-modal integration (McGurk effect reduced during illness); TNF-α disrupts
 *       binding of audio-visual information.
 * HOW:  Cytokines reduce cross-modal transfer efficiency and mutual information; inflammation
 *       creates bottlenecks in sensory integration; immune activation impairs multi-sensory
 *       binding in superior temporal sulcus and parietal cortex.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → CROSS-MODAL PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair superior temporal sulcus (STS) audiovisual integration
 *    - Reduce cross-modal binding in parietal cortex
 *    - Decrease mutual information transfer between modalities
 *    - Impair McGurk effect and multi-sensory enhancement
 *    - Reference: Harrar et al. (2014) "Multisensory integration is independent
 *      of perceived simultaneity"
 *    - Reference: Stevenson et al. (2012) "The impact of multisensory integration
 *      deficits on speech perception in children with autism"
 *
 * 2. Inflammation-Induced Binding Disruption:
 *    - TNF-α disrupts synchronous firing across cortices
 *    - Reduced coherence between visual and auditory cortex
 *    - Impaired temporal binding window
 *    - Decreased cross-modal enhancement
 *    - Reference: Calvert & Thesen (2004) "Multisensory integration:
 *      methodological approaches and emerging principles"
 *
 * 3. Sickness Behavior Effects:
 *    - Fever → reduced multi-sensory integration
 *    - Social withdrawal → impaired audiovisual speech perception
 *    - Malaise → narrowed sensory processing to single modality
 *    - Reference: Dantzer et al. (2008) "From inflammation to sickness
 *      and depression"
 *
 * 4. Cytokine Storm Effects:
 *    - Severe inflammation → complete cross-modal breakdown
 *    - Sensory modalities process independently
 *    - Loss of multi-sensory enhancement
 *    - Reference: Perry et al. (2003) "Microglia and macrophages of the
 *      central nervous system: the contribution of microglia priming
 *      and systemic inflammation to chronic neurodegeneration"
 *
 * CROSS-MODAL → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Integration Failure Detection:
 *    - Failed cross-modal binding → threat signal
 *    - Sensory mismatch (visual-auditory conflict) → immune alert
 *    - Loss of multi-sensory enhancement → stress response
 *    - Reference: Meredith & Stein (1983) "Interactions among converging
 *      sensory inputs in the superior colliculus"
 *
 * 2. Bottleneck-Induced Stress:
 *    - Persistent cross-modal bottleneck → cognitive strain
 *    - Information overload at integration sites → inflammation
 *    - Sensory conflict → cytokine release
 *
 * 3. Stable Integration Benefits:
 *    - Successful multi-sensory binding → anti-inflammatory signals
 *    - High cross-modal mutual information → IL-10 release
 *    - Enhanced integration → reduced stress markers
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    CROSS-MODAL-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → CROSS-MODAL PATHWAYS                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -20% │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -15% │         │                                       │  ║
 * ║   │   │ TNF-α → -35% │         ├──→ Cross-Modal Binding Disruption     │  ║
 * ║   │   │              │         │    (Integration Failure)              │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     CROSS-MODAL SYSTEM          │                             │  ║
 * ║   │   │  - Transfer efficiency reduced  │                             │  ║
 * ║   │   │  - Mutual information decreased │                             │  ║
 * ║   │   │  - Binding window narrowed      │                             │  ║
 * ║   │   │  - Enhancement suppressed       │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -10% transfer │                                     │  ║
 * ║   │   │ REGIONAL → -35% transfer │                                     │  ║
 * ║   │   │ SYSTEMIC → -65% transfer │                                     │  ║
 * ║   │   │ STORM    → -95% transfer │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  CROSS-MODAL → IMMUNE PATHWAYS                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ BINDING FAIL │ ──→ Threat Signal                              │  ║
 * ║   │   │ MISMATCH     │ ──→ Immune Alert                               │  ║
 * ║   │   │ BOTTLENECK   │ ──→ Stress Response → Inflammation             │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ GOOD BINDING │ ──→ Anti-inflammatory (IL-10)                  │  ║
 * ║   │   │ HIGH MI      │ ──→ Stress Reduction                           │  ║
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

#ifndef NIMCP_CROSS_MODAL_IMMUNE_BRIDGE_H
#define NIMCP_CROSS_MODAL_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "information/nimcp_cross_modal.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine cross-modal binding impact factors */
#define CYTOKINE_IL1_BINDING_IMPACT      -0.20f  /**< IL-1β → binding disruption */
#define CYTOKINE_IL6_BINDING_IMPACT      -0.15f  /**< IL-6 → binding disruption */
#define CYTOKINE_TNF_BINDING_IMPACT      -0.35f  /**< TNF-α → strong disruption */
#define CYTOKINE_IFN_GAMMA_BINDING_IMPACT -0.10f /**< IFN-γ → mild disruption */
#define CYTOKINE_IL10_BINDING_RECOVERY    0.20f  /**< IL-10 → recovery boost */

/* Inflammation transfer efficiency reduction */
#define INFLAMMATION_NONE_TRANSFER_FACTOR     1.0f   /**< No reduction */
#define INFLAMMATION_LOCAL_TRANSFER_FACTOR    0.9f   /**< -10% efficiency */
#define INFLAMMATION_REGIONAL_TRANSFER_FACTOR 0.65f  /**< -35% efficiency */
#define INFLAMMATION_SYSTEMIC_TRANSFER_FACTOR 0.35f  /**< -65% efficiency */
#define INFLAMMATION_STORM_TRANSFER_FACTOR    0.05f  /**< -95% efficiency */

/* Binding window narrowing from inflammation */
#define INFLAMMATION_WINDOW_BASE         0.05f   /**< Base narrowing */
#define INFLAMMATION_WINDOW_PER_LEVEL    0.15f   /**< Per inflammation level */

/* Integration thresholds for immune activation */
#define BINDING_FAILURE_THRESHOLD        0.3f    /**< Low binding → threat */
#define MUTUAL_INFO_COLLAPSE_THRESHOLD   0.2f    /**< Low MI → alert */
#define BOTTLENECK_THRESHOLD             0.5f    /**< Low transfer → stress */
#define MISMATCH_SENSITIVITY             2.0f    /**< Std devs for mismatch */

/* Multi-sensory enhancement factors */
#define ENHANCEMENT_BASELINE             1.0f    /**< No enhancement */
#define ENHANCEMENT_MAX                  2.0f    /**< Maximum enhancement */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on cross-modal binding
 *
 * Represents how cytokine levels impair multi-sensory integration
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_binding_disruption;      /**< IL-1β induced binding loss */
    float il6_binding_disruption;      /**< IL-6 induced binding loss */
    float tnf_binding_disruption;      /**< TNF-α induced binding loss */
    float ifn_gamma_binding_disruption; /**< IFN-γ induced binding loss */

    /* Anti-inflammatory effects */
    float il10_binding_recovery;       /**< IL-10 recovery boost */

    /* Aggregate effects */
    float total_binding_impairment;    /**< Combined binding loss [0-1] */
    float transfer_efficiency_loss;    /**< Transfer efficiency reduction [0-1] */
    float mutual_info_degradation;     /**< Mutual information loss [0-1] */
    float temporal_window_narrowing;   /**< Binding window reduction [0-1] */
} cross_modal_cytokine_effects_t;

/**
 * @brief Inflammation effects on cross-modal integration
 *
 * How chronic inflammation affects multi-sensory binding
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= threshold */

    /* Cross-modal impacts */
    float transfer_efficiency_factor;  /**< Overall efficiency [0-1] */
    float binding_strength_reduction;  /**< Binding strength loss [0-1] */
    float temporal_window_ms;          /**< Binding window in ms */
    float enhancement_suppression;     /**< Multi-sensory enhancement loss [0-1] */

    /* Integration loss */
    float coherence_degradation;       /**< Cross-cortex coherence loss [0-1] */
    float synchrony_impairment;        /**< Synchronous firing loss [0-1] */
} cross_modal_inflammation_state_t;

/**
 * @brief Cross-modal-driven immune modulation
 *
 * How integration state affects immune function
 */
typedef struct {
    /* Cross-modal state */
    float transfer_efficiency;         /**< Current transfer efficiency [0-1] */
    float mutual_information;          /**< Current mutual info (bits) */
    float binding_strength;            /**< Binding strength [0-1] */
    float enhancement_factor;          /**< Multi-sensory enhancement [1-2] */

    /* Integration failures */
    bool binding_failure_detected;     /**< Binding below threshold */
    bool sensory_mismatch_detected;    /**< Conflict between modalities */
    bool bottleneck_detected;          /**< Transfer bottleneck */

    /* Immune effects */
    float threat_signal_level;         /**< Threat signal [0-1] */
    float immune_alert_level;          /**< Alert level [0-1] */
    bool stress_inflammation_trigger;  /**< Stress-induced inflammation */

    /* Anti-inflammatory signals */
    float stable_integration_benefit;  /**< IL-10 from good integration */
} cross_modal_immune_modulation_t;

/**
 * @brief Cross-modal-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_binding_impairment;
    bool enable_inflammation_transfer_reduction;
    bool enable_binding_failure_immune;
    bool enable_mismatch_detection;
    bool enable_bottleneck_stress_response;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float mismatch_sensitivity;        /**< Mismatch detection sensitivity [0.5-3.0] */

    /* Thresholds */
    float binding_failure_threshold;   /**< Low binding threshold [0.1-0.5] */
    float mutual_info_threshold;       /**< Low MI threshold [0.1-0.4] */
    float bottleneck_threshold;        /**< Low efficiency threshold [0.3-0.7] */
} cross_modal_immune_config_t;

/**
 * @brief Complete cross-modal-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    cross_modal_channel_t* cross_modal_channel;  /**< Optional cross-modal channel */

    /* Current state */
    cross_modal_cytokine_effects_t cytokine_effects;
    cross_modal_inflammation_state_t inflammation_state;
    cross_modal_immune_modulation_t immune_modulation;

    /* Configuration */
    cross_modal_immune_config_t config;

    /* Timing */
    uint64_t last_update_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t binding_failures;
    uint32_t mismatch_detections;
    uint32_t bottleneck_events;

    nimcp_platform_mutex_t* mutex;
} cross_modal_immune_bridge_t;

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
int cross_modal_immune_default_config(cross_modal_immune_config_t* config);

/**
 * @brief Create cross-modal-immune bridge
 *
 * WHAT: Initialize bidirectional cross-modal-immune integration
 * WHY:  Enable realistic immune-integration coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param cross_modal_channel Cross-modal channel (optional, can be NULL)
 * @return New bridge or NULL on failure
 */
cross_modal_immune_bridge_t* cross_modal_immune_create(
    const cross_modal_immune_config_t* config,
    brain_immune_system_t* immune_system,
    cross_modal_channel_t* cross_modal_channel
);

/**
 * @brief Destroy cross-modal-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void cross_modal_immune_destroy(cross_modal_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Cross-Modal API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to cross-modal binding
 *
 * WHAT: Impair multi-sensory binding based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce cross-modal integration
 * HOW:  Query immune system cytokines, reduce binding strength/efficiency
 *
 * @param bridge Cross-modal-immune bridge
 * @return 0 on success
 */
int cross_modal_immune_apply_cytokine_effects(cross_modal_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to transfer efficiency
 *
 * WHAT: Reduce cross-modal transfer from inflammation
 * WHY:  Inflammation disrupts coherence and synchrony across cortices
 * HOW:  Check inflammation level/duration, adjust efficiency parameters
 *
 * @param bridge Cross-modal-immune bridge
 * @return 0 on success
 */
int cross_modal_immune_apply_inflammation_effects(cross_modal_immune_bridge_t* bridge);

/**
 * @brief Compute effective transfer efficiency from immune state
 *
 * WHAT: Calculate cross-modal efficiency given immune status
 * WHY:  Inflammation reduces information transfer across modalities
 * HOW:  Map inflammation level to efficiency factor [0-1]
 *
 * @param bridge Cross-modal-immune bridge
 * @return Efficiency factor [0-1] (1.0 = normal, 0.0 = complete impairment)
 */
float cross_modal_immune_compute_efficiency(const cross_modal_immune_bridge_t* bridge);

/**
 * @brief Compute binding window narrowing from inflammation
 *
 * WHAT: Calculate how much inflammation narrows temporal binding window
 * WHY:  Inflammation reduces window for multi-sensory integration
 * HOW:  Map inflammation level to window duration (ms)
 *
 * @param bridge Cross-modal-immune bridge
 * @return Binding window in milliseconds
 */
float cross_modal_immune_compute_binding_window(const cross_modal_immune_bridge_t* bridge);

/* ============================================================================
 * Cross-Modal → Immune API
 * ============================================================================ */

/**
 * @brief Detect binding failure and trigger immune response
 *
 * WHAT: Alert immune system when cross-modal binding fails
 * WHY:  Binding failure may indicate sensory disruption/threat
 * HOW:  Monitor binding strength, trigger immune if below threshold
 *
 * @param bridge Cross-modal-immune bridge
 * @param binding_strength Current binding strength [0-1]
 * @return 0 on success
 */
int cross_modal_immune_detect_binding_failure(
    cross_modal_immune_bridge_t* bridge,
    float binding_strength
);

/**
 * @brief Detect sensory mismatch and trigger immune alert
 *
 * WHAT: Alert immune system when modalities conflict
 * WHY:  Sensory conflict may indicate attack or system disruption
 * HOW:  Track cross-modal coherence, detect mismatches
 *
 * @param bridge Cross-modal-immune bridge
 * @param coherence Cross-modal coherence [0-1]
 * @return 0 on success
 */
int cross_modal_immune_detect_mismatch(
    cross_modal_immune_bridge_t* bridge,
    float coherence
);

/**
 * @brief Trigger stress inflammation from integration bottleneck
 *
 * WHAT: Activate inflammatory response from persistent low efficiency
 * WHY:  Cognitive strain from integration failure → stress → inflammation
 * HOW:  Track efficiency, trigger cytokine release if chronically low
 *
 * @param bridge Cross-modal-immune bridge
 * @param efficiency Current transfer efficiency
 * @return 0 on success
 */
int cross_modal_immune_trigger_bottleneck_stress(
    cross_modal_immune_bridge_t* bridge,
    float efficiency
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update cross-modal-immune bridge (both directions)
 *
 * WHAT: Process all cross-modal-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from failures, adjust parameters
 *
 * @param bridge Cross-modal-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int cross_modal_immune_update(
    cross_modal_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply modulation to cross-modal system
 *
 * WHAT: Apply immune-induced modulation to cross-modal integration
 * WHY:  Actually modify cross-modal system based on immune state
 * HOW:  Set efficiency factors, binding strength, temporal windows
 *
 * @param bridge Cross-modal-immune bridge
 * @return 0 on success
 */
int cross_modal_immune_apply_modulation(cross_modal_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects on cross-modal processing
 *
 * @param bridge Cross-modal-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int cross_modal_immune_get_cytokine_effects(
    const cross_modal_immune_bridge_t* bridge,
    cross_modal_cytokine_effects_t* effects
);

/**
 * @brief Get current inflammation state
 *
 * @param bridge Cross-modal-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int cross_modal_immune_get_inflammation_state(
    const cross_modal_immune_bridge_t* bridge,
    cross_modal_inflammation_state_t* state
);

/**
 * @brief Check if experiencing significant binding loss
 *
 * WHAT: Determine if inflammation causing major binding impairment
 * WHY:  Detect clinically significant multi-sensory deficits
 * HOW:  Check binding strength threshold
 *
 * @param bridge Cross-modal-immune bridge
 * @return true if significant loss (>50% binding reduction)
 */
bool cross_modal_immune_has_binding_deficit(const cross_modal_immune_bridge_t* bridge);

/**
 * @brief Get current transfer efficiency factor
 *
 * @param bridge Cross-modal-immune bridge
 * @return Efficiency factor [0-1]
 */
float cross_modal_immune_get_efficiency_factor(const cross_modal_immune_bridge_t* bridge);

/**
 * @brief Get current binding strength
 *
 * @param bridge Cross-modal-immune bridge
 * @return Binding strength [0-1]
 */
float cross_modal_immune_get_binding_strength(const cross_modal_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_CROSS_MODAL
 *
 * @param bridge Cross-modal-immune bridge
 * @return 0 on success, -1 on error
 */
int cross_modal_immune_connect_bio_async(cross_modal_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Cross-modal-immune bridge
 * @return 0 on success
 */
int cross_modal_immune_disconnect_bio_async(cross_modal_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Cross-modal-immune bridge
 * @return true if connected
 */
bool cross_modal_immune_is_bio_async_connected(const cross_modal_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CROSS_MODAL_IMMUNE_BRIDGE_H */
