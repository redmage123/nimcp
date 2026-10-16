/**
 * @file nimcp_self_model_immune_bridge.h
 * @brief Self-Model-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and self-model
 * WHY:  Biological evidence shows immune state is part of body representation and
 *       interoception. Essential for realistic self-awareness modeling.
 * HOW:  Immune state modulates self-representation (sickness in self-model),
 *       self-awareness of health triggers appropriate immune responses.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SELF-MODEL PATHWAYS:
 * -----------------------------
 * 1. Interoceptive Immune Signals:
 *    - Anterior insula integrates immune state into self-representation
 *    - Cytokines signal "I am sick" as part of self-awareness
 *    - Inflammation creates body state awareness (fatigue, malaise)
 *    - Reference: Craig (2009) "How do you feel—now? The anterior insula and human awareness"
 *
 * 2. Sickness Identity:
 *    - Self-concept changes when ill: "I am unwell"
 *    - Health status becomes core belief during illness
 *    - Self-efficacy reduced by immune activation
 *    - Reference: Leventhal et al. (2003) "The common-sense model of self-regulation"
 *
 * 3. Body Schema Modulation:
 *    - Inflammation affects perceived body state
 *    - Immune activation changes self-boundaries
 *    - Chronic illness integrates into identity
 *    - Reference: Moseley & Butler (2015) "Body in Mind"
 *
 * 4. Capability Assessment Updates:
 *    - Sickness behavior reduces perceived competence
 *    - Immune activation affects confidence in abilities
 *    - Recovery restores self-efficacy
 *    - Reference: Bandura (1997) "Self-efficacy: The exercise of control"
 *
 * SELF-MODEL → IMMUNE PATHWAYS:
 * -----------------------------
 * 1. Self-Awareness of Illness:
 *    - Conscious recognition of sickness triggers appropriate rest/recovery
 *    - Self-model updating ("I am sick") enables adaptive behavior
 *    - Metacognitive awareness of immune state
 *    - Reference: Kaptchuk et al. (2008) "Components of placebo effect"
 *
 * 2. Health Beliefs and Immunity:
 *    - Self-beliefs about health affect immune function
 *    - Perceived self-efficacy modulates immune response
 *    - Identity-based health behaviors
 *    - Reference: Segerstrom & Miller (2004) "Psychological stress and immune system"
 *
 * 3. Interoceptive Accuracy:
 *    - Accurate self-perception of body state aids diagnosis
 *    - Self-monitoring enables early threat detection
 *    - Body-awareness training improves immune function
 *    - Reference: Garfinkel & Critchley (2013) "Interoception, emotion and brain"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    SELF-MODEL-IMMUNE BRIDGE                                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → SELF-MODEL PATHWAYS                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  IMMUNE STATE    │                                             │  ║
 * ║   │   │ ──────────────── │                                             │  ║
 * ║   │   │ Inflammation     │  ───────┐                                   │  ║
 * ║   │   │ Cytokines        │         │                                   │  ║
 * ║   │   │ Sickness Behavior│         ├──→ Self-Representation            │  ║
 * ║   │   └──────────────────┘         │    ("I am sick/healthy")          │  ║
 * ║   │                                ▼                                   │  ║
 * ║   │   ┌─────────────────────────────────────┐                         │  ║
 * ║   │   │     SELF-MODEL UPDATES              │                         │  ║
 * ║   │   │  - Health status in identity        │                         │  ║
 * ║   │   │  - Body state awareness             │                         │  ║
 * ║   │   │  - Capability assessment            │                         │  ║
 * ║   │   │  - Self-efficacy modulation         │                         │  ║
 * ║   │   └─────────────────────────────────────┘                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  SELF-MODEL → IMMUNE PATHWAYS                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  SELF-AWARENESS  │ ──→ Recognition of illness                  │  ║
 * ║   │   │  OF SICKNESS     │ ──→ Adaptive behavior (rest)                │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  HEALTH BELIEFS  │ ──→ Immune Enhancement                      │  ║
 * ║   │   │  SELF-EFFICACY   │ ──→ Stress Reduction                        │  ║
 * ║   │   └──────────────────┘                                             │  ║
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

#ifndef NIMCP_SELF_MODEL_IMMUNE_BRIDGE_H
#define NIMCP_SELF_MODEL_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_self_model.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Health status thresholds */
#define HEALTH_EXCELLENT_THRESHOLD        0.9f   /**< Excellent health cutoff */
#define HEALTH_GOOD_THRESHOLD            0.7f   /**< Good health cutoff */
#define HEALTH_FAIR_THRESHOLD            0.5f   /**< Fair health cutoff */
#define HEALTH_POOR_THRESHOLD            0.3f   /**< Poor health cutoff */

/* Self-efficacy modulation from immune state */
#define INFLAMMATION_EFFICACY_REDUCTION  0.4f   /**< Max efficacy reduction */
#define SICKNESS_COMPETENCE_REDUCTION    0.5f   /**< Max competence reduction */

/* Interoceptive signal strengths */
#define INTEROCEPTION_WEAK_THRESHOLD     0.3f   /**< Weak body signal */
#define INTEROCEPTION_MODERATE_THRESHOLD 0.6f   /**< Moderate body signal */
#define INTEROCEPTION_STRONG_THRESHOLD   0.8f   /**< Strong body signal */

/* Health belief impact factors */
#define HEALTH_BELIEF_IMMUNE_BOOST       0.3f   /**< Max immune enhancement */
#define SELF_EFFICACY_RECOVERY_BOOST     0.2f   /**< Recovery acceleration */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Health status categories for self-representation
 */
typedef enum {
    HEALTH_STATUS_EXCELLENT,     /**< Peak health, no immune activity */
    HEALTH_STATUS_GOOD,          /**< Minor immune activity, good function */
    HEALTH_STATUS_FAIR,          /**< Moderate immune activity */
    HEALTH_STATUS_POOR,          /**< High inflammation, impaired function */
    HEALTH_STATUS_CRITICAL       /**< Cytokine storm, severe dysfunction */
} self_health_status_t;

/**
 * @brief Interoceptive signal types
 */
typedef enum {
    INTEROCEPTIVE_FATIGUE,       /**< Tiredness signal */
    INTEROCEPTIVE_MALAISE,       /**< General unwellness */
    INTEROCEPTIVE_PAIN,          /**< Discomfort/ache */
    INTEROCEPTIVE_WEAKNESS,      /**< Reduced strength */
    INTEROCEPTIVE_VITALITY       /**< Energy/wellness (positive) */
} interoceptive_signal_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Interoceptive immune signals
 *
 * How immune state creates body awareness signals
 */
typedef struct {
    /* Signal intensities [0-1] */
    float fatigue_signal;            /**< "I feel tired" */
    float malaise_signal;            /**< "I feel unwell" */
    float pain_signal;               /**< "I feel achy" */
    float weakness_signal;           /**< "I feel weak" */
    float vitality_signal;           /**< "I feel energetic" (inverse) */

    /* Aggregate interoception */
    float total_body_awareness;      /**< Overall body state awareness [0-1] */
    float sickness_intensity;        /**< How sick do I feel? [0-1] */
    bool consciously_aware_of_illness; /**< Do I know I'm sick? */
} interoceptive_immune_signals_t;

/**
 * @brief Self-model updates from immune state
 *
 * How immune system modulates self-representation
 */
typedef struct {
    /* Health identity */
    self_health_status_t perceived_health_status;
    char health_belief[256];         /**< "I am healthy/sick/recovering" */
    float health_certainty;          /**< Confidence in health assessment [0-1] */

    /* Capability modulation */
    float immune_competence_reduction; /**< Reduced overall competence [0-1] */
    float immune_efficacy_reduction;   /**< Reduced self-efficacy [0-1] */
    float cognitive_impairment;        /**< Sickness-induced brain fog [0-1] */

    /* Self-boundaries */
    bool illness_integrated_in_identity; /**< Chronic: "I am a sick person" */
    float body_schema_distortion;        /**< Inflammation-induced body perception change [0-1] */

    /* Mental state changes */
    float self_care_motivation;      /**< Drive to rest/recover [0-1] */
    float goal_adjustment;           /**< Lowered expectations [0-1] */
} self_model_immune_modulation_t;

/**
 * @brief Self-awareness effects on immunity
 *
 * How conscious self-representation affects immune function
 */
typedef struct {
    /* Self-awareness */
    bool aware_of_sickness;          /**< "I know I am sick" */
    float health_monitoring_level;   /**< How much I track my health [0-1] */
    float illness_acceptance;        /**< Acceptance of being unwell [0-1] */

    /* Health beliefs */
    float perceived_immune_strength; /**< "I have a strong/weak immune system" [0-1] */
    float recovery_expectation;      /**< "I will get better" [0-1] */
    float health_locus_of_control;   /**< Internal (high) vs external (low) [0-1] */

    /* Behavioral impacts on immunity */
    float rest_compliance;           /**< Following sickness behavior [0-1] */
    float stress_from_illness_belief; /**< Worry about being sick [0-1] */
    float immune_enhancement;        /**< Positive belief benefit [0-1] */
} self_awareness_immune_effects_t;

/**
 * @brief Complete self-model-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    self_model_system_t self_model;

    /* Current state */
    interoceptive_immune_signals_t interoceptive_signals;
    self_model_immune_modulation_t self_model_updates;
    self_awareness_immune_effects_t self_awareness_effects;

    /* Integration flags */
    bool enable_interoceptive_signaling;
    bool enable_self_model_health_update;
    bool enable_capability_modulation;
    bool enable_health_belief_immune_effects;
    bool enable_identity_integration;

    /* Statistics */
    uint64_t total_updates;
    uint32_t health_status_changes;
    uint32_t interoceptive_signals_sent;
    uint32_t capability_modulations;
    uint32_t belief_immune_boosts;
    } self_model_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_interoceptive_signaling;
    bool enable_self_model_health_update;
    bool enable_capability_modulation;
    bool enable_health_belief_immune_effects;
    bool enable_identity_integration;

    /* Sensitivity tuning */
    float interoceptive_sensitivity;     /**< Interoception signal multiplier [0.5-2.0] */
    float health_update_sensitivity;     /**< Self-model update multiplier [0.5-2.0] */
    float belief_immune_sensitivity;     /**< Health belief immune effect [0.5-2.0] */

    /* Thresholds */
    float sickness_awareness_threshold;  /**< Threshold for conscious illness [0.3-0.7] */
    float chronic_identity_threshold_days; /**< Days before illness in identity (default 30) */
} self_model_immune_config_t;

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
int self_model_immune_default_config(self_model_immune_config_t* config);

/**
 * @brief Create self-model-immune bridge
 *
 * WHAT: Initialize bidirectional self-model-immune integration
 * WHY:  Enable realistic self-awareness of immune state
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param self_model Self-model system
 * @return New bridge or NULL on failure
 */
self_model_immune_bridge_t* self_model_immune_bridge_create(
    const self_model_immune_config_t* config,
    brain_immune_system_t* immune_system,
    self_model_system_t self_model
);

/**
 * @brief Destroy self-model-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void self_model_immune_bridge_destroy(self_model_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Self-Model API
 * ============================================================================ */

/**
 * @brief Generate interoceptive signals from immune state
 *
 * WHAT: Create body awareness signals from immune activity
 * WHY:  Immune state is part of interoception
 * HOW:  Map inflammation/cytokines to fatigue/malaise/pain signals
 *
 * @param bridge Self-model-immune bridge
 * @return 0 on success
 */
int self_model_immune_generate_interoceptive_signals(
    self_model_immune_bridge_t* bridge
);

/**
 * @brief Update self-model health status
 *
 * WHAT: Integrate immune state into self-representation
 * WHY:  "I am sick/healthy" is core self-knowledge
 * HOW:  Update health beliefs, identity based on immune activity
 *
 * @param bridge Self-model-immune bridge
 * @return 0 on success
 */
int self_model_immune_update_health_status(
    self_model_immune_bridge_t* bridge
);

/**
 * @brief Modulate self-model capabilities from immune state
 *
 * WHAT: Reduce perceived competence/efficacy during illness
 * WHY:  Sickness affects capability assessment
 * HOW:  Scale down competence, efficacy based on inflammation
 *
 * @param bridge Self-model-immune bridge
 * @return 0 on success
 */
int self_model_immune_modulate_capabilities(
    self_model_immune_bridge_t* bridge
);

/**
 * @brief Integrate chronic illness into identity
 *
 * WHAT: Update self-concept for long-term illness
 * WHY:  Chronic conditions become part of "who I am"
 * HOW:  Add illness to core beliefs after threshold duration
 *
 * @param bridge Self-model-immune bridge
 * @return 0 on success
 */
int self_model_immune_integrate_chronic_illness(
    self_model_immune_bridge_t* bridge
);

/* ============================================================================
 * Self-Model → Immune API
 * ============================================================================ */

/**
 * @brief Trigger adaptive behavior from illness awareness
 *
 * WHAT: Activate rest/recovery behavior when aware of sickness
 * WHY:  Self-awareness enables adaptive responses
 * HOW:  Check sickness awareness, modulate activity level
 *
 * @param bridge Self-model-immune bridge
 * @return 0 on success
 */
int self_model_immune_trigger_adaptive_behavior(
    self_model_immune_bridge_t* bridge
);

/**
 * @brief Boost immunity from positive health beliefs
 *
 * WHAT: Enhance immune function from self-efficacy/optimism
 * WHY:  Health beliefs affect immune response
 * HOW:  Query self-efficacy, perceived immune strength, boost IL-10
 *
 * @param bridge Self-model-immune bridge
 * @return 0 on success
 */
int self_model_immune_boost_from_health_beliefs(
    self_model_immune_bridge_t* bridge
);

/**
 * @brief Suppress immunity from health anxiety
 *
 * WHAT: Reduce immune function from excessive worry about illness
 * WHY:  Health anxiety creates stress-induced immunosuppression
 * HOW:  Check worry/anxiety about health, trigger cortisol
 *
 * @param bridge Self-model-immune bridge
 * @return 0 on success
 */
int self_model_immune_suppress_from_health_anxiety(
    self_model_immune_bridge_t* bridge
);

/**
 * @brief Accelerate recovery from illness acceptance
 *
 * WHAT: Speed recovery when accepting illness without resistance
 * WHY:  Acceptance reduces stress, aids healing
 * HOW:  Check illness acceptance, boost resolution progress
 *
 * @param bridge Self-model-immune bridge
 * @return 0 on success
 */
int self_model_immune_accelerate_from_acceptance(
    self_model_immune_bridge_t* bridge
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update self-model-immune bridge (both directions)
 *
 * WHAT: Process all self-model-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply interoception, update health status, boost from beliefs
 *
 * @param bridge Self-model-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int self_model_immune_bridge_update(
    self_model_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current interoceptive signals
 *
 * @param bridge Self-model-immune bridge
 * @param signals Output signals structure
 * @return 0 on success
 */
int self_model_immune_get_interoceptive_signals(
    const self_model_immune_bridge_t* bridge,
    interoceptive_immune_signals_t* signals
);

/**
 * @brief Get current self-model updates
 *
 * @param bridge Self-model-immune bridge
 * @param updates Output updates structure
 * @return 0 on success
 */
int self_model_immune_get_self_model_updates(
    const self_model_immune_bridge_t* bridge,
    self_model_immune_modulation_t* updates
);

/**
 * @brief Get perceived health status
 *
 * @param bridge Self-model-immune bridge
 * @return Current perceived health status
 */
self_health_status_t self_model_immune_get_health_status(
    const self_model_immune_bridge_t* bridge
);

/**
 * @brief Check if consciously aware of illness
 *
 * WHAT: Determine if sickness has reached conscious awareness
 * WHY:  Conscious illness awareness is distinct from unconscious signals
 * HOW:  Check interoceptive signal strength and sickness threshold
 *
 * @param bridge Self-model-immune bridge
 * @return true if consciously aware of being sick
 */
bool self_model_immune_is_aware_of_sickness(
    const self_model_immune_bridge_t* bridge
);

/**
 * @brief Get interoceptive accuracy score
 *
 * WHAT: Measure how accurately self-model represents immune state
 * WHY:  Interoceptive accuracy predicts health outcomes
 * HOW:  Compare perceived vs actual immune state
 *
 * @param bridge Self-model-immune bridge
 * @return Accuracy score [0-1]
 */
float self_model_immune_get_interoceptive_accuracy(
    const self_model_immune_bridge_t* bridge
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_SELF_MODEL
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int self_model_immune_connect_bio_async(self_model_immune_bridge_t* bridge);

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
int self_model_immune_disconnect_bio_async(self_model_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool self_model_immune_is_bio_async_connected(const self_model_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_MODEL_IMMUNE_BRIDGE_H */
