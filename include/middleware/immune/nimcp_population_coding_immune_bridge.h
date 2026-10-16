/**
 * @file nimcp_population_coding_immune_bridge.h
 * @brief Population Coding-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and population coding
 * WHY:  Biological evidence shows inflammation affects neural noise and tuning curves.
 *       Essential for realistic sensory/motor processing under immune challenge.
 * HOW:  Cytokines increase neural noise reducing population code precision,
 *       inflammation modulates neural gain, population anomalies trigger immune responses.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → POPULATION CODING PATHWAYS:
 * ------------------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Increase neural noise in population responses
 *    - Reduce signal-to-noise ratio in population vectors
 *    - Broaden tuning curves → reduced selectivity
 *    - Decrease population coherence and synchrony
 *    - Reference: Dantzer et al. (2008) "Cytokine-induced sickness behavior"
 *
 * 2. IL-6 Effects on Sensory Precision:
 *    - Reduces sensory precision in population codes
 *    - Increases variability in tuning curve responses
 *    - Impairs center-of-mass localization accuracy
 *    - Reference: Felger & Miller (2012) "Cytokine effects on basal ganglia"
 *
 * 3. Chronic Inflammation:
 *    - Sustained reduction in population coding fidelity
 *    - Increased trial-to-trial variability
 *    - Reduced population vector magnitude (weak responses)
 *    - Impaired sparse distributed representations
 *    - Reference: Miller & Raison (2016) "Inflammation in depression"
 *
 * 4. Neural Gain Modulation:
 *    - TNF-α reduces neural gain (response amplitude)
 *    - IL-1β increases baseline firing → reduced dynamic range
 *    - Anti-inflammatory IL-10 restores normal gain
 *    - Reference: Stellwagen & Malenka (2006) "Synaptic scaling by TNF-α"
 *
 * POPULATION CODING → IMMUNE PATHWAYS:
 * ------------------------------------
 * 1. Population Code Anomalies:
 *    - Abnormal population vector directions → immune alert
 *    - Excessive noise in population responses → threat detection
 *    - Loss of synchrony → immune surveillance trigger
 *    - Reference: Fault detection via population code deviations
 *
 * 2. Tuning Curve Degradation:
 *    - Abnormal tuning width changes → immune response
 *    - Unexpected gain changes → cytokine release
 *    - Loss of preferred direction selectivity → inflammation
 *
 * 3. Population Reliability:
 *    - Low trial-to-trial reliability → immune system alert
 *    - Reduced correlation matrix structure → threat signal
 *    - Sparse code breakdown → immune activation
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              POPULATION CODING-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            IMMUNE → POPULATION CODING PATHWAYS                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +noise│  ───────┐                                      │  ║
 * ║   │   │ IL-6  → -precision      │                                      │  ║
 * ║   │   │ TNF-α → -gain │         ├──→ Neural Noise Increase             │  ║
 * ║   │   │              │         │    Tuning Curve Broadening            │  ║
 * ║   │   └──────────────┘         │    Population Coherence Loss          │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   POPULATION CODING SYSTEM      │                             │  ║
 * ║   │   │  - Vector sum precision         │                             │  ║
 * ║   │   │  - Tuning curve width           │                             │  ║
 * ║   │   │  - Neural gain                  │                             │  ║
 * ║   │   │  - Population synchrony         │                             │  ║
 * ║   │   │  - Sparse code reliability      │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │ +precision   │     Restore Precision & Gain                    │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │          POPULATION CODING → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ ANOMALIES    │ ──→ Immune Alert                                │  ║
 * ║   │   │ High Noise   │ ──→ Cytokine Release                            │  ║
 * ║   │   │ Low Synchrony│ ──→ Inflammation Trigger                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ RESTORATION  │ ──→ IL-10 Release                               │  ║
 * ║   │   │ Normal Codes │ ──→ Inflammation Resolution                     │  ║
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

#ifndef NIMCP_POPULATION_CODING_IMMUNE_BRIDGE_H
#define NIMCP_POPULATION_CODING_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/encoding/nimcp_population_coding.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine effects on population coding */
#define CYTOKINE_IL1_NOISE_INCREASE       0.3f   /**< IL-1β increases neural noise */
#define CYTOKINE_IL6_PRECISION_REDUCTION  0.4f   /**< IL-6 reduces precision */
#define CYTOKINE_TNF_GAIN_REDUCTION       0.5f   /**< TNF-α reduces gain */
#define CYTOKINE_IFN_NOISE_INCREASE       0.2f   /**< IFN-γ mild noise increase */
#define CYTOKINE_IL10_RESTORATION         0.3f   /**< IL-10 restores precision */

/* Inflammation effects on tuning curves */
#define INFLAMMATION_TUNING_BROADENING    1.5f   /**< Inflammation broadens tuning */
#define INFLAMMATION_GAIN_REDUCTION       0.3f   /**< Per inflammation level gain loss */

/* Population anomaly detection thresholds */
#define POPULATION_NOISE_THRESHOLD        0.7f   /**< Noise level for immune trigger */
#define POPULATION_SYNCHRONY_THRESHOLD    0.3f   /**< Low synchrony threshold */
#define POPULATION_GAIN_ANOMALY_THRESHOLD 0.5f   /**< Abnormal gain change threshold */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD    (86400.0f * 7)  /**< 7 days = chronic */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on population coding
 *
 * Represents how cytokine levels modulate population code quality
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_noise_increase;         /**< IL-1β induced noise */
    float il6_precision_loss;         /**< IL-6 precision reduction */
    float tnf_gain_reduction;         /**< TNF-α gain reduction */
    float ifn_gamma_noise_increase;   /**< IFN-γ noise increase */

    /* Anti-inflammatory effects */
    float il10_precision_restoration; /**< IL-10 precision boost */

    /* Aggregate effects */
    float total_noise_increase;       /**< Combined noise increase [0-1] */
    float total_precision_loss;       /**< Combined precision loss [0-1] */
    float total_gain_reduction;       /**< Combined gain reduction [0-1] */
    float tuning_width_broadening;    /**< Tuning curve broadening factor */
} cytokine_population_effects_t;

/**
 * @brief Inflammation effects on population coding
 *
 * How inflammation severity affects population code quality
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Population coding impacts */
    float noise_level;                 /**< Neural noise level [0-1] */
    float precision_degradation;       /**< Precision loss [0-1] */
    float gain_reduction;              /**< Neural gain reduction [0-1] */
    float tuning_broadening;           /**< Tuning curve broadening [1.0-3.0] */
    float synchrony_loss;              /**< Population synchrony loss [0-1] */
    float sparse_code_reliability;     /**< Reliability [0-1] */

    /* Trial-to-trial variability */
    float variability_increase;        /**< Increased variability [0-1] */
    float vector_magnitude_reduction;  /**< Weak population vectors [0-1] */
} inflammation_population_state_t;

/**
 * @brief Population anomaly immune trigger
 *
 * How population code anomalies trigger immune responses
 */
typedef struct {
    /* Anomaly indicators */
    float noise_level;                 /**< Current noise [0-1] */
    float synchrony_loss;              /**< Synchrony reduction [0-1] */
    float gain_anomaly;                /**< Abnormal gain change [0-1] */
    float vector_anomaly;              /**< Abnormal vector direction [0-1] */
    float reliability_loss;            /**< Reliability degradation [0-1] */

    /* Immune triggers */
    bool noise_triggered;              /**< High noise triggered immune */
    bool synchrony_triggered;          /**< Low synchrony triggered immune */
    bool gain_triggered;               /**< Gain anomaly triggered immune */

    /* Severity */
    float threat_severity;             /**< Overall threat [0-1] */
} population_immune_trigger_t;

/**
 * @brief Population code health metrics
 *
 * Overall health of population coding under immune challenge
 */
typedef struct {
    /* Current state */
    float precision;                   /**< Current precision [0-1] */
    float gain;                        /**< Current neural gain [0-1] */
    float synchrony;                   /**< Current synchrony [0-1] */
    float noise;                       /**< Current noise [0-1] */
    float reliability;                 /**< Code reliability [0-1] */

    /* Health score */
    float overall_health;              /**< Combined health [0-1] */
    float degradation_from_baseline;   /**< How much degraded [0-1] */

    /* Recovery indicators */
    float recovery_progress;           /**< Recovery [0-1] */
    bool fully_recovered;              /**< Back to baseline */
} population_health_metrics_t;

/**
 * @brief Complete population coding-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    population_coding_encoder_t population_encoder;

    /* Current state */
    cytokine_population_effects_t cytokine_effects;
    inflammation_population_state_t inflammation_state;
    population_immune_trigger_t immune_trigger;
    population_health_metrics_t health_metrics;

    /* Baseline for comparison */
    float baseline_noise;              /**< Baseline noise level */
    float baseline_gain;               /**< Baseline neural gain */
    float baseline_precision;          /**< Baseline precision */
    float baseline_synchrony;          /**< Baseline synchrony */

    /* Integration flags */
    bool enable_cytokine_noise_modulation;
    bool enable_inflammation_tuning_modulation;
    bool enable_population_anomaly_detection;
    bool enable_gain_modulation;
    bool enable_precision_restoration;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t anomaly_detections;
    uint32_t immune_triggers;
    uint32_t restorations;

    } population_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_noise_modulation;
    bool enable_inflammation_tuning_modulation;
    bool enable_population_anomaly_detection;
    bool enable_gain_modulation;
    bool enable_precision_restoration;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float anomaly_sensitivity;         /**< Anomaly detection sensitivity [0.5-2.0] */

    /* Thresholds */
    float noise_trigger_threshold;     /**< Noise level to trigger immune [0.5-0.9] */
    float synchrony_threshold;         /**< Low synchrony threshold [0.1-0.5] */
    float gain_anomaly_threshold;      /**< Gain change threshold [0.3-0.7] */

    /* Baselines (for anomaly detection) */
    float baseline_noise;              /**< Expected noise level */
    float baseline_gain;               /**< Expected gain */
    float baseline_precision;          /**< Expected precision */
} population_immune_config_t;

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
int population_immune_default_config(population_immune_config_t* config);

/**
 * @brief Create population coding-immune bridge
 *
 * WHAT: Initialize bidirectional population-immune integration
 * WHY:  Enable realistic population coding under immune challenge
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param population_encoder Population coding encoder
 * @return New bridge or NULL on failure
 */
population_immune_bridge_t* population_immune_bridge_create(
    const population_immune_config_t* config,
    brain_immune_system_t* immune_system,
    population_coding_encoder_t population_encoder
);

/**
 * @brief Destroy population coding-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void population_immune_bridge_destroy(population_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Population Coding API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to population coding
 *
 * WHAT: Modulate population code quality based on cytokine levels
 * WHY:  Pro-inflammatory cytokines increase noise and reduce precision
 * HOW:  Query immune system cytokines, adjust noise/gain/precision
 *
 * @param bridge Population-immune bridge
 * @return 0 on success
 */
int population_immune_apply_cytokine_effects(population_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to tuning curves
 *
 * WHAT: Broaden tuning curves and reduce gain from inflammation
 * WHY:  Inflammation impairs neural selectivity
 * HOW:  Check inflammation level, adjust tuning width and gain
 *
 * @param bridge Population-immune bridge
 * @return 0 on success
 */
int population_immune_apply_inflammation_effects(population_immune_bridge_t* bridge);

/**
 * @brief Compute neural noise level from immune state
 *
 * WHAT: Calculate noise increase from cytokines and inflammation
 * WHY:  Noise reduces population code precision
 * HOW:  Combine cytokine noise contributions with inflammation
 *
 * @param bridge Population-immune bridge
 * @return Noise level [0-1]
 */
float population_immune_compute_noise(const population_immune_bridge_t* bridge);

/**
 * @brief Compute neural gain modulation from immune state
 *
 * WHAT: Calculate gain reduction from TNF-α and inflammation
 * WHY:  TNF-α reduces synaptic scaling and response amplitude
 * HOW:  Map TNF level and inflammation to gain reduction
 *
 * @param bridge Population-immune bridge
 * @return Gain multiplier [0-1]
 */
float population_immune_compute_gain(const population_immune_bridge_t* bridge);

/**
 * @brief Compute precision degradation from immune state
 *
 * WHAT: Calculate precision loss from IL-6 and inflammation
 * WHY:  IL-6 specifically reduces sensory precision
 * HOW:  Map IL-6 level and inflammation to precision loss
 *
 * @param bridge Population-immune bridge
 * @return Precision [0-1]
 */
float population_immune_compute_precision(const population_immune_bridge_t* bridge);

/**
 * @brief Compute tuning width broadening from inflammation
 *
 * WHAT: Calculate how much tuning curves broaden
 * WHY:  Inflammation reduces neural selectivity
 * HOW:  Map inflammation level to tuning width multiplier
 *
 * @param bridge Population-immune bridge
 * @return Tuning width multiplier [1.0-3.0]
 */
float population_immune_compute_tuning_broadening(const population_immune_bridge_t* bridge);

/* ============================================================================
 * Population Coding → Immune API
 * ============================================================================ */

/**
 * @brief Detect population coding anomalies
 *
 * WHAT: Check for abnormal noise, synchrony, gain changes
 * WHY:  Population anomalies may indicate neural threats
 * HOW:  Compare current metrics to baseline thresholds
 *
 * @param bridge Population-immune bridge
 * @param noise Current noise level [0-1]
 * @param synchrony Current synchrony [0-1]
 * @param gain Current gain [0-1]
 * @return 0 on success
 */
int population_immune_detect_anomalies(
    population_immune_bridge_t* bridge,
    float noise,
    float synchrony,
    float gain
);

/**
 * @brief Trigger immune response from population anomaly
 *
 * WHAT: Activate immune system from detected anomaly
 * WHY:  Abnormal population codes may signal threats
 * HOW:  Create antigen from anomaly signature, trigger response
 *
 * @param bridge Population-immune bridge
 * @return 0 on success
 */
int population_immune_trigger_from_anomaly(population_immune_bridge_t* bridge);

/**
 * @brief Release IL-10 on population code restoration
 *
 * WHAT: Anti-inflammatory signal when codes return to normal
 * WHY:  Recovery indicator for immune system
 * HOW:  Check health metrics, release IL-10 if recovered
 *
 * @param bridge Population-immune bridge
 * @return 0 on success
 */
int population_immune_restoration_signal(population_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update population-immune bridge (both directions)
 *
 * WHAT: Process all population-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, detect anomalies, update health
 *
 * @param bridge Population-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int population_immune_bridge_update(
    population_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine effects on population coding
 *
 * @param bridge Population-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int population_immune_get_cytokine_effects(
    const population_immune_bridge_t* bridge,
    cytokine_population_effects_t* effects
);

/**
 * @brief Get current inflammation state
 *
 * @param bridge Population-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int population_immune_get_inflammation_state(
    const population_immune_bridge_t* bridge,
    inflammation_population_state_t* state
);

/**
 * @brief Get population code health metrics
 *
 * @param bridge Population-immune bridge
 * @param metrics Output metrics structure
 * @return 0 on success
 */
int population_immune_get_health_metrics(
    const population_immune_bridge_t* bridge,
    population_health_metrics_t* metrics
);

/**
 * @brief Check if population coding is degraded
 *
 * WHAT: Determine if immune state has degraded population codes
 * WHY:  Quick health check
 * HOW:  Compare current health to baseline
 *
 * @param bridge Population-immune bridge
 * @return true if degraded
 */
bool population_immune_is_degraded(const population_immune_bridge_t* bridge);

/**
 * @brief Get overall population code health score
 *
 * @param bridge Population-immune bridge
 * @return Health score [0-1]
 */
float population_immune_get_health_score(const population_immune_bridge_t* bridge);

/* ============================================================================
 * Advanced Integration API
 * ============================================================================ */

/**
 * @brief Modulate vector sum decoding with immune noise
 *
 * WHAT: Apply immune-induced noise to population vector decoding
 * WHY:  Realistic population vector degradation under inflammation
 * HOW:  Add noise to rates, broaden tuning curves, reduce gain
 *
 * @param bridge Population-immune bridge
 * @param rates Input firing rates [num_neurons]
 * @param tuning_curves Neuron tuning curves
 * @param num_neurons Number of neurons
 * @param noisy_rates_out Output noise-modulated rates
 * @return 0 on success
 */
int population_immune_modulate_vector_decoding(
    population_immune_bridge_t* bridge,
    const float* rates,
    const tuning_curve_t* tuning_curves,
    uint32_t num_neurons,
    float* noisy_rates_out
);

/**
 * @brief Modulate synchrony analysis with immune effects
 *
 * WHAT: Apply immune effects to synchrony computation
 * WHY:  Inflammation reduces population coherence
 * HOW:  Reduce correlation strength, increase lag variability
 *
 * @param bridge Population-immune bridge
 * @param baseline_synchrony Baseline synchrony result
 * @param modulated_synchrony_out Output modulated result
 * @return 0 on success
 */
int population_immune_modulate_synchrony(
    population_immune_bridge_t* bridge,
    const synchrony_result_t* baseline_synchrony,
    synchrony_result_t* modulated_synchrony_out
);

/**
 * @brief Modulate sparse coding with immune noise
 *
 * WHAT: Apply immune noise to sparse distributed representations
 * WHY:  Inflammation reduces code reliability
 * HOW:  Add random bit flips, reduce sparsity precision
 *
 * @param bridge Population-immune bridge
 * @param baseline_code Baseline sparse code [num_neurons]
 * @param num_neurons Number of neurons
 * @param noisy_code_out Output noise-modulated code
 * @return 0 on success
 */
int population_immune_modulate_sparse_code(
    population_immune_bridge_t* bridge,
    const bool* baseline_code,
    uint32_t num_neurons,
    bool* noisy_code_out
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_POPULATION_CODING
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int population_coding_immune_connect_bio_async(population_immune_bridge_t* bridge);

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
int population_coding_immune_disconnect_bio_async(population_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool population_coding_immune_is_bio_async_connected(const population_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_POPULATION_CODING_IMMUNE_BRIDGE_H */
