/**
 * @file nimcp_emotion_immune_bridge.h
 * @brief Emotion-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and emotional processing
 * WHY:  Biological evidence shows strong immune-emotion coupling (cytokines affect mood,
 *       stress affects immunity). Essential for realistic brain modeling.
 * HOW:  Cytokines modulate emotional states, chronic inflammation causes anhedonia,
 *       emotional stress triggers immune responses, IL-10 promotes recovery/positive affect.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → EMOTION PATHWAYS:
 * -------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Cross blood-brain barrier
 *    - Activate hypothalamic-pituitary-adrenal (HPA) axis
 *    - Reduce serotonin/dopamine synthesis → negative affect, anhedonia
 *    - Induce "sickness behavior": fatigue, withdrawal, sadness
 *    - Reference: Dantzer et al. (2008) "From inflammation to sickness and depression"
 *
 * 2. Chronic Inflammation:
 *    - Sustained elevation → depressive symptoms
 *    - Anhedonia (reduced reward sensitivity)
 *    - Fatigue, loss of motivation
 *    - Reduced positive emotion capacity
 *    - Reference: Miller & Raison (2016) "The role of inflammation in depression"
 *
 * 3. Anti-inflammatory Cytokines (IL-10, IL-4):
 *    - Promote tissue repair and homeostasis
 *    - Associated with recovery from negative emotional states
 *    - Enable restoration of positive affect
 *    - Reference: Maes et al. (1999) "Anti-inflammatory cytokines in depression"
 *
 * EMOTION → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Acute Stress/Negative Emotions:
 *    - Cortisol release → immune suppression (initially)
 *    - Followed by inflammatory rebound
 *    - Grief intensifies inflammatory response
 *    - Reference: Segerstrom & Miller (2004) "Psychological stress and the immune system"
 *
 * 2. Chronic Stress:
 *    - Dysregulates immune function
 *    - Increases pro-inflammatory cytokines
 *    - Impairs wound healing
 *    - Reference: Kiecolt-Glaser et al. (2002) "Emotions, morbidity, and mortality"
 *
 * 3. Positive Emotions:
 *    - Enhance immune function
 *    - Reduce inflammatory markers
 *    - Accelerate recovery
 *    - Reference: Pressman & Cohen (2005) "Positive affect and health"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    EMOTION-IMMUNE BRIDGE                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → EMOTION PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.4 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.3 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.5 │         ├──→ Negative Affect                    │  ║
 * ║   │   │              │         │    (Sadness, Fatigue)                 │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     EMOTIONAL SYSTEM            │                             │  ║
 * ║   │   │  - Valence modulation           │                             │  ║
 * ║   │   │  - Arousal changes              │                             │  ║
 * ║   │   │  - Grief intensity              │                             │  ║
 * ║   │   │  - Joy suppression (anhedonia)  │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +0.3       │     Recovery, Positive Affect                   │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  EMOTION → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  GRIEF       │ ──→ Inflammation Trigger                        │  ║
 * ║   │   │  STRESS      │ ──→ Cortisol → Immune Suppression               │  ║
 * ║   │   │  ANGER       │ ──→ TNF-α Release                               │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  JOY         │ ──→ Immune Enhancement                          │  ║
 * ║   │   │  CALM        │ ──→ IL-10 Release                               │  ║
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

#ifndef NIMCP_EMOTION_IMMUNE_BRIDGE_H
#define NIMCP_EMOTION_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_grief_and_loss.h"
#include "cognitive/nimcp_joy_euphoria.h"
#include "cognitive/nimcp_emotion_tensor.h"
#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "cognitive/nimcp_remorse_regret.h"
#include "cognitive/nimcp_shadow_emotions.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for emotion_recognition_system_t (avoids header conflict) */
typedef struct emotion_recognition_system emotion_recognition_system_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine emotion impact factors (valence modulation) */
#define CYTOKINE_IL1_VALENCE_IMPACT      -0.4f   /**< IL-1β → negative affect */
#define CYTOKINE_IL6_VALENCE_IMPACT      -0.3f   /**< IL-6 → negative affect */
#define CYTOKINE_TNF_VALENCE_IMPACT      -0.5f   /**< TNF-α → strong negative */
#define CYTOKINE_IFN_GAMMA_VALENCE_IMPACT -0.2f  /**< IFN-γ → mild negative */
#define CYTOKINE_IL10_VALENCE_IMPACT      0.3f   /**< IL-10 → positive (recovery) */

/* Inflammation-anhedonia mapping */
#define INFLAMMATION_ANHEDONIA_THRESHOLD  0.6f   /**< Inflammation level for anhedonia onset */
#define INFLAMMATION_ANHEDONIA_MAX        0.9f   /**< Maximum anhedonia from inflammation */

/* Emotional stress immune trigger thresholds */
#define STRESS_IMMUNE_TRIGGER_THRESHOLD   0.7f   /**< Stress level to trigger immune response */
#define GRIEF_INFLAMMATION_MULTIPLIER     1.5f   /**< Grief amplifies inflammation */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD    (86400.0f * 7)  /**< 7 days = chronic */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine emotional effects
 *
 * Represents how cytokine levels modulate emotional state
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_negative_affect;        /**< IL-1β induced negative emotion */
    float il6_negative_affect;        /**< IL-6 induced negative emotion */
    float tnf_negative_affect;        /**< TNF-α induced negative emotion */
    float ifn_gamma_negative_affect;  /**< IFN-γ induced negative emotion */

    /* Anti-inflammatory effects */
    float il10_positive_affect;       /**< IL-10 recovery/positive effect */

    /* Aggregate effects */
    float total_valence_shift;        /**< Combined valence modulation */
    float sickness_behavior_level;    /**< Overall sickness behavior [0-1] */
    float anhedonia_level;            /**< Joy suppression [0-1] */
    float fatigue_level;              /**< Energy depletion [0-1] */
} cytokine_emotion_effects_t;

/**
 * @brief Inflammation emotional state
 *
 * How chronic inflammation affects emotional processing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Emotional impacts */
    float depression_risk;             /**< Risk of depressive symptoms [0-1] */
    float anhedonia_severity;          /**< Reduced positive affect [0-1] */
    float fatigue_severity;            /**< Energy depletion [0-1] */
    float motivation_impairment;       /**< Reduced drive [0-1] */

    /* Grief interaction */
    float grief_amplification;         /**< How much inflammation amplifies grief */
    float grief_prolongation;          /**< How much inflammation extends grief duration */
} inflammation_emotion_state_t;

/**
 * @brief Emotional stress immune response
 *
 * How negative emotions trigger immune activity
 */
typedef struct {
    /* Stress indicators */
    float stress_level;                /**< Current stress [0-1] */
    float negative_valence;            /**< Negative emotion intensity [0-1] */
    float arousal_level;               /**< Activation level [0-1] */

    /* Immune triggers */
    bool cortisol_triggered;           /**< HPA axis activated */
    bool inflammatory_rebound;         /**< Post-stress inflammation */
    float immune_suppression;          /**< Stress-induced suppression [0-1] */

    /* Chronic stress effects */
    float chronic_stress_duration_sec; /**< How long stressed */
    float immune_dysregulation;        /**< Dysfunction level [0-1] */
} emotion_immune_trigger_t;

/**
 * @brief Positive emotion immune enhancement
 *
 * How joy/calm/positive states boost immunity
 */
typedef struct {
    /* Positive emotion state */
    float joy_intensity;               /**< Joy level [0-1] */
    float calm_level;                  /**< Calmness [0-1] */
    float positive_valence;            /**< Positive emotion [0-1] */

    /* Immune benefits */
    float immune_enhancement;          /**< Improved function [0-1] */
    float il10_release_boost;          /**< Anti-inflammatory boost */
    float inflammation_reduction;      /**< Reduced inflammation [0-1] */
    float recovery_acceleration;       /**< Faster healing */
} positive_emotion_immune_boost_t;

/**
 * @brief Complete emotion-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    emotional_system_t* emotion_system;
    grief_system_t* grief_system;
    joy_system_t* joy_system;
    emotion_recognition_system_t* emotion_recognition;
    emotion_tensor_system_t* emotion_tensor;
    social_bond_system_t* social_bonds;
    remorse_regret_system_t* remorse_regret;
    shadow_emotion_system_t* shadow_emotions;

    /* Current state */
    cytokine_emotion_effects_t cytokine_effects;
    inflammation_emotion_state_t inflammation_state;
    emotion_immune_trigger_t emotion_trigger;
    positive_emotion_immune_boost_t positive_boost;

    /* Integration flags */
    bool enable_cytokine_emotion_modulation;
    bool enable_inflammation_anhedonia;
    bool enable_emotion_immune_trigger;
    bool enable_positive_immune_boost;
    bool enable_grief_inflammation_coupling;
    bool enable_emotion_recognition_integration;
    bool enable_emotion_tensor_integration;
    bool enable_social_bond_integration;
    bool enable_remorse_integration;
    bool enable_shadow_integration;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t emotion_triggered_responses;
    uint32_t positive_boosts;
    uint32_t recognition_integrations;
    uint32_t tensor_integrations;
    uint32_t social_integrations;
    uint32_t remorse_integrations;
    uint32_t shadow_integrations;

    } emotion_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_emotion_modulation;
    bool enable_inflammation_anhedonia;
    bool enable_emotion_immune_trigger;
    bool enable_positive_immune_boost;
    bool enable_grief_inflammation_coupling;
    bool enable_emotion_recognition_integration;
    bool enable_emotion_tensor_integration;
    bool enable_social_bond_integration;
    bool enable_remorse_integration;
    bool enable_shadow_integration;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float emotion_trigger_sensitivity; /**< Emotion trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float stress_trigger_threshold;    /**< Stress level to trigger immune [0.5-0.9] */
    float inflammation_anhedonia_threshold; /**< Inflammation for anhedonia [0.4-0.8] */
} emotion_immune_config_t;

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
int emotion_immune_default_config(emotion_immune_config_t* config);

/**
 * @brief Create emotion-immune bridge
 *
 * WHAT: Initialize bidirectional emotion-immune integration
 * WHY:  Enable realistic immune-emotion coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param emotion_system Emotional system
 * @param grief_system Grief and loss system (optional, can be NULL)
 * @param joy_system Joy and euphoria system (optional, can be NULL)
 * @return New bridge or NULL on failure
 */
emotion_immune_bridge_t* emotion_immune_bridge_create(
    const emotion_immune_config_t* config,
    brain_immune_system_t* immune_system,
    emotional_system_t* emotion_system,
    grief_system_t* grief_system,
    joy_system_t* joy_system
);

/**
 * @brief Connect emotion recognition system to bridge
 *
 * WHAT: Link emotion recognition for immune-emotion bidirectional coupling
 * WHY:  Negative emotion recognition triggers immune responses
 * HOW:  Store pointer, enable integration flag
 *
 * @param bridge Emotion-immune bridge
 * @param recognition Emotion recognition system
 * @return 0 on success, -1 on error
 */
int emotion_immune_bridge_connect_recognition(
    emotion_immune_bridge_t* bridge,
    emotion_recognition_system_t* recognition
);

/**
 * @brief Connect emotion tensor system to bridge
 *
 * WHAT: Link emotion tensor for immune-emotion bidirectional coupling
 * WHY:  Multi-dimensional emotion state affects immune function
 * HOW:  Store pointer, enable integration flag
 *
 * @param bridge Emotion-immune bridge
 * @param tensor Emotion tensor system
 * @return 0 on success, -1 on error
 */
int emotion_immune_bridge_connect_tensor(
    emotion_immune_bridge_t* bridge,
    emotion_tensor_system_t* tensor
);

/**
 * @brief Connect social bond system to bridge
 *
 * WHAT: Link love/loyalty/friendship for immune-emotion coupling
 * WHY:  Social connection enhances immunity, loneliness suppresses it
 * HOW:  Store pointer, enable integration flag
 *
 * @param bridge Emotion-immune bridge
 * @param social_bonds Social bond system
 * @return 0 on success, -1 on error
 */
int emotion_immune_bridge_connect_social_bonds(
    emotion_immune_bridge_t* bridge,
    social_bond_system_t* social_bonds
);

/**
 * @brief Connect remorse/regret system to bridge
 *
 * WHAT: Link remorse/regret for immune-emotion coupling
 * WHY:  Guilt/shame trigger stress-induced immune responses
 * HOW:  Store pointer, enable integration flag
 *
 * @param bridge Emotion-immune bridge
 * @param remorse_regret Remorse/regret system
 * @return 0 on success, -1 on error
 */
int emotion_immune_bridge_connect_remorse(
    emotion_immune_bridge_t* bridge,
    remorse_regret_system_t* remorse_regret
);

/**
 * @brief Connect shadow emotions system to bridge
 *
 * WHAT: Link shadow emotions for immune-emotion coupling
 * WHY:  Jealousy/envy/narcissism increase inflammation
 * HOW:  Store pointer, enable integration flag
 *
 * @param bridge Emotion-immune bridge
 * @param shadow_emotions Shadow emotion system
 * @return 0 on success, -1 on error
 */
int emotion_immune_bridge_connect_shadow(
    emotion_immune_bridge_t* bridge,
    shadow_emotion_system_t* shadow_emotions
);

/**
 * @brief Destroy emotion-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void emotion_immune_bridge_destroy(emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Emotion API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to emotional state
 *
 * WHAT: Modulate emotion based on cytokine levels
 * WHY:  Pro-inflammatory cytokines induce negative affect
 * HOW:  Query immune system cytokines, adjust emotion valence/arousal
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_apply_cytokine_effects(emotion_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to emotional state
 *
 * WHAT: Induce anhedonia and fatigue from prolonged inflammation
 * WHY:  Chronic inflammation causes depressive symptoms
 * HOW:  Check inflammation duration/level, suppress positive affect
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_apply_inflammation_effects(emotion_immune_bridge_t* bridge);

/**
 * @brief Compute anhedonia level from inflammation
 *
 * WHAT: Calculate joy suppression from immune state
 * WHY:  Inflammation reduces reward sensitivity
 * HOW:  Map inflammation level/duration to anhedonia [0-1]
 *
 * @param bridge Emotion-immune bridge
 * @return Anhedonia level [0-1]
 */
float emotion_immune_compute_anhedonia(const emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Emotion → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from emotional stress
 *
 * WHAT: Activate immune system from high stress/negative emotion
 * WHY:  Stress activates HPA axis and inflammatory response
 * HOW:  Check emotion stress level, trigger cytokine release
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_trigger_from_stress(emotion_immune_bridge_t* bridge);

/**
 * @brief Amplify inflammation from grief
 *
 * WHAT: Increase inflammatory response during grief processing
 * WHY:  Grief intensifies immune activation
 * HOW:  Query grief system, scale immune inflammation level
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_amplify_grief_inflammation(emotion_immune_bridge_t* bridge);

/**
 * @brief Boost immune function from positive emotions
 *
 * WHAT: Enhance immunity from joy/calm/positive affect
 * WHY:  Positive emotions improve immune function
 * HOW:  Query joy system, release IL-10, reduce inflammation
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_boost_from_positive_affect(emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update emotion-immune bridge (both directions)
 *
 * WHAT: Process all immune-emotion interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from stress, boost from joy
 *
 * @param bridge Emotion-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int emotion_immune_bridge_update(
    emotion_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine emotional effects
 *
 * @param bridge Emotion-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int emotion_immune_get_cytokine_effects(
    const emotion_immune_bridge_t* bridge,
    cytokine_emotion_effects_t* effects
);

/**
 * @brief Get current inflammation emotional state
 *
 * @param bridge Emotion-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int emotion_immune_get_inflammation_state(
    const emotion_immune_bridge_t* bridge,
    inflammation_emotion_state_t* state
);

/**
 * @brief Check if experiencing sickness behavior
 *
 * WHAT: Determine if cytokines inducing sickness behavior
 * WHY:  Sickness behavior is distinct behavioral/emotional state
 * HOW:  Check cytokine levels and sickness behavior score
 *
 * @param bridge Emotion-immune bridge
 * @return true if experiencing sickness behavior
 */
bool emotion_immune_is_sick_behavior(const emotion_immune_bridge_t* bridge);

/**
 * @brief Get anhedonia severity
 *
 * @param bridge Emotion-immune bridge
 * @return Anhedonia level [0-1]
 */
float emotion_immune_get_anhedonia_severity(const emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Emotion Recognition Integration API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to emotion recognition
 *
 * WHAT: Modulate recognized emotions based on cytokine levels
 * WHY:  Pro-inflammatory cytokines bias toward negative emotion detection
 * HOW:  Adjust recognition thresholds, increase negative emotion sensitivity
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_modulate_recognition(emotion_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from recognized distress
 *
 * WHAT: Activate immune when recognizing extreme negative emotions
 * WHY:  Emotional distress (rage, panic, despair) triggers inflammation
 * HOW:  Check distress level from recognition, trigger cytokine release
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_trigger_from_recognition(emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Emotion Tensor Integration API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to emotion tensor channels
 *
 * WHAT: Modulate tensor emotion channels based on inflammation
 * WHY:  Cytokines suppress positive emotions, amplify negative ones
 * HOW:  Scale joy/trust channels down, sadness/anger channels up
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_modulate_tensor(emotion_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from emotion tensor state
 *
 * WHAT: Activate immune when tensor shows high stress/negative activation
 * WHY:  Mixed negative emotions (anxiety, despair) trigger inflammation
 * HOW:  Check negative channel activations, trigger immune response
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_trigger_from_tensor(emotion_immune_bridge_t* bridge);

/**
 * @brief Boost immune from positive tensor channels
 *
 * WHAT: Enhance immunity when joy/trust/calm channels are high
 * WHY:  Positive emotions improve immune function
 * HOW:  Check positive channels, release IL-10, reduce inflammation
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_boost_from_tensor(emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Social Bond Integration API
 * ============================================================================ */

/**
 * @brief Apply inflammation effects to social bonding
 *
 * WHAT: Reduce oxytocin and social motivation from inflammation
 * WHY:  Inflammation causes social withdrawal (sickness behavior)
 * HOW:  Suppress oxytocin, increase loneliness, reduce social engagement
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_modulate_social_bonds(emotion_immune_bridge_t* bridge);

/**
 * @brief Boost immune from social connection
 *
 * WHAT: Enhance immunity from love, friendship, and social support
 * WHY:  Social bonds improve immune function and reduce inflammation
 * HOW:  Check oxytocin levels, closeness scores, boost IL-10 release
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_boost_from_social_bonds(emotion_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from loneliness
 *
 * WHAT: Activate inflammatory response from social isolation
 * WHY:  Chronic loneliness increases inflammation and immune dysfunction
 * HOW:  Check loneliness levels, trigger pro-inflammatory cytokines
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_trigger_from_loneliness(emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Remorse/Regret Integration API
 * ============================================================================ */

/**
 * @brief Trigger immune response from guilt/shame
 *
 * WHAT: Activate inflammatory response from moral distress
 * WHY:  Guilt, remorse, and shame create stress-induced inflammation
 * HOW:  Check moral emotion intensity, trigger cortisol and cytokines
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_trigger_from_remorse(emotion_immune_bridge_t* bridge);

/**
 * @brief Reduce immune activation from self-forgiveness
 *
 * WHAT: Decrease inflammation when practicing self-compassion
 * WHY:  Self-forgiveness reduces stress and inflammatory response
 * HOW:  Check self-forgiveness progress, reduce cortisol, release IL-10
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_soothe_from_forgiveness(emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Shadow Emotions Integration API
 * ============================================================================ */

/**
 * @brief Trigger immune response from shadow emotions
 *
 * WHAT: Activate inflammatory response from jealousy, envy, narcissism
 * WHY:  Maladaptive emotions increase cortisol and inflammation
 * HOW:  Check shadow emotion intensity, trigger pro-inflammatory response
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_trigger_from_shadow(emotion_immune_bridge_t* bridge);

/**
 * @brief Modulate shadow emotions from chronic inflammation
 *
 * WHAT: Amplify maladaptive emotional patterns from inflammation
 * WHY:  Inflammation increases irritability, jealousy, and hostility
 * HOW:  Check inflammation levels, increase shadow emotion intensity
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_amplify_shadow_from_inflammation(emotion_immune_bridge_t* bridge);

/**
 * @brief Reduce inflammation from shadow emotion interventions
 *
 * WHAT: Decrease immune activation when shadow emotions are corrected
 * WHY:  CBT-based interventions reduce stress and inflammation
 * HOW:  Check intervention success, reduce cortisol, release IL-10
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_soothe_from_shadow_correction(emotion_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_EMOTION
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success, -1 on error
 */
int emotion_immune_connect_bio_async(emotion_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Emotion-immune bridge
 * @return 0 on success
 */
int emotion_immune_disconnect_bio_async(emotion_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Emotion-immune bridge
 * @return true if connected
 */
bool emotion_immune_is_bio_async_connected(const emotion_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_IMMUNE_BRIDGE_H */
