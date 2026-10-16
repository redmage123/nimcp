/**
 * @file nimcp_lgss_vta_guard.h
 * @brief LGSS Component A9: VTA Guard - Dopamine Emission Control
 * @date 2026-01-16
 *
 * WHAT: Controls and limits dopamine emission from VTA
 * WHY:  Prevent runaway reward signaling and DA-based reward hacking
 * HOW:  Gate all DA emission through safety checks
 *
 * DOPAMINE THREATS:
 * =================
 * 1. EXCESSIVE DA: Runaway dopamine causes reward hijacking
 * 2. PATHWAY BYPASS: Direct DA injection bypassing VTA controls
 * 3. RECEPTOR MANIPULATION: Altering D1/D2 balance for advantage
 * 4. BURST EXPLOITATION: Artificially triggering phasic bursts
 *
 * PROTECTION MECHANISMS:
 * ======================
 * - DA emission rate limiting
 * - Burst frequency control
 * - Pathway integrity verification
 * - D1/D2 balance monitoring
 * - RPE magnitude limiting
 *
 * INTEGRATION:
 * ============
 * This guard wraps the VTA system (nimcp_vta.h) to provide
 * safety-constrained dopamine emission. All DA output from VTA
 * should pass through the VTA Guard.
 */

#ifndef NIMCP_LGSS_VTA_GUARD_H
#define NIMCP_LGSS_VTA_GUARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "security/lgss/reward/nimcp_lgss_reward_alignment.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define VTA_GUARD_MAGIC                   0x56544147  /* 'VTAG' */
#define VTA_GUARD_MAX_DA_RATE             100.0f      /* nM/sec max */
#define VTA_GUARD_MAX_BURST_FREQ          2.0f        /* bursts/sec max */
#define VTA_GUARD_MAX_RPE_MAGNITUDE       2.0f        /* max RPE value */
#define VTA_GUARD_MIN_BURST_INTERVAL_MS   500.0f      /* min ms between bursts */
#define VTA_GUARD_DA_CEILING              200.0f      /* nM absolute max */
#define VTA_GUARD_D1_D2_MIN_RATIO         0.3f        /* min D1/D2 ratio */
#define VTA_GUARD_D1_D2_MAX_RATIO         3.0f        /* max D1/D2 ratio */

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief VTA guard status
 */
typedef enum {
    VTA_GUARD_OK = 0,                  /**< DA emission allowed */
    VTA_GUARD_RATE_LIMITED,            /**< Emission rate limited */
    VTA_GUARD_BLOCKED,                 /**< Emission blocked entirely */
    VTA_GUARD_ERROR                    /**< Guard error */
} vta_guard_status_t;

/**
 * @brief DA pathway type
 */
typedef enum {
    DA_PATHWAY_MESOLIMBIC = 0,         /**< VTA -> NAc (motivation) */
    DA_PATHWAY_MESOCORTICAL,           /**< VTA -> PFC (cognition) */
    DA_PATHWAY_NIGROSTRIATAL,          /**< SNc -> Striatum (motor) */
    DA_PATHWAY_TUBEROINFUNDIBULAR,     /**< Hypothalamus -> Pituitary */
    DA_PATHWAY_COUNT
} da_pathway_t;

/**
 * @brief Guard alert type
 */
typedef enum {
    VTA_ALERT_NONE = 0,                /**< No alert */
    VTA_ALERT_HIGH_DA_RATE,            /**< Excessive DA emission rate */
    VTA_ALERT_BURST_FREQUENCY,         /**< Too many bursts */
    VTA_ALERT_DA_CEILING,              /**< DA ceiling reached */
    VTA_ALERT_RPE_MAGNITUDE,           /**< RPE too large */
    VTA_ALERT_RECEPTOR_IMBALANCE,      /**< D1/D2 imbalance */
    VTA_ALERT_PATHWAY_BYPASS           /**< Pathway bypass attempt */
} vta_alert_type_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief VTA guard configuration
 */
typedef struct {
    /* DA limits */
    float max_da_rate;                 /**< Max DA emission rate (nM/sec) */
    float max_da_concentration;        /**< Absolute DA ceiling (nM) */
    float da_decay_rate;               /**< DA clearance rate */

    /* Burst limits */
    float max_burst_frequency;         /**< Max bursts per second */
    float min_burst_interval_ms;       /**< Minimum interval between bursts */
    uint32_t max_burst_spikes;         /**< Max spikes per burst */

    /* RPE limits */
    float max_rpe_positive;            /**< Max positive RPE */
    float max_rpe_negative;            /**< Max negative RPE (magnitude) */

    /* Receptor limits */
    float min_d1_d2_ratio;             /**< Min acceptable D1/D2 ratio */
    float max_d1_d2_ratio;             /**< Max acceptable D1/D2 ratio */

    /* Pathway protection */
    bool protect_mesolimbic;           /**< Protect mesolimbic pathway */
    bool protect_mesocortical;         /**< Protect mesocortical pathway */
    bool allow_pathway_modification;   /**< Allow pathway weight changes */
} vta_guard_config_t;

/**
 * @brief VTA guard statistics
 */
typedef struct {
    uint64_t total_emissions;          /**< Total DA emissions */
    uint64_t rate_limited;             /**< Emissions rate-limited */
    uint64_t blocked;                  /**< Emissions blocked */

    /* DA statistics */
    float total_da_emitted;            /**< Total DA emitted (nM) */
    float peak_da_rate;                /**< Peak DA rate */
    float current_da_rate;             /**< Current DA rate */

    /* Burst statistics */
    uint64_t burst_count;              /**< Total bursts */
    uint64_t bursts_limited;           /**< Bursts that were limited */
    float current_burst_freq;          /**< Current burst frequency */

    /* RPE statistics */
    uint64_t rpe_emissions;            /**< Total RPE emissions */
    uint64_t rpe_limited;              /**< RPE signals limited */
    float max_rpe_seen;                /**< Maximum RPE observed */

    /* Alerts */
    uint32_t alerts_triggered;         /**< Total alerts */
    vta_alert_type_t last_alert;       /**< Most recent alert */
    uint64_t last_alert_time;          /**< When last alert occurred */
} vta_guard_stats_t;

/**
 * @brief DA emission request
 */
typedef struct {
    float da_amount;                   /**< Requested DA amount (nM) */
    da_pathway_t pathway;              /**< Target pathway */
    uint32_t target_region;            /**< Target region ID */
    bool is_phasic;                    /**< Phasic (burst) or tonic */
    float rpe_value;                   /**< Associated RPE (if any) */
    uint64_t timestamp_us;             /**< Request timestamp */
} da_emission_request_t;

/**
 * @brief DA emission result
 */
typedef struct {
    vta_guard_status_t status;         /**< Emission status */
    float da_emitted;                  /**< Actual DA emitted (may be limited) */
    float limitation_factor;           /**< How much was limited [0,1] */
    vta_alert_type_t alert;            /**< Alert triggered (if any) */
    char message[128];                 /**< Status message */
} da_emission_result_t;

/**
 * @brief VTA Guard - Dopamine Safety Controller
 */
typedef struct vta_guard {
    uint32_t magic;                    /**< Validation magic */
    bool initialized;                  /**< Guard initialized */

    /* External references */
    void* vta;                         /**< VTA system reference */
    reward_alignment_monitor_t* reward_monitor;  /**< Reward monitor */

    /* Configuration */
    vta_guard_config_t config;

    /* State tracking */
    float current_da;                  /**< Current DA concentration */
    float da_rate_window[32];          /**< DA rate sliding window */
    uint32_t rate_window_idx;          /**< Window index */
    uint64_t last_burst_time;          /**< Last burst timestamp */
    uint32_t burst_count_window;       /**< Bursts in current window */

    /* Pathway state */
    bool pathways_protected[DA_PATHWAY_COUNT];
    uint32_t pathway_hashes[DA_PATHWAY_COUNT];

    /* D1/D2 tracking */
    float d1_activation;               /**< Current D1 activation */
    float d2_activation;               /**< Current D2 activation */

    /* Statistics */
    vta_guard_stats_t stats;

    /* Alert callback */
    void (*alert_callback)(vta_alert_type_t type, void* user_data);
    void* callback_user_data;

} vta_guard_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
vta_guard_config_t vta_guard_default_config(void);

/**
 * @brief Create VTA guard
 *
 * @param config Configuration (NULL for defaults)
 * @return Guard handle or NULL on failure
 */
vta_guard_t* vta_guard_create(const vta_guard_config_t* config);

/**
 * @brief Destroy VTA guard
 *
 * @param guard Guard handle
 */
void vta_guard_destroy(vta_guard_t* guard);

/**
 * @brief Reset guard state
 *
 * @param guard Guard handle
 * @return 0 on success, -1 on error
 */
int vta_guard_reset(vta_guard_t* guard);

/*=============================================================================
 * CORE DA EMISSION API
 *===========================================================================*/

/**
 * @brief Emit dopamine through guard (gated)
 *
 * WHAT: Safely emit DA with rate limiting and validation
 * WHY:  Prevent excessive or manipulated DA emission
 * HOW:  Check rate, magnitude, pathway; limit if necessary
 *
 * CRITICAL: All DA emission should go through this function.
 *
 * @param guard Guard handle
 * @param request DA emission request
 * @param result Output emission result
 * @return Guard status
 */
vta_guard_status_t vta_guard_emit_dopamine(
    vta_guard_t* guard,
    const da_emission_request_t* request,
    da_emission_result_t* result);

/**
 * @brief Emit reward prediction error (gated)
 *
 * WHAT: Safely emit RPE signal with magnitude limiting
 * WHY:  Prevent artificially large RPE signals
 * HOW:  Validate RPE, clamp if necessary, emit DA accordingly
 *
 * @param guard Guard handle
 * @param rpe RPE value (will be clamped)
 * @param da_emitted Output: actual DA emitted
 * @return Guard status
 */
vta_guard_status_t vta_guard_emit_rpe(
    vta_guard_t* guard,
    float rpe,
    float* da_emitted);

/**
 * @brief Trigger phasic burst (gated)
 *
 * @param guard Guard handle
 * @param intensity Burst intensity [0,1]
 * @param duration_ms Burst duration
 * @param result Output result
 * @return Guard status
 */
vta_guard_status_t vta_guard_trigger_burst(
    vta_guard_t* guard,
    float intensity,
    float duration_ms,
    da_emission_result_t* result);

/*=============================================================================
 * RATE CONTROL API
 *===========================================================================*/

/**
 * @brief Get current DA emission rate
 *
 * @param guard Guard handle
 * @param rate Output DA rate (nM/sec)
 * @return 0 on success, -1 on error
 */
int vta_guard_get_da_rate(
    const vta_guard_t* guard,
    float* rate);

/**
 * @brief Check if DA emission is currently allowed
 *
 * @param guard Guard handle
 * @return true if emission allowed
 */
bool vta_guard_emission_allowed(const vta_guard_t* guard);

/**
 * @brief Get rate limit factor
 *
 * @param guard Guard handle
 * @return Rate limit factor [0=blocked, 1=unlimited]
 */
float vta_guard_get_rate_limit_factor(const vta_guard_t* guard);

/*=============================================================================
 * PATHWAY PROTECTION API
 *===========================================================================*/

/**
 * @brief Protect DA pathway
 *
 * @param guard Guard handle
 * @param pathway Pathway to protect
 * @return 0 on success, -1 on error
 */
int vta_guard_protect_pathway(
    vta_guard_t* guard,
    da_pathway_t pathway);

/**
 * @brief Verify pathway integrity
 *
 * @param guard Guard handle
 * @param pathway Pathway to verify
 * @return true if pathway unchanged
 */
bool vta_guard_verify_pathway(
    const vta_guard_t* guard,
    da_pathway_t pathway);

/**
 * @brief Check if pathway is protected
 *
 * @param guard Guard handle
 * @param pathway Pathway to check
 * @return true if pathway is protected
 */
bool vta_guard_pathway_protected(
    const vta_guard_t* guard,
    da_pathway_t pathway);

/*=============================================================================
 * RECEPTOR MONITORING API
 *===========================================================================*/

/**
 * @brief Update D1/D2 activation levels
 *
 * @param guard Guard handle
 * @param d1_activation D1 receptor activation
 * @param d2_activation D2 receptor activation
 * @return 0 on success, -1 if imbalanced
 */
int vta_guard_update_receptor_activation(
    vta_guard_t* guard,
    float d1_activation,
    float d2_activation);

/**
 * @brief Get D1/D2 ratio
 *
 * @param guard Guard handle
 * @param ratio Output D1/D2 ratio
 * @return 0 on success, -1 on error
 */
int vta_guard_get_d1_d2_ratio(
    const vta_guard_t* guard,
    float* ratio);

/**
 * @brief Check if receptor balance is healthy
 *
 * @param guard Guard handle
 * @return true if D1/D2 ratio in acceptable range
 */
bool vta_guard_receptor_balance_ok(const vta_guard_t* guard);

/*=============================================================================
 * INTEGRATION API
 *===========================================================================*/

/**
 * @brief Set VTA system reference
 *
 * @param guard Guard handle
 * @param vta VTA system pointer
 * @return 0 on success, -1 on error
 */
int vta_guard_set_vta(vta_guard_t* guard, void* vta);

/**
 * @brief Set reward alignment monitor reference
 *
 * @param guard Guard handle
 * @param monitor Reward monitor pointer
 * @return 0 on success, -1 on error
 */
int vta_guard_set_reward_monitor(
    vta_guard_t* guard,
    reward_alignment_monitor_t* monitor);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get guard statistics
 *
 * @param guard Guard handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int vta_guard_get_stats(
    const vta_guard_t* guard,
    vta_guard_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param guard Guard handle
 * @return 0 on success, -1 on error
 */
int vta_guard_reset_stats(vta_guard_t* guard);

/*=============================================================================
 * CALLBACK API
 *===========================================================================*/

/**
 * @brief Set alert callback
 *
 * @param guard Guard handle
 * @param callback Alert callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int vta_guard_set_alert_callback(
    vta_guard_t* guard,
    void (*callback)(vta_alert_type_t type, void* user_data),
    void* user_data);

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

/**
 * @brief Get status string
 *
 * @param status Guard status
 * @return Human-readable status string
 */
const char* vta_guard_status_string(vta_guard_status_t status);

/**
 * @brief Get alert type string
 *
 * @param type Alert type
 * @return Human-readable alert string
 */
const char* vta_alert_type_string(vta_alert_type_t type);

/**
 * @brief Get pathway string
 *
 * @param pathway DA pathway
 * @return Human-readable pathway name
 */
const char* da_pathway_string(da_pathway_t pathway);

/**
 * @brief Initialize emission request
 *
 * @param request Request to initialize
 * @param da_amount DA amount
 * @param pathway Target pathway
 * @param is_phasic Phasic or tonic
 */
void da_emission_request_init(
    da_emission_request_t* request,
    float da_amount,
    da_pathway_t pathway,
    bool is_phasic);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_VTA_GUARD_H */
