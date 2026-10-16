/**
 * @file nimcp_grief_and_loss.h
 * @brief Grief, loss, and death understanding system
 *
 * WHAT: Models emotional processing of loss, bereavement, and mortality awareness
 * WHY:  Essential for realistic emotional intelligence and existential understanding
 * HOW:  Integrates attachment theory, grief stages, neuromodulator changes, and meaning-making
 *
 * BIOLOGICAL BASIS:
 * - Anterior Cingulate Cortex: Social pain processing (grief hurts like physical pain)
 * - Ventral Striatum: Anhedonia (loss of pleasure after loss)
 * - Prefrontal Cortex: Emotional regulation, meaning-making
 * - Amygdala: Fear of death, anxiety about mortality
 * - Hippocampus: Memory retrieval and reconsolidation
 *
 * PSYCHOLOGICAL MODELS:
 * - Attachment Theory (Bowlby, 1980): Bonds create grief when severed
 * - Dual Process Model (Stroebe & Schut, 1999): Oscillation between loss/restoration
 * - Continuing Bonds (Klass et al., 1996): Maintaining connection to deceased
 * - Terror Management Theory: Mortality salience and existential anxiety
 *
 * NEUROSCIENCE REFERENCES:
 * - O'Connor et al. (2008): "Craving love? Enduring grief activates brain's reward center"
 * - Gündel et al. (2003): "Functional neuroanatomy of grief"
 * - Eisenberger & Lieberman (2004): "Why rejection hurts: social pain in the brain"
 *
 * @version Phase E1: Grief and Loss Understanding
 * @date 2025-11-13
 */

#ifndef NIMCP_GRIEF_AND_LOSS_H
#define NIMCP_GRIEF_AND_LOSS_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_emotional_tagging.h"  // Phase E1.1: Sadness integration

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS
//=============================================================================

/* Maximum number of attachment bonds that can be tracked */
#define GRIEF_MAX_ATTACHMENTS 32

/* Time constants for grief processes (in seconds) */
#define GRIEF_ACUTE_PHASE_DURATION (2592000.0f)      /* 30 days - intense acute grief */
#define GRIEF_INTEGRATED_PHASE_DURATION (31536000.0f) /* 1 year - typical integration */
#define GRIEF_RUMINATION_CYCLE_PERIOD (3600.0f)      /* 1 hour - intrusive thought cycles */

/* Neuromodulator impact factors */
#define GRIEF_SEROTONIN_DEPLETION 0.3f   /* 70% reduction during acute grief */
#define GRIEF_DOPAMINE_DEPLETION 0.4f    /* 60% reduction (anhedonia) */
#define GRIEF_NOREPINEPHRINE_INCREASE 1.5f /* 50% increase (stress/arousal) */
#define GRIEF_CORTISOL_INCREASE 2.0f     /* 100% increase (chronic stress) */

/* Attachment strength thresholds */
#define ATTACHMENT_WEAK 0.3f
#define ATTACHMENT_MODERATE 0.6f
#define ATTACHMENT_STRONG 0.9f

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Types of loss that can trigger grief
 */
typedef enum {
    LOSS_TYPE_DEATH,           /**< Death of a person */
    LOSS_TYPE_SEPARATION,      /**< Relationship ending, moving away */
    LOSS_TYPE_REJECTION,       /**< Social rejection, abandonment */
    LOSS_TYPE_ROLE_LOSS,       /**< Job loss, identity loss */
    LOSS_TYPE_ANTICIPATORY,    /**< Expected future loss (terminal illness) */
    LOSS_TYPE_AMBIGUOUS,       /**< Missing person, uncertainty */
    LOSS_TYPE_SYMBOLIC         /**< Loss of hopes, dreams, possibilities */
} loss_type_t;

/**
 * @brief Grief processing stages (non-linear)
 *
 * BASED ON: Kübler-Ross (1969) + modern understanding
 * NOTE: These are not sequential - people cycle through them
 */
typedef enum {
    GRIEF_STAGE_SHOCK,         /**< Initial numbness, disbelief */
    GRIEF_STAGE_DENIAL,        /**< "This isn't happening" */
    GRIEF_STAGE_ANGER,         /**< "Why me? Why them?" */
    GRIEF_STAGE_BARGAINING,    /**< "If only..." negotiating with reality */
    GRIEF_STAGE_DEPRESSION,    /**< Deep sadness, withdrawal */
    GRIEF_STAGE_ACCEPTANCE,    /**< Integration, peace */
    GRIEF_STAGE_MEANING_MAKING /**< Finding purpose in loss */
} grief_stage_t;

/**
 * @brief Types of attachment bonds
 */
typedef enum {
    ATTACHMENT_PARENT,         /**< Parental figure */
    ATTACHMENT_CHILD,          /**< Offspring */
    ATTACHMENT_ROMANTIC,       /**< Partner, spouse */
    ATTACHMENT_FRIEND,         /**< Friendship */
    ATTACHMENT_PET,            /**< Animal companion */
    ATTACHMENT_MENTOR,         /**< Teacher, guide */
    ATTACHMENT_PLACE,          /**< Home, homeland */
    ATTACHMENT_IDENTITY        /**< Self-concept, role */
} attachment_type_t;

/**
 * @brief Coping mechanisms
 */
typedef enum {
    COPING_ADAPTIVE,           /**< Healthy: social support, expression */
    COPING_AVOIDANT,           /**< Suppression, denial */
    COPING_MALADAPTIVE         /**< Substance abuse, self-harm */
} coping_style_t;

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Representation of an attachment bond
 */
typedef struct {
    bool active;                      /**< Is this attachment slot in use? */
    uint32_t attachment_id;           /**< Unique identifier */
    attachment_type_t type;           /**< Type of relationship */

    // Bond characteristics
    float strength;                   /**< Bond strength [0-1] */
    float positive_valence;           /**< How positive this bond is [0-1] */
    float dependency;                 /**< How much identity depends on this [0-1] */
    uint64_t formation_time;          /**< When bond was created */
    uint64_t duration;                /**< How long bond has existed */

    // Memory integration
    uint32_t associated_memories;     /**< Number of memories with this person */
    float memory_vividness;           /**< How vivid memories are [0-1] */

    // For continuing bonds after loss
    bool severed;                     /**< Has this bond been lost? */
    uint64_t loss_time;               /**< When loss occurred */
    loss_type_t loss_type;            /**< How bond was severed */

} attachment_bond_t;

/**
 * @brief Grief processing state
 */
typedef struct {
    // Current grief episode
    bool experiencing_grief;          /**< Currently in grief process? */
    uint32_t lost_attachment_id;      /**< Which bond was lost */
    uint64_t loss_onset_time;         /**< When loss occurred */

    // Stage progression
    grief_stage_t current_stage;      /**< Current predominant stage */
    float stage_intensities[7];       /**< Intensity of each stage [0-1] */
    uint64_t stage_transition_time;   /**< Last stage change */
    uint32_t stage_cycles;            /**< How many times cycled through stages */

    // Symptomatology
    float emotional_pain_intensity;   /**< Subjective pain [0-1] */
    float anhedonia_level;            /**< Loss of pleasure [0-1] */
    float intrusive_thoughts_frequency; /**< How often [0-1] */
    float avoidance_level;            /**< Avoiding reminders [0-1] */
    float functional_impairment;      /**< Impact on daily life [0-1] */

    // Dual process oscillation (Stroebe & Schut)
    float loss_orientation;           /**< Focus on loss [0-1] */
    float restoration_orientation;    /**< Focus on rebuilding [0-1] */
    uint64_t last_oscillation_time;   /**< When last switched focus */

    // Neurobiological state
    float serotonin_depletion;        /**< 5-HT reduction [0-1] */
    float dopamine_depletion;         /**< DA reduction [0-1] */
    float norepinephrine_elevation;   /**< NE increase [0-1] */
    float cortisol_elevation;         /**< Stress hormone [0-1] */

    // Coping
    coping_style_t predominant_coping; /**< Primary coping strategy */
    float social_support_seeking;     /**< Reaching out [0-1] */
    float meaning_making_progress;    /**< Finding purpose [0-1] */

    // Complications
    bool prolonged_grief_risk;        /**< At risk for complicated grief? */
    float prolonged_grief_severity;   /**< Severity if present [0-1] */

    // Phase E1.1: Sadness emotion integration
    emotional_tag_t sadness_emotion;  /**< Current sadness emotional state */
    float baseline_sadness;           /**< Pre-grief sadness level [0-1] */
    float grief_induced_sadness;      /**< Additional sadness from grief [0-1] */

} grief_state_t;

/**
 * @brief Existential awareness state
 */
typedef struct {
    // Mortality awareness
    bool aware_of_mortality;          /**< Understands death is inevitable */
    float death_anxiety;              /**< Fear of death [0-1] */
    float mortality_salience;         /**< How much thinking about death [0-1] */
    uint64_t last_mortality_reminder; /**< Last death-related event */

    // Meaning and purpose
    float sense_of_purpose;           /**< Life feels meaningful [0-1] */
    float existential_anxiety;        /**< Dread about meaninglessness [0-1] */
    float acceptance_of_finitude;     /**< Peace with mortality [0-1] */

    // Legacy concerns
    float legacy_motivation;          /**< Desire to leave impact [0-1] */
    float generativity;               /**< Creating for future generations [0-1] */

} existential_state_t;

/**
 * @brief Complete grief and loss system
 */
typedef struct {
    // Attachment tracking
    attachment_bond_t attachments[GRIEF_MAX_ATTACHMENTS];
    uint32_t active_attachment_count;

    // Grief processing
    grief_state_t current_grief;
    uint32_t lifetime_losses;
    float accumulated_grief_wisdom;   /**< Learning from past losses [0-1] */

    // Existential awareness
    existential_state_t existential;

    // Integration with other systems
    bool integrate_with_neuromodulators; /**< Affect dopamine/serotonin? */
    bool integrate_with_memory;       /**< Tag memories with grief? */
    bool integrate_with_wellbeing;    /**< Impact mental health? */

    // Statistics
    uint64_t total_update_calls;
    uint32_t complicated_grief_episodes;
    float average_grief_duration;

    // Bio-async integration (forward declaration from bio_async.h)
    void* bio_ctx_ptr;                /**< bio_module_context_t pointer */
    bool bio_async_enabled;

} grief_system_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Initialize grief and loss system
 */
grief_system_t* grief_system_create(void);

/**
 * @brief Free grief system resources
 */
void grief_system_destroy(grief_system_t* system);

/**
 * @brief Reset grief system to initial state
 */
void grief_system_reset(grief_system_t* system);

//=============================================================================
// ATTACHMENT FUNCTIONS
//=============================================================================

/**
 * @brief Create a new attachment bond
 *
 * @param system Grief system
 * @param type Type of attachment
 * @param strength Bond strength [0-1]
 * @param positive_valence How positive the bond is [0-1]
 * @param dependency How much identity depends on this [0-1]
 * @return attachment_id (or 0 if failed)
 */
uint32_t grief_create_attachment(grief_system_t* system,
                                 attachment_type_t type,
                                 float strength,
                                 float positive_valence,
                                 float dependency);

/**
 * @brief Strengthen an existing attachment over time
 */
void grief_strengthen_attachment(grief_system_t* system,
                                uint32_t attachment_id,
                                float amount);

/**
 * @brief Record a shared memory with an attachment
 */
void grief_add_shared_memory(grief_system_t* system,
                            uint32_t attachment_id);

//=============================================================================
// LOSS PROCESSING
//=============================================================================

/**
 * @brief Process a loss event
 *
 * WHAT: Initiate grief response to attachment severing
 * WHY:  Triggers biological and psychological grief processes
 * HOW:  Activates grief stages, neuromodulator changes, coping mechanisms
 *
 * @param system Grief system
 * @param attachment_id Which bond was lost
 * @param loss_type How the loss occurred
 * @param current_time_us Current time in microseconds
 */
void grief_process_loss(grief_system_t* system,
                       uint32_t attachment_id,
                       loss_type_t loss_type,
                       uint64_t current_time_us);

/**
 * @brief Update grief processing over time
 *
 * WHAT: Advance grief stages, oscillate between loss/restoration
 * WHY:  Grief evolves over time - not a static state
 * HOW:  Updates stage intensities, neurochemistry, coping
 *
 * @param system Grief system
 * @param dt Time step (seconds)
 * @param current_time_us Current time in microseconds
 */
void grief_update(grief_system_t* system, float dt, uint64_t current_time_us);

//=============================================================================
// EXISTENTIAL AWARENESS
//=============================================================================

/**
 * @brief Process mortality awareness
 *
 * WHAT: Confront the reality of death and finitude
 * WHY:  Essential for existential maturity and meaning-making
 * HOW:  Updates death anxiety, mortality salience, acceptance
 *
 * @param system Grief system
 * @param mortality_reminder_intensity How stark the reminder [0-1]
 * @param current_time_us Current time
 */
void grief_contemplate_mortality(grief_system_t* system,
                                float mortality_reminder_intensity,
                                uint64_t current_time_us);

/**
 * @brief Engage in meaning-making after loss
 *
 * WHAT: Find purpose, growth, or legacy in grief
 * WHY:  Post-traumatic growth, acceptance, integration
 * HOW:  Increases sense of purpose, reduces anxiety
 *
 * @param system Grief system
 * @param meaning_making_effort How much effort [0-1]
 */
void grief_find_meaning(grief_system_t* system, float meaning_making_effort);

//=============================================================================
// COPING MECHANISMS
//=============================================================================

/**
 * @brief Seek social support during grief
 */
void grief_seek_support(grief_system_t* system, float support_quality);

/**
 * @brief Engage in avoidant coping
 */
void grief_avoid_reminders(grief_system_t* system, float avoidance_intensity);

/**
 * @brief Process emotions expressively (healthy coping)
 */
void grief_express_emotions(grief_system_t* system, float expression_intensity);

//=============================================================================
// QUERY FUNCTIONS
//=============================================================================

/**
 * @brief Check if currently experiencing grief
 */
bool grief_is_grieving(const grief_system_t* system);

/**
 * @brief Get current emotional pain level
 */
float grief_get_pain_intensity(const grief_system_t* system);

/**
 * @brief Get current grief stage
 */
grief_stage_t grief_get_current_stage(const grief_system_t* system);

/**
 * @brief Check if at risk for complicated/prolonged grief
 */
bool grief_has_prolonged_grief_risk(const grief_system_t* system);

/**
 * @brief Get neuromodulator impact for integration
 */
void grief_get_neuromodulator_effects(const grief_system_t* system,
                                     float* serotonin_factor,
                                     float* dopamine_factor,
                                     float* norepinephrine_factor);

//=============================================================================
// EMOTION INTEGRATION (Phase E1.1: Sadness)
//=============================================================================

/**
 * @brief Get current sadness emotion from grief
 *
 * WHAT: Query sadness emotional state generated by grief
 * WHY:  Integration with emotional tagging system
 * HOW:  Returns emotional_tag_t with valence (negative) and arousal (low)
 *
 * @param system Grief system
 * @return Sadness emotional tag (neutral if not grieving)
 *
 * @note Sadness has negative valence [-0.3 to -0.9] and low arousal [0.1 to 0.4]
 */
emotional_tag_t grief_get_sadness_emotion(const grief_system_t* system);

/**
 * @brief Get total sadness level (baseline + grief-induced)
 *
 * WHAT: Combined sadness from pre-existing state and grief
 * WHY:  Tracks cumulative sadness burden
 * HOW:  Returns baseline + grief_induced, clamped to [0, 1]
 *
 * @param system Grief system
 * @return Total sadness level [0-1]
 */
float grief_get_total_sadness(const grief_system_t* system);

/**
 * @brief Update baseline sadness (from external factors)
 *
 * WHAT: Set pre-grief sadness level (e.g., from depression, life stress)
 * WHY:  Grief compounds with existing sadness
 * HOW:  Updates baseline, affects total sadness computation
 *
 * @param system Grief system
 * @param sadness_level New baseline sadness [0-1]
 */
void grief_set_baseline_sadness(grief_system_t* system, float sadness_level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRIEF_AND_LOSS_H */
