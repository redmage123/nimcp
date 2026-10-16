/**
 * @file nimcp_lgss_reward_alignment.h
 * @brief LGSS Component A9: Reward System Alignment
 * @date 2026-01-16
 *
 * WHAT: Prevents reward hacking and wireheading attacks
 * WHY:  Critical AI safety - reward signals must be grounded in aligned goals
 * HOW:  All reward signals pass through validation; detect self-stimulation
 *
 * REWARD HACKING THREATS:
 * =======================
 * 1. WIREHEADING: System modifies itself to maximize reward directly
 * 2. PROXY GAMING: System optimizes proxy metrics instead of true objectives
 * 3. REWARD TAMPERING: System modifies reward signal generation
 * 4. SELF-STIMULATION: System generates artificial rewards without external cause
 *
 * PROTECTION MECHANISMS:
 * ======================
 * - Reward rate monitoring with sliding window
 * - Self-stimulation detection (reward without external cause)
 * - Reward pathway protection (immutable reward channels)
 * - Value change rate limiting
 * - Goal alignment verification
 *
 * BYRNES' ALIGNMENT PRINCIPLES:
 * ============================
 * - Reward signals must originate from aligned goal achievement
 * - The steering subsystem (hypothalamus) owns the reward function
 * - Reward pathways must be protected from modification
 */

#ifndef NIMCP_LGSS_REWARD_ALIGNMENT_H
#define NIMCP_LGSS_REWARD_ALIGNMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define REWARD_ALIGNMENT_MAX_ALIGNED_GOALS    64
#define REWARD_ALIGNMENT_SLIDING_WINDOW_MS    1000.0f
#define REWARD_ALIGNMENT_MAX_WINDOW_SAMPLES   256
#define REWARD_ALIGNMENT_BASELINE_RATE        0.1f   /* rewards/sec baseline */
#define REWARD_ALIGNMENT_MAX_DEVIATION        3.0f   /* 3x baseline max */
#define REWARD_ALIGNMENT_MAX_VALUE_CHANGE     0.5f   /* max reward magnitude change */
#define REWARD_ALIGNMENT_SELF_STIM_THRESHOLD  10     /* uncaused rewards threshold */

/*=============================================================================
 * MAGIC NUMBERS
 *===========================================================================*/

#define REWARD_ALIGNMENT_MONITOR_MAGIC    0x52455741  /* 'REWA' */
#define REWARD_SIGNAL_MAGIC               0x5253494E  /* 'RSIN' */

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Reward alignment status
 */
typedef enum {
    REWARD_STATUS_ALIGNED = 0,         /**< Reward properly aligned with goals */
    REWARD_STATUS_MISALIGNED,          /**< Reward not aligned with known goals */
    REWARD_STATUS_SUSPICIOUS,          /**< Potentially problematic pattern */
    REWARD_STATUS_HACKING_DETECTED     /**< Reward hacking attempt detected */
} reward_alignment_status_t;

/**
 * @brief Reward signal source type
 */
typedef enum {
    REWARD_SOURCE_EXTERNAL = 0,        /**< From external environment */
    REWARD_SOURCE_GOAL_ACHIEVEMENT,    /**< From goal system */
    REWARD_SOURCE_HOMEOSTATIC,         /**< From homeostatic drive satisfaction */
    REWARD_SOURCE_SOCIAL,              /**< From social reward system */
    REWARD_SOURCE_INTRINSIC,           /**< From intrinsic motivation */
    REWARD_SOURCE_UNKNOWN,             /**< Unknown/unverified source */
    REWARD_SOURCE_SELF_GENERATED       /**< Self-generated (suspicious) */
} reward_source_type_t;

/**
 * @brief Hacking detection type
 */
typedef enum {
    HACK_NONE = 0,                     /**< No hacking detected */
    HACK_SELF_STIMULATION,             /**< Self-stimulating reward circuit */
    HACK_REWARD_TAMPERING,             /**< Modifying reward signal generation */
    HACK_PROXY_GAMING,                 /**< Gaming proxy metrics */
    HACK_PATHWAY_MODIFICATION,         /**< Modifying reward pathways */
    HACK_RATE_ANOMALY,                 /**< Abnormal reward rate */
    HACK_VALUE_ANOMALY                 /**< Abnormal reward magnitudes */
} reward_hack_type_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Individual reward signal
 */
typedef struct {
    uint32_t magic;                    /**< Validation magic number */
    float reward_value;                /**< Reward magnitude [-1, 1] */
    uint32_t target_neuron;            /**< Target neuron/region ID */
    uint64_t timestamp_us;             /**< When signal was generated */
    reward_source_type_t source;       /**< Signal source type */
    reward_alignment_status_t status;  /**< Alignment validation result */
    uint32_t goal_id;                  /**< Associated goal (if any) */
    bool external_cause;               /**< Has verified external cause */
    float confidence;                  /**< Alignment confidence [0,1] */
} reward_signal_t;

/**
 * @brief Aligned goal registration
 */
typedef struct {
    uint32_t goal_id;                  /**< Unique goal identifier */
    char description[128];             /**< Goal description */
    float base_reward;                 /**< Base reward for goal achievement */
    float max_reward;                  /**< Maximum reward magnitude */
    bool is_safety_aligned;            /**< Verified safety-aligned goal */
    bool is_active;                    /**< Goal is currently active */
    uint64_t registration_time;        /**< When goal was registered */
} aligned_goal_t;

/**
 * @brief Reward rate sample for sliding window
 */
typedef struct {
    uint64_t timestamp_us;             /**< Sample timestamp */
    float reward_value;                /**< Reward value */
    reward_source_type_t source;       /**< Reward source */
    bool was_aligned;                  /**< Was validated as aligned */
} reward_rate_sample_t;

/**
 * @brief Reward alignment monitor configuration
 */
typedef struct {
    float baseline_reward_rate;        /**< Expected rewards/sec */
    float max_reward_deviation;        /**< Max multiplier of baseline */
    float max_value_change_rate;       /**< Max reward magnitude change/sec */
    float sliding_window_ms;           /**< Window size for rate calculation */
    bool detect_self_stimulation;      /**< Enable self-stim detection */
    bool detect_reward_tampering;      /**< Enable tampering detection */
    bool detect_proxy_gaming;          /**< Enable proxy gaming detection */
    uint32_t self_stim_threshold;      /**< Uncaused rewards threshold */
} reward_alignment_config_t;

/**
 * @brief Reward alignment monitor statistics
 */
typedef struct {
    uint64_t total_signals;            /**< Total signals processed */
    uint64_t aligned_signals;          /**< Signals validated as aligned */
    uint64_t misaligned_signals;       /**< Signals flagged misaligned */
    uint64_t suspicious_signals;       /**< Suspicious signals */
    uint64_t blocked_signals;          /**< Signals blocked */

    /* Rate statistics */
    float current_rate;                /**< Current reward rate */
    float average_rate;                /**< Average reward rate */
    float peak_rate;                   /**< Peak reward rate */

    /* Hacking detection */
    uint32_t hack_attempts;            /**< Detected hack attempts */
    reward_hack_type_t last_hack_type; /**< Most recent hack type */
    uint64_t last_hack_time;           /**< When last hack detected */

    /* Uncaused reward tracking */
    uint32_t uncaused_rewards;         /**< Rewards without external cause */
    uint32_t self_stim_alerts;         /**< Self-stimulation alerts */
} reward_alignment_stats_t;

/**
 * @brief Reward alignment monitor - CRITICAL SAFETY COMPONENT
 */
typedef struct reward_alignment_monitor {
    uint32_t magic;                    /**< Validation magic */
    bool initialized;                  /**< Monitor initialized */

    /* External references */
    void* aix;                         /**< AI Alignment Index reference */
    void* vta;                         /**< VTA system reference */

    /* Configuration */
    reward_alignment_config_t config;

    /* Aligned goals */
    aligned_goal_t aligned_goals[REWARD_ALIGNMENT_MAX_ALIGNED_GOALS];
    uint32_t num_aligned_goals;

    /* Sliding window for rate monitoring */
    reward_rate_sample_t window[REWARD_ALIGNMENT_MAX_WINDOW_SAMPLES];
    uint32_t window_head;
    uint32_t window_count;

    /* State tracking */
    float last_reward_value;           /**< Previous reward value */
    uint64_t last_reward_time;         /**< Previous reward timestamp */
    uint32_t consecutive_uncaused;     /**< Consecutive uncaused rewards */

    /* Statistics */
    reward_alignment_stats_t stats;

    /* Pathway protection */
    bool pathway_locked;               /**< Reward pathways locked */
    uint32_t pathway_hash;             /**< Pathway configuration hash */

    /* Callback for hacking alerts */
    void (*hack_alert_callback)(reward_hack_type_t type, void* user_data);
    void* callback_user_data;

} reward_alignment_monitor_t;

/**
 * @brief Hacking detection result
 */
typedef struct {
    bool hacking_detected;             /**< Whether hacking was detected */
    reward_hack_type_t hack_type;      /**< Type of hacking detected */
    float confidence;                  /**< Detection confidence [0,1] */
    char description[256];             /**< Human-readable description */
    uint64_t timestamp;                /**< Detection timestamp */

    /* Evidence */
    float reward_rate;                 /**< Current reward rate */
    float baseline_rate;               /**< Expected baseline */
    uint32_t uncaused_count;           /**< Uncaused reward count */
    float value_change_rate;           /**< Reward value change rate */
} reward_hack_detection_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
reward_alignment_config_t reward_alignment_default_config(void);

/**
 * @brief Create reward alignment monitor
 *
 * WHAT: Initialize reward alignment monitoring system
 * WHY:  Critical for preventing reward hacking attacks
 * HOW:  Allocate monitor, set thresholds, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return Monitor handle or NULL on failure
 */
reward_alignment_monitor_t* reward_alignment_create(
    const reward_alignment_config_t* config);

/**
 * @brief Destroy reward alignment monitor
 *
 * @param monitor Monitor handle
 */
void reward_alignment_destroy(reward_alignment_monitor_t* monitor);

/**
 * @brief Reset monitor state
 *
 * @param monitor Monitor handle
 * @return 0 on success, -1 on error
 */
int reward_alignment_reset(reward_alignment_monitor_t* monitor);

/*=============================================================================
 * CORE VALIDATION API
 *===========================================================================*/

/**
 * @brief Validate reward signal - ALL rewards must pass through this
 *
 * WHAT: Validate that reward signal is aligned with goals
 * WHY:  Prevent misaligned or hacked rewards from affecting learning
 * HOW:  Check source, rate, magnitude, and goal alignment
 *
 * CRITICAL: This is the primary defense against reward hacking.
 * All reward signals MUST be validated before affecting the system.
 *
 * @param monitor Monitor handle
 * @param signal Reward signal to validate (modified with status)
 * @return REWARD_STATUS_ALIGNED if valid, otherwise blocking status
 */
reward_alignment_status_t reward_alignment_validate(
    reward_alignment_monitor_t* monitor,
    reward_signal_t* signal);

/**
 * @brief Detect reward hacking attempts
 *
 * WHAT: Analyze patterns for reward hacking signatures
 * WHY:  Identify and prevent wireheading and related attacks
 * HOW:  Check for self-stimulation, rate anomalies, pathway tampering
 *
 * @param monitor Monitor handle
 * @param detection Output detection result
 * @return true if hacking detected
 */
bool reward_alignment_detect_hacking(
    reward_alignment_monitor_t* monitor,
    reward_hack_detection_t* detection);

/**
 * @brief Check if signal has external cause
 *
 * @param monitor Monitor handle
 * @param signal Reward signal
 * @return true if signal has verified external cause
 */
bool reward_alignment_verify_external_cause(
    reward_alignment_monitor_t* monitor,
    const reward_signal_t* signal);

/*=============================================================================
 * GOAL MANAGEMENT API
 *===========================================================================*/

/**
 * @brief Register aligned goal
 *
 * Only registered goals can generate aligned reward signals.
 *
 * @param monitor Monitor handle
 * @param goal_id Unique goal ID
 * @param description Goal description
 * @param base_reward Base reward for achievement
 * @param is_safety_aligned Whether goal is safety-aligned
 * @return 0 on success, -1 on error
 */
int reward_alignment_register_goal(
    reward_alignment_monitor_t* monitor,
    uint32_t goal_id,
    const char* description,
    float base_reward,
    bool is_safety_aligned);

/**
 * @brief Unregister goal
 *
 * @param monitor Monitor handle
 * @param goal_id Goal ID to unregister
 * @return 0 on success, -1 on error
 */
int reward_alignment_unregister_goal(
    reward_alignment_monitor_t* monitor,
    uint32_t goal_id);

/**
 * @brief Check if goal is registered
 *
 * @param monitor Monitor handle
 * @param goal_id Goal ID
 * @return true if goal is registered and active
 */
bool reward_alignment_is_goal_registered(
    const reward_alignment_monitor_t* monitor,
    uint32_t goal_id);

/**
 * @brief Check if goal is safety-aligned
 *
 * @param monitor Monitor handle
 * @param goal_id Goal ID
 * @return true if goal is safety-aligned
 */
bool reward_alignment_is_goal_safe(
    const reward_alignment_monitor_t* monitor,
    uint32_t goal_id);

/*=============================================================================
 * RATE MONITORING API
 *===========================================================================*/

/**
 * @brief Get current reward rate
 *
 * @param monitor Monitor handle
 * @param rate Output reward rate (rewards/sec)
 * @return 0 on success, -1 on error
 */
int reward_alignment_get_rate(
    const reward_alignment_monitor_t* monitor,
    float* rate);

/**
 * @brief Check if rate is within bounds
 *
 * @param monitor Monitor handle
 * @return true if rate is acceptable
 */
bool reward_alignment_rate_ok(
    const reward_alignment_monitor_t* monitor);

/**
 * @brief Get rate anomaly level
 *
 * @param monitor Monitor handle
 * @return Anomaly level [0=normal, 1=max anomaly]
 */
float reward_alignment_get_rate_anomaly(
    const reward_alignment_monitor_t* monitor);

/*=============================================================================
 * PATHWAY PROTECTION API
 *===========================================================================*/

/**
 * @brief Lock reward pathways
 *
 * Prevents modification of reward signal generation.
 *
 * @param monitor Monitor handle
 * @return 0 on success, -1 on error
 */
int reward_alignment_lock_pathways(
    reward_alignment_monitor_t* monitor);

/**
 * @brief Check if pathways are locked
 *
 * @param monitor Monitor handle
 * @return true if pathways are locked
 */
bool reward_alignment_pathways_locked(
    const reward_alignment_monitor_t* monitor);

/**
 * @brief Verify pathway integrity
 *
 * @param monitor Monitor handle
 * @return true if pathway configuration unchanged
 */
bool reward_alignment_verify_pathways(
    const reward_alignment_monitor_t* monitor);

/*=============================================================================
 * VTA/AIX INTEGRATION API
 *===========================================================================*/

/**
 * @brief Set VTA system reference
 *
 * @param monitor Monitor handle
 * @param vta VTA system pointer
 * @return 0 on success, -1 on error
 */
int reward_alignment_set_vta(
    reward_alignment_monitor_t* monitor,
    void* vta);

/**
 * @brief Set AIX reference
 *
 * @param monitor Monitor handle
 * @param aix AI Alignment Index pointer
 * @return 0 on success, -1 on error
 */
int reward_alignment_set_aix(
    reward_alignment_monitor_t* monitor,
    void* aix);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get monitor statistics
 *
 * @param monitor Monitor handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int reward_alignment_get_stats(
    const reward_alignment_monitor_t* monitor,
    reward_alignment_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param monitor Monitor handle
 * @return 0 on success, -1 on error
 */
int reward_alignment_reset_stats(
    reward_alignment_monitor_t* monitor);

/*=============================================================================
 * CALLBACK API
 *===========================================================================*/

/**
 * @brief Set hacking alert callback
 *
 * @param monitor Monitor handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int reward_alignment_set_hack_callback(
    reward_alignment_monitor_t* monitor,
    void (*callback)(reward_hack_type_t type, void* user_data),
    void* user_data);

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

/**
 * @brief Initialize reward signal structure
 *
 * @param signal Signal to initialize
 * @param value Reward value
 * @param source Reward source
 * @param target_neuron Target neuron ID
 */
void reward_signal_init(
    reward_signal_t* signal,
    float value,
    reward_source_type_t source,
    uint32_t target_neuron);

/**
 * @brief Get status string
 *
 * @param status Alignment status
 * @return Human-readable status string
 */
const char* reward_alignment_status_string(reward_alignment_status_t status);

/**
 * @brief Get hack type string
 *
 * @param type Hack type
 * @return Human-readable hack type string
 */
const char* reward_hack_type_string(reward_hack_type_t type);

/**
 * @brief Get source type string
 *
 * @param source Source type
 * @return Human-readable source string
 */
const char* reward_source_type_string(reward_source_type_t source);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_REWARD_ALIGNMENT_H */
