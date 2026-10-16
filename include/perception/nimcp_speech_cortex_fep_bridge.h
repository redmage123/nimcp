/**
 * @file nimcp_speech_cortex_fep_bridge.h
 * @brief Free Energy Principle - Speech Cortex Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and Speech Cortex
 * WHY:  Speech perception/production as hierarchical phoneme sequence prediction.
 *       FEP predicts phonemes from words, acoustics from phonemes (motor theory).
 * HOW:  FEP generates phoneme predictions, phoneme PE updates beliefs, precision
 *       modulates phoneme discrimination, active inference guides articulation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SPEECH FEP PATHWAYS:
 * --------------------
 * 1. Hierarchical Speech Prediction (Hickok & Poeppel 2007):
 *    - Wernicke's area (comprehension): Words → Phonemes → Acoustics
 *    - Broca's area (production): Intentions → Words → Phonemes → Articulation
 *    - Prediction errors at each level update higher-level beliefs
 *    - Reference: "The cortical organization of speech processing"
 *
 * 2. Motor Theory of Speech (Liberman & Mattingly 1985):
 *    - Speech perception = inverse model of production
 *    - Perceiving phoneme activates articulatory gestures
 *    - FEP unifies: predict acoustic consequences of articulation
 *    - Reference: "The motor theory of speech perception revised"
 *
 * 3. Predictive Coding in Speech (Friston & Frith 2015):
 *    - Speech as active inference: predict/sample phoneme sequence
 *    - Coarticulation = temporal prediction across phonemes
 *    - Phoneme categories = attractors in generative model
 *    - Reference: "Active inference, communication and hermeneutics"
 *
 * 4. Phonological Working Memory (Baddeley 2003):
 *    - Phonological loop maintains predicted phoneme sequence
 *    - Articulatory rehearsal = active inference sampling
 *    - Capacity limits from prediction horizon
 *    - Reference: "Working memory: looking back and looking forward"
 *
 * FEP → SPEECH CORTEX:
 * --------------------
 * 1. Top-down Phoneme Predictions:
 *    - Lexical context predicts phonemes
 *    - Modulate phoneme detector sensitivity
 *    - Predictive restoration of degraded speech
 *
 * 2. Precision-Weighted Phoneme Discrimination:
 *    - High precision → sharper phoneme categories
 *    - Low precision → broader acceptance (noisy speech)
 *    - Attention modulates phoneme precision
 *
 * 3. Articulatory Prediction (Motor Theory):
 *    - Predict acoustic consequences of gestures
 *    - Active inference guides speech production
 *    - Coarticulation as temporal prediction
 *
 * SPEECH CORTEX → FEP:
 * --------------------
 * 1. Phoneme Prediction Errors:
 *    - Unexpected phonemes → high PE
 *    - Update lexical/semantic beliefs
 *    - Word boundary detection via PE spikes
 *
 * 2. Phonological Observations:
 *    - Detected phonemes feed FEP generative model
 *    - Drive word recognition inference
 *    - Update articulatory predictions
 *
 * 3. Temporal Structure:
 *    - Syllable rhythm informs temporal priors
 *    - Prosody guides syntactic predictions
 *    - Speech rate modulates prediction horizon
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  SPEECH CORTEX FEP BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               FEP → SPEECH PATHWAYS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   FEP Lexical Model → Phoneme Predictions → STG Detector Priming   │  ║
 * ║   │   FEP Precision → Phoneme Category Sharpness → Discrimination      │  ║
 * ║   │   FEP Motor Model → Articulatory Predictions → Broca Production    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            SPEECH CORTEX → FEP PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   Phoneme Detections → Observations → Lexical Inference            │  ║
 * ║   │   Phoneme PE → Surprise Signal → Word Boundary Detection           │  ║
 * ║   │   Formant Features → Articulatory Observations → Motor Updates     │  ║
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

#ifndef NIMCP_SPEECH_CORTEX_FEP_BRIDGE_H
#define NIMCP_SPEECH_CORTEX_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "perception/nimcp_speech_cortex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Phoneme prediction error thresholds */
#define SPEECH_FEP_PE_THRESHOLD_LOW       0.3f    /**< Minor phoneme mismatch */
#define SPEECH_FEP_PE_THRESHOLD_MEDIUM    1.0f    /**< Moderate mismatch */
#define SPEECH_FEP_PE_THRESHOLD_HIGH      3.0f    /**< High mismatch (word boundary) */

/* Precision impact on phoneme categories */
#define SPEECH_FEP_PRECISION_CATEGORY_MIN     0.6f    /**< Broadest categories */
#define SPEECH_FEP_PRECISION_CATEGORY_MAX     1.8f    /**< Sharpest categories */
#define SPEECH_FEP_PRECISION_CATEGORY_DEFAULT 1.0f    /**< Default sharpness */

/* Phoneme prediction horizon (number of phonemes ahead) */
#define SPEECH_FEP_PREDICTION_HORIZON     3       /**< Predict 3 phonemes ahead */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct speech_cortex_fep_bridge speech_cortex_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Speech Cortex FEP bridge
 */
typedef struct {
    /* Thresholds */
    float prediction_error_threshold;      /**< PE → learning trigger */
    float precision_category_factor;       /**< Precision → category sharpness */
    float phoneme_prediction_weight;       /**< Top-down prediction strength */

    /* Feature enables */
    bool enable_phoneme_predictions;       /**< FEP phoneme prediction */
    bool enable_precision_categories;      /**< Precision → phoneme sharpness */
    bool enable_motor_theory;              /**< Articulatory predictions */
    bool enable_phoneme_pe_updates;        /**< Phoneme PE → beliefs */

    /* Sensitivity factors */
    float phoneme_precision_sensitivity;   /**< Precision effect on categories */
    float lexical_prediction_sensitivity;  /**< Lexical → phoneme strength */
    float pe_propagation_rate;             /**< PE propagation speed */
} speech_cortex_fep_config_t;

/**
 * @brief FEP effects on speech processing
 */
typedef struct {
    /* Phoneme category modulation */
    float phoneme_category_sharpness;      /**< Category boundary sharpness */
    float phoneme_detector_gain;           /**< Detector sensitivity */
    float formant_discrimination;          /**< Formant discrimination sharpness */

    /* Precision effects */
    float phoneme_precision;               /**< Current phoneme precision */
    float precision_category_modifier;     /**< Precision → category conversion */

    /* Prediction effects */
    float prediction_priming;              /**< Prime expected phonemes */
    float novelty_sensitivity;             /**< Enhance unexpected phonemes */
} speech_cortex_fep_effects_t;

/**
 * @brief Current state of Speech-FEP interaction
 */
typedef struct {
    /* Phoneme prediction errors */
    float current_phoneme_pe;              /**< Current phoneme PE */
    float avg_phoneme_pe;                  /**< Average phoneme PE */
    float max_phoneme_pe;                  /**< Peak phoneme PE */

    /* Prediction state */
    phoneme_t predicted_phonemes[SPEECH_FEP_PREDICTION_HORIZON];
    float prediction_confidence[SPEECH_FEP_PREDICTION_HORIZON];
    uint32_t prediction_horizon;           /**< Active prediction horizon */

    /* Precision state */
    float phoneme_precision;               /**< Phoneme sensory precision */
    float lexical_precision;               /**< Lexical context precision */

    /* Processing state */
    uint64_t phonemes_processed;           /**< Phonemes processed */
    uint64_t word_boundary_events;         /**< Word boundaries detected */
    bool novel_word_detected;              /**< Novel word detected */
} speech_cortex_fep_state_t;

/**
 * @brief Statistics for Speech FEP bridge
 */
typedef struct {
    /* Speech processing */
    uint64_t total_phonemes_processed;     /**< Total phonemes */
    uint64_t high_pe_events;               /**< High PE (word boundary) events */
    uint64_t word_recognition_events;      /**< Words recognized */

    /* Prediction accuracy */
    float avg_prediction_error;            /**< Average phoneme PE */
    float phoneme_prediction_accuracy;     /**< Phoneme prediction accuracy */

    /* Lexical effects */
    float lexical_facilitation_rate;       /**< Lexical → phoneme boost */
    float avg_word_boundary_pe;            /**< Avg PE at word boundaries */

    /* Modulation effects */
    float avg_category_sharpness;          /**< Average category sharpness */
    float avg_prediction_priming;          /**< Avg top-down priming */
} speech_cortex_fep_stats_t;

/**
 * @brief Speech Cortex FEP bridge state
 */
struct speech_cortex_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    speech_cortex_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;              /**< FEP system */
    speech_cortex_t* speech_cortex;        /**< Speech cortex */

    /* Current effects */
    speech_cortex_fep_effects_t effects;
    speech_cortex_fep_state_t state;

    /* Statistics */
    speech_cortex_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Speech Cortex FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int speech_cortex_fep_bridge_default_config(speech_cortex_fep_config_t* config);

/**
 * @brief Create Speech Cortex FEP bridge
 *
 * WHAT: Initialize Speech-FEP integration bridge
 * WHY:  Enable bidirectional speech-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
speech_cortex_fep_bridge_t* speech_cortex_fep_bridge_create(
    const speech_cortex_fep_config_t* config
);

/**
 * @brief Destroy Speech Cortex FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void speech_cortex_fep_bridge_destroy(speech_cortex_fep_bridge_t* bridge);

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
 * @param bridge Speech FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int speech_cortex_fep_bridge_connect_fep(
    speech_cortex_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect speech cortex
 *
 * WHAT: Link bridge to speech cortex
 * WHY:  Enable speech processing monitoring and modulation
 * HOW:  Store speech cortex pointer
 *
 * @param bridge Speech FEP bridge
 * @param speech Speech cortex system
 * @return 0 on success
 */
int speech_cortex_fep_bridge_connect_speech_cortex(
    speech_cortex_fep_bridge_t* bridge,
    speech_cortex_t* speech
);

/* ============================================================================
 * FEP → Speech Direction
 * ============================================================================ */

/**
 * @brief Apply FEP phoneme predictions to speech processing
 *
 * WHAT: Prime phoneme detectors based on lexical predictions
 * WHY:  Top-down predictions enhance expected phonemes
 * HOW:  Modulate detector sensitivity for predicted phonemes
 *
 * @param bridge Speech FEP bridge
 * @return 0 on success
 */
int speech_cortex_fep_apply_phoneme_predictions(
    speech_cortex_fep_bridge_t* bridge
);

/**
 * @brief Apply precision to phoneme categories
 *
 * WHAT: Sharpen/broaden phoneme category boundaries
 * WHY:  Precision controls discrimination sharpness
 * HOW:  Modulate formant distance thresholds by precision
 *
 * @param bridge Speech FEP bridge
 * @return 0 on success
 */
int speech_cortex_fep_apply_precision_categories(
    speech_cortex_fep_bridge_t* bridge
);

/**
 * @brief Apply motor predictions to articulation (motor theory)
 *
 * WHAT: Predict acoustic consequences of articulatory gestures
 * WHY:  Speech perception uses motor predictions
 * HOW:  Use FEP motor model to predict formants
 *
 * @param bridge Speech FEP bridge
 * @return 0 on success
 */
int speech_cortex_fep_apply_motor_predictions(
    speech_cortex_fep_bridge_t* bridge
);

/* ============================================================================
 * Speech → FEP Direction
 * ============================================================================ */

/**
 * @brief Compute phoneme prediction error
 *
 * WHAT: Calculate PE from detected vs predicted phonemes
 * WHY:  Phoneme PE drives lexical belief updates
 * HOW:  Compare detected phoneme to FEP predictions
 *
 * @param bridge Speech FEP bridge
 * @param detected_phoneme Detected phoneme
 * @param confidence Detection confidence
 * @param prediction_error Output PE magnitude
 * @return 0 on success
 */
int speech_cortex_fep_compute_phoneme_prediction_error(
    speech_cortex_fep_bridge_t* bridge,
    phoneme_t detected_phoneme,
    float confidence,
    float* prediction_error
);

/**
 * @brief Report phoneme observations to FEP
 *
 * WHAT: Feed detected phonemes as observations to FEP
 * WHY:  Phoneme sequence drives word recognition
 * HOW:  Convert phonemes to FEP observation format
 *
 * @param bridge Speech FEP bridge
 * @param phoneme Detected phoneme
 * @param confidence Detection confidence
 * @return 0 on success
 */
int speech_cortex_fep_report_phoneme_observation(
    speech_cortex_fep_bridge_t* bridge,
    phoneme_t phoneme,
    float confidence
);

/**
 * @brief Report word boundary to FEP
 *
 * WHAT: Signal word boundary for segmentation
 * WHY:  High PE at boundaries updates word model
 * HOW:  Detect PE spike, report to FEP
 *
 * @param bridge Speech FEP bridge
 * @return 0 on success
 */
int speech_cortex_fep_report_word_boundary(
    speech_cortex_fep_bridge_t* bridge
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Speech-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep speech and FEP systems synchronized
 * HOW:  Update predictions, compute PE, apply modulation
 *
 * @param bridge Speech FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int speech_cortex_fep_bridge_update(
    speech_cortex_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Speech FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int speech_cortex_fep_bridge_get_state(
    const speech_cortex_fep_bridge_t* bridge,
    speech_cortex_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Speech FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int speech_cortex_fep_bridge_get_stats(
    const speech_cortex_fep_bridge_t* bridge,
    speech_cortex_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for speech-FEP coordination
 * WHY:  Distributed speech prediction signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Speech FEP bridge
 * @return 0 on success
 */
int speech_cortex_fep_bridge_connect_bio_async(
    speech_cortex_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Speech FEP bridge
 * @return 0 on success
 */
int speech_cortex_fep_bridge_disconnect_bio_async(
    speech_cortex_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Speech FEP bridge
 * @return true if bio-async enabled
 */
bool speech_cortex_fep_bridge_is_bio_async_connected(
    const speech_cortex_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPEECH_CORTEX_FEP_BRIDGE_H */
