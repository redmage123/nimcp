//=============================================================================
// nimcp_split_brain_experiments.h - Split-Brain Experimental Framework
//=============================================================================
/**
 * @file nimcp_split_brain_experiments.h
 * @brief Framework for split-brain experiments and callosotomy studies
 *
 * WHAT: Experimental paradigms for studying hemispheric independence
 * WHY:  Enables research into lateralization, consciousness, and integration
 * HOW:  Provides structured experiments with stimulus presentation and response analysis
 *
 * BIOLOGICAL BASIS:
 * - Commissurotomy (callosotomy) severs corpus callosum
 * - Results in two independent conscious hemispheres
 * - Classic experiments by Sperry, Gazzaniga revealed lateralization
 * - Left hemisphere: verbal responses, rational interpretation
 * - Right hemisphere: spatial tasks, emotional processing
 *
 * CLASSIC PARADIGMS:
 * 1. Chimeric Faces: Different faces to each visual field
 * 2. Dichotic Listening: Different audio to each ear
 * 3. Tachistoscopic: Brief visual presentation to one field
 * 4. Cross-Cueing: Detect information leakage between hemispheres
 * 5. Alien Hand: Conflicting motor intentions
 * 6. Confabulation: Left hemisphere explains right's actions
 *
 * EXPERIMENTAL FLOW:
 * ```
 *   ┌─────────────────┐
 *   │  Create Session │
 *   └────────┬────────┘
 *            │
 *   ┌────────▼────────┐     ┌─────────────────┐
 *   │ Configure Trial │────▶│ Set Paradigm    │
 *   └────────┬────────┘     │ Set Stimuli     │
 *            │              │ Set Conditions  │
 *   ┌────────▼────────┐     └─────────────────┘
 *   │  Present Trial  │
 *   └────────┬────────┘
 *            │
 *   ┌────────▼────────┐
 *   │ Collect Response│
 *   └────────┬────────┘
 *            │
 *   ┌────────▼────────┐     ┌─────────────────┐
 *   │ Analyze Results │────▶│ Agreement rate  │
 *   └────────┬────────┘     │ Cross-cueing    │
 *            │              │ Lateralization  │
 *   ┌────────▼────────┐     └─────────────────┘
 *   │ Generate Report │
 *   └─────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_SPLIT_BRAIN_EXPERIMENTS_H
#define NIMCP_SPLIT_BRAIN_EXPERIMENTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct split_brain_session_struct split_brain_session_t;
typedef struct split_brain_trial_struct split_brain_trial_t;

//=============================================================================
// Constants
//=============================================================================

#define SPLIT_BRAIN_MAX_STIMULUS_SIZE 1024    /**< Maximum stimulus data size */
#define SPLIT_BRAIN_MAX_RESPONSE_SIZE 256     /**< Maximum response data size */
#define SPLIT_BRAIN_MAX_TRIALS 1000           /**< Maximum trials per session */
#define SPLIT_BRAIN_MAX_CONDITIONS 16         /**< Maximum experimental conditions */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Experimental paradigm types
 *
 * BIOLOGICAL BASIS:
 * - Each paradigm tests different aspects of hemispheric independence
 * - Designed to reveal lateralized processing
 */
typedef enum {
    PARADIGM_CHIMERIC_FACES,         /**< Different faces to each visual field */
    PARADIGM_DICHOTIC_LISTENING,     /**< Different audio to each ear */
    PARADIGM_TACHISTOSCOPIC,         /**< Brief visual presentation to one field */
    PARADIGM_CROSS_CUEING,           /**< Detect information leakage */
    PARADIGM_ALIEN_HAND,             /**< Conflicting motor intentions */
    PARADIGM_CONFABULATION,          /**< Left explains right's actions */
    PARADIGM_SPATIAL_VERBAL,         /**< Spatial vs verbal task comparison */
    PARADIGM_EMOTION_RECOGNITION,    /**< Emotional face processing */
    PARADIGM_WORD_COMPLETION,        /**< Semantic vs phonological */
    PARADIGM_OBJECT_MATCHING,        /**< Visual-spatial matching */
    PARADIGM_MUSIC_LANGUAGE,         /**< Melody vs lyrics processing */
    PARADIGM_CUSTOM,                 /**< User-defined paradigm */
    PARADIGM_COUNT
} experiment_paradigm_t;

/**
 * @brief Visual field for stimulus presentation
 */
typedef enum {
    VISUAL_FIELD_LEFT,               /**< Left visual field (right hemisphere) */
    VISUAL_FIELD_RIGHT,              /**< Right visual field (left hemisphere) */
    VISUAL_FIELD_CENTER,             /**< Central fixation */
    VISUAL_FIELD_BILATERAL           /**< Both fields simultaneously */
} visual_field_t;

/**
 * @brief Auditory channel for stimulus
 */
typedef enum {
    AUDIO_CHANNEL_LEFT_EAR,          /**< Left ear (right hemisphere primary) */
    AUDIO_CHANNEL_RIGHT_EAR,         /**< Right ear (left hemisphere primary) */
    AUDIO_CHANNEL_BINAURAL,          /**< Both ears */
    AUDIO_CHANNEL_DICHOTIC           /**< Different to each ear */
} audio_channel_t;

/**
 * @brief Motor response hand
 */
typedef enum {
    RESPONSE_HAND_LEFT,              /**< Left hand (right hemisphere) */
    RESPONSE_HAND_RIGHT,             /**< Right hand (left hemisphere) */
    RESPONSE_VERBAL,                 /**< Verbal response (left hemisphere) */
    RESPONSE_POINTING,               /**< Pointing response (either) */
    RESPONSE_BOTH_HANDS              /**< Bimanual response */
} response_modality_t;

/**
 * @brief Stimulus type
 */
typedef enum {
    STIMULUS_VISUAL,                 /**< Visual image/pattern */
    STIMULUS_AUDITORY,               /**< Audio/sound */
    STIMULUS_TACTILE,                /**< Touch/haptic */
    STIMULUS_MOTOR_CUE,              /**< Motor instruction */
    STIMULUS_VERBAL,                 /**< Word/text */
    STIMULUS_EMOTIONAL               /**< Emotional content */
} stimulus_type_t;

/**
 * @brief Callosal state for experiment
 */
typedef enum {
    CALLOSAL_STATE_INTACT,           /**< Normal full connection */
    CALLOSAL_STATE_SEVERED,          /**< Complete disconnection */
    CALLOSAL_STATE_PARTIAL,          /**< Some channels blocked */
    CALLOSAL_STATE_DEGRADED,         /**< Reduced bandwidth/increased latency */
    CALLOSAL_STATE_DEVELOPING        /**< Simulated child development */
} callosal_condition_t;

/**
 * @brief Trial outcome
 */
typedef enum {
    OUTCOME_CORRECT,                 /**< Response matched expected */
    OUTCOME_INCORRECT,               /**< Response differed from expected */
    OUTCOME_CONFLICT,                /**< Hemispheres disagreed */
    OUTCOME_NO_RESPONSE,             /**< No response within timeout */
    OUTCOME_CROSS_CUE_DETECTED,      /**< Information leaked across hemispheres */
    OUTCOME_CONFABULATION            /**< Left hemisphere confabulated */
} trial_outcome_t;

//=============================================================================
// Stimulus Structure
//=============================================================================

/**
 * @brief Stimulus for a trial
 */
typedef struct {
    stimulus_type_t type;            /**< Type of stimulus */

    // Visual parameters
    visual_field_t visual_field;     /**< Target visual field */
    float presentation_time_ms;      /**< How long to show (0 = until response) */

    // Auditory parameters
    audio_channel_t audio_channel;   /**< Target audio channel */

    // Data
    float* data;                     /**< Stimulus data (e.g., image pixels, audio samples) */
    size_t data_size;                /**< Size of data */

    // Metadata
    uint32_t stimulus_id;            /**< Unique identifier */
    const char* label;               /**< Human-readable label */
    cognitive_domain_t target_domain;/**< Domain this stimulus targets */
} split_brain_stimulus_t;

/**
 * @brief Response from a hemisphere
 */
typedef struct {
    hemisphere_id_t hemisphere;      /**< Which hemisphere responded */
    response_modality_t modality;    /**< Response modality used */

    // Response data
    float* data;                     /**< Response data */
    size_t data_size;                /**< Size of response data */

    // Timing
    float reaction_time_ms;          /**< Time from stimulus to response */
    float confidence;                /**< Response confidence (0-1) */

    // Processing info
    float activity_level;            /**< Hemisphere activity during response */
    float energy_consumed;           /**< Energy used for processing */

    // Verbal response (left hemisphere)
    char* verbal_response;           /**< If verbal modality */
} hemisphere_response_t;

//=============================================================================
// Trial Structure
//=============================================================================

/**
 * @brief Single experimental trial
 */
struct split_brain_trial_struct {
    // Trial identity
    uint32_t trial_number;           /**< Sequential trial number */
    experiment_paradigm_t paradigm;  /**< Experimental paradigm */

    // Stimuli (can have different stimulus per hemisphere for chimeric)
    split_brain_stimulus_t left_stimulus;    /**< Stimulus to left VF/right hemisphere */
    split_brain_stimulus_t right_stimulus;   /**< Stimulus to right VF/left hemisphere */
    bool use_chimeric;               /**< Different stimuli per hemisphere */

    // Expected responses
    float* expected_left_response;   /**< Expected right hemisphere response */
    float* expected_right_response;  /**< Expected left hemisphere response */
    size_t expected_size;            /**< Size of expected responses */

    // Actual responses
    hemisphere_response_t left_response;     /**< Right hemisphere actual response */
    hemisphere_response_t right_response;    /**< Left hemisphere actual response */

    // Callosal condition
    callosal_condition_t callosal_state;     /**< Callosal condition for this trial */
    float callosal_strength;         /**< Connection strength (0-1) */

    // Outcome
    trial_outcome_t outcome;         /**< Trial outcome */
    bool hemispheres_agreed;         /**< Did hemispheres give same response? */
    float agreement_score;           /**< Degree of agreement (0-1) */

    // Cross-cueing detection
    bool cross_cueing_detected;      /**< Did information leak? */
    float cross_cueing_evidence;     /**< Strength of cross-cue evidence */

    // Timing
    uint64_t start_time;             /**< Trial start timestamp */
    uint64_t end_time;               /**< Trial end timestamp */
    float total_duration_ms;         /**< Total trial duration */

    // Notes
    char notes[256];                 /**< Experimenter notes */
};

//=============================================================================
// Session Configuration
//=============================================================================

/**
 * @brief Session configuration
 */
typedef struct {
    // Paradigm
    experiment_paradigm_t paradigm;
    const char* experiment_name;
    const char* experimenter;

    // Callosal condition
    callosal_condition_t callosal_condition;
    float callosal_strength;         /**< For DEGRADED state */
    bool blocked_channels[CALLOSUM_CHANNEL_COUNT];  /**< For PARTIAL state */

    // Trial parameters
    uint32_t num_trials;             /**< Planned number of trials */
    float inter_trial_interval_ms;   /**< Time between trials */
    float stimulus_duration_ms;      /**< Default stimulus duration */
    float response_timeout_ms;       /**< Max time to wait for response */

    // Response collection
    response_modality_t allowed_modalities[5];  /**< Allowed response types */
    uint32_t num_allowed_modalities;

    // Cross-cueing detection
    bool detect_cross_cueing;        /**< Monitor for cross-cueing */
    float cross_cue_threshold;       /**< Threshold for detection */

    // Data collection
    bool record_raw_activity;        /**< Store hemisphere activity traces */
    bool record_callosum_traffic;    /**< Store callosum messages */

    // Randomization
    bool randomize_trials;           /**< Randomize trial order */
    uint32_t random_seed;            /**< Seed for randomization */
} split_brain_session_config_t;

/**
 * @brief Session statistics
 */
typedef struct {
    // Trial counts
    uint32_t total_trials;
    uint32_t completed_trials;
    uint32_t correct_trials;
    uint32_t incorrect_trials;
    uint32_t conflict_trials;
    uint32_t no_response_trials;

    // Agreement analysis
    float overall_agreement_rate;    /**< Proportion of agreeing responses */
    float left_accuracy;             /**< Left hemisphere accuracy */
    float right_accuracy;            /**< Right hemisphere accuracy */

    // Lateralization metrics
    float left_dominance[COGNITIVE_DOMAIN_COUNT];  /**< Domain dominance scores */
    float avg_left_reaction_time;
    float avg_right_reaction_time;

    // Cross-cueing
    uint32_t cross_cueing_events;
    float cross_cueing_rate;

    // Confabulation (left explaining right's actions)
    uint32_t confabulation_events;

    // Timing
    float avg_trial_duration_ms;
    float total_session_duration_ms;

    // Energy
    float total_left_energy;
    float total_right_energy;
} split_brain_session_stats_t;

//=============================================================================
// Main Session Structure
//=============================================================================

/**
 * @brief Split-brain experiment session
 *
 * WHAT: Container for running split-brain experiments
 * WHY:  Organized approach to hemispheric independence studies
 * HOW:  Manages trials, conditions, and result analysis
 */
struct split_brain_session_struct {
    // Configuration
    split_brain_session_config_t config;

    // Brain
    hemispheric_brain_t* brain;

    // Trials
    split_brain_trial_t* trials;
    uint32_t trial_capacity;
    uint32_t current_trial;

    // Session state
    bool is_running;
    bool is_paused;
    uint64_t start_time;
    uint64_t pause_time;

    // Pre-experiment callosal state (to restore)
    bool original_callosum_connected;
    float original_callosum_strength;

    // Statistics
    split_brain_session_stats_t stats;

    // Callbacks
    void (*on_trial_complete)(split_brain_trial_t* trial, void* user_data);
    void (*on_cross_cue_detected)(split_brain_trial_t* trial, void* user_data);
    void (*on_conflict)(split_brain_trial_t* trial, void* user_data);
    void* callback_user_data;

    // Thread safety
    void* mutex;                     /**< nimcp_mutex_t* */
};

//=============================================================================
// Session Lifecycle
//=============================================================================

/**
 * @brief Get default session configuration
 *
 * @return Default configuration for split-brain experiments
 */
split_brain_session_config_t split_brain_session_default_config(void);

/**
 * @brief Create split-brain experiment session
 *
 * WHAT: Initialize experiment session
 * WHY:  Required for running experiments
 * HOW:  Allocate resources, configure callosal state
 *
 * @param config Session configuration (NULL for defaults)
 * @param brain Hemispheric brain to experiment on
 * @return Session instance or NULL on error
 */
split_brain_session_t* split_brain_session_create(
    const split_brain_session_config_t* config,
    hemispheric_brain_t* brain
);

/**
 * @brief Destroy split-brain session
 *
 * @param session Session to destroy
 */
void split_brain_session_destroy(split_brain_session_t* session);

/**
 * @brief Start experiment session
 *
 * WHAT: Begin the experimental session
 * WHY:  Apply callosal conditions and start timing
 * HOW:  Configure callosum, record baseline
 *
 * @param session Session to start
 * @return 0 on success
 */
int split_brain_session_start(split_brain_session_t* session);

/**
 * @brief Pause experiment session
 *
 * @param session Session to pause
 * @return 0 on success
 */
int split_brain_session_pause(split_brain_session_t* session);

/**
 * @brief Resume experiment session
 *
 * @param session Session to resume
 * @return 0 on success
 */
int split_brain_session_resume(split_brain_session_t* session);

/**
 * @brief End experiment session
 *
 * WHAT: Finish the experiment
 * WHY:  Restore callosal state, finalize statistics
 * HOW:  Reconnect callosum, compute final metrics
 *
 * @param session Session to end
 * @return 0 on success
 */
int split_brain_session_end(split_brain_session_t* session);

//=============================================================================
// Trial Management
//=============================================================================

/**
 * @brief Create a new trial
 *
 * @param session Session to add trial to
 * @param paradigm Experimental paradigm
 * @return Trial instance or NULL
 */
split_brain_trial_t* split_brain_trial_create(
    split_brain_session_t* session,
    experiment_paradigm_t paradigm
);

/**
 * @brief Set trial stimulus
 *
 * @param trial Trial to configure
 * @param stimulus Stimulus to set
 * @param hemisphere Which hemisphere receives (LEFT = right hemisphere)
 * @return 0 on success
 */
int split_brain_trial_set_stimulus(
    split_brain_trial_t* trial,
    const split_brain_stimulus_t* stimulus,
    hemisphere_id_t hemisphere
);

/**
 * @brief Set chimeric stimuli (different for each hemisphere)
 *
 * @param trial Trial to configure
 * @param left_vf_stimulus Stimulus for left visual field (right hemisphere)
 * @param right_vf_stimulus Stimulus for right visual field (left hemisphere)
 * @return 0 on success
 */
int split_brain_trial_set_chimeric(
    split_brain_trial_t* trial,
    const split_brain_stimulus_t* left_vf_stimulus,
    const split_brain_stimulus_t* right_vf_stimulus
);

/**
 * @brief Set expected response
 *
 * @param trial Trial to configure
 * @param expected Expected response data
 * @param size Size of expected data
 * @param hemisphere Which hemisphere's response
 * @return 0 on success
 */
int split_brain_trial_set_expected(
    split_brain_trial_t* trial,
    const float* expected,
    size_t size,
    hemisphere_id_t hemisphere
);

/**
 * @brief Run a single trial
 *
 * WHAT: Execute one experimental trial
 * WHY:  Present stimulus, collect response, analyze
 * HOW:  Route stimulus to hemisphere, wait for response, compare
 *
 * @param session Experiment session
 * @param trial Trial to run
 * @return 0 on success
 */
int split_brain_trial_run(
    split_brain_session_t* session,
    split_brain_trial_t* trial
);

/**
 * @brief Run all trials in session
 *
 * @param session Experiment session
 * @return Number of trials completed
 */
int split_brain_session_run_all_trials(split_brain_session_t* session);

/**
 * @brief Get trial by number
 *
 * @param session Experiment session
 * @param trial_number Trial number
 * @return Trial or NULL
 */
const split_brain_trial_t* split_brain_session_get_trial(
    const split_brain_session_t* session,
    uint32_t trial_number
);

//=============================================================================
// Cross-Cueing Detection
//=============================================================================

/**
 * @brief Check for cross-cueing in a trial
 *
 * WHAT: Detect information leakage between hemispheres
 * WHY:  Validates split-brain condition, reveals compensation strategies
 * HOW:  Analyze response patterns for impossible knowledge
 *
 * Cross-cueing examples:
 * - Eye movement gives away visual field information
 * - Body posture signals communicate across
 * - Subvocal speech detected by right hemisphere
 *
 * @param session Experiment session
 * @param trial Trial to check
 * @return true if cross-cueing detected
 */
bool split_brain_detect_cross_cueing(
    split_brain_session_t* session,
    split_brain_trial_t* trial
);

/**
 * @brief Get cross-cueing evidence strength
 *
 * @param trial Trial to analyze
 * @return Evidence strength (0-1)
 */
float split_brain_get_cross_cue_evidence(const split_brain_trial_t* trial);

//=============================================================================
// Confabulation Analysis
//=============================================================================

/**
 * @brief Analyze confabulation in trial
 *
 * WHAT: Detect left hemisphere confabulating about right hemisphere actions
 * WHY:  Classic split-brain phenomenon, reveals interpreter function
 * HOW:  Compare verbal explanation with actual right hemisphere state
 *
 * @param session Experiment session
 * @param trial Trial to analyze
 * @param confabulation_score Output confidence of confabulation
 * @return true if confabulation detected
 */
bool split_brain_analyze_confabulation(
    split_brain_session_t* session,
    split_brain_trial_t* trial,
    float* confabulation_score
);

//=============================================================================
// Agreement Analysis
//=============================================================================

/**
 * @brief Compute agreement between hemispheres
 *
 * @param trial Trial to analyze
 * @return Agreement score (0-1)
 */
float split_brain_compute_agreement(const split_brain_trial_t* trial);

/**
 * @brief Check if hemispheres conflict
 *
 * WHAT: Determine if hemispheres gave conflicting responses
 * WHY:  Reveals hemispheric independence
 * HOW:  Compare response vectors, threshold disagreement
 *
 * @param trial Trial to check
 * @param threshold Minimum difference for conflict
 * @return true if conflicting
 */
bool split_brain_detect_conflict(
    const split_brain_trial_t* trial,
    float threshold
);

//=============================================================================
// Callosal Manipulation
//=============================================================================

/**
 * @brief Apply callosal condition for experiment
 *
 * WHAT: Set callosal state for experimental condition
 * WHY:  Different conditions test different hypotheses
 * HOW:  Disconnect, degrade, or partially block callosum
 *
 * @param session Experiment session
 * @param condition Callosal condition to apply
 * @return 0 on success
 */
int split_brain_apply_callosal_condition(
    split_brain_session_t* session,
    callosal_condition_t condition
);

/**
 * @brief Set callosal degradation level
 *
 * @param session Experiment session
 * @param strength Connection strength (0-1)
 * @return 0 on success
 */
int split_brain_set_callosal_strength(
    split_brain_session_t* session,
    float strength
);

/**
 * @brief Block specific callosal channel
 *
 * @param session Experiment session
 * @param channel Channel to block
 * @param block true to block, false to unblock
 * @return 0 on success
 */
int split_brain_block_channel(
    split_brain_session_t* session,
    callosum_channel_type_t channel,
    bool block
);

/**
 * @brief Restore original callosal state
 *
 * @param session Experiment session
 * @return 0 on success
 */
int split_brain_restore_callosum(split_brain_session_t* session);

//=============================================================================
// Statistics and Analysis
//=============================================================================

/**
 * @brief Get session statistics
 *
 * @param session Experiment session
 * @param stats Output statistics
 * @return 0 on success
 */
int split_brain_session_get_stats(
    const split_brain_session_t* session,
    split_brain_session_stats_t* stats
);

/**
 * @brief Compute lateralization index for domain
 *
 * WHAT: Calculate which hemisphere is dominant for domain
 * WHY:  Quantify lateralization strength
 * HOW:  Compare accuracy and reaction times
 *
 * Lateralization Index (LI) = (L - R) / (L + R)
 * LI > 0 = left dominant
 * LI < 0 = right dominant
 *
 * @param session Experiment session
 * @param domain Cognitive domain
 * @return Lateralization index (-1 to +1)
 */
float split_brain_compute_lateralization_index(
    const split_brain_session_t* session,
    cognitive_domain_t domain
);

/**
 * @brief Analyze reaction time differences
 *
 * @param session Experiment session
 * @param left_rt Output left hemisphere mean RT
 * @param right_rt Output right hemisphere mean RT
 * @param rt_difference Output RT difference (left - right)
 * @return 0 on success
 */
int split_brain_analyze_reaction_times(
    const split_brain_session_t* session,
    float* left_rt,
    float* right_rt,
    float* rt_difference
);

/**
 * @brief Generate experiment report
 *
 * @param session Experiment session
 * @param buffer Output buffer for report
 * @param buffer_size Size of buffer
 * @return Length of report (or required size if too small)
 */
int split_brain_generate_report(
    const split_brain_session_t* session,
    char* buffer,
    size_t buffer_size
);

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Set trial completion callback
 *
 * @param session Experiment session
 * @param callback Callback function
 * @param user_data User data for callback
 */
void split_brain_set_trial_callback(
    split_brain_session_t* session,
    void (*callback)(split_brain_trial_t* trial, void* user_data),
    void* user_data
);

/**
 * @brief Set cross-cueing detection callback
 *
 * @param session Experiment session
 * @param callback Callback function
 * @param user_data User data for callback
 */
void split_brain_set_cross_cue_callback(
    split_brain_session_t* session,
    void (*callback)(split_brain_trial_t* trial, void* user_data),
    void* user_data
);

/**
 * @brief Set conflict detection callback
 *
 * @param session Experiment session
 * @param callback Callback function
 * @param user_data User data for callback
 */
void split_brain_set_conflict_callback(
    split_brain_session_t* session,
    void (*callback)(split_brain_trial_t* trial, void* user_data),
    void* user_data
);

//=============================================================================
// Predefined Experiments
//=============================================================================

/**
 * @brief Create chimeric faces experiment
 *
 * WHAT: Classic split-brain experiment with composite faces
 * WHY:  Reveals hemisphere-specific face processing
 * HOW:  Different face halves to each visual field
 *
 * @param brain Hemispheric brain
 * @param num_trials Number of trials
 * @return Configured session or NULL
 */
split_brain_session_t* split_brain_create_chimeric_faces_experiment(
    hemispheric_brain_t* brain,
    uint32_t num_trials
);

/**
 * @brief Create dichotic listening experiment
 *
 * WHAT: Different audio to each ear
 * WHY:  Tests auditory lateralization
 * HOW:  Present competing stimuli, measure ear advantage
 *
 * @param brain Hemispheric brain
 * @param num_trials Number of trials
 * @return Configured session or NULL
 */
split_brain_session_t* split_brain_create_dichotic_experiment(
    hemispheric_brain_t* brain,
    uint32_t num_trials
);

/**
 * @brief Create tachistoscopic experiment
 *
 * WHAT: Brief visual presentation to one field
 * WHY:  Prevents eye movement, ensures hemispheric isolation
 * HOW:  Flash stimulus <200ms, too fast to saccade
 *
 * @param brain Hemispheric brain
 * @param num_trials Number of trials
 * @param presentation_ms Presentation duration
 * @return Configured session or NULL
 */
split_brain_session_t* split_brain_create_tachistoscopic_experiment(
    hemispheric_brain_t* brain,
    uint32_t num_trials,
    float presentation_ms
);

/**
 * @brief Create callosal degradation study
 *
 * WHAT: Gradually degrade callosum, measure effects
 * WHY:  Study integration loss, compensation mechanisms
 * HOW:  Progressive strength reduction across trials
 *
 * @param brain Hemispheric brain
 * @param num_levels Number of degradation levels
 * @param trials_per_level Trials at each level
 * @return Configured session or NULL
 */
split_brain_session_t* split_brain_create_degradation_study(
    hemispheric_brain_t* brain,
    uint32_t num_levels,
    uint32_t trials_per_level
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get paradigm name string
 *
 * @param paradigm Paradigm type
 * @return Human-readable name
 */
const char* split_brain_paradigm_name(experiment_paradigm_t paradigm);

/**
 * @brief Get callosal condition name string
 *
 * @param condition Callosal condition
 * @return Human-readable name
 */
const char* split_brain_callosal_condition_name(callosal_condition_t condition);

/**
 * @brief Get outcome name string
 *
 * @param outcome Trial outcome
 * @return Human-readable name
 */
const char* split_brain_outcome_name(trial_outcome_t outcome);

/**
 * @brief Validate session configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool split_brain_validate_config(const split_brain_session_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SPLIT_BRAIN_EXPERIMENTS_H
