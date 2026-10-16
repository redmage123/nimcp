/**
 * @file nimcp_shannon_immune_bridge.h
 * @brief Shannon Entropy-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and Shannon entropy processing
 * WHY:  Biological evidence shows inflammation impairs information processing capacity;
 *       reduced entropy estimation accuracy during sickness behavior; fever-induced
 *       cognitive slowdown reduces channel capacity.
 * HOW:  Cytokines reduce entropy estimation accuracy and channel capacity; inflammation
 *       decreases information processing bandwidth; immune activation creates noise in
 *       information channels.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SHANNON ENTROPY PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair prefrontal cortex information processing
 *    - Reduce entropy estimation accuracy ("fuzzy cognition")
 *    - Decrease channel capacity due to increased neural noise
 *    - Narrow information bandwidth (cognitive tunnel vision)
 *    - Reference: Brydon et al. (2008) "Peripheral inflammation is associated
 *      with altered substantia nigra activity and psychomotor slowing"
 *
 * 2. Inflammation-Induced Noise:
 *    - Inflammation → increased spontaneous neural firing
 *    - Reduced signal-to-noise ratio in neural channels
 *    - Decreased Shannon capacity: C = B × log₂(1 + SNR)
 *    - Reference: Cunningham & Sanderson (2008) "Malaise in the water maze"
 *
 * 3. Fever-Induced Capacity Reduction:
 *    - Fever → reduced cognitive processing capacity
 *    - Decreased bandwidth for information transmission
 *    - Impaired entropy estimation ("everything seems the same")
 *    - Reference: Vollmer-Conna et al. (2004) "Cytokine correlates of sickness
 *      behavior dimensions"
 *
 * 4. Cytokine Storm Effects:
 *    - Severe inflammation → profound information processing disruption
 *    - Near-zero entropy discrimination ability
 *    - Minimal channel capacity (cognitive shutdown)
 *    - Reference: Baller et al. (2010) "Inflammation-induced cognitive dysfunction"
 *
 * SHANNON → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Low Entropy Detection (Pattern Recognition):
 *    - Detection of low-entropy patterns → potential threat signature
 *    - Repetitive/predictable patterns trigger immune surveillance
 *    - Anomaly detection (high-entropy outliers) → immune response
 *    - Reference: Poltorak et al. (1998) "Pattern recognition receptors"
 *
 * 2. Channel Capacity Bottlenecks:
 *    - Persistent low capacity → stress response → inflammation
 *    - Information overload → cytokine release
 *    - Cognitive strain → immune activation
 *
 * 3. Entropy Collapse (Information Loss):
 *    - Sudden entropy reduction → threat detection signal
 *    - Loss of information diversity → immune alert
 *    - Convergence to low-entropy state → danger signal
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    SHANNON-IMMUNE BRIDGE                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → SHANNON PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -25% │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -20% │         │                                       │  ║
 * ║   │   │ TNF-α → -30% │         ├──→ Entropy Estimation Impairment      │  ║
 * ║   │   │              │         │    (Fuzzy Cognition)                  │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     SHANNON SYSTEM              │                             │  ║
 * ║   │   │  - Entropy estimation error     │                             │  ║
 * ║   │   │  - Channel capacity reduction   │                             │  ║
 * ║   │   │  - SNR degradation              │                             │  ║
 * ║   │   │  - Bandwidth narrowing          │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -10% capacity │                                     │  ║
 * ║   │   │ REGIONAL → -30% capacity │                                     │  ║
 * ║   │   │ SYSTEMIC → -60% capacity │                                     │  ║
 * ║   │   │ STORM    → -90% capacity │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  SHANNON → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ LOW ENTROPY  │ ──→ Pattern Recognition Alert                  │  ║
 * ║   │   │ ANOMALY      │ ──→ Immune Surveillance                        │  ║
 * ║   │   │ OVERLOAD     │ ──→ Stress Response → Inflammation             │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ HIGH ENTROPY │ ──→ Normal State                               │  ║
 * ║   │   │ STABLE       │ ──→ Anti-inflammatory Signals                  │  ║
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

#ifndef NIMCP_SHANNON_IMMUNE_BRIDGE_H
#define NIMCP_SHANNON_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "information/nimcp_shannon.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine entropy estimation impact factors */
#define CYTOKINE_IL1_ENTROPY_IMPACT      -0.25f  /**< IL-1β → entropy estimation error */
#define CYTOKINE_IL6_ENTROPY_IMPACT      -0.20f  /**< IL-6 → entropy estimation error */
#define CYTOKINE_TNF_ENTROPY_IMPACT      -0.30f  /**< TNF-α → strong entropy error */
#define CYTOKINE_IFN_GAMMA_ENTROPY_IMPACT -0.15f /**< IFN-γ → mild error */
#define CYTOKINE_IL10_ENTROPY_RECOVERY    0.15f  /**< IL-10 → recovery boost */

/* Inflammation channel capacity reduction */
#define INFLAMMATION_NONE_CHANNEL_FACTOR     1.0f   /**< No reduction */
#define INFLAMMATION_LOCAL_CHANNEL_FACTOR    0.9f   /**< -10% capacity */
#define INFLAMMATION_REGIONAL_CHANNEL_FACTOR 0.7f   /**< -30% capacity */
#define INFLAMMATION_SYSTEMIC_CHANNEL_FACTOR 0.4f   /**< -60% capacity */
#define INFLAMMATION_STORM_CHANNEL_FACTOR    0.1f   /**< -90% capacity */

/* SNR degradation from inflammation */
#define INFLAMMATION_SNR_DEGRADATION_BASE       0.1f   /**< Base SNR loss */
#define INFLAMMATION_SNR_DEGRADATION_PER_LEVEL  0.15f  /**< Per inflammation level */

/* Entropy thresholds for immune activation */
#define ENTROPY_COLLAPSE_THRESHOLD        0.3f   /**< Low entropy → threat signal */
#define ENTROPY_OVERLOAD_THRESHOLD        0.95f  /**< High entropy → stress */
#define CAPACITY_BOTTLENECK_THRESHOLD     0.5f   /**< Low capacity → immune response */

/* Anomaly detection */
#define ENTROPY_ANOMALY_SENSITIVITY       2.0f   /**< Std devs for anomaly */
#define PATTERN_REPETITION_THRESHOLD      3      /**< Repetitions to trigger immune */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on Shannon entropy estimation
 *
 * Represents how cytokine levels impair information processing
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_entropy_error;           /**< IL-1β induced estimation error */
    float il6_entropy_error;           /**< IL-6 induced estimation error */
    float tnf_entropy_error;           /**< TNF-α induced estimation error */
    float ifn_gamma_entropy_error;     /**< IFN-γ induced estimation error */

    /* Anti-inflammatory effects */
    float il10_entropy_recovery;       /**< IL-10 recovery boost */

    /* Aggregate effects */
    float total_entropy_error;         /**< Combined entropy estimation error [0-1] */
    float capacity_reduction;          /**< Channel capacity loss [0-1] */
    float snr_degradation;             /**< Signal-to-noise degradation [0-1] */
    float bandwidth_narrowing;         /**< Bandwidth reduction [0-1] */
} shannon_cytokine_effects_t;

/**
 * @brief Inflammation effects on Shannon capacity
 *
 * How chronic inflammation affects information processing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= threshold */

    /* Shannon impacts */
    float channel_capacity_factor;     /**< Overall capacity [0-1] */
    float entropy_estimation_error;    /**< Entropy error [0-1] */
    float snr_reduction;               /**< SNR loss [0-1] */
    float bandwidth_reduction;         /**< Bandwidth loss [0-1] */

    /* Information loss */
    float mutual_info_degradation;     /**< Mutual info loss [0-1] */
    float discrimination_impairment;   /**< Pattern discrimination loss [0-1] */
} shannon_inflammation_state_t;

/**
 * @brief Shannon-driven immune modulation
 *
 * How entropy state affects immune function
 */
typedef struct {
    /* Shannon state */
    float current_entropy;             /**< Current entropy value [0-1] */
    float entropy_rate_change;         /**< Rate of entropy change */
    float channel_capacity;            /**< Current channel capacity */
    float snr;                         /**< Signal-to-noise ratio */

    /* Pattern detection */
    bool low_entropy_pattern_detected; /**< Repetitive pattern found */
    bool high_entropy_anomaly;         /**< Unusual high-entropy outlier */
    bool capacity_bottleneck;          /**< Channel capacity too low */

    /* Immune effects */
    float pattern_recognition_alert;   /**< Alert level [0-1] */
    float immune_surveillance_boost;   /**< Surveillance increase [0-1] */
    bool stress_inflammation_trigger;  /**< Stress-induced inflammation */

    /* Anti-inflammatory signals */
    float stable_entropy_benefit;      /**< IL-10 from stable entropy */
} shannon_immune_modulation_t;

/**
 * @brief Shannon-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_entropy_impairment;
    bool enable_inflammation_capacity_reduction;
    bool enable_pattern_recognition_immune;
    bool enable_anomaly_detection;
    bool enable_capacity_stress_response;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float anomaly_sensitivity;         /**< Anomaly detection sensitivity [0.5-3.0] */

    /* Thresholds */
    float entropy_collapse_threshold;  /**< Low entropy threshold [0.1-0.5] */
    float entropy_overload_threshold;  /**< High entropy threshold [0.8-1.0] */
    float capacity_bottleneck_threshold; /**< Low capacity threshold [0.3-0.7] */
} shannon_immune_config_t;

/**
 * @brief Complete Shannon-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    shannon_channel_t* shannon_channel;  /**< Optional Shannon channel */

    /* Current state */
    shannon_cytokine_effects_t cytokine_effects;
    shannon_inflammation_state_t inflammation_state;
    shannon_immune_modulation_t immune_modulation;

    /* Configuration */
    shannon_immune_config_t config;

    /* Timing */
    uint64_t last_update_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t pattern_alerts;
    uint32_t anomaly_detections;
    uint32_t capacity_stress_events;

    nimcp_platform_mutex_t* mutex;
} shannon_immune_bridge_t;

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
int shannon_immune_default_config(shannon_immune_config_t* config);

/**
 * @brief Create Shannon-immune bridge
 *
 * WHAT: Initialize bidirectional Shannon-immune integration
 * WHY:  Enable realistic immune-entropy coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param shannon_channel Shannon channel (optional, can be NULL)
 * @return New bridge or NULL on failure
 */
shannon_immune_bridge_t* shannon_immune_create(
    const shannon_immune_config_t* config,
    brain_immune_system_t* immune_system,
    shannon_channel_t* shannon_channel
);

/**
 * @brief Destroy Shannon-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void shannon_immune_destroy(shannon_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Shannon API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to Shannon entropy estimation
 *
 * WHAT: Impair entropy estimation based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce entropy estimation accuracy
 * HOW:  Query immune system cytokines, increase estimation error
 *
 * @param bridge Shannon-immune bridge
 * @return 0 on success
 */
int shannon_immune_apply_cytokine_effects(shannon_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to channel capacity
 *
 * WHAT: Reduce channel capacity and SNR from inflammation
 * WHY:  Inflammation increases neural noise and reduces processing
 * HOW:  Check inflammation level/duration, adjust capacity parameters
 *
 * @param bridge Shannon-immune bridge
 * @return 0 on success
 */
int shannon_immune_apply_inflammation_effects(shannon_immune_bridge_t* bridge);

/**
 * @brief Compute effective channel capacity from immune state
 *
 * WHAT: Calculate Shannon capacity given immune status
 * WHY:  Inflammation reduces information transmission capacity
 * HOW:  Map inflammation level to capacity factor [0-1]
 *
 * @param bridge Shannon-immune bridge
 * @return Capacity factor [0-1] (1.0 = normal, 0.0 = complete impairment)
 */
float shannon_immune_compute_capacity(const shannon_immune_bridge_t* bridge);

/**
 * @brief Compute SNR degradation from inflammation
 *
 * WHAT: Calculate how much inflammation reduces signal-to-noise ratio
 * WHY:  Inflammation increases spontaneous neural firing (noise)
 * HOW:  Map inflammation level to SNR degradation
 *
 * @param bridge Shannon-immune bridge
 * @return SNR degradation factor [0-1] (0 = no degradation, 1 = complete loss)
 */
float shannon_immune_compute_snr_degradation(const shannon_immune_bridge_t* bridge);

/* ============================================================================
 * Shannon → Immune API
 * ============================================================================ */

/**
 * @brief Detect low-entropy patterns and trigger immune response
 *
 * WHAT: Alert immune system when repetitive patterns detected
 * WHY:  Low entropy patterns may indicate threats
 * HOW:  Monitor entropy, trigger immune surveillance if below threshold
 *
 * @param bridge Shannon-immune bridge
 * @param entropy Current entropy value [0-1]
 * @return 0 on success
 */
int shannon_immune_detect_pattern_threat(
    shannon_immune_bridge_t* bridge,
    float entropy
);

/**
 * @brief Detect entropy anomalies and trigger immune response
 *
 * WHAT: Alert immune system when unusual entropy spikes occur
 * WHY:  Anomalies may indicate attacks or system disruption
 * HOW:  Track entropy statistics, detect outliers
 *
 * @param bridge Shannon-immune bridge
 * @param entropy Current entropy value [0-1]
 * @return 0 on success
 */
int shannon_immune_detect_anomaly(
    shannon_immune_bridge_t* bridge,
    float entropy
);

/**
 * @brief Trigger stress inflammation from capacity bottleneck
 *
 * WHAT: Activate inflammatory response from persistent low capacity
 * WHY:  Cognitive strain → stress → inflammation
 * HOW:  Track capacity, trigger cytokine release if chronically low
 *
 * @param bridge Shannon-immune bridge
 * @param capacity Current channel capacity
 * @return 0 on success
 */
int shannon_immune_trigger_capacity_stress(
    shannon_immune_bridge_t* bridge,
    float capacity
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update Shannon-immune bridge (both directions)
 *
 * WHAT: Process all Shannon-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from patterns, adjust parameters
 *
 * @param bridge Shannon-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int shannon_immune_update(
    shannon_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply modulation to Shannon system
 *
 * WHAT: Apply immune-induced modulation to Shannon processing
 * WHY:  Actually modify Shannon system based on immune state
 * HOW:  Set capacity factors, SNR, entropy estimation error
 *
 * @param bridge Shannon-immune bridge
 * @return 0 on success
 */
int shannon_immune_apply_modulation(shannon_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects on Shannon processing
 *
 * @param bridge Shannon-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int shannon_immune_get_cytokine_effects(
    const shannon_immune_bridge_t* bridge,
    shannon_cytokine_effects_t* effects
);

/**
 * @brief Get current inflammation state
 *
 * @param bridge Shannon-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int shannon_immune_get_inflammation_state(
    const shannon_immune_bridge_t* bridge,
    shannon_inflammation_state_t* state
);

/**
 * @brief Check if experiencing significant capacity loss
 *
 * WHAT: Determine if inflammation causing major capacity impairment
 * WHY:  Detect clinically significant information processing deficits
 * HOW:  Check capacity reduction threshold
 *
 * @param bridge Shannon-immune bridge
 * @return true if significant loss (>40% capacity reduction)
 */
bool shannon_immune_has_capacity_deficit(const shannon_immune_bridge_t* bridge);

/**
 * @brief Get current channel capacity factor
 *
 * @param bridge Shannon-immune bridge
 * @return Capacity factor [0-1]
 */
float shannon_immune_get_capacity_factor(const shannon_immune_bridge_t* bridge);

/**
 * @brief Get current entropy estimation error
 *
 * @param bridge Shannon-immune bridge
 * @return Entropy error [0-1]
 */
float shannon_immune_get_entropy_error(const shannon_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_SHANNON
 *
 * @param bridge Shannon-immune bridge
 * @return 0 on success, -1 on error
 */
int shannon_immune_connect_bio_async(shannon_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Shannon-immune bridge
 * @return 0 on success
 */
int shannon_immune_disconnect_bio_async(shannon_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Shannon-immune bridge
 * @return true if connected
 */
bool shannon_immune_is_bio_async_connected(const shannon_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHANNON_IMMUNE_BRIDGE_H */
