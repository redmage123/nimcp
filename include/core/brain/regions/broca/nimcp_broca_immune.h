/**
 * @file nimcp_broca_immune.h
 * @brief Broca's Region - Brain Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and Broca's region
 *       (speech production, syntax processing, phonological encoding, motor planning)
 * WHY:  Biological evidence shows neuroinflammation in Broca's area causes expressive
 *       aphasia symptoms. Fever and inflammation consistently impair speech production.
 *       Essential for realistic brain modeling of language production deficits.
 * HOW:  Immune system modulates speech production fluency, word-finding, and motor
 *       control. Speech production errors may signal neural damage and trigger immune
 *       surveillance. Inflammation models aphasia-like symptoms.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → BROCA PATHWAYS:
 * -------------------------
 * 1. Neuroinflammation in Broca's Area:
 *    - Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) disrupt speech production
 *    - Impairs left inferior frontal gyrus function
 *    - Causes expressive aphasia-like symptoms
 *    - Word-finding difficulty (anomia)
 *    - Speech dysfluency and hesitations
 *    - Reference: Dronkers (1996) "A new brain region for coordinating speech articulation"
 *    - Reference: Hillis et al. (2004) "Hypoperfusion of Wernicke's area predicts severity"
 *
 * 2. Fever-Induced Speech Impairment:
 *    - Elevated body temperature → cytokine release
 *    - Temporary expressive aphasia during fever
 *    - Slowed articulation and increased errors
 *    - Reduced phonological working memory
 *    - Reference: Caplan (1987) "Neurolinguistics and linguistic aphasiology"
 *
 * 3. Inflammation Severity → Aphasia Symptoms:
 *    - Mild: Occasional word-finding hesitations, mild dysfluency
 *    - Moderate: Frequent paraphasias, agrammatism, effortful speech
 *    - Severe: Telegraphic speech, severe anomia, motor speech apraxia
 *    - Storm: Non-fluent aphasia, near-mutism
 *    - Reference: Goodglass (1993) "Understanding aphasia"
 *
 * 4. Chronic Inflammation Effects:
 *    - Progressive syntactic simplification
 *    - Reduced lexical diversity
 *    - Increased reliance on automatic speech
 *    - Motor planning deterioration
 *    - Reference: Mesulam (2001) "Primary progressive aphasia"
 *
 * BROCA → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Speech Production Errors as Neural Damage Indicators:
 *    - Sudden onset aphasia symptoms → potential stroke/lesion
 *    - Phonological errors → phonological loop damage
 *    - Syntactic breakdown → syntax processor damage
 *    - Motor speech errors → motor planning damage
 *    - Triggers immune surveillance and response
 *    - Reference: Geschwind (1965) "Disconnexion syndromes"
 *
 * 2. Chronic Speech Errors:
 *    - Persistent errors indicate ongoing neural stress
 *    - May trigger low-grade inflammation
 *    - Feedback loop: errors → stress → inflammation → more errors
 *    - Reference: Crosson et al. (2007) "Functional MRI of language"
 *
 * 3. Speech Production Load:
 *    - High cognitive load during speech → metabolic demand
 *    - Increased metabolic waste → microglial activation
 *    - Complex sentence production → higher immune surveillance
 *    - Reference: Just & Carpenter (1992) "A capacity theory of comprehension"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    BROCA-IMMUNE BRIDGE                                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → BROCA PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │  INFLAMMATION LEVEL → SPEECH PRODUCTION IMPAIRMENT           │ │  ║
 * ║   │   │ ──────────────────────────────────────────────────────────── │ │  ║
 * ║   │   │ None:     Normal fluent speech                               │ │  ║
 * ║   │   │ Mild:     Occasional word-finding pauses (±10% slowdown)     │ │  ║
 * ║   │   │ Moderate: Frequent paraphasias, agrammatism (±40% errors)    │ │  ║
 * ║   │   │ Severe:   Telegraphic speech, severe anomia (±70% errors)    │ │  ║
 * ║   │   │ Storm:    Non-fluent aphasia, near-mutism (±95% impairment)  │ │  ║
 * ║   │   └──────────────────────────────────────────────────────────────┘ │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │     BROCA'S REGION SUBSYSTEMS                               │ │  ║
 * ║   │   │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐│ │  ║
 * ║   │   │  │   Syntax     │  │ Phonological │  │  Speech Motor      ││ │  ║
 * ║   │   │  │  Processor   │  │  Processor   │  │    Planner         ││ │  ║
 * ║   │   │  ├──────────────┤  ├──────────────┤  ├────────────────────┤│ │  ║
 * ║   │   │  │ Agrammatism  │  │ Paraphasias  │  │ Apraxia of speech  ││ │  ║
 * ║   │   │  │ Simple syntax│  │ Phoneme sub. │  │ Effortful artic.   ││ │  ║
 * ║   │   │  │ Telegraphic  │  │ Sound errors │  │ Dysarthria         ││ │  ║
 * ║   │   │  └──────────────┘  └──────────────┘  └────────────────────┘│ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │  CYTOKINE MODULATION                                         │ │  ║
 * ║   │   │ ──────────────────────────────────────────────────────────── │ │  ║
 * ║   │   │ IL-1β → Reduced lexical access speed                         │ │  ║
 * ║   │   │ IL-6  → Impaired syntax generation                           │ │  ║
 * ║   │   │ TNF-α → Phonological loop disruption                         │ │  ║
 * ║   │   │ IL-10 → Recovery, fluency restoration                        │ │  ║
 * ║   │   └──────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  BROCA → IMMUNE PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │  SPEECH ERRORS → NEURAL DAMAGE DETECTION                     │ │  ║
 * ║   │   │ ──────────────────────────────────────────────────────────── │ │  ║
 * ║   │   │ Sudden aphasia onset   → Present antigen (stroke/lesion)     │ │  ║
 * ║   │   │ High phonological error → Phonological loop damage signal    │ │  ║
 * ║   │   │ Syntax breakdown       → Syntax processor damage signal      │ │  ║
 * ║   │   │ Motor speech apraxia   → Motor planning damage signal        │ │  ║
 * ║   │   └──────────────────────────────────────────────────────────────┘ │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌──────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │     IMMUNE SURVEILLANCE ACTIVATION                           │ │  ║
 * ║   │   │  - Present antigen from error pattern signature              │ │  ║
 * ║   │   │  - Trigger localized inflammation in Broca's region          │ │  ║
 * ║   │   │  - Activate repair mechanisms                                │ │  ║
 * ║   │   └──────────────────────────────────────────────────────────────┘ │  ║
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

#ifndef NIMCP_BROCA_IMMUNE_H
#define NIMCP_BROCA_IMMUNE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Broca's region modules */
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/broca/nimcp_syntax_processor.h"
#include "core/brain/regions/broca/nimcp_phonological.h"
#include "core/brain/regions/broca/nimcp_speech_motor.h"
#include "core/brain/regions/broca/nimcp_language_production_bridge.h"

/* Immune system integration */
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BROCA_IMMUNE_MODULE_NAME            "broca_immune_bridge"

/* Inflammation → Speech Impairment Mapping */
#define IMPAIRMENT_NONE                     0.0f    /**< No impairment */
#define IMPAIRMENT_MILD                     0.1f    /**< Mild dysfluency */
#define IMPAIRMENT_MODERATE                 0.4f    /**< Moderate aphasia */
#define IMPAIRMENT_SEVERE                   0.7f    /**< Severe aphasia */
#define IMPAIRMENT_STORM                    0.95f   /**< Near-mutism */

/* Word-Finding Difficulty (Anomia) Thresholds */
#define ANOMIA_THRESHOLD_MILD               0.15f   /**< Occasional pauses */
#define ANOMIA_THRESHOLD_MODERATE           0.45f   /**< Frequent difficulty */
#define ANOMIA_THRESHOLD_SEVERE             0.75f   /**< Severe word-finding */

/* Speech Error Rate Thresholds */
#define ERROR_RATE_NORMAL                   0.02f   /**< 2% errors (normal) */
#define ERROR_RATE_MILD_INFLAMMATION        0.12f   /**< 12% errors */
#define ERROR_RATE_MODERATE_INFLAMMATION    0.40f   /**< 40% errors */
#define ERROR_RATE_SEVERE_INFLAMMATION      0.70f   /**< 70% errors */

/* Cytokine-Specific Impairments */
#define CYTOKINE_IL1_LEXICAL_SLOWDOWN       0.3f    /**< IL-1β slows lexical access */
#define CYTOKINE_IL6_SYNTAX_IMPAIRMENT      0.4f    /**< IL-6 impairs syntax */
#define CYTOKINE_TNF_PHONOLOGICAL_DISRUPTION 0.5f   /**< TNF-α disrupts phonology */
#define CYTOKINE_IL10_RECOVERY_BOOST        0.3f    /**< IL-10 accelerates recovery */

/* Error Detection Thresholds for Immune Triggering */
#define ERROR_TRIGGER_THRESHOLD             0.25f   /**< Error rate to trigger immune */
#define ERROR_SEVERITY_MULTIPLIER           2.0f    /**< Multiplier for severity */
#define SUDDEN_ONSET_WINDOW_MS              5000    /**< 5 sec for sudden onset detection */

/* Chronic Inflammation Duration (milliseconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD_MS   (86400000ULL * 7)  /**< 7 days */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Aphasia symptom types
 *
 * Maps to specific Broca's region subsystem impairments
 */
typedef enum {
    APHASIA_NONE = 0,              /**< No aphasia symptoms */
    APHASIA_ANOMIA,                /**< Word-finding difficulty */
    APHASIA_AGRAMMATISM,           /**< Syntax impairment (telegraphic) */
    APHASIA_PHONOLOGICAL,          /**< Phonological paraphasias */
    APHASIA_MOTOR_SPEECH,          /**< Apraxia of speech */
    APHASIA_NONFLUENT,             /**< Non-fluent Broca's aphasia */
    APHASIA_MUTISM                 /**< Near-total speech loss */
} broca_aphasia_type_t;

/**
 * @brief Broca's region immune state
 */
typedef enum {
    BROCA_IMMUNE_NORMAL = 0,       /**< Normal speech production */
    BROCA_IMMUNE_MILD_IMPAIRMENT,  /**< Mild dysfluency */
    BROCA_IMMUNE_MODERATE_APHASIA, /**< Moderate expressive aphasia */
    BROCA_IMMUNE_SEVERE_APHASIA,   /**< Severe expressive aphasia */
    BROCA_IMMUNE_STORM,            /**< Cytokine storm (near-mutism) */
    BROCA_IMMUNE_RECOVERING        /**< Recovery phase */
} broca_immune_state_t;

/**
 * @brief Speech error type
 */
typedef enum {
    SPEECH_ERROR_NONE = 0,         /**< No error */
    SPEECH_ERROR_PHONOLOGICAL,     /**< Phoneme substitution/paraphasia */
    SPEECH_ERROR_LEXICAL,          /**< Word substitution/anomia */
    SPEECH_ERROR_SYNTACTIC,        /**< Grammar error/agrammatism */
    SPEECH_ERROR_MOTOR,            /**< Articulation error/apraxia */
    SPEECH_ERROR_HESITATION,       /**< Word-finding pause */
    SPEECH_ERROR_PERSEVERATION     /**< Repetition/stuck utterance */
} broca_speech_error_type_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Speech production impairment metrics
 *
 * Quantifies how inflammation affects speech production subsystems
 */
typedef struct {
    /* Overall impairment */
    float overall_impairment;          /**< Global speech impairment [0-1] */
    broca_aphasia_type_t dominant_symptom; /**< Primary symptom */

    /* Subsystem impairments */
    float lexical_access_impairment;   /**< Word retrieval difficulty [0-1] */
    float syntax_impairment;           /**< Grammar generation difficulty [0-1] */
    float phonological_impairment;     /**< Sound pattern difficulty [0-1] */
    float motor_planning_impairment;   /**< Articulation difficulty [0-1] */

    /* Specific symptoms */
    float anomia_severity;             /**< Word-finding difficulty [0-1] */
    float agrammatism_severity;        /**< Telegraphic speech [0-1] */
    float paraphasia_rate;             /**< Phoneme substitution rate [0-1] */
    float apraxia_severity;            /**< Motor speech difficulty [0-1] */
    float dysfluency_rate;             /**< Hesitations/pauses rate [0-1] */

    /* Performance metrics */
    float speech_rate_multiplier;      /**< Speed reduction (0.5 = half speed) */
    float error_rate;                  /**< Overall error rate [0-1] */
    float lexical_diversity;           /**< Vocabulary diversity [0-1] */
    float mean_utterance_length;       /**< Words per utterance */
} broca_speech_impairment_t;

/**
 * @brief Cytokine effects on Broca's region
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_lexical_slowdown;        /**< IL-1β slows word access */
    float il6_syntax_disruption;       /**< IL-6 impairs grammar */
    float tnf_phonological_disruption; /**< TNF-α disrupts phonology */
    float ifn_gamma_motor_impairment;  /**< IFN-γ impairs motor planning */

    /* Anti-inflammatory effects */
    float il10_recovery_boost;         /**< IL-10 recovery acceleration */

    /* Aggregate modulation */
    float total_lexical_modulation;    /**< Combined lexical effect */
    float total_syntax_modulation;     /**< Combined syntax effect */
    float total_phonological_modulation; /**< Combined phonological effect */
    float total_motor_modulation;      /**< Combined motor effect */
} broca_cytokine_effects_t;

/**
 * @brief Speech error pattern for immune detection
 *
 * Analyzed to detect potential neural damage
 */
typedef struct {
    uint32_t error_id;                 /**< Unique error ID */
    broca_speech_error_type_t type;    /**< Error type */
    uint64_t timestamp_ms;             /**< When error occurred */

    /* Error details */
    char intended_utterance[256];      /**< What was intended */
    char actual_output[256];           /**< What was produced */
    uint32_t error_position;           /**< Position in utterance */

    /* Severity */
    float severity;                    /**< Error severity [0-1] */
    bool catastrophic;                 /**< Sudden severe error */

    /* Pattern signature (for antigen) */
    uint8_t error_signature[64];       /**< Error pattern signature */
    size_t signature_len;              /**< Signature length */
} broca_speech_error_t;

/**
 * @brief Broca's region inflammation state
 */
typedef struct {
    /* Inflammation tracking */
    brain_inflammation_level_t current_level;
    uint32_t inflammation_site_id;     /**< Immune system site ID */
    uint64_t inflammation_duration_ms; /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Symptom progression */
    broca_aphasia_type_t current_symptoms[4]; /**< Active symptoms */
    uint32_t symptom_count;            /**< Number of symptoms */
    float symptom_progression_rate;    /**< Rate of worsening */

    /* Recovery tracking */
    bool in_recovery;                  /**< Recovery phase active */
    uint64_t recovery_start_ms;        /**< When recovery began */
    float recovery_progress;           /**< Recovery progress [0-1] */
} broca_inflammation_state_t;

/**
 * @brief Speech error history for damage detection
 */
typedef struct {
    broca_speech_error_t* errors;      /**< Error array */
    size_t error_count;                /**< Number of errors */
    size_t error_capacity;             /**< Array capacity */

    /* Pattern analysis */
    float recent_error_rate;           /**< Errors in last minute */
    float baseline_error_rate;         /**< Normal error rate */
    bool sudden_onset_detected;        /**< Sudden increase in errors */
    uint64_t onset_timestamp_ms;       /**< When sudden onset occurred */

    /* Damage indicators */
    float phonological_damage_score;   /**< Phonological subsystem damage */
    float syntactic_damage_score;      /**< Syntax subsystem damage */
    float motor_damage_score;          /**< Motor planning damage */
} broca_error_history_t;

/**
 * @brief Configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_inflammation_impairment;  /**< Inflammation affects speech */
    bool enable_cytokine_modulation;      /**< Cytokines affect subsystems */
    bool enable_error_immune_trigger;     /**< Errors trigger immune */
    bool enable_chronic_inflammation_tracking; /**< Track chronic effects */
    bool enable_recovery_monitoring;      /**< Monitor recovery */

    /* Sensitivity tuning */
    float inflammation_sensitivity;    /**< Impairment multiplier [0.5-2.0] */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier */
    float error_detection_sensitivity; /**< Error detection threshold */

    /* Thresholds */
    float anomia_threshold;            /**< Threshold for word-finding difficulty */
    float agrammatism_threshold;       /**< Threshold for telegraphic speech */
    float error_trigger_threshold;     /**< Error rate to trigger immune */

    /* Error tracking */
    size_t max_error_history;          /**< Max errors to track */
    uint64_t error_analysis_window_ms; /**< Window for error rate calculation */

    /* Logging */
    bool enable_logging;               /**< Enable integration logging */
} broca_immune_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    /* Impairment episodes */
    uint64_t total_impairment_episodes; /**< Times entered impaired state */
    uint64_t mild_episodes;            /**< Mild impairment episodes */
    uint64_t moderate_episodes;        /**< Moderate aphasia episodes */
    uint64_t severe_episodes;          /**< Severe aphasia episodes */
    uint64_t storm_episodes;           /**< Cytokine storm episodes */

    /* Error tracking */
    uint64_t total_speech_errors;      /**< Total errors detected */
    uint64_t phonological_errors;      /**< Phonological errors */
    uint64_t syntactic_errors;         /**< Syntax errors */
    uint64_t motor_errors;             /**< Motor speech errors */
    uint64_t immune_triggers_from_errors; /**< Errors that triggered immune */

    /* Recovery */
    uint64_t recovery_episodes;        /**< Recovery phases entered */
    float avg_recovery_time_ms;        /**< Average recovery duration */

    /* Performance impact */
    float avg_speech_rate_reduction;   /**< Average slowdown */
    float avg_error_rate;              /**< Average error rate */
    float max_impairment_observed;     /**< Worst impairment */
} broca_immune_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Broca-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    broca_immune_config_t config;      /**< Configuration */
    broca_immune_state_t state;        /**< Current state */

    /* Module handles */
    brain_immune_system_t* immune_system;
    broca_adapter_t* broca_adapter;
    syntax_processor_t* syntax_processor;
    phonological_processor_t* phonological_processor;
    speech_motor_planner_t* speech_motor_planner;
    language_production_bridge_t* language_bridge;

    /* Current state */
    broca_speech_impairment_t impairment;
    broca_cytokine_effects_t cytokine_effects;
    broca_inflammation_state_t inflammation_state;
    broca_error_history_t error_history;

    /* Statistics */
    broca_immune_stats_t stats;

    /* State tracking */
    uint64_t last_update_time_ms;      /**< Last state update */
    uint64_t state_entry_time_ms;      /**< When entered current state */

    /* Status */
    bool running;                      /**< Integration active */

    /* Callbacks (stored as void* since types defined after struct) */
    void (*aphasia_cb)(void);
    void* aphasia_cb_data;
    void (*error_cb)(void);
    void* error_cb_data;
    void (*impairment_cb)(void);
    void* impairment_cb_data;
    void (*recovery_cb)(void);
    void* recovery_cb_data;
} broca_immune_bridge_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for aphasia onset
 */
typedef void (*broca_immune_aphasia_onset_cb_t)(
    broca_immune_bridge_t* bridge,
    broca_aphasia_type_t symptom,
    float severity,
    void* user_data
);

/**
 * @brief Callback for speech error detection
 */
typedef void (*broca_immune_error_cb_t)(
    broca_immune_bridge_t* bridge,
    const broca_speech_error_t* error,
    bool triggered_immune,
    void* user_data
);

/**
 * @brief Callback for impairment changes
 */
typedef void (*broca_immune_impairment_cb_t)(
    broca_immune_bridge_t* bridge,
    float old_impairment,
    float new_impairment,
    brain_inflammation_level_t inflammation,
    void* user_data
);

/**
 * @brief Callback for recovery progress
 */
typedef void (*broca_immune_recovery_cb_t)(
    broca_immune_bridge_t* bridge,
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
int broca_immune_default_config(broca_immune_config_t* config);

/**
 * @brief Create Broca-immune bridge
 *
 * WHAT: Initialize bidirectional Broca-immune integration
 * WHY:  Enable realistic speech production impairment from inflammation
 * HOW:  Allocate structure, link subsystems, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system (required)
 * @param broca_adapter Broca's adapter (required)
 * @return New bridge or NULL on failure
 */
broca_immune_bridge_t* broca_immune_bridge_create(
    const broca_immune_config_t* config,
    brain_immune_system_t* immune_system,
    broca_adapter_t* broca_adapter
);

/**
 * @brief Destroy Broca-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structures, unregister callbacks
 *
 * @param bridge Bridge to destroy
 */
void broca_immune_bridge_destroy(broca_immune_bridge_t* bridge);

/**
 * @brief Start integration monitoring
 *
 * WHAT: Activate immune-Broca monitoring
 * WHY:  Begin modulation and error detection
 * HOW:  Register callbacks, start periodic updates
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int broca_immune_bridge_start(broca_immune_bridge_t* bridge);

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
int broca_immune_bridge_stop(broca_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Broca API (Inflammation Effects)
 * ============================================================================ */

/**
 * @brief Apply inflammation effects to speech production
 *
 * WHAT: Modulate all Broca subsystems based on inflammation level
 * WHY:  Inflammation causes expressive aphasia symptoms
 * HOW:  Query inflammation → Map to impairments → Update subsystems
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int broca_immune_apply_inflammation_effects(broca_immune_bridge_t* bridge);

/**
 * @brief Apply cytokine effects to specific subsystems
 *
 * WHAT: Modulate lexical, syntax, phonological, motor based on cytokines
 * WHY:  Different cytokines have specific neural effects
 * HOW:  Query cytokine levels → Compute modulation → Apply to subsystems
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int broca_immune_apply_cytokine_effects(broca_immune_bridge_t* bridge);

/**
 * @brief Compute speech impairment from inflammation
 *
 * WHAT: Calculate detailed impairment metrics
 * WHY:  Quantify aphasia symptoms
 * HOW:  Map inflammation level/duration to symptom severity
 *
 * @param bridge Bridge instance
 * @param impairment Output: impairment metrics
 * @return 0 on success
 */
int broca_immune_compute_impairment(
    broca_immune_bridge_t* bridge,
    broca_speech_impairment_t* impairment
);

/* ============================================================================
 * Broca → Immune API (Error Detection)
 * ============================================================================ */

/**
 * @brief Report speech production error
 *
 * WHAT: Log speech error for damage detection
 * WHY:  Errors may indicate neural damage
 * HOW:  Create error record, analyze pattern, potentially trigger immune
 *
 * @param bridge Bridge instance
 * @param error_type Type of error
 * @param intended What was intended
 * @param actual What was produced
 * @param severity Error severity [0-1]
 * @return 0 on success
 */
int broca_immune_report_speech_error(
    broca_immune_bridge_t* bridge,
    broca_speech_error_type_t error_type,
    const char* intended,
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
 * @param syntactic_damage Output: syntactic damage score
 * @param motor_damage Output: motor damage score
 * @return 0 on success
 */
int broca_immune_analyze_error_patterns(
    broca_immune_bridge_t* bridge,
    float* phonological_damage,
    float* syntactic_damage,
    float* motor_damage
);

/**
 * @brief Trigger immune response from speech errors
 *
 * WHAT: Present antigen from error pattern if threshold exceeded
 * WHY:  High error rate indicates potential neural damage
 * HOW:  Create epitope from error signature, present to immune system
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 if no trigger needed
 */
int broca_immune_trigger_from_errors(broca_immune_bridge_t* bridge);

/* ============================================================================
 * Update and State Management
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Process current immune state and update Broca modulation
 * WHY:  Maintain synchronization
 * HOW:  Query immune → Analyze → Apply effects → Detect errors
 *
 * @param bridge Bridge instance
 * @param current_time_ms Current timestamp
 * @return 0 on success
 */
int broca_immune_bridge_update(
    broca_immune_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Get current state
 *
 * @param bridge Bridge instance
 * @return Current state
 */
broca_immune_state_t broca_immune_get_state(
    const broca_immune_bridge_t* bridge
);

/**
 * @brief Get current impairment metrics
 *
 * @param bridge Bridge instance
 * @param impairment Output: impairment metrics
 * @return 0 on success
 */
int broca_immune_get_impairment(
    const broca_immune_bridge_t* bridge,
    broca_speech_impairment_t* impairment
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics
 * @return 0 on success
 */
int broca_immune_get_stats(
    const broca_immune_bridge_t* bridge,
    broca_immune_stats_t* stats
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set aphasia onset callback
 */
int broca_immune_set_aphasia_callback(
    broca_immune_bridge_t* bridge,
    broca_immune_aphasia_onset_cb_t callback,
    void* user_data
);

/**
 * @brief Set speech error callback
 */
int broca_immune_set_error_callback(
    broca_immune_bridge_t* bridge,
    broca_immune_error_cb_t callback,
    void* user_data
);

/**
 * @brief Set impairment change callback
 */
int broca_immune_set_impairment_callback(
    broca_immune_bridge_t* bridge,
    broca_immune_impairment_cb_t callback,
    void* user_data
);

/**
 * @brief Set recovery progress callback
 */
int broca_immune_set_recovery_callback(
    broca_immune_bridge_t* bridge,
    broca_immune_recovery_cb_t callback,
    void* user_data
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* broca_aphasia_type_to_string(broca_aphasia_type_t type);
const char* broca_immune_state_to_string(broca_immune_state_t state);
const char* broca_speech_error_type_to_string(broca_speech_error_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BROCA_IMMUNE_H */
