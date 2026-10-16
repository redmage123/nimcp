/**
 * @file nimcp_wernicke_immune.h
 * @brief Wernicke's Region - Brain Immune System Integration
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Bidirectional integration between brain immune system and Wernicke's region
 *       (language comprehension, phoneme recognition, lexical access, semantic integration)
 * WHY:  Biological evidence shows neuroinflammation in Wernicke's area causes receptive
 *       aphasia symptoms. Fever and inflammation consistently impair language comprehension.
 *       Essential for realistic brain modeling of language comprehension deficits.
 * HOW:  Immune system modulates comprehension speed, word recognition, and semantic access.
 *       Comprehension errors may signal neural damage and trigger immune surveillance.
 *       Inflammation models Wernicke's aphasia-like symptoms.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE -> WERNICKE PATHWAYS:
 * -------------------------
 * 1. Neuroinflammation in Wernicke's Area:
 *    - Pro-inflammatory cytokines (IL-1beta, IL-6, TNF-alpha) disrupt comprehension
 *    - Impairs posterior superior temporal gyrus (STG) function
 *    - Causes receptive aphasia-like symptoms
 *    - Impaired word recognition and semantic access
 *    - Fluent but meaningless speech output (word salad)
 *    - Reference: Wernicke (1874) "The symptom complex of aphasia"
 *    - Reference: Damasio (1992) "Aphasia" NEJM review
 *
 * 2. Fever-Induced Comprehension Impairment:
 *    - Elevated body temperature -> cytokine release
 *    - Temporary receptive aphasia during fever
 *    - Slowed phoneme discrimination
 *    - Impaired semantic priming
 *    - Reference: Hart & Kraut (2007) "Neural basis of semantic memory"
 *
 * 3. Inflammation Severity -> Comprehension Symptoms:
 *    - Mild: Occasional word misunderstanding, semantic priming delays
 *    - Moderate: Frequent comprehension failures, paraphasic errors
 *    - Severe: Word deafness, severe comprehension loss, neologisms
 *    - Storm: Global receptive aphasia, no comprehension
 *    - Reference: Goodglass (1993) "Understanding aphasia"
 *
 * 4. Chronic Inflammation Effects:
 *    - Progressive semantic memory degradation
 *    - Reduced vocabulary recognition
 *    - Impaired phonological discrimination
 *    - Reference: Mesulam (2001) "Primary progressive aphasia"
 *
 * WERNICKE -> IMMUNE PATHWAYS:
 * -------------------------
 * 1. Comprehension Errors as Neural Damage Indicators:
 *    - Sudden onset comprehension loss -> potential stroke/lesion
 *    - Phonological errors -> auditory cortex damage
 *    - Semantic errors -> temporal lobe damage
 *    - Repetition failures -> arcuate fasciculus damage
 *    - Triggers immune surveillance and response
 *    - Reference: Geschwind (1965) "Disconnexion syndromes"
 *
 * 2. Chronic Comprehension Errors:
 *    - Persistent errors indicate ongoing neural stress
 *    - May trigger low-grade inflammation
 *    - Feedback loop: errors -> stress -> inflammation -> more errors
 *
 * 3. Comprehension Load:
 *    - High cognitive load during comprehension -> metabolic demand
 *    - Complex sentence parsing -> higher immune surveillance
 *    - Reference: Just & Carpenter (1992) "A capacity theory of comprehension"
 *
 * WERNICKE'S APHASIA CHARACTERISTICS:
 * - Fluent speech (unlike Broca's non-fluent aphasia)
 * - Poor comprehension of spoken/written language
 * - Paraphasias (phonemic and semantic word substitutions)
 * - Neologisms (made-up words)
 * - Press of speech (logorrhea)
 * - Poor repetition ability
 * - Anosognosia (unawareness of deficit)
 *
 * @version Phase W6: Wernicke's Area Immune Integration
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WERNICKE_IMMUNE_H
#define NIMCP_WERNICKE_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/bridge/nimcp_bridge_base.h"

/* Immune system integration */
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations (to avoid header conflicts)
 * ============================================================================ */

struct wernicke_adapter;
typedef struct wernicke_adapter wernicke_adapter_t;

struct phonological_analyzer;
typedef struct phonological_analyzer phonological_analyzer_t;

struct lexical_access;
typedef struct lexical_access lexical_access_t;

struct semantic_integrator;
typedef struct semantic_integrator semantic_integrator_t;

struct syntactic_comprehension;
typedef struct syntactic_comprehension syntactic_comprehension_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define WERNICKE_IMMUNE_MODULE_NAME         "wernicke_immune_bridge"

/* Bio-async module ID */
#define BIO_MODULE_WERNICKE_IMMUNE          0x0E5B

/* Inflammation -> Comprehension Impairment Mapping */
#define COMPREHENSION_IMPAIRMENT_NONE       0.0f    /**< No impairment */
#define COMPREHENSION_IMPAIRMENT_MILD       0.1f    /**< Mild delays */
#define COMPREHENSION_IMPAIRMENT_MODERATE   0.4f    /**< Moderate aphasia */
#define COMPREHENSION_IMPAIRMENT_SEVERE     0.7f    /**< Severe aphasia */
#define COMPREHENSION_IMPAIRMENT_STORM      0.95f   /**< Global aphasia */

/* Word Recognition Difficulty Thresholds */
#define WORD_RECOGNITION_THRESHOLD_MILD     0.15f   /**< Occasional failures */
#define WORD_RECOGNITION_THRESHOLD_MODERATE 0.45f   /**< Frequent failures */
#define WORD_RECOGNITION_THRESHOLD_SEVERE   0.75f   /**< Severe word deafness */

/* Comprehension Error Rate Thresholds */
#define COMP_ERROR_RATE_NORMAL              0.05f   /**< 5% errors (normal) */
#define COMP_ERROR_RATE_MILD_INFLAMMATION   0.20f   /**< 20% errors */
#define COMP_ERROR_RATE_MODERATE_INFLAMMATION 0.50f /**< 50% errors */
#define COMP_ERROR_RATE_SEVERE_INFLAMMATION 0.80f   /**< 80% errors */

/* Cytokine-Specific Impairments */
#define CYTOKINE_IL1_PHONEME_SLOWDOWN       0.3f    /**< IL-1beta slows phoneme recognition */
#define CYTOKINE_IL6_LEXICAL_IMPAIRMENT     0.4f    /**< IL-6 impairs word recognition */
#define CYTOKINE_TNF_SEMANTIC_DISRUPTION    0.5f    /**< TNF-alpha disrupts semantic access */
#define CYTOKINE_IL10_RECOVERY_BOOST        0.3f    /**< IL-10 accelerates recovery */

/* Error Detection Thresholds for Immune Triggering */
#define COMP_ERROR_TRIGGER_THRESHOLD        0.30f   /**< Error rate to trigger immune */
#define COMP_ERROR_SEVERITY_MULTIPLIER      2.0f    /**< Multiplier for severity */
#define SUDDEN_COMP_ONSET_WINDOW_MS         5000    /**< 5 sec for sudden onset detection */

/* Chronic Inflammation Duration (milliseconds) */
#define CHRONIC_COMP_INFLAMMATION_THRESHOLD_MS (86400000ULL * 7)  /**< 7 days */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Wernicke's aphasia symptom types
 *
 * Maps to specific Wernicke's region subsystem impairments
 */
typedef enum {
    WERNICKE_APHASIA_NONE = 0,         /**< No aphasia symptoms */
    WERNICKE_APHASIA_WORD_DEAFNESS,    /**< Pure word deafness (phoneme recognition) */
    WERNICKE_APHASIA_SEMANTIC,         /**< Semantic aphasia (meaning access) */
    WERNICKE_APHASIA_ANOMIC,           /**< Anomic aphasia (word finding) */
    WERNICKE_APHASIA_PARAPHASIC,       /**< Paraphasic errors (substitutions) */
    WERNICKE_APHASIA_NEOLOGISTIC,      /**< Neologistic jargon (made-up words) */
    WERNICKE_APHASIA_RECEPTIVE,        /**< Full receptive aphasia */
    WERNICKE_APHASIA_GLOBAL            /**< Global comprehension loss */
} wernicke_aphasia_type_t;

/**
 * @brief Wernicke's region immune state
 */
typedef enum {
    WERNICKE_IMMUNE_NORMAL = 0,        /**< Normal comprehension */
    WERNICKE_IMMUNE_MILD_IMPAIRMENT,   /**< Mild comprehension delays */
    WERNICKE_IMMUNE_MODERATE_APHASIA,  /**< Moderate receptive aphasia */
    WERNICKE_IMMUNE_SEVERE_APHASIA,    /**< Severe receptive aphasia */
    WERNICKE_IMMUNE_STORM,             /**< Cytokine storm (no comprehension) */
    WERNICKE_IMMUNE_RECOVERING         /**< Recovery phase */
} wernicke_immune_state_t;

/**
 * @brief Comprehension error type
 */
typedef enum {
    COMP_ERROR_NONE = 0,               /**< No error */
    COMP_ERROR_PHONOLOGICAL,           /**< Phoneme discrimination failure */
    COMP_ERROR_LEXICAL,                /**< Word recognition failure */
    COMP_ERROR_SEMANTIC,               /**< Meaning extraction failure */
    COMP_ERROR_SYNTACTIC,              /**< Sentence parsing failure */
    COMP_ERROR_REPETITION,             /**< Repetition failure (arcuate) */
    COMP_ERROR_CONTEXT,                /**< Context integration failure */
    COMP_ERROR_NEOLOGISM               /**< Neologism in output */
} wernicke_comp_error_type_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Comprehension impairment metrics
 *
 * Quantifies how inflammation affects comprehension subsystems
 */
typedef struct {
    /* Overall impairment */
    float overall_impairment;              /**< Global comprehension impairment [0-1] */
    wernicke_aphasia_type_t dominant_symptom; /**< Primary symptom */

    /* Subsystem impairments */
    float phoneme_recognition_impairment;  /**< Sound recognition difficulty [0-1] */
    float lexical_access_impairment;       /**< Word recognition difficulty [0-1] */
    float semantic_integration_impairment; /**< Meaning extraction difficulty [0-1] */
    float syntactic_parsing_impairment;    /**< Sentence parsing difficulty [0-1] */

    /* Specific symptoms */
    float word_deafness_severity;          /**< Pure word deafness [0-1] */
    float semantic_paraphasia_rate;        /**< Semantic substitution rate [0-1] */
    float phonemic_paraphasia_rate;        /**< Phonemic substitution rate [0-1] */
    float neologism_rate;                  /**< Made-up word rate [0-1] */
    float comprehension_delay_ms;          /**< Processing delay in milliseconds */

    /* Performance metrics */
    float processing_speed_multiplier;     /**< Speed reduction (0.5 = half speed) */
    float error_rate;                      /**< Overall error rate [0-1] */
    float semantic_priming_strength;       /**< Semantic priming effectiveness [0-1] */
    float phoneme_discrimination_accuracy; /**< Phoneme discrimination [0-1] */
} wernicke_comprehension_impairment_t;

/**
 * @brief Cytokine effects on Wernicke's region
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_phoneme_slowdown;            /**< IL-1beta slows phoneme recognition */
    float il6_lexical_disruption;          /**< IL-6 impairs word recognition */
    float tnf_semantic_disruption;         /**< TNF-alpha disrupts semantic access */
    float ifn_gamma_parsing_impairment;    /**< IFN-gamma impairs syntax parsing */

    /* Anti-inflammatory effects */
    float il10_recovery_boost;             /**< IL-10 recovery acceleration */

    /* Aggregate modulation */
    float total_phoneme_modulation;        /**< Combined phoneme effect */
    float total_lexical_modulation;        /**< Combined lexical effect */
    float total_semantic_modulation;       /**< Combined semantic effect */
    float total_syntactic_modulation;      /**< Combined syntax effect */
} wernicke_cytokine_effects_t;

/**
 * @brief Comprehension error pattern for immune detection
 *
 * Analyzed to detect potential neural damage
 */
typedef struct {
    uint32_t error_id;                     /**< Unique error ID */
    wernicke_comp_error_type_t type;       /**< Error type */
    uint64_t timestamp_ms;                 /**< When error occurred */

    /* Error details */
    char expected_meaning[256];            /**< What should have been understood */
    char actual_interpretation[256];       /**< What was actually understood */
    uint32_t word_position;                /**< Position in utterance */

    /* Severity */
    float severity;                        /**< Error severity [0-1] */
    bool catastrophic;                     /**< Sudden severe error */

    /* Pattern signature (for antigen) */
    uint8_t error_signature[64];           /**< Error pattern signature */
    size_t signature_len;                  /**< Signature length */
} wernicke_comp_error_t;

/**
 * @brief Wernicke's region inflammation state
 */
typedef struct {
    /* Inflammation tracking */
    brain_inflammation_level_t current_level;
    uint32_t inflammation_site_id;         /**< Immune system site ID */
    uint64_t inflammation_duration_ms;     /**< How long inflamed */
    bool is_chronic;                       /**< >= 7 days */

    /* Symptom progression */
    wernicke_aphasia_type_t current_symptoms[4]; /**< Active symptoms */
    uint32_t symptom_count;                /**< Number of symptoms */
    float symptom_progression_rate;        /**< Rate of worsening */

    /* Recovery tracking */
    bool in_recovery;                      /**< Recovery phase active */
    uint64_t recovery_start_ms;            /**< When recovery began */
    float recovery_progress;               /**< Recovery progress [0-1] */
} wernicke_inflammation_state_t;

/**
 * @brief Comprehension error history for damage detection
 */
typedef struct {
    wernicke_comp_error_t* errors;         /**< Error array */
    size_t error_count;                    /**< Number of errors */
    size_t error_capacity;                 /**< Array capacity */

    /* Pattern analysis */
    float recent_error_rate;               /**< Errors in last minute */
    float baseline_error_rate;             /**< Normal error rate */
    bool sudden_onset_detected;            /**< Sudden increase in errors */
    uint64_t onset_timestamp_ms;           /**< When sudden onset occurred */

    /* Damage indicators */
    float phonological_damage_score;       /**< Phonological subsystem damage */
    float lexical_damage_score;            /**< Lexical subsystem damage */
    float semantic_damage_score;           /**< Semantic subsystem damage */
    float syntactic_damage_score;          /**< Syntactic subsystem damage */
} wernicke_error_history_t;

/**
 * @brief Configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_inflammation_impairment;   /**< Inflammation affects comprehension */
    bool enable_cytokine_modulation;       /**< Cytokines affect subsystems */
    bool enable_error_immune_trigger;      /**< Errors trigger immune */
    bool enable_chronic_inflammation_tracking; /**< Track chronic effects */
    bool enable_recovery_monitoring;       /**< Monitor recovery */

    /* Sensitivity tuning */
    float inflammation_sensitivity;        /**< Impairment multiplier [0.5-2.0] */
    float cytokine_sensitivity;            /**< Cytokine effect multiplier */
    float error_detection_sensitivity;     /**< Error detection threshold */

    /* Thresholds */
    float word_deafness_threshold;         /**< Threshold for word deafness */
    float semantic_aphasia_threshold;      /**< Threshold for semantic impairment */
    float error_trigger_threshold;         /**< Error rate to trigger immune */

    /* Error tracking */
    size_t max_error_history;              /**< Max errors to track */
    uint64_t error_analysis_window_ms;     /**< Window for error rate calculation */

    /* Logging */
    bool enable_logging;                   /**< Enable integration logging */
} wernicke_immune_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    /* Impairment episodes */
    uint64_t total_impairment_episodes;    /**< Times entered impaired state */
    uint64_t mild_episodes;                /**< Mild impairment episodes */
    uint64_t moderate_episodes;            /**< Moderate aphasia episodes */
    uint64_t severe_episodes;              /**< Severe aphasia episodes */
    uint64_t storm_episodes;               /**< Cytokine storm episodes */

    /* Error tracking */
    uint64_t total_comp_errors;            /**< Total errors detected */
    uint64_t phonological_errors;          /**< Phonological errors */
    uint64_t lexical_errors;               /**< Lexical errors */
    uint64_t semantic_errors;              /**< Semantic errors */
    uint64_t syntactic_errors;             /**< Syntactic errors */
    uint64_t immune_triggers_from_errors;  /**< Errors that triggered immune */

    /* Recovery */
    uint64_t recovery_episodes;            /**< Recovery phases entered */
    float avg_recovery_time_ms;            /**< Average recovery duration */

    /* Performance impact */
    float avg_processing_delay;            /**< Average delay increase */
    float avg_error_rate;                  /**< Average error rate */
    float max_impairment_observed;         /**< Worst impairment */
} wernicke_immune_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Wernicke-immune bridge state
 */
typedef struct {
    bridge_base_t base;                    /**< MUST be first: base bridge infrastructure */
    wernicke_immune_config_t config;       /**< Configuration */
    wernicke_immune_state_t state;         /**< Current state */

    /* Module handles */
    brain_immune_system_t* immune_system;
    wernicke_adapter_t* wernicke_adapter;
    phonological_analyzer_t* phonological_analyzer;
    lexical_access_t* lexical_access;
    semantic_integrator_t* semantic_integrator;
    syntactic_comprehension_t* syntactic_comprehension;

    /* Current state */
    wernicke_comprehension_impairment_t impairment;
    wernicke_cytokine_effects_t cytokine_effects;
    wernicke_inflammation_state_t inflammation_state;
    wernicke_error_history_t error_history;

    /* Statistics */
    wernicke_immune_stats_t stats;

    /* State tracking */
    uint64_t last_update_time_ms;          /**< Last state update */
    uint64_t state_entry_time_ms;          /**< When entered current state */

    /* Status */
    bool running;                          /**< Integration active */
} wernicke_immune_bridge_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for aphasia onset
 */
typedef void (*wernicke_immune_aphasia_onset_cb_t)(
    wernicke_immune_bridge_t* bridge,
    wernicke_aphasia_type_t symptom,
    float severity,
    void* user_data
);

/**
 * @brief Callback for comprehension error detection
 */
typedef void (*wernicke_immune_error_cb_t)(
    wernicke_immune_bridge_t* bridge,
    const wernicke_comp_error_t* error,
    bool triggered_immune,
    void* user_data
);

/**
 * @brief Callback for impairment changes
 */
typedef void (*wernicke_immune_impairment_cb_t)(
    wernicke_immune_bridge_t* bridge,
    float old_impairment,
    float new_impairment,
    brain_inflammation_level_t inflammation,
    void* user_data
);

/**
 * @brief Callback for recovery progress
 */
typedef void (*wernicke_immune_recovery_cb_t)(
    wernicke_immune_bridge_t* bridge,
    float recovery_progress,
    void* user_data
);

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
int wernicke_immune_default_config(wernicke_immune_config_t* config);

/**
 * @brief Create Wernicke-immune bridge
 *
 * WHAT: Initialize bidirectional Wernicke-immune integration
 * WHY:  Enable realistic comprehension impairment from inflammation
 * HOW:  Allocate structure, link subsystems, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system (required)
 * @param wernicke_adapter Wernicke's adapter (required)
 * @return New bridge or NULL on failure
 */
wernicke_immune_bridge_t* wernicke_immune_bridge_create(
    const wernicke_immune_config_t* config,
    brain_immune_system_t* immune_system,
    wernicke_adapter_t* wernicke_adapter
);

/**
 * @brief Destroy Wernicke-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structures, unregister callbacks
 *
 * @param bridge Bridge to destroy
 */
void wernicke_immune_bridge_destroy(wernicke_immune_bridge_t* bridge);

/**
 * @brief Start integration monitoring
 *
 * WHAT: Activate immune-Wernicke monitoring
 * WHY:  Begin modulation and error detection
 * HOW:  Register callbacks, start periodic updates
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int wernicke_immune_bridge_start(wernicke_immune_bridge_t* bridge);

/**
 * @brief Stop integration
 *
 * WHAT: Deactivate integration
 * WHY:  Graceful shutdown
 * HOW:  Unregister callbacks, complete pending operations
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int wernicke_immune_bridge_stop(wernicke_immune_bridge_t* bridge);

/* ============================================================================
 * Immune -> Wernicke API (Inflammation Effects)
 * ============================================================================ */

/**
 * @brief Apply inflammation effects to language comprehension
 *
 * WHAT: Modulate all Wernicke subsystems based on inflammation level
 * WHY:  Inflammation causes receptive aphasia symptoms
 * HOW:  Query inflammation -> Map to impairments -> Update subsystems
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int wernicke_immune_apply_inflammation_effects(wernicke_immune_bridge_t* bridge);

/**
 * @brief Apply cytokine effects to specific subsystems
 *
 * WHAT: Modulate phoneme, lexical, semantic, syntactic based on cytokines
 * WHY:  Different cytokines have specific neural effects
 * HOW:  Query cytokine levels -> Compute modulation -> Apply to subsystems
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int wernicke_immune_apply_cytokine_effects(wernicke_immune_bridge_t* bridge);

/**
 * @brief Compute comprehension impairment from inflammation
 *
 * WHAT: Calculate detailed impairment metrics
 * WHY:  Quantify Wernicke's aphasia symptoms
 * HOW:  Map inflammation level/duration to symptom severity
 *
 * @param bridge Bridge instance
 * @param impairment Output: impairment metrics
 * @return 0 on success
 */
int wernicke_immune_compute_impairment(
    wernicke_immune_bridge_t* bridge,
    wernicke_comprehension_impairment_t* impairment
);

/* ============================================================================
 * Wernicke -> Immune API (Error Detection)
 * ============================================================================ */

/**
 * @brief Report comprehension error
 *
 * WHAT: Log comprehension error for damage detection
 * WHY:  Errors may indicate neural damage
 * HOW:  Create error record, analyze pattern, potentially trigger immune
 *
 * @param bridge Bridge instance
 * @param error_type Type of error
 * @param expected What should have been understood
 * @param actual What was actually understood
 * @param severity Error severity [0-1]
 * @return 0 on success
 */
int wernicke_immune_report_comp_error(
    wernicke_immune_bridge_t* bridge,
    wernicke_comp_error_type_t error_type,
    const char* expected,
    const char* actual,
    float severity
);

/**
 * @brief Analyze error pattern for damage indicators
 *
 * WHAT: Examine error history for neural damage patterns
 * WHY:  Sudden onset or persistent errors indicate damage
 * HOW:  Analyze error rates, types, patterns
 *
 * @param bridge Bridge instance
 * @param phonological_damage Output: phonological damage score
 * @param lexical_damage Output: lexical damage score
 * @param semantic_damage Output: semantic damage score
 * @param syntactic_damage Output: syntactic damage score
 * @return 0 on success
 */
int wernicke_immune_analyze_error_patterns(
    wernicke_immune_bridge_t* bridge,
    float* phonological_damage,
    float* lexical_damage,
    float* semantic_damage,
    float* syntactic_damage
);

/**
 * @brief Trigger immune response from comprehension errors
 *
 * WHAT: Present antigen from error pattern if threshold exceeded
 * WHY:  High error rate indicates potential neural damage
 * HOW:  Create epitope from error signature, present to immune system
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 if no trigger needed
 */
int wernicke_immune_trigger_from_errors(wernicke_immune_bridge_t* bridge);

/* ============================================================================
 * Update and State Management
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Process current immune state and update Wernicke modulation
 * WHY:  Maintain synchronization
 * HOW:  Query immune -> Analyze -> Apply effects -> Detect errors
 *
 * @param bridge Bridge instance
 * @param current_time_ms Current timestamp
 * @return 0 on success
 */
int wernicke_immune_bridge_update(
    wernicke_immune_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Get current state
 *
 * @param bridge Bridge instance
 * @return Current state
 */
wernicke_immune_state_t wernicke_immune_get_state(
    const wernicke_immune_bridge_t* bridge
);

/**
 * @brief Get current impairment metrics
 *
 * @param bridge Bridge instance
 * @param impairment Output: impairment metrics
 * @return 0 on success
 */
int wernicke_immune_get_impairment(
    const wernicke_immune_bridge_t* bridge,
    wernicke_comprehension_impairment_t* impairment
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics
 * @return 0 on success
 */
int wernicke_immune_get_stats(
    const wernicke_immune_bridge_t* bridge,
    wernicke_immune_stats_t* stats
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set aphasia onset callback
 */
int wernicke_immune_set_aphasia_callback(
    wernicke_immune_bridge_t* bridge,
    wernicke_immune_aphasia_onset_cb_t callback,
    void* user_data
);

/**
 * @brief Set comprehension error callback
 */
int wernicke_immune_set_error_callback(
    wernicke_immune_bridge_t* bridge,
    wernicke_immune_error_cb_t callback,
    void* user_data
);

/**
 * @brief Set impairment change callback
 */
int wernicke_immune_set_impairment_callback(
    wernicke_immune_bridge_t* bridge,
    wernicke_immune_impairment_cb_t callback,
    void* user_data
);

/**
 * @brief Set recovery progress callback
 */
int wernicke_immune_set_recovery_callback(
    wernicke_immune_bridge_t* bridge,
    wernicke_immune_recovery_cb_t callback,
    void* user_data
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* wernicke_aphasia_type_to_string(wernicke_aphasia_type_t type);
const char* wernicke_immune_state_to_string(wernicke_immune_state_t state);
const char* wernicke_comp_error_type_to_string(wernicke_comp_error_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_IMMUNE_H */
