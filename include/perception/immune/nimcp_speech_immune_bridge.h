/**
 * @file nimcp_speech_immune_bridge.h
 * @brief Speech Cortex-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and speech processing
 * WHY:  Biological evidence shows sickness affects speech fluency, verbal expression
 *       affects immune function. Essential for realistic brain modeling.
 * HOW:  Cytokines impair phoneme discrimination and word retrieval, inflammation
 *       reduces speech rate and increases errors, distress vocalization triggers immune.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SPEECH PATHWAYS:
 * -------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Cross blood-brain barrier
 *    - Affect Broca's area (BA 44/45) → reduced speech fluency
 *    - Impair Wernicke's area (BA 22) → word retrieval deficits
 *    - Slow articulatory planning → speech rate reduction
 *    - Reduce prosody and pitch variability
 *    - Reference: Albuquerque et al. (2012) "Inflammatory cytokines and speech"
 *
 * 2. Sickness Behavior Effects on Speech:
 *    - Reduced verbal fluency (fewer words per minute)
 *    - Increased word-finding latency (tip-of-the-tongue states)
 *    - Phonological errors (substitutions, omissions)
 *    - Monotone prosody (reduced pitch contour)
 *    - Quieter speech (hypophonia)
 *    - Reference: Capuron et al. (2007) "Cytokines and language processing"
 *
 * 3. IL-6 and Cognitive Slowing:
 *    - Impairs phoneme discrimination in STG
 *    - Slows lexical access in Wernicke's area
 *    - Reduces working memory for phonological loop
 *    - Affects speech comprehension speed
 *    - Reference: Marsland et al. (2006) "IL-6 and cognitive function"
 *
 * 4. Chronic Inflammation Effects:
 *    - Sustained reduction in verbal fluency
 *    - Impaired phonological processing
 *    - Reduced phonological working memory capacity
 *    - Reference: Reichenberg et al. (2001) "Cytokine-induced cognitive impairment"
 *
 * SPEECH → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Distress Vocalization:
 *    - Pain/distress vocalizations trigger HPA axis
 *    - Increase cortisol → temporary immune suppression
 *    - Followed by inflammatory rebound
 *    - Reference: Knutson et al. (2002) "Vocal expression and immune function"
 *
 * 2. Verbal Expression of Illness:
 *    - Verbalizing symptoms affects immune response
 *    - Illness-related speech modulates cytokine release
 *    - Social disclosure influences recovery
 *    - Reference: Pennebaker (1997) "Writing about emotional experiences"
 *
 * 3. Speech Effort and Stress:
 *    - Effortful speech (dysarthria compensation) activates stress response
 *    - Chronic speech difficulties → sustained cortisol elevation
 *    - Communication frustration → inflammatory markers
 *    - Reference: Bowers et al. (2010) "Speech effort and stress hormones"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    SPEECH-IMMUNE BRIDGE                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → SPEECH PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.4 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.5 │         ├──→ Speech Impairment                  │  ║
 * ║   │   │              │         │    (Fluency, Word Retrieval)          │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     SPEECH CORTEX               │                             │  ║
 * ║   │   │  - Phoneme discrimination       │                             │  ║
 * ║   │   │  - Word retrieval latency       │                             │  ║
 * ║   │   │  - Speech rate reduction        │                             │  ║
 * ║   │   │  - Prosody flattening           │                             │  ║
 * ║   │   │  - Error rate increase          │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  SPEECH → IMMUNE PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  DISTRESS    │ ──→ Cortisol → Immune Suppression               │  ║
 * ║   │   │  VOCALIZATION│ ──→ Followed by Inflammatory Rebound            │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  SPEECH      │ ──→ Stress → TNF-α Release                      │  ║
 * ║   │   │  EFFORT      │                                                 │  ║
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

#ifndef NIMCP_SPEECH_IMMUNE_BRIDGE_H
#define NIMCP_SPEECH_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "perception/nimcp_speech_cortex.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine speech impact factors */
#define CYTOKINE_IL1_FLUENCY_IMPACT      -0.3f   /**< IL-1β → fluency reduction */
#define CYTOKINE_IL6_FLUENCY_IMPACT      -0.4f   /**< IL-6 → strong fluency reduction */
#define CYTOKINE_TNF_FLUENCY_IMPACT      -0.5f   /**< TNF-α → strongest fluency impairment */
#define CYTOKINE_IFN_GAMMA_FLUENCY_IMPACT -0.2f  /**< IFN-γ → mild fluency reduction */

/* Inflammation speech impairment mapping */
#define INFLAMMATION_WORD_RETRIEVAL_THRESHOLD  0.35f  /**< Inflammation level for word-finding difficulty */
#define INFLAMMATION_PHONEME_ERROR_THRESHOLD   0.35f  /**< Inflammation level for phoneme errors */
#define INFLAMMATION_MAX_SPEECH_IMPAIRMENT     0.8f   /**< Maximum speech impairment from inflammation */

/* Speech effort immune trigger thresholds */
#define SPEECH_EFFORT_IMMUNE_TRIGGER   0.7f   /**< Effort level to trigger immune response */
#define DISTRESS_VOCALIZATION_THRESHOLD 0.8f  /**< Distress intensity for immune activation */

/* Chronic sickness duration (seconds) */
#define CHRONIC_SICKNESS_THRESHOLD    (86400.0f * 3)  /**< 3 days = chronic sickness effects on speech */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine speech effects
 *
 * Represents how cytokine levels modulate speech processing
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_fluency_reduction;        /**< IL-1β induced fluency impairment */
    float il6_word_retrieval_delay;     /**< IL-6 induced retrieval latency */
    float tnf_phoneme_discrimination;   /**< TNF-α induced discrimination impairment */
    float ifn_gamma_prosody_reduction;  /**< IFN-γ induced prosody flattening */

    /* Aggregate effects */
    float total_fluency_impairment;     /**< Combined fluency reduction [0-1] */
    float word_retrieval_latency_ms;    /**< Added latency in word retrieval */
    float phoneme_error_rate;           /**< Increased phoneme error rate [0-1] */
    float speech_rate_reduction;        /**< Speech rate reduction factor [0-1] */
    float prosody_flattening;           /**< Pitch contour reduction [0-1] */
} cytokine_speech_effects_t;

/**
 * @brief Inflammation speech state
 *
 * How chronic inflammation affects speech processing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 3 days for speech effects */

    /* Speech impacts */
    float verbal_fluency_reduction;    /**< Reduced words per minute [0-1] */
    float word_finding_difficulty;     /**< Increased tip-of-tongue states [0-1] */
    float phonological_error_rate;     /**< Substitutions, omissions [0-1] */
    float articulation_slowing;        /**< Articulatory planning delay [0-1] */
    float comprehension_impairment;    /**< Reduced comprehension speed [0-1] */
    float working_memory_capacity;     /**< Phonological loop capacity reduction [0-1] */
} inflammation_speech_state_t;

/**
 * @brief Speech effort immune response
 *
 * How speech difficulties trigger immune activity
 */
typedef struct {
    /* Speech effort indicators */
    float speech_effort_level;         /**< Current effort [0-1] */
    float error_rate;                  /**< Phoneme/word error rate [0-1] */
    float retrieval_latency_ms;        /**< Word-finding latency */
    float frustration_level;           /**< Communication frustration [0-1] */

    /* Immune triggers */
    bool cortisol_triggered;           /**< HPA axis activated */
    bool inflammatory_rebound;         /**< Post-stress inflammation */
    float immune_suppression;          /**< Stress-induced suppression [0-1] */

    /* Distress vocalization */
    bool distress_detected;            /**< Distress vocalization present */
    float distress_intensity;          /**< Distress level [0-1] */
} speech_immune_trigger_t;

/**
 * @brief Complete speech-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    speech_cortex_t* speech_cortex;

    /* Current state */
    cytokine_speech_effects_t cytokine_effects;
    inflammation_speech_state_t inflammation_state;
    speech_immune_trigger_t speech_trigger;

    /* Integration flags */
    bool enable_cytokine_speech_modulation;
    bool enable_inflammation_impairment;
    bool enable_speech_immune_trigger;
    bool enable_distress_vocalization_trigger;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t speech_triggered_responses;
    uint32_t distress_events;
    } speech_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_speech_modulation;
    bool enable_inflammation_impairment;
    bool enable_speech_immune_trigger;
    bool enable_distress_vocalization_trigger;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float speech_trigger_sensitivity;  /**< Speech trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float effort_trigger_threshold;    /**< Effort level to trigger immune [0.5-0.9] */
    float distress_threshold;          /**< Distress level for vocalization trigger [0.6-1.0] */
} speech_immune_config_t;

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
int speech_immune_default_config(speech_immune_config_t* config);

/**
 * @brief Create speech-immune bridge
 *
 * WHAT: Initialize bidirectional speech-immune integration
 * WHY:  Enable realistic immune-speech coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param speech_cortex Speech cortex
 * @return New bridge or NULL on failure
 */
speech_immune_bridge_t* speech_immune_bridge_create(
    const speech_immune_config_t* config,
    brain_immune_system_t* immune_system,
    speech_cortex_t* speech_cortex
);

/**
 * @brief Destroy speech-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void speech_immune_bridge_destroy(speech_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Speech API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to speech processing
 *
 * WHAT: Modulate speech based on cytokine levels
 * WHY:  Pro-inflammatory cytokines impair speech fluency
 * HOW:  Query immune system cytokines, adjust speech parameters
 *
 * @param bridge Speech-immune bridge
 * @return 0 on success
 */
int speech_immune_apply_cytokine_effects(speech_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to speech state
 *
 * WHAT: Induce fluency and retrieval impairments from prolonged inflammation
 * WHY:  Chronic inflammation causes persistent speech deficits
 * HOW:  Check inflammation duration/level, reduce fluency and increase errors
 *
 * @param bridge Speech-immune bridge
 * @return 0 on success
 */
int speech_immune_apply_inflammation_effects(speech_immune_bridge_t* bridge);

/**
 * @brief Compute speech impairment level from inflammation
 *
 * WHAT: Calculate overall speech dysfunction from immune state
 * WHY:  Inflammation reduces multiple speech dimensions
 * HOW:  Map inflammation level/duration to impairment [0-1]
 *
 * @param bridge Speech-immune bridge
 * @return Speech impairment level [0-1]
 */
float speech_immune_compute_impairment(const speech_immune_bridge_t* bridge);

/**
 * @brief Get word retrieval latency increase from cytokines
 *
 * WHAT: Calculate added latency in lexical access
 * WHY:  IL-6 specifically slows word retrieval
 * HOW:  Map cytokine levels to retrieval delay (ms)
 *
 * @param bridge Speech-immune bridge
 * @return Added retrieval latency in milliseconds
 */
float speech_immune_get_retrieval_latency_increase(
    const speech_immune_bridge_t* bridge
);

/**
 * @brief Get phoneme error rate increase from inflammation
 *
 * WHAT: Calculate increased phonological error probability
 * WHY:  Inflammation impairs phoneme discrimination
 * HOW:  Map inflammation to error rate [0-1]
 *
 * @param bridge Speech-immune bridge
 * @return Added error rate [0-1]
 */
float speech_immune_get_phoneme_error_rate(
    const speech_immune_bridge_t* bridge
);

/**
 * @brief Get speech rate reduction from sickness
 *
 * WHAT: Calculate speech rate scaling factor
 * WHY:  Sickness behavior slows articulatory planning
 * HOW:  Return multiplier [0-1] where 1.0 = normal, 0.5 = half speed
 *
 * @param bridge Speech-immune bridge
 * @return Speech rate multiplier [0-1]
 */
float speech_immune_get_speech_rate_factor(
    const speech_immune_bridge_t* bridge
);

/* ============================================================================
 * Speech → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from speech effort
 *
 * WHAT: Activate immune system from high speech effort/frustration
 * WHY:  Chronic speech difficulty activates stress response
 * HOW:  Check speech effort level, trigger cytokine release
 *
 * @param bridge Speech-immune bridge
 * @return 0 on success
 */
int speech_immune_trigger_from_effort(speech_immune_bridge_t* bridge);

/**
 * @brief Detect and process distress vocalization
 *
 * WHAT: Identify distress in speech patterns and trigger immune
 * WHY:  Distress vocalizations trigger HPA axis activation
 * HOW:  Analyze prosody, pitch, intensity for distress markers
 *
 * @param bridge Speech-immune bridge
 * @param prosody_features Current prosody features
 * @return 0 on success
 */
int speech_immune_detect_distress_vocalization(
    speech_immune_bridge_t* bridge,
    const void* prosody_features  /* Can be speech_cortex prosody struct */
);

/**
 * @brief Trigger immune from verbal illness expression
 *
 * WHAT: Activate immune when illness-related words detected
 * WHY:  Verbalizing symptoms modulates immune response
 * HOW:  Word recognition triggers cytokine modulation
 *
 * @param bridge Speech-immune bridge
 * @param word Recognized word
 * @return 0 on success
 */
int speech_immune_trigger_from_illness_expression(
    speech_immune_bridge_t* bridge,
    const char* word
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update speech-immune bridge (both directions)
 *
 * WHAT: Process all immune-speech interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from speech stress
 *
 * @param bridge Speech-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int speech_immune_bridge_update(
    speech_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine speech effects
 *
 * @param bridge Speech-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int speech_immune_get_cytokine_effects(
    const speech_immune_bridge_t* bridge,
    cytokine_speech_effects_t* effects
);

/**
 * @brief Get current inflammation speech state
 *
 * @param bridge Speech-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int speech_immune_get_inflammation_state(
    const speech_immune_bridge_t* bridge,
    inflammation_speech_state_t* state
);

/**
 * @brief Check if experiencing sickness-related speech impairment
 *
 * WHAT: Determine if cytokines inducing speech deficits
 * WHY:  Speech impairment is measurable behavioral marker
 * HOW:  Check cytokine levels and impairment scores
 *
 * @param bridge Speech-immune bridge
 * @return true if experiencing speech impairment
 */
bool speech_immune_is_speech_impaired(const speech_immune_bridge_t* bridge);

/**
 * @brief Get verbal fluency reduction severity
 *
 * @param bridge Speech-immune bridge
 * @return Fluency reduction [0-1]
 */
float speech_immune_get_fluency_reduction(const speech_immune_bridge_t* bridge);

/**
 * @brief Get phonological working memory capacity
 *
 * WHAT: Query current phonological loop capacity
 * WHY:  Inflammation reduces working memory for phonemes
 * HOW:  Return current capacity relative to normal (7±2 items)
 *
 * @param bridge Speech-immune bridge
 * @return Capacity multiplier [0-1], where 1.0 = full capacity
 */
float speech_immune_get_working_memory_capacity(
    const speech_immune_bridge_t* bridge
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_SPEECH
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int speech_immune_connect_bio_async(speech_immune_bridge_t* bridge);

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
int speech_immune_disconnect_bio_async(speech_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool speech_immune_is_bio_async_connected(const speech_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPEECH_IMMUNE_BRIDGE_H */
