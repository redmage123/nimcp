/**
 * @file nimcp_vae_logging_bridge.h
 * @brief Bridge between VAE and Logging System
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Comprehensive logging integration for VAE operations
 *
 * WHY:  Provides detailed logging for debugging, monitoring, and analysis:
 *       - Training progress tracking
 *       - Loss component logging
 *       - Latent space statistics
 *       - Anomaly event logging
 *       - Performance metrics
 *
 * HOW:  Bridge wraps VAE operations with configurable logging:
 *       - Per-epoch training summaries
 *       - Per-batch detailed logs
 *       - Event-based anomaly logging
 *       - Periodic health reports
 *
 * BIO_MODULE: 0x1F13 (VAE-Logging Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_LOGGING_BRIDGE_H
#define NIMCP_VAE_LOGGING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bridge version */
#define VAE_LOGGING_BRIDGE_VERSION      "1.0.0"

/** Bio-async module ID */
#define BIO_MODULE_VAE_LOGGING_BRIDGE   0x1F13

/** Log categories */
#define VAE_LOG_CATEGORY_TRAINING       "VAE.Training"
#define VAE_LOG_CATEGORY_INFERENCE      "VAE.Inference"
#define VAE_LOG_CATEGORY_LATENT         "VAE.Latent"
#define VAE_LOG_CATEGORY_ANOMALY        "VAE.Anomaly"
#define VAE_LOG_CATEGORY_HEALTH         "VAE.Health"
#define VAE_LOG_CATEGORY_BRIDGE         "VAE.Bridge"

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Log verbosity levels
 */
typedef enum {
    VAE_LOG_VERBOSITY_NONE = 0,      /**< No logging */
    VAE_LOG_VERBOSITY_ERROR,          /**< Errors only */
    VAE_LOG_VERBOSITY_WARN,           /**< Warnings and errors */
    VAE_LOG_VERBOSITY_INFO,           /**< Standard info */
    VAE_LOG_VERBOSITY_DEBUG,          /**< Detailed debug */
    VAE_LOG_VERBOSITY_TRACE           /**< Very detailed trace */
} vae_log_verbosity_t;

/**
 * @brief Log event types
 */
typedef enum {
    VAE_LOG_EVENT_CREATED = 0,
    VAE_LOG_EVENT_DESTROYED,
    VAE_LOG_EVENT_RESET,
    VAE_LOG_EVENT_FORWARD,
    VAE_LOG_EVENT_BACKWARD,
    VAE_LOG_EVENT_TRAIN_START,
    VAE_LOG_EVENT_TRAIN_END,
    VAE_LOG_EVENT_EPOCH_START,
    VAE_LOG_EVENT_EPOCH_END,
    VAE_LOG_EVENT_BATCH,
    VAE_LOG_EVENT_ANOMALY,
    VAE_LOG_EVENT_HEALTH_CHECK,
    VAE_LOG_EVENT_BRIDGE_CONNECTED,
    VAE_LOG_EVENT_BRIDGE_DISCONNECTED,
    VAE_LOG_EVENT_COUNT
} vae_log_event_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Logging configuration
 */
typedef struct {
    vae_log_verbosity_t verbosity;   /**< Overall verbosity */

    /* Category enables */
    bool log_training;               /**< Log training events */
    bool log_inference;              /**< Log inference events */
    bool log_latent;                 /**< Log latent space stats */
    bool log_anomalies;              /**< Log anomaly events */
    bool log_health;                 /**< Log health reports */
    bool log_bridges;                /**< Log bridge events */

    /* Frequency controls */
    uint32_t batch_log_interval;     /**< Log every N batches */
    uint32_t health_log_interval_ms; /**< Health report interval (ms) */

    /* Detail levels */
    bool log_loss_components;        /**< Log individual loss terms */
    bool log_latent_stats;           /**< Log latent mean/var stats */
    bool log_gradient_norms;         /**< Log gradient norms */
    bool log_timing;                 /**< Log operation timing */

    /* Output options */
    bool include_timestamp;          /**< Include timestamps */
    bool include_batch_info;         /**< Include batch/epoch info */
    bool format_json;                /**< Output as JSON */
} vae_logging_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Logging statistics
 */
typedef struct {
    uint64_t total_logs;
    uint64_t error_logs;
    uint64_t warning_logs;
    uint64_t info_logs;
    uint64_t debug_logs;
    uint64_t suppressed_logs;        /**< Logs suppressed by verbosity */
    uint64_t training_logs;
    uint64_t anomaly_logs;
    uint64_t health_logs;
} vae_logging_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief VAE-Logging bridge instance
 */
typedef struct vae_logging_bridge {
    vae_logging_config_t config;
    vae_system_t* vae;
    bool is_initialized;

    /* Training state tracking */
    uint32_t current_epoch;
    uint32_t current_batch;
    uint64_t train_start_time_us;

    /* Statistics */
    vae_logging_stats_t stats;

    /* Health logging state */
    uint64_t last_health_log_us;

    /* Timing */
    uint64_t creation_time_us;
} vae_logging_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default logging configuration
 */
int vae_logging_bridge_default_config(vae_logging_config_t* config);

/**
 * @brief Create VAE-Logging bridge
 */
vae_logging_bridge_t* vae_logging_bridge_create(const vae_logging_config_t* config);

/**
 * @brief Destroy VAE-Logging bridge
 */
void vae_logging_bridge_destroy(vae_logging_bridge_t* bridge);

/**
 * @brief Connect VAE system to bridge
 */
int vae_logging_bridge_connect_vae(vae_logging_bridge_t* bridge, vae_system_t* vae);

/**
 * @brief Disconnect VAE system
 */
int vae_logging_bridge_disconnect(vae_logging_bridge_t* bridge);

/* ============================================================================
 * Event Logging API
 * ============================================================================ */

/**
 * @brief Log VAE event
 */
void vae_log_event(vae_logging_bridge_t* bridge, vae_log_event_t event,
                   const char* format, ...);

/**
 * @brief Log training start
 */
void vae_log_train_start(vae_logging_bridge_t* bridge,
                         uint32_t num_epochs, uint32_t batch_size);

/**
 * @brief Log training end
 */
void vae_log_train_end(vae_logging_bridge_t* bridge,
                       float final_loss, uint64_t total_time_ms);

/**
 * @brief Log epoch start
 */
void vae_log_epoch_start(vae_logging_bridge_t* bridge, uint32_t epoch);

/**
 * @brief Log epoch end with summary
 */
void vae_log_epoch_end(vae_logging_bridge_t* bridge, uint32_t epoch,
                       float avg_loss, float avg_recon, float avg_kl);

/**
 * @brief Log batch with loss details
 */
void vae_log_batch(vae_logging_bridge_t* bridge, uint32_t batch,
                   const vae_loss_t* loss, float learning_rate);

/**
 * @brief Log forward pass
 */
void vae_log_forward(vae_logging_bridge_t* bridge,
                     uint32_t input_dim, uint32_t latent_dim,
                     float recon_error, uint64_t time_us);

/**
 * @brief Log backward pass
 */
void vae_log_backward(vae_logging_bridge_t* bridge,
                      float gradient_norm, uint64_t time_us);

/* ============================================================================
 * Latent Space Logging
 * ============================================================================ */

/**
 * @brief Log latent space statistics
 */
void vae_log_latent_stats(vae_logging_bridge_t* bridge,
                          const vae_latent_state_t* latent);

/**
 * @brief Log latent space histogram (per-dimension)
 */
void vae_log_latent_histogram(vae_logging_bridge_t* bridge,
                              const vae_latent_state_t* latent,
                              uint32_t num_bins);

/* ============================================================================
 * Anomaly Logging
 * ============================================================================ */

/**
 * @brief Log anomaly detection event
 */
void vae_log_anomaly(vae_logging_bridge_t* bridge,
                     const char* anomaly_type,
                     float severity,
                     float value,
                     float threshold);

/**
 * @brief Log out-of-distribution detection
 */
void vae_log_ood(vae_logging_bridge_t* bridge,
                 float ood_score,
                 bool is_ood);

/* ============================================================================
 * Health Logging
 * ============================================================================ */

/**
 * @brief Log health report
 */
void vae_log_health(vae_logging_bridge_t* bridge,
                    float health_score,
                    uint32_t consecutive_failures,
                    const char* status);

/**
 * @brief Periodic health check (call from main loop)
 */
void vae_log_health_check(vae_logging_bridge_t* bridge);

/* ============================================================================
 * Bridge Event Logging
 * ============================================================================ */

/**
 * @brief Log bridge connection event
 */
void vae_log_bridge_connected(vae_logging_bridge_t* bridge,
                               const char* bridge_name);

/**
 * @brief Log bridge disconnection event
 */
void vae_log_bridge_disconnected(vae_logging_bridge_t* bridge,
                                  const char* bridge_name);

/**
 * @brief Log bridge sync event
 */
void vae_log_bridge_sync(vae_logging_bridge_t* bridge,
                          const char* bridge_name,
                          const char* direction,
                          uint64_t time_us);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Set verbosity level
 */
int vae_logging_set_verbosity(vae_logging_bridge_t* bridge,
                               vae_log_verbosity_t verbosity);

/**
 * @brief Enable/disable category
 */
int vae_logging_set_category(vae_logging_bridge_t* bridge,
                              const char* category,
                              bool enabled);

/**
 * @brief Get logging statistics
 */
int vae_logging_get_stats(const vae_logging_bridge_t* bridge,
                           vae_logging_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_LOGGING_BRIDGE_H */
