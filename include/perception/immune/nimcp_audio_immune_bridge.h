/**
 * @file nimcp_audio_immune_bridge.h
 * @brief Audio Cortex-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and auditory processing
 * WHY:  Biological evidence shows inflammation impairs auditory processing and attention,
 *       while auditory anomalies can trigger immune responses. Essential for realistic
 *       sensory-immune modeling.
 * HOW:  Cytokines reduce processing accuracy and increase noise sensitivity,
 *       auditory threats trigger immune activation, inflammation reduces processing bandwidth.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → AUDIO PATHWAYS:
 * -------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Cross blood-brain barrier
 *    - Impair cochlear function and auditory nerve conduction
 *    - Reduce frequency discrimination and temporal processing
 *    - Increase auditory fatigue and noise sensitivity
 *    - Reference: Taishi et al. (2012) "Interleukin-1 and auditory processing"
 *
 * 2. IL-6 and Auditory Cortex Plasticity:
 *    - Elevated IL-6 reduces synaptic plasticity in A1
 *    - Impairs auditory learning and sound discrimination
 *    - Associated with hearing loss during inflammation
 *    - Reference: Fujioka et al. (2014) "IL-6 and auditory cortex function"
 *
 * 3. Chronic Inflammation:
 *    - Sustained elevation → auditory processing deficits
 *    - Reduced attention to auditory stimuli (sickness behavior)
 *    - Increased susceptibility to noise-induced damage
 *    - Tinnitus onset and maintenance
 *    - Reference: Eggermont & Roberts (2004) "Tinnitus and neuroinflammation"
 *
 * 4. Sickness Behavior:
 *    - Cytokines induce withdrawal and reduced sensory engagement
 *    - Decreased auditory attention and orienting
 *    - Reduced processing bandwidth (energy conservation)
 *    - Reference: Dantzer et al. (2008) "From inflammation to sickness and depression"
 *
 * AUDIO → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Auditory Threat Detection:
 *    - Sudden loud noise → stress response → cortisol + inflammation
 *    - Persistent noise pollution → chronic immune activation
 *    - Acoustic startle → HPA axis activation
 *    - Reference: Munzel et al. (2018) "Noise pollution and inflammatory response"
 *
 * 2. Auditory Anomalies as Threats:
 *    - Unexpected/novel sounds trigger immune surveillance
 *    - Pattern violations (missing expected sounds) signal danger
 *    - Tinnitus (phantom sounds) trigger immune activation
 *    - Reference: Mazurek et al. (2010) "Immune system and tinnitus"
 *
 * 3. Auditory Processing Failure:
 *    - Speech comprehension failure under noise → stress response
 *    - Processing overload → inflammatory markers
 *    - Sensory gating failure → immune activation
 *    - Reference: Kraus & White-Schwoch (2015) "Auditory processing and immunity"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    AUDIO-IMMUNE BRIDGE                                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → AUDIO PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.4 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.3 │         ├──→ Auditory Impairment                │  ║
 * ║   │   │              │         │    (Reduced accuracy, noise sensitive)│  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     AUDIO CORTEX                │                             │  ║
 * ║   │   │  - Frequency discrimination ↓   │                             │  ║
 * ║   │   │  - Temporal resolution ↓        │                             │  ║
 * ║   │   │  - Noise sensitivity ↑          │                             │  ║
 * ║   │   │  - Processing bandwidth ↓       │                             │  ║
 * ║   │   │  - Attention to sound ↓         │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +0.2       │     Recovery, Improved Processing               │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  AUDIO → IMMUNE PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ LOUD NOISE   │ ──→ Stress Response → Inflammation              │  ║
 * ║   │   │ ANOMALIES    │ ──→ Immune Surveillance                         │  ║
 * ║   │   │ TINNITUS     │ ──→ Chronic Immune Activation                   │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ QUIET/CALM   │ ──→ Immune Enhancement                          │  ║
 * ║   │   │ MUSIC        │ ──→ IL-10 Release                               │  ║
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

#ifndef NIMCP_AUDIO_IMMUNE_BRIDGE_H
#define NIMCP_AUDIO_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "perception/nimcp_audio_cortex.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine audio impact factors (processing modulation) */
#define CYTOKINE_IL1_AUDIO_IMPACT      -0.3f   /**< IL-1β → reduced discrimination */
#define CYTOKINE_IL6_AUDIO_IMPACT      -0.4f   /**< IL-6 → strong processing impairment */
#define CYTOKINE_TNF_AUDIO_IMPACT      -0.3f   /**< TNF-α → reduced accuracy */
#define CYTOKINE_IFN_GAMMA_AUDIO_IMPACT -0.2f  /**< IFN-γ → mild impairment */
#define CYTOKINE_IL10_AUDIO_IMPACT      0.2f   /**< IL-10 → recovery/enhancement */

/* Inflammation-auditory impairment mapping */
#define INFLAMMATION_AUDIO_THRESHOLD    0.5f   /**< Inflammation level for noticeable impairment */
#define INFLAMMATION_NOISE_SENSITIVITY  0.7f   /**< Inflammation level for high noise sensitivity */

/* Auditory threat thresholds */
#define AUDIO_THREAT_LOUDNESS_THRESHOLD 0.8f   /**< Loudness level to trigger immune response */
#define AUDIO_THREAT_NOVELTY_THRESHOLD  0.9f   /**< Novelty level to trigger surveillance */
#define AUDIO_ANOMALY_SEVERITY_MULTIPLIER 1.2f /**< Auditory anomaly severity boost */

/* Processing bandwidth reduction under inflammation */
#define MAX_BANDWIDTH_REDUCTION         0.6f   /**< Maximum bandwidth reduction (40% capacity) */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine auditory effects
 *
 * Represents how cytokine levels modulate auditory processing
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_discrimination_loss;    /**< IL-1β induced frequency discrimination loss */
    float il6_processing_impairment;  /**< IL-6 induced processing impairment */
    float tnf_accuracy_reduction;     /**< TNF-α induced accuracy reduction */
    float ifn_gamma_sensitivity_loss; /**< IFN-γ induced sensitivity loss */

    /* Anti-inflammatory effects */
    float il10_recovery_boost;        /**< IL-10 recovery/enhancement effect */

    /* Aggregate effects */
    float total_processing_impact;    /**< Combined processing modulation */
    float noise_sensitivity_increase; /**< Increased noise sensitivity [0-1] */
    float attention_impairment;       /**< Auditory attention reduction [0-1] */
    float fatigue_level;              /**< Auditory fatigue [0-1] */
} cytokine_audio_effects_t;

/**
 * @brief Inflammation auditory state
 *
 * How chronic inflammation affects auditory processing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Auditory impacts */
    float processing_accuracy;         /**< Processing accuracy [0-1], 1=normal */
    float frequency_discrimination;    /**< Freq discrimination [0-1], 1=normal */
    float temporal_resolution;         /**< Temporal resolution [0-1], 1=normal */
    float noise_tolerance;             /**< Noise tolerance [0-1], 1=high */
    float processing_bandwidth;        /**< Bandwidth available [0-1], 1=full */
    float tinnitus_severity;           /**< Tinnitus severity [0-1] */

    /* Attention */
    float auditory_attention;          /**< Attention to sound [0-1] */
    float orienting_response;          /**< Orienting to novel sound [0-1] */
} inflammation_audio_state_t;

/**
 * @brief Auditory threat immune response
 *
 * How auditory anomalies trigger immune activity
 */
typedef struct {
    /* Threat indicators */
    float loudness_level;              /**< Sound intensity [0-1] */
    float novelty_score;               /**< Novelty/unexpectedness [0-1] */
    float anomaly_score;               /**< Pattern violation severity [0-1] */
    float processing_failure_rate;     /**< Processing failure rate [0-1] */

    /* Immune triggers */
    bool stress_response_triggered;    /**< HPA axis activated */
    bool immune_surveillance_active;   /**< Immune monitoring enabled */
    float immune_activation_level;     /**< Activation level [0-1] */

    /* Chronic exposure effects */
    float noise_exposure_duration_sec; /**< Duration of noise exposure */
    float immune_sensitization;        /**< Sensitization level [0-1] */
} audio_immune_trigger_t;

/**
 * @brief Auditory recovery immune enhancement
 *
 * How calm/music/quiet environments boost immunity
 */
typedef struct {
    /* Auditory environment */
    float quietness_level;             /**< Quietness [0-1] */
    float music_presence;              /**< Music detected [0-1] */
    float predictability;              /**< Sound pattern predictability [0-1] */

    /* Immune benefits */
    float immune_enhancement;          /**< Improved function [0-1] */
    float il10_release_boost;          /**< Anti-inflammatory boost */
    float inflammation_reduction;      /**< Reduced inflammation [0-1] */
    float stress_reduction;            /**< Reduced stress response */
} audio_immune_boost_t;

/**
 * @brief Complete audio-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    audio_cortex_t* audio_cortex;

    /* Current state */
    cytokine_audio_effects_t cytokine_effects;
    inflammation_audio_state_t inflammation_state;
    audio_immune_trigger_t audio_trigger;
    audio_immune_boost_t audio_boost;

    /* Integration flags */
    bool enable_cytokine_audio_modulation;
    bool enable_inflammation_processing_impairment;
    bool enable_audio_immune_trigger;
    bool enable_audio_immune_boost;
    bool enable_tinnitus_inflammation_coupling;

    /* Processing state tracking */
    float baseline_processing_accuracy;  /**< Baseline before inflammation */
    float baseline_noise_tolerance;      /**< Baseline noise tolerance */
    uint32_t anomaly_count;              /**< Anomalies detected */
    uint32_t processing_failures;        /**< Processing failures */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t audio_triggered_responses;
    uint32_t audio_boosts;
    uint32_t tinnitus_episodes;
    } audio_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_audio_modulation;
    bool enable_inflammation_processing_impairment;
    bool enable_audio_immune_trigger;
    bool enable_audio_immune_boost;
    bool enable_tinnitus_inflammation_coupling;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float threat_trigger_sensitivity;  /**< Audio threat multiplier [0.5-2.0] */

    /* Thresholds */
    float loudness_trigger_threshold;  /**< Loudness to trigger immune [0.6-0.9] */
    float anomaly_trigger_threshold;   /**< Anomaly severity to trigger [0.6-0.9] */
    float inflammation_audio_threshold; /**< Inflammation for processing impairment [0.3-0.7] */
} audio_immune_config_t;

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
int audio_immune_default_config(audio_immune_config_t* config);

/**
 * @brief Create audio-immune bridge
 *
 * WHAT: Initialize bidirectional audio-immune integration
 * WHY:  Enable realistic immune-auditory coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param audio_cortex Audio cortex
 * @return New bridge or NULL on failure
 */
audio_immune_bridge_t* audio_immune_bridge_create(
    const audio_immune_config_t* config,
    brain_immune_system_t* immune_system,
    audio_cortex_t* audio_cortex
);

/**
 * @brief Destroy audio-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void audio_immune_bridge_destroy(audio_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Audio API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to auditory processing
 *
 * WHAT: Modulate audio processing based on cytokine levels
 * WHY:  Pro-inflammatory cytokines impair auditory function
 * HOW:  Query immune system cytokines, adjust processing accuracy/bandwidth
 *
 * @param bridge Audio-immune bridge
 * @return 0 on success
 */
int audio_immune_apply_cytokine_effects(audio_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to auditory processing
 *
 * WHAT: Induce processing impairment and noise sensitivity from inflammation
 * WHY:  Chronic inflammation causes auditory deficits
 * HOW:  Check inflammation duration/level, reduce processing capabilities
 *
 * @param bridge Audio-immune bridge
 * @return 0 on success
 */
int audio_immune_apply_inflammation_effects(audio_immune_bridge_t* bridge);

/**
 * @brief Compute processing bandwidth reduction from inflammation
 *
 * WHAT: Calculate processing bandwidth loss from immune state
 * WHY:  Inflammation reduces neural resources for auditory processing
 * HOW:  Map inflammation level/duration to bandwidth reduction [0-1]
 *
 * @param bridge Audio-immune bridge
 * @return Bandwidth reduction factor [0-1], 0=no reduction, 1=max reduction
 */
float audio_immune_compute_bandwidth_reduction(const audio_immune_bridge_t* bridge);

/**
 * @brief Compute noise sensitivity from inflammation
 *
 * WHAT: Calculate increased noise sensitivity from immune state
 * WHY:  Inflammation increases susceptibility to noise-induced damage
 * HOW:  Map inflammation to noise sensitivity increase
 *
 * @param bridge Audio-immune bridge
 * @return Noise sensitivity multiplier [1.0-3.0], 1.0=normal
 */
float audio_immune_compute_noise_sensitivity(const audio_immune_bridge_t* bridge);

/* ============================================================================
 * Audio → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from auditory threat
 *
 * WHAT: Activate immune system from loud/novel/anomalous sound
 * WHY:  Auditory threats activate stress and immune responses
 * HOW:  Check audio threat levels, trigger cytokine release
 *
 * @param bridge Audio-immune bridge
 * @param loudness Sound loudness [0-1]
 * @param novelty Sound novelty [0-1]
 * @param anomaly_score Anomaly severity [0-1]
 * @return 0 on success
 */
int audio_immune_trigger_from_threat(
    audio_immune_bridge_t* bridge,
    float loudness,
    float novelty,
    float anomaly_score
);

/**
 * @brief Trigger immune response from processing failure
 *
 * WHAT: Activate immune system when auditory processing fails
 * WHY:  Processing failure signals sensory system stress
 * HOW:  Monitor failure rate, trigger immune activation
 *
 * @param bridge Audio-immune bridge
 * @param failure_rate Processing failure rate [0-1]
 * @return 0 on success
 */
int audio_immune_trigger_from_processing_failure(
    audio_immune_bridge_t* bridge,
    float failure_rate
);

/**
 * @brief Amplify inflammation from tinnitus
 *
 * WHAT: Increase inflammatory response during tinnitus episodes
 * WHY:  Tinnitus is associated with neuroinflammation
 * HOW:  Query tinnitus severity, scale immune inflammation level
 *
 * @param bridge Audio-immune bridge
 * @param tinnitus_severity Tinnitus severity [0-1]
 * @return 0 on success
 */
int audio_immune_amplify_tinnitus_inflammation(
    audio_immune_bridge_t* bridge,
    float tinnitus_severity
);

/**
 * @brief Boost immune function from calm auditory environment
 *
 * WHAT: Enhance immunity from quiet/music/predictable sounds
 * WHY:  Calm auditory environment reduces stress and boosts immunity
 * HOW:  Query audio environment, release IL-10, reduce inflammation
 *
 * @param bridge Audio-immune bridge
 * @param quietness Quietness level [0-1]
 * @param music_presence Music detected [0-1]
 * @param predictability Pattern predictability [0-1]
 * @return 0 on success
 */
int audio_immune_boost_from_calm_environment(
    audio_immune_bridge_t* bridge,
    float quietness,
    float music_presence,
    float predictability
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update audio-immune bridge (both directions)
 *
 * WHAT: Process all immune-audio interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from threats, boost from calm
 *
 * @param bridge Audio-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int audio_immune_bridge_update(
    audio_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine auditory effects
 *
 * @param bridge Audio-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int audio_immune_get_cytokine_effects(
    const audio_immune_bridge_t* bridge,
    cytokine_audio_effects_t* effects
);

/**
 * @brief Get current inflammation auditory state
 *
 * @param bridge Audio-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int audio_immune_get_inflammation_state(
    const audio_immune_bridge_t* bridge,
    inflammation_audio_state_t* state
);

/**
 * @brief Check if experiencing auditory impairment
 *
 * WHAT: Determine if cytokines causing significant auditory impairment
 * WHY:  Auditory impairment is distinct from other sickness behaviors
 * HOW:  Check processing accuracy and bandwidth thresholds
 *
 * @param bridge Audio-immune bridge
 * @return true if experiencing significant impairment
 */
bool audio_immune_is_impaired(const audio_immune_bridge_t* bridge);

/**
 * @brief Get processing accuracy reduction
 *
 * @param bridge Audio-immune bridge
 * @return Accuracy reduction [0-1], 0=no reduction, 1=complete loss
 */
float audio_immune_get_accuracy_reduction(const audio_immune_bridge_t* bridge);

/**
 * @brief Get tinnitus severity
 *
 * @param bridge Audio-immune bridge
 * @return Tinnitus severity [0-1]
 */
float audio_immune_get_tinnitus_severity(const audio_immune_bridge_t* bridge);

/**
 * @brief Get auditory attention level
 *
 * @param bridge Audio-immune bridge
 * @return Attention level [0-1], 1=full attention, 0=no attention
 */
float audio_immune_get_attention_level(const audio_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_AUDIO
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int audio_immune_connect_bio_async(audio_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int audio_immune_disconnect_bio_async(audio_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool audio_immune_is_bio_async_connected(const audio_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIO_IMMUNE_BRIDGE_H */
