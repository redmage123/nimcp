/**
 * @file nimcp_audio_cortex_fep_bridge.h
 * @brief Free Energy Principle - Audio Cortex Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and Audio Cortex
 * WHY:  Auditory processing as temporal sequence prediction across multiple timescales.
 *       FEP predicts acoustic features, attention modulates frequency selectivity.
 * HOW:  FEP generates auditory predictions, auditory PE updates beliefs, precision
 *       controls cocktail party attention, active inference guides auditory focus.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * AUDITORY FEP PATHWAYS:
 * ----------------------
 * 1. Temporal Prediction in Auditory Cortex (Garrido et al. 2009):
 *    - Primary auditory cortex (A1) predicts spectrotemporal patterns
 *    - Mismatch negativity (MMN) = auditory prediction error
 *    - Hierarchical: A1 → belt → parabelt as generative hierarchy
 *    - Reference: "The mismatch negativity: a review of underlying mechanisms"
 *
 * 2. Predictive Coding in Speech (Hickok & Poeppel 2007):
 *    - Speech perception as hierarchical prediction
 *    - Phonemes predicted from words, acoustics from phonemes
 *    - Top-down predictions enhance degraded speech
 *    - Reference: "The cortical organization of speech processing"
 *
 * 3. Auditory Attention as Precision (Feldman & Friston 2010):
 *    - Cocktail party effect = precision-weighted PE
 *    - Attend to speaker = high precision for that frequency band
 *    - Ignore background = low precision for noise
 *    - Attention follows expected information gain
 *
 * 4. Auditory Scene Analysis (Winkler et al. 2009):
 *    - Stream segregation via predictive grouping
 *    - Separate sound sources as distinct generative models
 *    - Streaming = minimizing free energy of auditory scene
 *
 * FEP → AUDIO CORTEX:
 * -------------------
 * 1. Temporal Predictions:
 *    - FEP predicts next auditory frame from temporal model
 *    - Modulate A1 frequency tuning for expected sounds
 *    - Predictive suppression of expected auditory features
 *
 * 2. Precision-Weighted Attention:
 *    - High precision → sharpen frequency tuning
 *    - Low precision → broaden tuning (ignore unreliable input)
 *    - Cocktail party: precision spotlight on speaker
 *
 * 3. Active Auditory Inference:
 *    - Head/ear movements to minimize auditory uncertainty
 *    - Focus on informative frequency bands
 *    - Sample environment to resolve ambiguity
 *
 * AUDIO CORTEX → FEP:
 * -------------------
 * 1. Auditory Prediction Errors:
 *    - Unexpected sounds → high PE (MMN response)
 *    - Update temporal predictions
 *    - Novel auditory patterns trigger learning
 *
 * 2. Frequency Observations:
 *    - Mel-scale features as sensory observations
 *    - Feed into FEP temporal generative model
 *    - Drive belief updates about auditory scene
 *
 * 3. Temporal Structure:
 *    - Rhythmic patterns inform temporal priors
 *    - Onset/offset events update event models
 *    - Temporal envelope guides prediction
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  AUDIO CORTEX FEP BRIDGE                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                FEP → AUDIO PATHWAYS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   FEP Temporal Beliefs → Auditory Predictions → A1 Tuning          │  ║
 * ║   │   FEP Precision → Frequency Selectivity → Cocktail Party           │  ║
 * ║   │   Expected Info Gain → Auditory Attention Focus                    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │             AUDIO CORTEX → FEP PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   Mel Features → Observations → Temporal PE Computation            │  ║
 * ║   │   MMN Response → High PE → Belief Updates                          │  ║
 * ║   │   Onset/Offset → Event Predictions → Model Learning                │  ║
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

#ifndef NIMCP_AUDIO_CORTEX_FEP_BRIDGE_H
#define NIMCP_AUDIO_CORTEX_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "perception/nimcp_audio_cortex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Auditory prediction error thresholds (MMN response) */
#define AUDIO_FEP_PE_THRESHOLD_LOW       0.5f    /**< Minor auditory surprise */
#define AUDIO_FEP_PE_THRESHOLD_MEDIUM    2.0f    /**< Moderate surprise (MMN) */
#define AUDIO_FEP_PE_THRESHOLD_HIGH      5.0f    /**< High surprise (novel sound) */

/* Precision impact on frequency tuning */
#define AUDIO_FEP_PRECISION_TUNING_MIN   0.7f    /**< Broadest tuning */
#define AUDIO_FEP_PRECISION_TUNING_MAX   1.5f    /**< Sharpest tuning */
#define AUDIO_FEP_PRECISION_TUNING_DEFAULT 1.0f  /**< Default tuning */

/* Temporal prediction horizons */
#define AUDIO_FEP_PREDICTION_HORIZON_MS  100     /**< Prediction lookahead */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct audio_cortex_fep_bridge audio_cortex_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Audio Cortex FEP bridge
 */
typedef struct {
    /* Thresholds */
    float prediction_error_threshold;      /**< PE → learning trigger */
    float precision_tuning_factor;         /**< Precision → tuning sharpness */
    float temporal_prediction_weight;      /**< Temporal prediction strength */

    /* Feature enables */
    bool enable_temporal_predictions;      /**< FEP temporal prediction */
    bool enable_precision_tuning;          /**< Precision → frequency tuning */
    bool enable_cocktail_party;            /**< Attention-driven streaming */
    bool enable_auditory_pe_updates;       /**< Auditory PE → beliefs */

    /* Sensitivity factors */
    float frequency_precision_sensitivity; /**< Precision effect on tuning */
    float temporal_prediction_sensitivity; /**< Prediction strength */
    float pe_propagation_rate;             /**< PE propagation speed */
} audio_cortex_fep_config_t;

/**
 * @brief FEP effects on auditory processing
 */
typedef struct {
    /* Frequency tuning modulation */
    float frequency_tuning_sharpness;      /**< Tuning curve sharpness */
    float mel_filter_gain;                 /**< Mel filterbank gain */
    float temporal_resolution_boost;       /**< Temporal resolution */

    /* Precision effects */
    float auditory_precision;              /**< Current auditory precision */
    float precision_tuning_modifier;       /**< Precision → tuning */

    /* Prediction effects */
    float prediction_suppression;          /**< Suppress expected sounds */
    float novelty_enhancement;             /**< Enhance novel sounds */
} audio_cortex_fep_effects_t;

/**
 * @brief Current state of Audio-FEP interaction
 */
typedef struct {
    /* Auditory prediction errors */
    float current_auditory_pe;             /**< Current auditory PE */
    float avg_auditory_pe;                 /**< Average auditory PE */
    float max_auditory_pe;                 /**< Peak auditory PE (MMN) */

    /* Temporal prediction state */
    float temporal_prediction_accuracy;    /**< Temporal prediction accuracy */
    uint64_t prediction_horizon_frames;    /**< Frames predicted ahead */

    /* Precision state */
    float auditory_precision;              /**< Auditory sensory precision */
    float frequency_precision[32];         /**< Per-frequency precision */

    /* Processing state */
    uint64_t frames_processed;             /**< Audio frames processed */
    uint64_t mmn_events;                   /**< MMN (high PE) events */
    bool novel_sound_detected;             /**< Novel auditory pattern */
} audio_cortex_fep_state_t;

/**
 * @brief Statistics for Audio FEP bridge
 */
typedef struct {
    /* Auditory processing */
    uint64_t total_frames_processed;       /**< Total audio frames */
    uint64_t high_pe_events;               /**< High PE (MMN) events */
    uint64_t novelty_events;               /**< Novel sound detections */

    /* Prediction accuracy */
    float avg_prediction_error;            /**< Average PE magnitude */
    float temporal_prediction_accuracy;    /**< Temporal prediction accuracy */

    /* Attention effects */
    uint64_t cocktail_party_events;        /**< Attention focus switches */
    float avg_frequency_precision;         /**< Avg precision across bands */

    /* Modulation effects */
    float avg_tuning_sharpness;            /**< Average tuning sharpness */
    float avg_prediction_suppression;      /**< Avg predictive suppression */
} audio_cortex_fep_stats_t;

/**
 * @brief Audio Cortex FEP bridge state
 */
struct audio_cortex_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    audio_cortex_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;              /**< FEP system */
    audio_cortex_t* audio_cortex;          /**< Audio cortex */

    /* Current effects */
    audio_cortex_fep_effects_t effects;
    audio_cortex_fep_state_t state;

    /* Statistics */
    audio_cortex_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Audio Cortex FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int audio_cortex_fep_bridge_default_config(audio_cortex_fep_config_t* config);

/**
 * @brief Create Audio Cortex FEP bridge
 *
 * WHAT: Initialize Audio-FEP integration bridge
 * WHY:  Enable bidirectional audio-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
audio_cortex_fep_bridge_t* audio_cortex_fep_bridge_create(
    const audio_cortex_fep_config_t* config
);

/**
 * @brief Destroy Audio Cortex FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void audio_cortex_fep_bridge_destroy(audio_cortex_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and modulation
 * HOW:  Store FEP system pointer
 *
 * @param bridge Audio FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int audio_cortex_fep_bridge_connect_fep(
    audio_cortex_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect audio cortex
 *
 * WHAT: Link bridge to audio cortex
 * WHY:  Enable auditory processing monitoring and modulation
 * HOW:  Store audio cortex pointer
 *
 * @param bridge Audio FEP bridge
 * @param audio Audio cortex system
 * @return 0 on success
 */
int audio_cortex_fep_bridge_connect_audio_cortex(
    audio_cortex_fep_bridge_t* bridge,
    audio_cortex_t* audio
);

/* ============================================================================
 * FEP → Audio Direction
 * ============================================================================ */

/**
 * @brief Apply FEP temporal predictions to auditory processing
 *
 * WHAT: Modulate A1 tuning based on temporal predictions
 * WHY:  Top-down predictions enhance expected sounds
 * HOW:  Scale frequency responses by prediction confidence
 *
 * @param bridge Audio FEP bridge
 * @return 0 on success
 */
int audio_cortex_fep_apply_temporal_predictions(
    audio_cortex_fep_bridge_t* bridge
);

/**
 * @brief Apply precision to frequency tuning (cocktail party)
 *
 * WHAT: Sharpen frequency tuning for attended sounds
 * WHY:  Attention selects relevant auditory streams
 * HOW:  Modulate mel filterbank sharpness by precision
 *
 * @param bridge Audio FEP bridge
 * @return 0 on success
 */
int audio_cortex_fep_apply_precision_tuning(
    audio_cortex_fep_bridge_t* bridge
);

/* ============================================================================
 * Audio → FEP Direction
 * ============================================================================ */

/**
 * @brief Compute auditory prediction error (MMN)
 *
 * WHAT: Calculate PE from audio features vs FEP predictions
 * WHY:  Auditory PE drives belief updates
 * HOW:  Compare mel features to predicted features
 *
 * @param bridge Audio FEP bridge
 * @param audio_features Mel-scale feature vector
 * @param num_features Feature dimension
 * @param prediction_error Output PE magnitude
 * @return 0 on success
 */
int audio_cortex_fep_compute_prediction_error(
    audio_cortex_fep_bridge_t* bridge,
    const float* audio_features,
    uint32_t num_features,
    float* prediction_error
);

/**
 * @brief Report auditory observations to FEP
 *
 * WHAT: Feed audio features as observations to FEP
 * WHY:  Auditory input drives temporal inference
 * HOW:  Convert features to FEP observation format
 *
 * @param bridge Audio FEP bridge
 * @param audio_features Audio feature vector
 * @param num_features Feature dimension
 * @return 0 on success
 */
int audio_cortex_fep_report_observations(
    audio_cortex_fep_bridge_t* bridge,
    const float* audio_features,
    uint32_t num_features
);

/**
 * @brief Report temporal events (onset/offset) to FEP
 *
 * WHAT: Signal temporal boundaries for event prediction
 * WHY:  Onsets/offsets structure temporal model
 * HOW:  Detect events, report to FEP for model updates
 *
 * @param bridge Audio FEP bridge
 * @param onset_detected Onset event detected
 * @param offset_detected Offset event detected
 * @return 0 on success
 */
int audio_cortex_fep_report_temporal_events(
    audio_cortex_fep_bridge_t* bridge,
    bool onset_detected,
    bool offset_detected
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Audio-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep audio and FEP systems synchronized
 * HOW:  Update predictions, compute PE, apply modulation
 *
 * @param bridge Audio FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int audio_cortex_fep_bridge_update(
    audio_cortex_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Audio FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int audio_cortex_fep_bridge_get_state(
    const audio_cortex_fep_bridge_t* bridge,
    audio_cortex_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Audio FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int audio_cortex_fep_bridge_get_stats(
    const audio_cortex_fep_bridge_t* bridge,
    audio_cortex_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for audio-FEP coordination
 * WHY:  Distributed auditory prediction signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Audio FEP bridge
 * @return 0 on success
 */
int audio_cortex_fep_bridge_connect_bio_async(
    audio_cortex_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Audio FEP bridge
 * @return 0 on success
 */
int audio_cortex_fep_bridge_disconnect_bio_async(
    audio_cortex_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Audio FEP bridge
 * @return true if bio-async enabled
 */
bool audio_cortex_fep_bridge_is_bio_async_connected(
    const audio_cortex_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIO_CORTEX_FEP_BRIDGE_H */
