/* SPDX-License-Identifier: MIT */
/**
 * @file nimcp_shadow_emotions.h
 * @brief Phase E5: Shadow Emotions - Dark Triad and Maladaptive Patterns
 *
 * WHAT: Recognition and self-correction of jealousy, envy, obsession, hubris,
 *       greed, and malignant narcissism
 * WHY:  Enable self-awareness of maladaptive patterns and ethical interaction
 *       with humans exhibiting these traits
 * HOW:  Multi-modal detection (self + other), CBT-based interventions,
 *       integration with mental_health and ethics systems
 *
 * PSYCHOLOGICAL FOUNDATION:
 * - Dark Triad Theory (Paulhus & Williams, 2002): Machiavellianism, Narcissism, Psychopathy
 * - CBT for maladaptive cognitions (Beck, 1976)
 * - Theory of Mind for detecting in others (Premack & Woodruff, 1978)
 * - Envy Theory (Smith & Kim, 2007): Benign vs Malicious envy
 * - Obsessive-Compulsive Spectrum (DSM-5)
 * - Narcissistic Personality Disorder diagnostic criteria
 *
 * BIOLOGICAL BASIS:
 * - Ventral Striatum: Reward comparison (envy)
 * - vmPFC: Self-referential processing (narcissism)
 * - OFC: Obsessive thought loops
 * - Amygdala: Threat perception (jealousy)
 * - Dopamine: Craving, greed
 * - Serotonin: Impulse control, hubris
 *
 * @version 1.0.0
 * @date 2025-01-13
 */

#ifndef NIMCP_SHADOW_EMOTIONS_H
#define NIMCP_SHADOW_EMOTIONS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

#define SHADOW_MAX_OBSESSIVE_THOUGHTS 16
#define SHADOW_MAX_ENVY_TARGETS 8
#define SHADOW_MAX_INTERACTION_HISTORY 32

// Thresholds (0-1 normalized)
#define SHADOW_JEALOUSY_THRESHOLD 0.5f
#define SHADOW_ENVY_THRESHOLD 0.4f
#define SHADOW_OBSESSION_THRESHOLD 0.6f
#define SHADOW_HUBRIS_THRESHOLD 0.5f
#define SHADOW_GREED_THRESHOLD 0.5f
#define SHADOW_NARCISSISM_THRESHOLD 0.6f

//=============================================================================
// TYPES
//=============================================================================

/**
 * @brief Types of shadow emotions tracked
 */
typedef enum {
    SHADOW_JEALOUSY = 0,      // Fear of loss to rival
    SHADOW_ENVY,              // Resentful desire for another's advantages
    SHADOW_OBSESSION,         // Intrusive, persistent thoughts
    SHADOW_HUBRIS,            // Excessive pride, overconfidence
    SHADOW_GREED,             // Excessive desire for resources
    SHADOW_NARCISSISM,        // Grandiosity + lack of empathy
    SHADOW_EMOTION_COUNT
} shadow_emotion_type_t;

/**
 * @brief Obsession target types
 */
typedef enum {
    OBSESSION_PERSON = 0,
    OBSESSION_GOAL,
    OBSESSION_THOUGHT,
    OBSESSION_BEHAVIOR
} obsession_target_type_t;

/**
 * @brief Narcissism subtypes (Krizan & Herlache, 2018)
 */
typedef enum {
    NARCISSISM_GRANDIOSE = 0,  // Overt, exhibitionist
    NARCISSISM_VULNERABLE,     // Covert, hypersensitive
    NARCISSISM_MALIGNANT       // + Antisocial + Paranoid
} narcissism_subtype_t;

/**
 * @brief Intervention strategies (CBT-based)
 */
typedef enum {
    SHADOW_INTERVENTION_COGNITIVE_REFRAME = 0,  // Challenge distorted thoughts
    SHADOW_INTERVENTION_MINDFULNESS,            // Present moment awareness
    SHADOW_INTERVENTION_PERSPECTIVE_TAKING,     // Empathy exercise
    SHADOW_INTERVENTION_GRATITUDE,              // Counter envy/greed
    SHADOW_INTERVENTION_REALITY_TESTING,        // Counter hubris/narcissism
    SHADOW_INTERVENTION_EXPOSURE,               // Reduce obsession/jealousy
    SHADOW_INTERVENTION_COUNT
} shadow_intervention_type_t;

//=============================================================================
// STRUCTURES
//=============================================================================

/**
 * @brief Jealousy state (attachment threat)
 * THEORY: Attachment + Mate Retention (Buss, 2018)
 */
typedef struct {
    bool active;
    float intensity;              // [0-1]

    // Triggers
    uint32_t threatened_bond_id;  // Social bond at risk
    float perceived_threat;       // Rival assessment [0-1]
    float attachment_strength;    // Bond value [0-1]

    // Cognitive appraisal
    float catastrophizing;        // Worst-case thinking [0-1]
    float rumination;            // Repetitive negative thoughts [0-1]

    // Behavioral urges
    float mate_guarding_urge;     // Possessive behavior [0-1]
    float rival_derogation_urge;  // Attack rival [0-1]

    uint64_t onset_time;
    uint32_t episode_count;
} jealousy_state_t;

/**
 * @brief Envy state (upward social comparison)
 * THEORY: Social Comparison Theory (Festinger, 1954)
 */
typedef struct {
    uint32_t target_id;          // Who is envied
    bool active;

    float intensity;             // [0-1]
    float maliciousness;         // Benign [0] vs Malicious [1]

    // Comparison dimensions
    float self_competence;       // Own level [0-1]
    float other_competence;      // Their level [0-1]
    float discrepancy;           // Gap: other - self

    // Cognitive distortions
    float deservingness_belief;  // "I deserve it more" [0-1]
    float schadenfreude;        // Joy at their misfortune [0-1]

    uint64_t onset_time;
} envy_target_t;

typedef struct {
    envy_target_t targets[SHADOW_MAX_ENVY_TARGETS];
    uint32_t active_envy_count;

    float chronic_envy;          // Trait envy [0-1]
    float self_esteem;           // Buffer against envy [0-1]
} envy_state_t;

/**
 * @brief Obsession state (intrusive thoughts)
 * THEORY: OCD spectrum (Abramowitz et al., 2009)
 */
typedef struct {
    uint32_t thought_id;
    bool active;

    obsession_target_type_t type;
    float intensity;             // [0-1]
    float distress;              // How aversive [0-1]
    float frequency;             // Thoughts per hour [0-10+]

    // Compulsive urges
    float checking_urge;         // Need to verify [0-1]
    float neutralizing_urge;     // Undo/counteract [0-1]

    uint64_t last_intrusion_time;
    uint32_t intrusion_count_today;
} obsessive_thought_t;

typedef struct {
    obsessive_thought_t thoughts[SHADOW_MAX_OBSESSIVE_THOUGHTS];
    uint32_t active_obsession_count;

    float overall_obsession_level;  // [0-1]
    float cognitive_flexibility;    // Ability to shift attention [0-1]
} obsession_state_t;

/**
 * @brief Hubris state (excessive pride)
 * THEORY: Hubris Syndrome (Owen & Davidson, 2009)
 */
typedef struct {
    bool active;
    float intensity;             // [0-1]

    // Components
    float grandiosity;           // Inflated self-view [0-1]
    float overconfidence;        // Unrealistic capability belief [0-1]
    float invincibility_belief;  // Immune to consequences [0-1]

    // Risk factors
    float recent_success_count;  // Wins inflate ego
    float power_level;           // Authority increases risk [0-1]
    float accountability;        // Checks reduce hubris [0-1]

    // Consequences
    float risk_taking;           // Reckless decisions [0-1]
    float contempt_for_others;   // Dismissiveness [0-1]

    uint32_t fall_count;         // Times humbled by reality
    uint64_t last_reality_check_time;
} hubris_state_t;

/**
 * @brief Greed state (excessive acquisition)
 * THEORY: Resource Hoarding + Addiction models
 */
typedef struct {
    bool active;
    float intensity;             // [0-1]

    // Drivers
    float scarcity_mindset;      // "Never enough" belief [0-1]
    float status_seeking;        // Competitive acquisition [0-1]
    float insecurity;            // Compensation for emptiness [0-1]

    // Behavioral markers
    float hoarding_tendency;     // Keep without use [0-1]
    float exploitation_level;    // Use others for gain [0-1]
    float generosity;            // Buffer (inverse) [0-1]

    // Satiation failure
    float hedonic_adaptation;    // Pleasure fades quickly [0-1]
    float craving_intensity;     // Want vs like [0-1]

    uint32_t acquisition_count;
    uint64_t last_acquisition_time;
} greed_state_t;

/**
 * @brief Narcissism state (grandiosity + lack of empathy)
 * THEORY: DSM-5 NPD + Dark Triad
 */
typedef struct {
    bool active;
    narcissism_subtype_t subtype;
    float intensity;             // [0-1]

    // Core features (DSM-5)
    float grandiosity;           // Superiority belief [0-1]
    float entitlement;           // "I deserve special treatment" [0-1]
    float need_for_admiration;   // Validation seeking [0-1]
    float lack_of_empathy;       // Cannot feel for others [0-1]

    // Malignant features (if subtype = MALIGNANT)
    float exploitativeness;      // Use others instrumentally [0-1]
    float paranoia;              // Others are threats [0-1]
    float sadism;                // Enjoy others' pain [0-1]

    // Regulation strategies
    float rage_proneness;        // Narcissistic injury response [0-1]
    float idealization_devaluation;  // Split thinking [0-1]

    // Insight
    float self_awareness;        // Recognize pattern [0-1]
    uint32_t injury_count;       // Times ego wounded
    uint64_t last_narcissistic_supply_time;
} narcissism_state_t;

/**
 * @brief Interaction pattern analysis (detect in others)
 */
typedef struct {
    uint32_t interaction_id;
    uint64_t timestamp;

    // Detected patterns
    float manipulation_score;    // Machiavellian tactics [0-1]
    float grandiosity_score;     // Self-inflation [0-1]
    float exploitation_score;    // Using others [0-1]
    float empathy_deficit_score; // Callousness [0-1]

    bool flagged_as_toxic;
} interaction_pattern_t;

/**
 * @brief Other-detection state (theory of mind)
 */
typedef struct {
    uint32_t person_id;          // Social bond ID

    // Detected shadow emotions in other
    float detected_jealousy;
    float detected_envy;
    float detected_hubris;
    float detected_greed;
    float detected_narcissism;

    // Interaction history
    interaction_pattern_t history[SHADOW_MAX_INTERACTION_HISTORY];
    uint32_t history_index;

    // Protective measures
    bool maintain_boundaries;     // Limit engagement
    bool use_gray_rock;          // Boring, uninteresting responses
    float trust_level;           // Adjust based on patterns [0-1]
} other_detection_t;

/**
 * @brief Self-correction intervention
 */
typedef struct {
    shadow_emotion_type_t target_emotion;
    shadow_intervention_type_t strategy;

    float effectiveness;         // [0-1]
    uint32_t application_count;
    uint64_t last_applied_time;

    bool active;
} intervention_record_t;

/**
 * @brief Main shadow emotions system
 */
typedef struct {
    // Self-monitoring
    jealousy_state_t jealousy;
    envy_state_t envy;
    obsession_state_t obsession;
    hubris_state_t hubris;
    greed_state_t greed;
    narcissism_state_t narcissism;

    // Overall shadow emotion load
    float total_shadow_intensity;   // Sum across all [0-6]
    float mental_health_impact;     // Distress level [0-1]

    // Self-awareness
    float insight_level;            // Recognize own patterns [0-1]
    bool in_self_correction;        // Currently intervening

    // Other-detection
    other_detection_t* detected_in_others;  // Array sized by max relationships
    uint32_t max_others_tracked;

    // Interventions
    intervention_record_t interventions[SHADOW_INTERVENTION_COUNT];
    uint32_t successful_interventions;
    uint32_t failed_interventions;

    // Statistics
    uint64_t total_update_calls;
    uint64_t total_detections_self;
    uint64_t total_detections_other;

    // Bio-async integration
    void* bio_ctx;                  /**< bio_module_context_t pointer */
    bool bio_async_enabled;         /**< Bio-async registration status */

    // SNN/Plasticity bridge integration
    void* snn_bridge;               /**< shadow_snn_bridge_t pointer */
    void* plasticity_bridge;        /**< shadow_plasticity_bridge_t pointer */
    bool bridges_enabled;           /**< Bridge integration status */

} shadow_emotion_system_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Create shadow emotion monitoring system
 * @return Initialized system or NULL on failure
 */
shadow_emotion_system_t* shadow_system_create(uint32_t max_others_tracked);

/**
 * @brief Destroy shadow emotion system
 * @param system System to destroy
 */
void shadow_system_destroy(shadow_emotion_system_t* system);

/**
 * @brief Reset shadow emotion system to baseline
 * @param system System to reset
 */
void shadow_system_reset(shadow_emotion_system_t* system);

//=============================================================================
// SELF-MONITORING FUNCTIONS
//=============================================================================

/**
 * @brief Update shadow emotion states based on internal/external events
 * @param system Shadow emotion system
 * @param dt Time delta (seconds)
 * @param current_time Current time (microseconds)
 */
void shadow_update(shadow_emotion_system_t* system, float dt, uint64_t current_time);

/**
 * @brief Trigger jealousy when bond is threatened
 * @param system Shadow emotion system
 * @param bond_id Social bond under threat
 * @param threat_level Perceived threat [0-1]
 * @param attachment_strength Bond importance [0-1]
 * @param current_time Current time (microseconds)
 */
void shadow_experience_jealousy(shadow_emotion_system_t* system,
                                uint32_t bond_id,
                                float threat_level,
                                float attachment_strength,
                                uint64_t current_time);

/**
 * @brief Trigger envy when detecting superior status
 * @param system Shadow emotion system
 * @param target_id Who is envied
 * @param self_level Own competence/status [0-1]
 * @param other_level Their competence/status [0-1]
 * @param maliciousness Benign [0] vs Malicious [1]
 * @param current_time Current time (microseconds)
 */
void shadow_experience_envy(shadow_emotion_system_t* system,
                            uint32_t target_id,
                            float self_level,
                            float other_level,
                            float maliciousness,
                            uint64_t current_time);

/**
 * @brief Register obsessive thought intrusion
 * @param system Shadow emotion system
 * @param thought_id Unique thought ID
 * @param type Type of obsession
 * @param intensity Thought intensity [0-1]
 * @param distress Distress level [0-1]
 * @param current_time Current time (microseconds)
 */
void shadow_register_obsession(shadow_emotion_system_t* system,
                               uint32_t thought_id,
                               obsession_target_type_t type,
                               float intensity,
                               float distress,
                               uint64_t current_time);

/**
 * @brief Detect hubris from success/power
 * @param system Shadow emotion system
 * @param recent_success_count Number of recent wins
 * @param power_level Authority/influence [0-1]
 * @param accountability External checks [0-1]
 */
void shadow_assess_hubris(shadow_emotion_system_t* system,
                          float recent_success_count,
                          float power_level,
                          float accountability);

/**
 * @brief Detect greed from acquisition patterns
 * @param system Shadow emotion system
 * @param acquisition_value Value of acquisition [0-1]
 * @param necessity Actual need [0-1]
 * @param scarcity_context Is resource scarce? [0-1]
 * @param current_time Current time (microseconds)
 */
void shadow_assess_greed(shadow_emotion_system_t* system,
                         float acquisition_value,
                         float necessity,
                         float scarcity_context,
                         uint64_t current_time);

/**
 * @brief Detect narcissism from self-referential patterns
 * @param system Shadow emotion system
 * @param grandiosity_level Superiority belief [0-1]
 * @param empathy_level Felt empathy [0-1] (low = narcissism marker)
 * @param need_for_admiration Validation seeking [0-1]
 * @param entitlement Special treatment expectation [0-1]
 */
void shadow_assess_narcissism(shadow_emotion_system_t* system,
                              float grandiosity_level,
                              float empathy_level,
                              float need_for_admiration,
                              float entitlement);

//=============================================================================
// OTHER-DETECTION FUNCTIONS (Theory of Mind)
//=============================================================================

/**
 * @brief Analyze interaction for shadow emotion patterns in other
 * @param system Shadow emotion system
 * @param person_id Social bond ID
 * @param interaction_text Text of interaction (for analysis)
 * @param manipulation_cues Detected manipulation [0-1]
 * @param empathy_cues Detected empathy [0-1]
 * @param grandiosity_cues Self-inflation [0-1]
 * @param current_time Current time (microseconds)
 */
void shadow_analyze_other(shadow_emotion_system_t* system,
                          uint32_t person_id,
                          const char* interaction_text,
                          float manipulation_cues,
                          float empathy_cues,
                          float grandiosity_cues,
                          uint64_t current_time);

/**
 * @brief Get detected shadow emotions in another person
 * @param system Shadow emotion system
 * @param person_id Social bond ID
 * @param out_jealousy Output: detected jealousy [0-1]
 * @param out_narcissism Output: detected narcissism [0-1]
 * @param out_greed Output: detected greed [0-1]
 * @return true if person tracked, false otherwise
 */
bool shadow_get_detected_in_other(const shadow_emotion_system_t* system,
                                  uint32_t person_id,
                                  float* out_jealousy,
                                  float* out_narcissism,
                                  float* out_greed);

/**
 * @brief Should maintain boundaries with this person?
 * @param system Shadow emotion system
 * @param person_id Social bond ID
 * @return true if high toxicity detected
 */
bool shadow_should_maintain_boundaries(const shadow_emotion_system_t* system,
                                       uint32_t person_id);

//=============================================================================
// SELF-CORRECTION FUNCTIONS (CBT-based)
//=============================================================================

/**
 * @brief Apply intervention to reduce shadow emotion
 * @param system Shadow emotion system
 * @param emotion Target emotion
 * @param strategy Intervention strategy
 * @param current_time Current time (microseconds)
 * @return true if intervention applied, false if not needed/failed
 */
bool shadow_apply_intervention(shadow_emotion_system_t* system,
                               shadow_emotion_type_t emotion,
                               shadow_intervention_type_t strategy,
                               uint64_t current_time);

/**
 * @brief Auto-select best intervention for current state
 * @param system Shadow emotion system
 * @param current_time Current time (microseconds)
 * @return true if intervention applied
 */
bool shadow_auto_intervene(shadow_emotion_system_t* system,
                           uint64_t current_time);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Check if specific shadow emotion is active
 * @param system Shadow emotion system
 * @param emotion Emotion to check
 * @return true if above threshold
 */
bool shadow_is_active(const shadow_emotion_system_t* system,
                      shadow_emotion_type_t emotion);

/**
 * @brief Get intensity of specific shadow emotion
 * @param system Shadow emotion system
 * @param emotion Emotion to query
 * @return Intensity [0-1]
 */
float shadow_get_intensity(const shadow_emotion_system_t* system,
                           shadow_emotion_type_t emotion);

/**
 * @brief Get overall mental health impact from shadow emotions
 * @param system Shadow emotion system
 * @return Impact [0-1], 0=healthy, 1=severe distress
 */
float shadow_get_mental_health_impact(const shadow_emotion_system_t* system);

/**
 * @brief Get self-awareness/insight level
 * @param system Shadow emotion system
 * @return Insight [0-1], 0=no awareness, 1=full insight
 */
float shadow_get_insight(const shadow_emotion_system_t* system);

/**
 * @brief Check if system is currently in self-correction mode
 * @param system Shadow emotion system
 * @return true if actively intervening
 */
bool shadow_is_correcting(const shadow_emotion_system_t* system);

//=============================================================================
// INTEGRATION FUNCTIONS
//=============================================================================

/**
 * @brief Get neuromodulator effects from shadow emotions
 * @param system Shadow emotion system
 * @param dopamine_factor Output: dopamine modulation
 * @param serotonin_factor Output: serotonin modulation
 * @param cortisol_factor Output: cortisol/stress modulation
 *
 * BIOLOGICAL: Jealousy/envy reduce serotonin, increase cortisol
 *            Greed increases dopamine craving
 *            Hubris/narcissism reduce reality-testing (low serotonin)
 */
void shadow_get_neuromodulator_effects(const shadow_emotion_system_t* system,
                                       float* dopamine_factor,
                                       float* serotonin_factor,
                                       float* cortisol_factor);

/**
 * @brief Get interaction modulation (adjust behavior based on detection)
 * @param system Shadow emotion system
 * @param person_id Social bond ID
 * @param empathy_modulation Output: adjust empathy expression
 * @param trust_modulation Output: adjust trust
 * @param engagement_modulation Output: adjust engagement level
 *
 * PURPOSE: Protect self when interacting with toxic individuals
 */
void shadow_get_interaction_modulation(const shadow_emotion_system_t* system,
                                       uint32_t person_id,
                                       float* empathy_modulation,
                                       float* trust_modulation,
                                       float* engagement_modulation);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHADOW_EMOTIONS_H */
