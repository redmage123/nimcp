/**
 * @file nimcp_perception_immune.h
 * @brief Perception-Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integration layer connecting perception modules (visual, audio, speech)
 *       with the brain immune system for anomaly detection and response.
 * WHY:  Perception anomalies can indicate threats (corrupted input, adversarial
 *       attacks) that should trigger immune responses; immune inflammation affects
 *       perception accuracy as protective mechanism.
 * HOW:  Perception modules report anomalies as antigens; immune system modulates
 *       perception thresholds and accuracy based on inflammation state.
 *
 * BIOLOGICAL ANALOGY:
 * ```
 * SENSORY CORRUPTION          IMMUNE RESPONSE
 * ────────────────────────────────────────────────────────────────
 * Visual noise/distortion  → Antigen presentation
 * Audio artifacts          → B cell activation
 * Phoneme confusion        → Antibody production
 * Sustained overload       → Inflammation (protective shutdown)
 * Cytokines                → Modulate sensory thresholds
 * Resolution               → Restore normal perception
 * ```
 *
 * INTEGRATION PATTERNS:
 * 1. Anomaly-to-Antigen: Perception anomalies trigger immune response
 * 2. Inflammation-to-Threshold: Immune inflammation reduces perception sensitivity
 * 3. Cytokine-to-Gain: Cytokine levels modulate sensory gain
 * 4. Overload-to-Protection: Sensory overload triggers protective inflammation
 * 5. Memory-to-Adaptation: Immune memory speeds recovery from known threats
 *
 * USE CASES:
 * - Adversarial attack detection (visual perturbations)
 * - Audio corruption handling (noise, interference)
 * - Speech recognition under stress (inflammation reduces false positives)
 * - Sensory overload protection (autism-like protective shutdown)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PERCEPTION_IMMUNE_H
#define NIMCP_PERCEPTION_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PERCEPTION_IMMUNE_MAX_ANOMALIES    64
#define PERCEPTION_IMMUNE_OVERLOAD_THRESHOLD 0.8f
#define PERCEPTION_IMMUNE_MIN_GAIN         0.3f
#define PERCEPTION_IMMUNE_MAX_GAIN         2.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Perception anomaly types
 */
typedef enum {
    ANOMALY_VISUAL_NOISE = 0,      /**< Visual noise/corruption */
    ANOMALY_VISUAL_ADVERSARIAL,    /**< Adversarial perturbation */
    ANOMALY_VISUAL_OVERLOAD,       /**< Visual sensory overload */
    ANOMALY_AUDIO_CORRUPTION,      /**< Audio artifacts/noise */
    ANOMALY_AUDIO_JAMMING,         /**< Intentional audio interference */
    ANOMALY_AUDIO_OVERLOAD,        /**< Auditory overload */
    ANOMALY_SPEECH_CONFUSION,      /**< Phoneme confusion/error */
    ANOMALY_SPEECH_PROSODY,        /**< Abnormal prosody pattern */
    ANOMALY_SPEECH_OVERLOAD,       /**< Speech processing overload */
    ANOMALY_COUNT
} perception_anomaly_type_t;

/**
 * @brief Perception modality
 */
typedef enum {
    PERCEPTION_VISUAL = 0,
    PERCEPTION_AUDIO,
    PERCEPTION_SPEECH,
    PERCEPTION_COUNT
} perception_modality_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Perception anomaly event
 */
typedef struct {
    uint32_t id;                           /**< Anomaly ID */
    perception_anomaly_type_t type;        /**< Anomaly type */
    perception_modality_t modality;        /**< Affected modality */

    float severity;                        /**< Severity (0-1) */
    float confidence;                      /**< Detection confidence (0-1) */
    uint64_t timestamp_ms;                 /**< When detected */

    uint32_t antigen_id;                   /**< Corresponding immune antigen */
    bool immune_responded;                 /**< Immune system responded */
} perception_anomaly_t;

/**
 * @brief Perception immune modulation state
 */
typedef struct {
    /* Per-modality gain modulation (inflammation reduces gain) */
    float visual_gain;                     /**< Visual processing gain */
    float audio_gain;                      /**< Audio processing gain */
    float speech_gain;                     /**< Speech processing gain */

    /* Per-modality threshold modulation (cytokines adjust) */
    float visual_threshold;                /**< Visual detection threshold */
    float audio_threshold;                 /**< Audio detection threshold */
    float speech_threshold;                /**< Phoneme confidence threshold */

    /* Inflammation state */
    brain_inflammation_level_t visual_inflammation;
    brain_inflammation_level_t audio_inflammation;
    brain_inflammation_level_t speech_inflammation;

    /* Cytokine effects */
    float il1_level;                       /**< IL-1 (pro-inflammatory) */
    float il6_level;                       /**< IL-6 (acute phase) */
    float il10_level;                      /**< IL-10 (anti-inflammatory) */
    float tnf_alpha_level;                 /**< TNF-alpha (severe) */

    /* Protection flags */
    bool visual_overload_protection;
    bool audio_overload_protection;
    bool speech_overload_protection;
} perception_immune_modulation_t;

/**
 * @brief Perception immune integration context
 */
typedef struct {
    brain_immune_system_t* immune_system;  /**< Brain immune system */
    visual_cortex_t* visual_cortex;        /**< Visual cortex */
    audio_cortex_t* audio_cortex;          /**< Audio cortex */
    speech_cortex_t* speech_cortex;        /**< Speech cortex */

    perception_anomaly_t* anomalies;       /**< Anomaly history */
    size_t anomaly_count;
    size_t anomaly_capacity;
    uint32_t next_anomaly_id;

    perception_immune_modulation_t modulation; /**< Current modulation state */

    /* Statistics */
    uint64_t visual_anomalies_detected;
    uint64_t audio_anomalies_detected;
    uint64_t speech_anomalies_detected;
    uint64_t immune_responses_triggered;
    uint64_t overload_protections_activated;

    bool enabled;
} perception_immune_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create perception immune integration context
 *
 * WHAT: Initialize perception-immune integration
 * WHY:  Enable coordinated anomaly detection and response
 * HOW:  Allocate context, link modules
 *
 * @param immune_system Brain immune system
 * @return Context or NULL on failure
 */
perception_immune_context_t* perception_immune_create(
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy perception immune context
 *
 * @param ctx Context to destroy
 */
void perception_immune_destroy(perception_immune_context_t* ctx);

/**
 * @brief Connect visual cortex
 *
 * WHAT: Link visual cortex to immune integration
 * WHY:  Enable visual anomaly detection
 * HOW:  Store visual cortex reference
 *
 * @param ctx Perception immune context
 * @param visual Visual cortex instance
 * @return 0 on success
 */
int perception_immune_connect_visual(
    perception_immune_context_t* ctx,
    visual_cortex_t* visual
);

/**
 * @brief Connect audio cortex
 *
 * @param ctx Perception immune context
 * @param audio Audio cortex instance
 * @return 0 on success
 */
int perception_immune_connect_audio(
    perception_immune_context_t* ctx,
    audio_cortex_t* audio
);

/**
 * @brief Connect speech cortex
 *
 * @param ctx Perception immune context
 * @param speech Speech cortex instance
 * @return 0 on success
 */
int perception_immune_connect_speech(
    perception_immune_context_t* ctx,
    speech_cortex_t* speech
);

/* ============================================================================
 * Anomaly Detection and Reporting API
 * ============================================================================ */

/**
 * @brief Report visual anomaly
 *
 * WHAT: Detect and report visual processing anomaly
 * WHY:  Visual corruption/adversarial inputs are threats
 * HOW:  Create anomaly, present as antigen to immune system
 *
 * @param ctx Perception immune context
 * @param type Anomaly type
 * @param severity Severity (0-1)
 * @param confidence Detection confidence (0-1)
 * @param features Visual features (for epitope)
 * @param feature_dim Feature dimension
 * @param anomaly_id Output: assigned anomaly ID
 * @return 0 on success
 */
int perception_immune_report_visual_anomaly(
    perception_immune_context_t* ctx,
    perception_anomaly_type_t type,
    float severity,
    float confidence,
    const float* features,
    uint32_t feature_dim,
    uint32_t* anomaly_id
);

/**
 * @brief Report audio anomaly
 *
 * @param ctx Perception immune context
 * @param type Anomaly type
 * @param severity Severity (0-1)
 * @param confidence Detection confidence (0-1)
 * @param spectrum Audio spectrum (for epitope)
 * @param num_bins Spectrum bins
 * @param anomaly_id Output: assigned anomaly ID
 * @return 0 on success
 */
int perception_immune_report_audio_anomaly(
    perception_immune_context_t* ctx,
    perception_anomaly_type_t type,
    float severity,
    float confidence,
    const float* spectrum,
    uint32_t num_bins,
    uint32_t* anomaly_id
);

/**
 * @brief Report speech anomaly
 *
 * @param ctx Perception immune context
 * @param type Anomaly type
 * @param severity Severity (0-1)
 * @param confidence Detection confidence (0-1)
 * @param phoneme_features Phoneme features (for epitope)
 * @param num_phonemes Number of phonemes
 * @param anomaly_id Output: assigned anomaly ID
 * @return 0 on success
 */
int perception_immune_report_speech_anomaly(
    perception_immune_context_t* ctx,
    perception_anomaly_type_t type,
    float severity,
    float confidence,
    const void* phoneme_features,
    uint32_t num_phonemes,
    uint32_t* anomaly_id
);

/* ============================================================================
 * Immune Modulation API
 * ============================================================================ */

/**
 * @brief Update perception modulation from immune state
 *
 * WHAT: Apply immune system state to perception parameters
 * WHY:  Inflammation should reduce perception sensitivity
 * HOW:  Read inflammation/cytokines, compute gain/threshold adjustments
 *
 * @param ctx Perception immune context
 * @return 0 on success
 *
 * ALGORITHM:
 * 1. Query immune system inflammation levels
 * 2. Query cytokine concentrations
 * 3. Compute gain: gain = base_gain * (1.0 - inflammation_factor)
 * 4. Compute threshold: threshold = base_threshold * (1.0 + il1_factor)
 * 5. Apply to perception modules
 */
int perception_immune_update_modulation(perception_immune_context_t* ctx);

/**
 * @brief Apply visual modulation
 *
 * WHAT: Apply immune-driven modulation to visual cortex
 * WHY:  Inflammation reduces visual processing to protect
 * HOW:  Adjust Gabor gains, attention thresholds
 *
 * @param ctx Perception immune context
 * @return 0 on success
 */
int perception_immune_apply_visual_modulation(perception_immune_context_t* ctx);

/**
 * @brief Apply audio modulation
 *
 * @param ctx Perception immune context
 * @return 0 on success
 */
int perception_immune_apply_audio_modulation(perception_immune_context_t* ctx);

/**
 * @brief Apply speech modulation
 *
 * @param ctx Perception immune context
 * @return 0 on success
 */
int perception_immune_apply_speech_modulation(perception_immune_context_t* ctx);

/* ============================================================================
 * Overload Protection API
 * ============================================================================ */

/**
 * @brief Check for visual overload
 *
 * WHAT: Detect visual sensory overload condition
 * WHY:  Overload should trigger protective inflammation
 * HOW:  Monitor processing load, feature variance, attention spread
 *
 * @param ctx Perception immune context
 * @param features Recent visual features
 * @param num_features Feature dimension
 * @param overload Output: overload detected
 * @return 0 on success
 */
int perception_immune_check_visual_overload(
    perception_immune_context_t* ctx,
    const float* features,
    uint32_t num_features,
    bool* overload
);

/**
 * @brief Check for audio overload
 *
 * @param ctx Perception immune context
 * @param spectrum Recent audio spectrum
 * @param num_bins Spectrum bins
 * @param overload Output: overload detected
 * @return 0 on success
 */
int perception_immune_check_audio_overload(
    perception_immune_context_t* ctx,
    const float* spectrum,
    uint32_t num_bins,
    bool* overload
);

/**
 * @brief Check for speech overload
 *
 * @param ctx Perception immune context
 * @param phoneme_confidence Recent phoneme confidences
 * @param num_phonemes Number of phonemes
 * @param overload Output: overload detected
 * @return 0 on success
 */
int perception_immune_check_speech_overload(
    perception_immune_context_t* ctx,
    const float* phoneme_confidence,
    uint32_t num_phonemes,
    bool* overload
);

/**
 * @brief Trigger overload protection
 *
 * WHAT: Activate protective inflammation for overload
 * WHY:  Prevent damage from sustained sensory overload
 * HOW:  Initiate inflammation, reduce modality gain
 *
 * @param ctx Perception immune context
 * @param modality Affected modality
 * @return 0 on success
 */
int perception_immune_trigger_overload_protection(
    perception_immune_context_t* ctx,
    perception_modality_t modality
);

/**
 * @brief Release overload protection
 *
 * @param ctx Perception immune context
 * @param modality Affected modality
 * @return 0 on success
 */
int perception_immune_release_overload_protection(
    perception_immune_context_t* ctx,
    perception_modality_t modality
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current modulation state
 *
 * @param ctx Perception immune context
 * @param modulation Output: modulation state
 * @return 0 on success
 */
int perception_immune_get_modulation(
    const perception_immune_context_t* ctx,
    perception_immune_modulation_t* modulation
);

/**
 * @brief Get anomaly by ID
 *
 * @param ctx Perception immune context
 * @param anomaly_id Anomaly ID
 * @return Anomaly or NULL if not found
 */
const perception_anomaly_t* perception_immune_get_anomaly(
    const perception_immune_context_t* ctx,
    uint32_t anomaly_id
);

/**
 * @brief Check if modality is under overload protection
 *
 * @param ctx Perception immune context
 * @param modality Modality to check
 * @return true if protected
 */
bool perception_immune_is_protected(
    const perception_immune_context_t* ctx,
    perception_modality_t modality
);

/**
 * @brief Get visual gain factor
 *
 * @param ctx Perception immune context
 * @return Gain factor (0.3-2.0)
 */
float perception_immune_get_visual_gain(const perception_immune_context_t* ctx);

/**
 * @brief Get audio gain factor
 *
 * @param ctx Perception immune context
 * @return Gain factor (0.3-2.0)
 */
float perception_immune_get_audio_gain(const perception_immune_context_t* ctx);

/**
 * @brief Get speech gain factor
 *
 * @param ctx Perception immune context
 * @return Gain factor (0.3-2.0)
 */
float perception_immune_get_speech_gain(const perception_immune_context_t* ctx);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert anomaly type to string
 *
 * @param type Anomaly type
 * @return Human-readable string
 */
const char* perception_immune_anomaly_type_to_string(
    perception_anomaly_type_t type
);

/**
 * @brief Convert modality to string
 *
 * @param modality Modality enum
 * @return String representation
 */
const char* perception_immune_modality_to_string(
    perception_modality_t modality
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERCEPTION_IMMUNE_H */
