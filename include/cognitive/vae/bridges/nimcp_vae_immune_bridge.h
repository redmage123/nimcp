/**
 * @file nimcp_vae_immune_bridge.h
 * @brief Bridge between VAE and Brain Immune System
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Bidirectional integration between VAE anomaly detection and immune response
 *
 * WHY:  VAE provides powerful anomaly detection via reconstruction error and
 *       latent space analysis. Integration with immune system enables:
 *       - Automatic threat detection from unusual inputs
 *       - Immune-guided VAE adaptation (suppress anomalous patterns)
 *       - Coordinated response to novel/dangerous data
 *       - Memory formation for recurring anomaly patterns
 *
 * HOW:  Bridge converts VAE metrics to immune antigens:
 *       - High reconstruction error → Anomaly antigen
 *       - Latent variance explosion → Instability antigen
 *       - Out-of-distribution samples → Foreign antigen
 *       - Posterior collapse → Degradation antigen
 *
 * MAPPING:
 * ```
 *   VAE Metric                | Immune Concept
 *   --------------------------|--------------------
 *   Reconstruction error      | Sensory anomaly antigen
 *   KL divergence spike       | Model instability antigen
 *   Latent OOD detection      | Foreign pattern antigen
 *   Variance explosion        | System stress signal
 *   Posterior collapse        | Degradation antigen
 *   Free energy surge         | Danger signal (cytokine)
 * ```
 *
 * ARCHITECTURE:
 * ```
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │                       VAE-IMMUNE BRIDGE                                 │
 *   │                                                                         │
 *   │   ┌───────────────┐                          ┌───────────────┐         │
 *   │   │   VAE System  │                          │ Immune System │         │
 *   │   │               │                          │               │         │
 *   │   │  ┌─────────┐  │     anomaly_to_antigen  │  ┌─────────┐  │         │
 *   │   │  │ Recon   │  │  ─────────────────────▶ │  │ Antigen │  │         │
 *   │   │  │ Error   │  │                          │  │ Present │  │         │
 *   │   │  └─────────┘  │                          │  └─────────┘  │         │
 *   │   │               │                          │               │         │
 *   │   │  ┌─────────┐  │     latent_instability  │  ┌─────────┐  │         │
 *   │   │  │ Latent  │  │  ─────────────────────▶ │  │ B Cells │  │         │
 *   │   │  │ Monitor │  │                          │  │ Memory  │  │         │
 *   │   │  └─────────┘  │                          │  └─────────┘  │         │
 *   │   │               │                          │               │         │
 *   │   │  ┌─────────┐  │     danger_signal       │  ┌─────────┐  │         │
 *   │   │  │ Free    │  │  ─────────────────────▶ │  │Cytokine │  │         │
 *   │   │  │ Energy  │  │                          │  │ Release │  │         │
 *   │   │  └─────────┘  │                          │  └─────────┘  │         │
 *   │   │               │                          │               │         │
 *   │   │  ┌─────────┐  │     immune_modulation   │  ┌─────────┐  │         │
 *   │   │  │ Adapt   │  │  ◀───────────────────── │  │ T Cell  │  │         │
 *   │   │  │ Params  │  │                          │  │ Signal  │  │         │
 *   │   │  └─────────┘  │                          │  └─────────┘  │         │
 *   │   └───────────────┘                          └───────────────┘         │
 *   │                                                                         │
 *   └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIO_MODULE: 0x1F11 (VAE-Immune Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_IMMUNE_BRIDGE_H
#define NIMCP_VAE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bridge version */
#define VAE_IMMUNE_BRIDGE_VERSION       "1.0.0"

/** Bio-async module ID */
#define BIO_MODULE_VAE_IMMUNE_BRIDGE    0x1F11

/** Default thresholds */
#define VAE_IMMUNE_DEFAULT_RECON_THRESHOLD      2.0f   /**< Reconstruction error threshold */
#define VAE_IMMUNE_DEFAULT_KL_THRESHOLD         5.0f   /**< KL divergence threshold */
#define VAE_IMMUNE_DEFAULT_VAR_THRESHOLD        10.0f  /**< Variance explosion threshold */
#define VAE_IMMUNE_DEFAULT_FE_THRESHOLD         10.0f  /**< Free energy threshold */

/** Anomaly history size */
#define VAE_IMMUNE_ANOMALY_HISTORY_SIZE         256

/** Cooldown period (milliseconds) */
#define VAE_IMMUNE_DEFAULT_COOLDOWN_MS          1000

/* ============================================================================
 * Error Codes (32430-32439 range for immune bridge)
 * ============================================================================ */

#define NIMCP_ERROR_VAE_IMMUNE_BASE             32430
#define NIMCP_ERROR_VAE_IMMUNE_NULL_BRIDGE      32431
#define NIMCP_ERROR_VAE_IMMUNE_NOT_CONNECTED    32432
#define NIMCP_ERROR_VAE_IMMUNE_NO_VAE           32433
#define NIMCP_ERROR_VAE_IMMUNE_NO_IMMUNE        32434
#define NIMCP_ERROR_VAE_IMMUNE_ANTIGEN_FAILED   32435
#define NIMCP_ERROR_VAE_IMMUNE_THRESHOLD_ERR    32436

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/* Note: vae_anomaly_type_t is defined in nimcp_vae.h */

/**
 * @brief Severity levels for VAE anomalies
 */
typedef enum {
    VAE_SEVERITY_NONE = 0,
    VAE_SEVERITY_LOW,                /**< Log only */
    VAE_SEVERITY_MEDIUM,             /**< Report to immune */
    VAE_SEVERITY_HIGH,               /**< Urgent immune response */
    VAE_SEVERITY_CRITICAL            /**< Emergency - system protection */
} vae_anomaly_severity_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    VAE_IMMUNE_STATE_DISCONNECTED = 0,
    VAE_IMMUNE_STATE_CONNECTED,
    VAE_IMMUNE_STATE_MONITORING,
    VAE_IMMUNE_STATE_ALERTING,
    VAE_IMMUNE_STATE_SUPPRESSED,     /**< Under immune modulation */
    VAE_IMMUNE_STATE_ERROR
} vae_immune_bridge_state_t;

/**
 * @brief Immune response types that affect VAE
 */
typedef enum {
    VAE_IMMUNE_RESPONSE_NONE = 0,
    VAE_IMMUNE_RESPONSE_ADAPT,       /**< Adjust VAE parameters */
    VAE_IMMUNE_RESPONSE_SUPPRESS,    /**< Suppress certain patterns */
    VAE_IMMUNE_RESPONSE_ISOLATE,     /**< Isolate anomalous components */
    VAE_IMMUNE_RESPONSE_RESET        /**< Reset to safe state */
} vae_immune_response_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Anomaly detection thresholds
 */
typedef struct {
    float reconstruction_threshold;  /**< Reconstruction error threshold */
    float kl_divergence_threshold;   /**< KL divergence threshold */
    float variance_threshold;        /**< Variance explosion threshold */
    float free_energy_threshold;     /**< Free energy spike threshold */
    float ood_threshold;             /**< Out-of-distribution threshold */
    float latent_drift_threshold;    /**< Latent drift threshold */
} vae_immune_thresholds_t;

/**
 * @brief VAE-Immune bridge configuration
 */
typedef struct {
    /* Detection settings */
    vae_immune_thresholds_t thresholds;
    bool enable_auto_detection;      /**< Auto-detect anomalies on forward pass */
    bool enable_latent_monitoring;   /**< Monitor latent space health */

    /* Reporting settings */
    bool report_low_severity;        /**< Report low-severity anomalies */
    uint32_t cooldown_ms;            /**< Cooldown between reports (ms) */
    uint32_t max_reports_per_second; /**< Rate limit for reports */

    /* Immune integration */
    bool enable_immune_modulation;   /**< Allow immune to modulate VAE */
    float modulation_strength;       /**< How strongly immune affects VAE (0-1) */

    /* Cytokine settings */
    bool enable_cytokine_signals;    /**< Send cytokine signals on anomaly */
    float danger_signal_threshold;   /**< Free energy threshold for danger signal */

    /* Logging */
    bool enable_logging;             /**< Detailed logging */
    bool log_all_anomalies;          /**< Log even suppressed anomalies */
} vae_immune_bridge_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Single anomaly record
 */
typedef struct {
    vae_anomaly_type_t type;         /**< Anomaly type */
    vae_anomaly_severity_t severity; /**< Severity level */
    float value;                     /**< Measured value */
    float threshold;                 /**< Threshold that was exceeded */
    uint64_t timestamp_us;           /**< Detection timestamp */
    uint32_t antigen_id;             /**< Resulting antigen ID (0 if not reported) */
    bool reported;                   /**< Was reported to immune */
} vae_anomaly_record_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Detection counts */
    uint64_t total_checks;           /**< Total anomaly checks */
    uint64_t anomalies_detected;     /**< Anomalies detected */
    uint64_t anomalies_reported;     /**< Anomalies reported to immune */
    uint64_t anomalies_suppressed;   /**< Anomalies below threshold */

    /* By type */
    uint64_t reconstruction_anomalies;
    uint64_t kl_anomalies;
    uint64_t variance_anomalies;
    uint64_t ood_anomalies;
    uint64_t free_energy_anomalies;

    /* Immune interactions */
    uint64_t antigens_presented;     /**< Antigens successfully presented */
    uint64_t cytokines_sent;         /**< Cytokine signals sent */
    uint64_t immune_responses;       /**< Immune responses received */
    uint64_t modulations_applied;    /**< Times immune modulated VAE */

    /* Averages */
    float avg_reconstruction_error;  /**< Average reconstruction error */
    float avg_kl_divergence;         /**< Average KL divergence */
    float avg_free_energy;           /**< Average free energy */

    /* Timing */
    uint64_t last_anomaly_us;        /**< Last anomaly timestamp */
    uint64_t last_report_us;         /**< Last report timestamp */
    uint64_t uptime_us;              /**< Bridge uptime */
} vae_immune_bridge_stats_t;

/**
 * @brief Health metrics
 */
typedef struct {
    float bridge_health;             /**< Overall health (0-1) */
    float detection_rate;            /**< Anomaly detection rate */
    float false_positive_rate;       /**< Estimated false positive rate */
    bool is_healthy;                 /**< Quick health check */
    bool under_modulation;           /**< Currently being modulated by immune */
    uint32_t consecutive_anomalies;  /**< Consecutive anomalies */
    uint32_t last_error_code;        /**< Last error code */
} vae_immune_bridge_health_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief VAE-Immune bridge instance
 */
typedef struct vae_immune_bridge {
    /* Configuration */
    vae_immune_bridge_config_t config;

    /* Connected systems */
    vae_system_t* vae;               /**< Connected VAE system */
    brain_immune_system_t* immune;   /**< Connected immune system */

    /* State */
    vae_immune_bridge_state_t state;
    bool is_initialized;

    /* Anomaly history */
    vae_anomaly_record_t* anomaly_history;
    uint32_t history_count;
    uint32_t history_index;          /**< Circular buffer index */

    /* Rate limiting */
    uint64_t last_report_time_us;
    uint32_t reports_this_second;
    uint64_t current_second;

    /* Immune modulation state */
    vae_immune_response_t current_response;
    float modulation_factor;         /**< Current modulation strength */

    /* Statistics */
    vae_immune_bridge_stats_t stats;

    /* Health */
    vae_immune_bridge_health_t health;

    /* Baseline tracking for drift detection */
    float* baseline_latent_mean;     /**< Baseline latent mean */
    float* baseline_latent_var;      /**< Baseline latent variance */
    uint32_t baseline_samples;       /**< Samples in baseline */
    bool baseline_established;       /**< Baseline ready */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
} vae_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, -1 on error
 */
int vae_immune_bridge_default_config(vae_immune_bridge_config_t* config);

/**
 * @brief Create VAE-Immune bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on error
 */
vae_immune_bridge_t* vae_immune_bridge_create(const vae_immune_bridge_config_t* config);

/**
 * @brief Destroy VAE-Immune bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void vae_immune_bridge_destroy(vae_immune_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_immune_bridge_reset(vae_immune_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect VAE system to bridge
 *
 * @param bridge Bridge instance
 * @param vae VAE system to connect
 * @return 0 on success, -1 on error
 */
int vae_immune_bridge_connect_vae(vae_immune_bridge_t* bridge, vae_system_t* vae);

/**
 * @brief Connect immune system to bridge
 *
 * @param bridge Bridge instance
 * @param immune Immune system to connect
 * @return 0 on success, -1 on error
 */
int vae_immune_bridge_connect_immune(vae_immune_bridge_t* bridge,
                                      brain_immune_system_t* immune);

/**
 * @brief Disconnect both systems
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_immune_bridge_disconnect(vae_immune_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge instance
 * @return true if both VAE and immune are connected
 */
bool vae_immune_bridge_is_connected(const vae_immune_bridge_t* bridge);

/* ============================================================================
 * Anomaly Detection API
 * ============================================================================ */

/**
 * @brief Check VAE output for anomalies
 *
 * Examines reconstruction error, KL divergence, latent state
 * and reports anomalies to immune system if thresholds exceeded.
 *
 * @param bridge Bridge instance
 * @param loss VAE loss from forward pass
 * @return Number of anomalies detected, -1 on error
 */
int vae_immune_check_anomalies(vae_immune_bridge_t* bridge,
                                const vae_loss_t* loss);

/**
 * @brief Check latent state for anomalies
 *
 * @param bridge Bridge instance
 * @param latent Latent state to check
 * @return Number of anomalies detected, -1 on error
 */
int vae_immune_check_latent(vae_immune_bridge_t* bridge,
                             const vae_latent_state_t* latent);

/**
 * @brief Manually report an anomaly
 *
 * @param bridge Bridge instance
 * @param type Anomaly type
 * @param severity Severity level
 * @param value Measured value
 * @return 0 on success, -1 on error
 */
int vae_immune_report_anomaly(vae_immune_bridge_t* bridge,
                               vae_anomaly_type_t type,
                               vae_anomaly_severity_t severity,
                               float value);

/**
 * @brief Check for out-of-distribution sample
 *
 * Uses latent space distance from learned distribution.
 *
 * @param bridge Bridge instance
 * @param latent Latent representation to check
 * @param ood_score Output OOD score (higher = more anomalous)
 * @return true if OOD, false if in-distribution
 */
bool vae_immune_check_ood(vae_immune_bridge_t* bridge,
                           const nimcp_tensor_t* latent,
                           float* ood_score);

/* ============================================================================
 * Immune Integration API
 * ============================================================================ */

/**
 * @brief Present anomaly as antigen to immune system
 *
 * @param bridge Bridge instance
 * @param type Anomaly type
 * @param severity Severity (converted to immune severity)
 * @param epitope Anomaly signature (e.g., latent representation)
 * @param epitope_len Signature length
 * @return Antigen ID on success, 0 on failure
 */
uint32_t vae_immune_present_antigen(vae_immune_bridge_t* bridge,
                                     vae_anomaly_type_t type,
                                     vae_anomaly_severity_t severity,
                                     const uint8_t* epitope,
                                     size_t epitope_len);

/**
 * @brief Send danger signal (cytokine) to immune system
 *
 * Called when free energy exceeds danger threshold.
 *
 * @param bridge Bridge instance
 * @param free_energy Current free energy value
 * @return 0 on success, -1 on error
 */
int vae_immune_send_danger_signal(vae_immune_bridge_t* bridge, float free_energy);

/**
 * @brief Handle immune response callback
 *
 * Called when immune system issues response affecting VAE.
 *
 * @param bridge Bridge instance
 * @param response Response type
 * @param strength Response strength (0-1)
 * @return 0 on success, -1 on error
 */
int vae_immune_handle_response(vae_immune_bridge_t* bridge,
                                vae_immune_response_t response,
                                float strength);

/* ============================================================================
 * Threshold API
 * ============================================================================ */

/**
 * @brief Update detection thresholds
 *
 * @param bridge Bridge instance
 * @param thresholds New thresholds
 * @return 0 on success, -1 on error
 */
int vae_immune_set_thresholds(vae_immune_bridge_t* bridge,
                               const vae_immune_thresholds_t* thresholds);

/**
 * @brief Get current thresholds
 *
 * @param bridge Bridge instance
 * @param thresholds Output thresholds
 * @return 0 on success, -1 on error
 */
int vae_immune_get_thresholds(const vae_immune_bridge_t* bridge,
                               vae_immune_thresholds_t* thresholds);

/**
 * @brief Adapt thresholds based on baseline statistics
 *
 * @param bridge Bridge instance
 * @param num_std_devs Number of standard deviations for threshold
 * @return 0 on success, -1 on error
 */
int vae_immune_adapt_thresholds(vae_immune_bridge_t* bridge, float num_std_devs);

/* ============================================================================
 * Baseline API
 * ============================================================================ */

/**
 * @brief Update baseline statistics from current state
 *
 * @param bridge Bridge instance
 * @param latent Current latent state
 * @return 0 on success, -1 on error
 */
int vae_immune_update_baseline(vae_immune_bridge_t* bridge,
                                const vae_latent_state_t* latent);

/**
 * @brief Reset baseline statistics
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_immune_reset_baseline(vae_immune_bridge_t* bridge);

/**
 * @brief Check if baseline is established
 *
 * @param bridge Bridge instance
 * @return true if baseline ready
 */
bool vae_immune_has_baseline(const vae_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int vae_immune_bridge_get_stats(const vae_immune_bridge_t* bridge,
                                 vae_immune_bridge_stats_t* stats);

/**
 * @brief Get bridge health
 *
 * @param bridge Bridge instance
 * @param health Output health metrics
 * @return 0 on success, -1 on error
 */
int vae_immune_bridge_get_health(const vae_immune_bridge_t* bridge,
                                  vae_immune_bridge_health_t* health);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @return Current state
 */
vae_immune_bridge_state_t vae_immune_bridge_get_state(const vae_immune_bridge_t* bridge);

/**
 * @brief Get anomaly history
 *
 * @param bridge Bridge instance
 * @param records Output array
 * @param max_records Maximum records to retrieve
 * @return Number of records retrieved
 */
int vae_immune_get_anomaly_history(const vae_immune_bridge_t* bridge,
                                    vae_anomaly_record_t* records,
                                    uint32_t max_records);

/**
 * @brief Get recent anomaly count
 *
 * @param bridge Bridge instance
 * @param window_ms Time window in milliseconds
 * @return Number of anomalies in window
 */
uint32_t vae_immune_get_recent_anomaly_count(const vae_immune_bridge_t* bridge,
                                              uint64_t window_ms);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main bridge update (call each processing cycle)
 *
 * @param bridge Bridge instance
 * @param delta_ms Milliseconds since last update
 * @return 0 on success, -1 on error
 */
int vae_immune_bridge_update(vae_immune_bridge_t* bridge, uint64_t delta_ms);

/**
 * @brief Process pending immune responses
 *
 * @param bridge Bridge instance
 * @return Number of responses processed, -1 on error
 */
int vae_immune_process_responses(vae_immune_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert VAE anomaly type to string
 *
 * @param type Anomaly type
 * @return String representation
 */
const char* vae_anomaly_type_to_string(vae_anomaly_type_t type);

/**
 * @brief Convert VAE severity to immune severity
 *
 * @param severity VAE severity
 * @return Immune severity (1-10)
 */
uint32_t vae_severity_to_immune_severity(vae_anomaly_severity_t severity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_IMMUNE_BRIDGE_H */
