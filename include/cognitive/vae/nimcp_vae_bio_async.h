/**
 * @file nimcp_vae_bio_async.h
 * @brief Bio-Async Message Types and Handlers for VAE System
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Defines bio-async message types, payloads, and handler registration
 *       for the VAE cognitive system
 *
 * WHY:  Bio-async messaging enables:
 *       - Decoupled communication between VAE and other brain modules
 *       - Neuromodulator-channel prioritization (dopamine for rewards, etc.)
 *       - Phase-synchronized multi-module coordination
 *       - Health monitoring and fault tolerance
 *
 * HOW:  Messages use 0x1F00-0x1FFF range for VAE operations:
 *       - 0x1F00-0x1F0F: Core VAE operations (encode, decode, sample)
 *       - 0x1F10-0x1F1F: VAE-FEP integration messages
 *       - 0x1F20-0x1F2F: VAE-Cognitive bridge messages
 *       - 0x1F30-0x1F3F: VAE-Neural integration messages
 *       - 0x1F40-0x1F4F: VAE health/monitoring messages
 *
 * BIO_MODULE: 0x1F00 (VAE Bio-Async Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_BIO_ASYNC_H
#define NIMCP_VAE_BIO_ASYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "async/nimcp_bio_router.h"
#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_BIO_ASYNC_VERSION       "1.0.0"
#define BIO_MODULE_VAE_BIO_ASYNC    0x1F00

/** Default update interval */
#define VAE_BIO_ASYNC_UPDATE_INTERVAL_MS    100

/** Maximum latent dimensions in messages */
#define VAE_BIO_ASYNC_MAX_LATENT_DIM        256

/** Maximum input dimensions in messages */
#define VAE_BIO_ASYNC_MAX_INPUT_DIM         4096

/** Error codes (32570-32579) */
#define NIMCP_ERROR_VAE_BIO_ASYNC_BASE          32570
#define NIMCP_ERROR_VAE_BIO_ASYNC_NULL          32571
#define NIMCP_ERROR_VAE_BIO_ASYNC_NOT_CONNECTED 32572
#define NIMCP_ERROR_VAE_BIO_ASYNC_SEND_FAILED   32573
#define NIMCP_ERROR_VAE_BIO_ASYNC_HANDLER_FAIL  32574
#define NIMCP_ERROR_VAE_BIO_ASYNC_TIMEOUT       32575

/* ============================================================================
 * Message Type Enumerations (0x1F00 Range)
 * ============================================================================ */

/**
 * @brief VAE bio-async message types
 */
typedef enum {
    /* Core VAE Operations (0x1F00-0x1F0F) */
    BIO_MSG_VAE_ENCODE_REQUEST      = 0x1F00,  /**< Request VAE encoding */
    BIO_MSG_VAE_ENCODE_RESPONSE     = 0x1F01,  /**< Encoding result */
    BIO_MSG_VAE_DECODE_REQUEST      = 0x1F02,  /**< Request VAE decoding */
    BIO_MSG_VAE_DECODE_RESPONSE     = 0x1F03,  /**< Decoding result */
    BIO_MSG_VAE_SAMPLE_REQUEST      = 0x1F04,  /**< Request latent sampling */
    BIO_MSG_VAE_SAMPLE_RESPONSE     = 0x1F05,  /**< Sampling result */
    BIO_MSG_VAE_RECONSTRUCT_REQUEST = 0x1F06,  /**< Request reconstruction */
    BIO_MSG_VAE_RECONSTRUCT_RESPONSE= 0x1F07,  /**< Reconstruction result */
    BIO_MSG_VAE_INTERPOLATE_REQUEST = 0x1F08,  /**< Request latent interpolation */
    BIO_MSG_VAE_INTERPOLATE_RESPONSE= 0x1F09,  /**< Interpolation result */
    BIO_MSG_VAE_STATE_UPDATE        = 0x1F0A,  /**< VAE state changed */
    BIO_MSG_VAE_TRAINING_UPDATE     = 0x1F0B,  /**< Training metrics update */

    /* VAE-FEP Integration (0x1F10-0x1F1F) */
    BIO_MSG_VAE_FEP_SYNC_REQUEST    = 0x1F10,  /**< Request VAE-FEP sync */
    BIO_MSG_VAE_FEP_SYNC_RESPONSE   = 0x1F11,  /**< Sync result */
    BIO_MSG_VAE_FEP_LATENT_TO_BELIEF= 0x1F12,  /**< Latent → belief mapping */
    BIO_MSG_VAE_FEP_BELIEF_TO_LATENT= 0x1F13,  /**< Belief → latent mapping */
    BIO_MSG_VAE_FEP_FREE_ENERGY     = 0x1F14,  /**< Free energy computed */
    BIO_MSG_VAE_FEP_PREDICTION_ERROR= 0x1F15,  /**< Prediction error signal */
    BIO_MSG_VAE_FEP_PRECISION_UPDATE= 0x1F16,  /**< Precision weights update */

    /* VAE-Cognitive Bridges (0x1F20-0x1F2F) */
    BIO_MSG_VAE_HIPPOCAMPUS_ENCODE  = 0x1F20,  /**< Encode to hippocampus */
    BIO_MSG_VAE_HIPPOCAMPUS_RETRIEVE= 0x1F21,  /**< Retrieve from hippocampus */
    BIO_MSG_VAE_IMAGINATION_GENERATE= 0x1F22,  /**< Generate imagination */
    BIO_MSG_VAE_VISUAL_ENCODE       = 0x1F23,  /**< Encode visual input */
    BIO_MSG_VAE_AUDITORY_ENCODE     = 0x1F24,  /**< Encode auditory input */
    BIO_MSG_VAE_EMOTION_MODULATE    = 0x1F25,  /**< Emotion modulation */
    BIO_MSG_VAE_INTROSPECTION_STATE = 0x1F26,  /**< Introspection state */
    BIO_MSG_VAE_WORLD_MODEL_PREDICT = 0x1F27,  /**< World model prediction */

    /* VAE-Neural Integration (0x1F30-0x1F3F) */
    BIO_MSG_VAE_SNN_ENCODE          = 0x1F30,  /**< Encode latent to spikes */
    BIO_MSG_VAE_SNN_DECODE          = 0x1F31,  /**< Decode spikes to latent */
    BIO_MSG_VAE_PLASTICITY_MODULATE = 0x1F32,  /**< Plasticity modulation signal */
    BIO_MSG_VAE_TRAINING_STEP       = 0x1F33,  /**< Training step request */
    BIO_MSG_VAE_SUBSTRATE_STATE     = 0x1F34,  /**< Substrate metabolic state */
    BIO_MSG_VAE_THALAMIC_RELAY      = 0x1F35,  /**< Thalamic relay operation */
    BIO_MSG_VAE_ATTENTION_GATE      = 0x1F36,  /**< Attention gating signal */

    /* VAE Health/Monitoring (0x1F40-0x1F4F) */
    BIO_MSG_VAE_HEARTBEAT           = 0x1F40,  /**< VAE heartbeat */
    BIO_MSG_VAE_HEALTH_STATUS       = 0x1F41,  /**< Health status report */
    BIO_MSG_VAE_ANOMALY_DETECTED    = 0x1F42,  /**< Anomaly detected */
    BIO_MSG_VAE_COLLAPSE_WARNING    = 0x1F43,  /**< Posterior collapse warning */
    BIO_MSG_VAE_VARIANCE_ALERT      = 0x1F44,  /**< Variance explosion alert */
    BIO_MSG_VAE_GRADIENT_ALERT      = 0x1F45,  /**< Gradient NaN/Inf alert */
    BIO_MSG_VAE_RECOVERY_EVENT      = 0x1F46,  /**< Recovery from error */
    BIO_MSG_VAE_METRICS_REPORT      = 0x1F47,  /**< Periodic metrics report */

    /* Bridge-specific messages (0x1F50-0x1F5F) */
    BIO_MSG_VAE_IMMUNE_ANOMALY      = 0x1F50,  /**< Immune anomaly detection */
    BIO_MSG_VAE_BBB_VALIDATION      = 0x1F51,  /**< BBB validation result */
    BIO_MSG_VAE_LOGGING_EVENT       = 0x1F52,  /**< Logging event */

    BIO_MSG_VAE_MAX                 = 0x1F5F   /**< End of VAE message range */
} vae_bio_message_type_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Common message header
 */
typedef struct {
    vae_bio_message_type_t type;
    uint32_t sequence_id;
    uint64_t timestamp_us;
    bio_module_id_t source_module;
    uint8_t priority;
    uint8_t channel;              /**< Neuromodulator channel */
} vae_bio_msg_header_t;

/* --- Core VAE Messages --- */

/**
 * @brief Encode request payload
 */
typedef struct {
    vae_bio_msg_header_t header;
    float input[VAE_BIO_ASYNC_MAX_INPUT_DIM];
    uint32_t input_dim;
    bool return_full_result;      /**< Include mu, logvar in response */
} vae_bio_msg_encode_request_t;

/**
 * @brief Encode response payload
 */
typedef struct {
    vae_bio_msg_header_t header;
    float latent_mu[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    float latent_log_var[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    float latent_sample[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    uint32_t latent_dim;
    float encoding_time_us;
    int32_t error_code;           /**< 0 on success */
} vae_bio_msg_encode_response_t;

/**
 * @brief Decode request payload
 */
typedef struct {
    vae_bio_msg_header_t header;
    float latent[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    uint32_t latent_dim;
} vae_bio_msg_decode_request_t;

/**
 * @brief Decode response payload
 */
typedef struct {
    vae_bio_msg_header_t header;
    float output[VAE_BIO_ASYNC_MAX_INPUT_DIM];
    uint32_t output_dim;
    float reconstruction_loss;
    float decoding_time_us;
    int32_t error_code;
} vae_bio_msg_decode_response_t;

/**
 * @brief Sample request payload
 */
typedef struct {
    vae_bio_msg_header_t header;
    uint32_t num_samples;
    float temperature;            /**< Sampling temperature */
    bool from_prior;              /**< Sample from prior or posterior */
} vae_bio_msg_sample_request_t;

/**
 * @brief Sample response payload
 */
typedef struct {
    vae_bio_msg_header_t header;
    float samples[VAE_BIO_ASYNC_MAX_LATENT_DIM];  /**< First sample */
    uint32_t latent_dim;
    uint32_t num_samples;
    float sampling_time_us;
    int32_t error_code;
} vae_bio_msg_sample_response_t;

/* --- VAE-FEP Messages --- */

/**
 * @brief Free energy computation result
 */
typedef struct {
    vae_bio_msg_header_t header;
    float free_energy;
    float inaccuracy;             /**< Reconstruction term */
    float complexity;             /**< KL divergence term */
    float beta;                   /**< Current beta weight */
    float elbo;                   /**< Evidence lower bound */
} vae_bio_msg_free_energy_t;

/**
 * @brief Prediction error signal
 */
typedef struct {
    vae_bio_msg_header_t header;
    float prediction_error[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    uint32_t dim;
    float mean_error;
    float max_error;
    bool significant;             /**< Error exceeds threshold */
} vae_bio_msg_prediction_error_t;

/**
 * @brief Precision update
 */
typedef struct {
    vae_bio_msg_header_t header;
    float precision[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    uint32_t dim;
    float mean_precision;
    bool precision_weighted;
} vae_bio_msg_precision_update_t;

/* --- VAE-Neural Messages --- */

/**
 * @brief SNN encode message (latent → spikes)
 */
typedef struct {
    vae_bio_msg_header_t header;
    float latent[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    uint32_t latent_dim;
    float window_ms;
    uint32_t encode_method;       /**< Rate, temporal, burst, etc. */
} vae_bio_msg_snn_encode_t;

/**
 * @brief Plasticity modulation signal
 */
typedef struct {
    vae_bio_msg_header_t header;
    float learning_rate_mod;
    float threshold_mod;
    float scaling_factor;
    float recon_error;
    float kl_divergence;
    float novelty_score;
    bool plasticity_enabled;
} vae_bio_msg_plasticity_modulate_t;

/**
 * @brief Substrate state update
 */
typedef struct {
    vae_bio_msg_header_t header;
    float atp_level;
    float o2_saturation;
    float glucose_level;
    float ion_balance;
    float temperature_c;
    uint32_t health_status;       /**< vae_substrate_health_t */
    uint32_t effective_latent_dim;
    float energy_efficiency;
} vae_bio_msg_substrate_state_t;

/**
 * @brief Thalamic relay request
 */
typedef struct {
    vae_bio_msg_header_t header;
    float latent[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    uint32_t latent_dim;
    uint32_t nucleus;             /**< vae_thalamic_nucleus_t */
    float attention_level;
    uint32_t mode;                /**< vae_thalamic_mode_t */
} vae_bio_msg_thalamic_relay_t;

/* --- Health/Monitoring Messages --- */

/**
 * @brief VAE heartbeat
 */
typedef struct {
    vae_bio_msg_header_t header;
    uint64_t heartbeat_count;
    float health_score;           /**< 0-1 health metric */
    uint32_t state;               /**< vae_state_t */
    uint64_t uptime_us;
} vae_bio_msg_heartbeat_t;

/**
 * @brief Health status report
 */
typedef struct {
    vae_bio_msg_header_t header;
    float avg_recon_error;
    float avg_kl_divergence;
    float avg_elbo;
    uint64_t total_encodes;
    uint64_t total_decodes;
    uint64_t error_count;
    bool is_healthy;
    char status_message[64];
} vae_bio_msg_health_status_t;

/**
 * @brief Anomaly detection message
 */
typedef struct {
    vae_bio_msg_header_t header;
    uint32_t anomaly_type;        /**< Type of anomaly detected */
    float anomaly_score;          /**< Severity 0-1 */
    float input_likelihood;       /**< Log-likelihood of input */
    float latent_distance;        /**< Distance from mean */
    char description[128];
} vae_bio_msg_anomaly_t;

/**
 * @brief Metrics report
 */
typedef struct {
    vae_bio_msg_header_t header;
    float avg_encode_time_us;
    float avg_decode_time_us;
    float throughput_ops_sec;
    float memory_usage_mb;
    uint64_t messages_sent;
    uint64_t messages_received;
} vae_bio_msg_metrics_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief VAE bio-async bridge configuration
 */
typedef struct {
    /* Channel assignments */
    nimcp_bio_channel_type_t encode_channel;    /**< Encoding operations */
    nimcp_bio_channel_type_t decode_channel;    /**< Decoding operations */
    nimcp_bio_channel_type_t alert_channel;     /**< Alerts (anomaly, collapse) */
    nimcp_bio_channel_type_t health_channel;    /**< Health/heartbeat */

    /* Timing */
    uint32_t heartbeat_interval_ms;
    uint32_t metrics_interval_ms;
    uint32_t health_check_interval_ms;

    /* Priorities (0-15, higher = more urgent) */
    uint8_t encode_priority;
    uint8_t decode_priority;
    uint8_t alert_priority;
    uint8_t health_priority;

    /* Message buffering */
    uint32_t inbox_capacity;
    uint32_t outbox_capacity;
    bool batch_encode_requests;
    uint32_t batch_threshold;

    /* Features */
    bool enable_anomaly_detection;
    bool enable_collapse_detection;
    bool enable_metrics_reporting;
    bool enable_logging;
} vae_bio_async_config_t;

/* ============================================================================
 * Bridge State
 * ============================================================================ */

/**
 * @brief VAE bio-async bridge state
 */
typedef enum {
    VAE_BIO_ASYNC_DISCONNECTED = 0,
    VAE_BIO_ASYNC_CONNECTING,
    VAE_BIO_ASYNC_CONNECTED,
    VAE_BIO_ASYNC_PROCESSING,
    VAE_BIO_ASYNC_ERROR
} vae_bio_async_state_t;

/**
 * @brief VAE bio-async bridge statistics
 */
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t handler_errors;
    uint64_t heartbeats_sent;
    float avg_latency_us;
    float max_latency_us;
    uint64_t creation_time_us;
    uint64_t last_activity_us;
} vae_bio_async_stats_t;

/**
 * @brief Main VAE bio-async bridge structure
 */
typedef struct vae_bio_async_bridge {
    vae_bio_async_config_t config;
    vae_system_t* vae;
    bio_module_context_t bio_context;
    vae_bio_async_state_t state;
    bool is_initialized;

    /* Message sequence tracking */
    uint32_t next_sequence_id;

    /* Pending response tracking */
    void* pending_responses;      /**< Hash map of pending responses */
    uint32_t pending_count;

    /* Handler registrations */
    bool handlers_registered;

    /* Statistics */
    vae_bio_async_stats_t stats;

    /* Heartbeat timer */
    uint64_t last_heartbeat_us;
    uint64_t heartbeat_count;

    uint64_t creation_time_us;
} vae_bio_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bio-async configuration
 */
int vae_bio_async_default_config(vae_bio_async_config_t* config);

/**
 * @brief Create VAE bio-async bridge
 */
vae_bio_async_bridge_t* vae_bio_async_create(const vae_bio_async_config_t* config);

/**
 * @brief Destroy VAE bio-async bridge
 */
void vae_bio_async_destroy(vae_bio_async_bridge_t* bridge);

/**
 * @brief Connect bridge to VAE system
 */
int vae_bio_async_connect_vae(vae_bio_async_bridge_t* bridge, vae_system_t* vae);

/**
 * @brief Connect to bio-async router
 */
int vae_bio_async_connect_router(vae_bio_async_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int vae_bio_async_disconnect(vae_bio_async_bridge_t* bridge);

/**
 * @brief Check if connected
 */
bool vae_bio_async_is_connected(const vae_bio_async_bridge_t* bridge);

/* ============================================================================
 * Handler Registration API
 * ============================================================================ */

/**
 * @brief Register all VAE message handlers
 */
int vae_bio_async_register_handlers(vae_bio_async_bridge_t* bridge);

/**
 * @brief Unregister all VAE message handlers
 */
int vae_bio_async_unregister_handlers(vae_bio_async_bridge_t* bridge);

/**
 * @brief Register handler for specific message type
 */
int vae_bio_async_register_handler(vae_bio_async_bridge_t* bridge,
                                    vae_bio_message_type_t msg_type,
                                    bio_message_handler_t handler,
                                    void* user_data);

/* ============================================================================
 * Message Sending API
 * ============================================================================ */

/**
 * @brief Send encode request
 */
int vae_bio_async_send_encode_request(vae_bio_async_bridge_t* bridge,
                                       const float* input, uint32_t input_dim,
                                       nimcp_bio_promise_t* promise);

/**
 * @brief Send decode request
 */
int vae_bio_async_send_decode_request(vae_bio_async_bridge_t* bridge,
                                       const float* latent, uint32_t latent_dim,
                                       nimcp_bio_promise_t* promise);

/**
 * @brief Send sample request
 */
int vae_bio_async_send_sample_request(vae_bio_async_bridge_t* bridge,
                                       uint32_t num_samples,
                                       float temperature,
                                       nimcp_bio_promise_t* promise);

/**
 * @brief Send free energy update
 */
int vae_bio_async_send_free_energy(vae_bio_async_bridge_t* bridge,
                                    float free_energy,
                                    float inaccuracy,
                                    float complexity);

/**
 * @brief Send anomaly detection
 */
int vae_bio_async_send_anomaly(vae_bio_async_bridge_t* bridge,
                                uint32_t anomaly_type,
                                float anomaly_score,
                                const char* description);

/**
 * @brief Send heartbeat
 */
int vae_bio_async_send_heartbeat(vae_bio_async_bridge_t* bridge);

/**
 * @brief Send health status
 */
int vae_bio_async_send_health_status(vae_bio_async_bridge_t* bridge);

/**
 * @brief Send metrics report
 */
int vae_bio_async_send_metrics(vae_bio_async_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages
 */
uint32_t vae_bio_async_process_messages(vae_bio_async_bridge_t* bridge,
                                         uint32_t max_messages);

/**
 * @brief Periodic update (heartbeat, metrics)
 */
int vae_bio_async_periodic_update(vae_bio_async_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge state
 */
vae_bio_async_state_t vae_bio_async_get_state(const vae_bio_async_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 */
int vae_bio_async_get_stats(const vae_bio_async_bridge_t* bridge,
                             vae_bio_async_stats_t* stats);

/**
 * @brief Get message type name
 */
const char* vae_bio_message_type_to_string(vae_bio_message_type_t type);

/* ============================================================================
 * Default Message Handlers
 * ============================================================================ */

/**
 * @brief Handle encode request
 */
nimcp_error_t vae_bio_async_handle_encode_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/**
 * @brief Handle decode request
 */
nimcp_error_t vae_bio_async_handle_decode_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/**
 * @brief Handle sample request
 */
nimcp_error_t vae_bio_async_handle_sample_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/**
 * @brief Handle FEP sync request
 */
nimcp_error_t vae_bio_async_handle_fep_sync_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_BIO_ASYNC_H */
