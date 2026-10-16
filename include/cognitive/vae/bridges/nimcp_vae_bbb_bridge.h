/**
 * @file nimcp_vae_bbb_bridge.h
 * @brief Bridge between VAE and Blood-Brain Barrier
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Security integration between VAE and BBB perimeter defense
 *
 * WHY:  VAE processes input data that must be validated before entering
 *       the cognitive system. Integration with BBB enables:
 *       - Input validation before VAE encoding
 *       - Output sanitization after VAE decoding
 *       - Anomaly-based threat detection
 *       - Protection against adversarial inputs
 *
 * HOW:  Bridge intercepts VAE I/O for BBB validation:
 *       - Input gate: Validate before encoder
 *       - Output gate: Sanitize decoder output
 *       - Latent gate: Validate latent representations
 *       - Threat reporting: Report anomalies to BBB
 *
 * MAPPING:
 * ```
 *   VAE Component            | BBB Component
 *   -------------------------|--------------------
 *   Input tensor             | Input validation gate
 *   Latent representation    | Memory boundary monitor
 *   Reconstructed output     | Output sanitization
 *   Adversarial detection    | Threat detection
 *   Anomaly score            | Severity assessment
 * ```
 *
 * ARCHITECTURE:
 * ```
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │                         VAE-BBB BRIDGE                                  │
 *   │                                                                         │
 *   │   External Input                                                        │
 *   │        │                                                                │
 *   │        ▼                                                                │
 *   │   ┌─────────────┐     ┌───────────────┐                                │
 *   │   │ BBB Input   │────▶│  VAE Encoder  │                                │
 *   │   │ Validation  │     │               │                                │
 *   │   └─────────────┘     └───────┬───────┘                                │
 *   │        │                      │                                         │
 *   │        │ blocked              ▼                                         │
 *   │        ▼              ┌───────────────┐                                │
 *   │   [Threat Report]     │ Latent Space  │                                │
 *   │                       │   z ~ q(z|x)  │                                │
 *   │                       └───────┬───────┘                                │
 *   │                               │                                         │
 *   │   ┌─────────────┐     ┌──────┴────────┐                                │
 *   │   │ BBB Latent  │◀────│ Latent Check  │                                │
 *   │   │ Validation  │     │ (bounds/NaN)  │                                │
 *   │   └─────────────┘     └───────┬───────┘                                │
 *   │                               │                                         │
 *   │                       ┌───────▼───────┐                                │
 *   │                       │  VAE Decoder  │                                │
 *   │                       │               │                                │
 *   │                       └───────┬───────┘                                │
 *   │                               │                                         │
 *   │   ┌─────────────┐     ┌──────┴────────┐                                │
 *   │   │ BBB Output  │◀────│ Output Check  │                                │
 *   │   │ Sanitization│     │ & Sanitize    │                                │
 *   │   └─────────────┘     └───────┬───────┘                                │
 *   │                               │                                         │
 *   │                               ▼                                         │
 *   │                        Safe Output                                      │
 *   │                                                                         │
 *   └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIO_MODULE: 0x1F12 (VAE-BBB Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_BBB_BRIDGE_H
#define NIMCP_VAE_BBB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bridge version */
#define VAE_BBB_BRIDGE_VERSION          "1.0.0"

/** Bio-async module ID */
#define BIO_MODULE_VAE_BBB_BRIDGE       0x1F12

/** Default thresholds */
#define VAE_BBB_DEFAULT_LATENT_MAX      100.0f  /**< Max latent value magnitude */
#define VAE_BBB_DEFAULT_OUTPUT_MAX      1.0f    /**< Max output value (normalized) */
#define VAE_BBB_DEFAULT_GRADIENT_MAX    1000.0f /**< Max gradient magnitude */

/** Validation modes */
#define VAE_BBB_VALIDATE_INPUT          0x01
#define VAE_BBB_VALIDATE_LATENT         0x02
#define VAE_BBB_VALIDATE_OUTPUT         0x04
#define VAE_BBB_VALIDATE_GRADIENT       0x08
#define VAE_BBB_VALIDATE_ALL            0x0F

/* ============================================================================
 * Error Codes (32440-32449 range for BBB bridge)
 * ============================================================================ */

#define NIMCP_ERROR_VAE_BBB_BASE            32440
#define NIMCP_ERROR_VAE_BBB_NULL_BRIDGE     32441
#define NIMCP_ERROR_VAE_BBB_NOT_CONNECTED   32442
#define NIMCP_ERROR_VAE_BBB_NO_VAE          32443
#define NIMCP_ERROR_VAE_BBB_NO_BBB          32444
#define NIMCP_ERROR_VAE_BBB_VALIDATION_FAIL 32445
#define NIMCP_ERROR_VAE_BBB_BLOCKED         32446
#define NIMCP_ERROR_VAE_BBB_ADVERSARIAL     32447

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Validation stages
 */
typedef enum {
    VAE_BBB_STAGE_INPUT = 0,         /**< Input validation */
    VAE_BBB_STAGE_LATENT,            /**< Latent space validation */
    VAE_BBB_STAGE_OUTPUT,            /**< Output validation */
    VAE_BBB_STAGE_GRADIENT           /**< Gradient validation */
} vae_bbb_stage_t;

/**
 * @brief Validation result
 */
typedef enum {
    VAE_BBB_RESULT_PASS = 0,         /**< Validation passed */
    VAE_BBB_RESULT_SANITIZED,        /**< Passed after sanitization */
    VAE_BBB_RESULT_BLOCKED,          /**< Blocked - threat detected */
    VAE_BBB_RESULT_QUARANTINED,      /**< Quarantined for analysis */
    VAE_BBB_RESULT_ERROR             /**< Validation error */
} vae_bbb_result_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    VAE_BBB_STATE_DISCONNECTED = 0,
    VAE_BBB_STATE_CONNECTED,
    VAE_BBB_STATE_VALIDATING,
    VAE_BBB_STATE_BLOCKING,
    VAE_BBB_STATE_ERROR
} vae_bbb_bridge_state_t;

/**
 * @brief Threat types specific to VAE
 */
typedef enum {
    VAE_THREAT_NONE = 0,
    VAE_THREAT_ADVERSARIAL_INPUT,    /**< Adversarial example detected */
    VAE_THREAT_NAN_INF,              /**< NaN/Inf values */
    VAE_THREAT_OUT_OF_BOUNDS,        /**< Values exceed bounds */
    VAE_THREAT_MALFORMED_TENSOR,     /**< Invalid tensor structure */
    VAE_THREAT_GRADIENT_ATTACK,      /**< Gradient-based attack */
    VAE_THREAT_LATENT_INJECTION,     /**< Latent space injection */
    VAE_THREAT_RECONSTRUCTION_ATTACK /**< Reconstruction-based attack */
} vae_bbb_threat_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Validation bounds
 */
typedef struct {
    float input_min;                 /**< Minimum input value */
    float input_max;                 /**< Maximum input value */
    float latent_max;                /**< Maximum latent magnitude */
    float output_min;                /**< Minimum output value */
    float output_max;                /**< Maximum output value */
    float gradient_max;              /**< Maximum gradient magnitude */
} vae_bbb_bounds_t;

/**
 * @brief VAE-BBB bridge configuration
 */
typedef struct {
    /* Validation settings */
    uint32_t validation_mask;        /**< Which stages to validate (VAE_BBB_VALIDATE_*) */
    vae_bbb_bounds_t bounds;         /**< Value bounds */
    bool strict_mode;                /**< Strict validation mode */

    /* Sanitization */
    bool enable_sanitization;        /**< Sanitize instead of block when possible */
    bool clamp_values;               /**< Clamp out-of-bounds values */
    bool replace_nan;                /**< Replace NaN/Inf with defaults */
    float nan_replacement;           /**< Value to replace NaN with */

    /* Adversarial detection */
    bool detect_adversarial;         /**< Enable adversarial detection */
    float adversarial_threshold;     /**< L2 perturbation threshold */
    bool block_adversarial;          /**< Block detected adversarial inputs */

    /* Actions */
    bool report_to_bbb;              /**< Report threats to BBB */
    bool quarantine_threats;         /**< Quarantine threatening data */
    bool log_validations;            /**< Log all validations */

    /* Performance */
    bool async_validation;           /**< Enable async validation */
    uint32_t batch_size;             /**< Batch size for validation */
} vae_bbb_bridge_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Validation result details
 */
typedef struct {
    vae_bbb_result_t result;         /**< Overall result */
    vae_bbb_stage_t stage;           /**< Stage where issue found */
    vae_bbb_threat_t threat;         /**< Threat type if blocked */
    bbb_severity_t severity;         /**< Severity level */
    float threat_score;              /**< Threat score (0-1) */
    bool sanitized;                  /**< Was data sanitized */
    uint32_t violations_found;       /**< Number of violations */
    char reason[128];                /**< Reason for result */
} vae_bbb_validation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Validation counts */
    uint64_t total_validations;
    uint64_t input_validations;
    uint64_t latent_validations;
    uint64_t output_validations;
    uint64_t gradient_validations;

    /* Results */
    uint64_t passed;
    uint64_t sanitized;
    uint64_t blocked;
    uint64_t quarantined;
    uint64_t errors;

    /* Threats by type */
    uint64_t adversarial_detected;
    uint64_t nan_inf_detected;
    uint64_t out_of_bounds;
    uint64_t malformed_tensors;

    /* BBB integration */
    uint64_t reports_to_bbb;
    uint64_t bbb_responses;

    /* Performance */
    float avg_validation_time_us;
    float max_validation_time_us;
    uint64_t uptime_us;
} vae_bbb_bridge_stats_t;

/**
 * @brief Health metrics
 */
typedef struct {
    float bridge_health;             /**< Overall health (0-1) */
    float pass_rate;                 /**< Validation pass rate */
    float threat_rate;               /**< Threat detection rate */
    bool is_healthy;                 /**< Quick health check */
    uint32_t consecutive_blocks;     /**< Consecutive blocks */
    uint32_t last_error_code;        /**< Last error code */
} vae_bbb_bridge_health_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief VAE-BBB bridge instance
 */
typedef struct vae_bbb_bridge {
    /* Configuration */
    vae_bbb_bridge_config_t config;

    /* Connected systems */
    vae_system_t* vae;               /**< Connected VAE system */
    bbb_system_t bbb;                /**< Connected BBB system */

    /* State */
    vae_bbb_bridge_state_t state;
    bool is_initialized;

    /* Last validation */
    vae_bbb_validation_t last_validation;

    /* Statistics */
    vae_bbb_bridge_stats_t stats;

    /* Health */
    vae_bbb_bridge_health_t health;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
} vae_bbb_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, -1 on error
 */
int vae_bbb_bridge_default_config(vae_bbb_bridge_config_t* config);

/**
 * @brief Create VAE-BBB bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on error
 */
vae_bbb_bridge_t* vae_bbb_bridge_create(const vae_bbb_bridge_config_t* config);

/**
 * @brief Destroy VAE-BBB bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void vae_bbb_bridge_destroy(vae_bbb_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_bbb_bridge_reset(vae_bbb_bridge_t* bridge);

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
int vae_bbb_bridge_connect_vae(vae_bbb_bridge_t* bridge, vae_system_t* vae);

/**
 * @brief Connect BBB system to bridge
 *
 * @param bridge Bridge instance
 * @param bbb BBB system to connect
 * @return 0 on success, -1 on error
 */
int vae_bbb_bridge_connect_bbb(vae_bbb_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Disconnect both systems
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_bbb_bridge_disconnect(vae_bbb_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge instance
 * @return true if both VAE and BBB are connected
 */
bool vae_bbb_bridge_is_connected(const vae_bbb_bridge_t* bridge);

/* ============================================================================
 * Validation API
 * ============================================================================ */

/**
 * @brief Validate input tensor before VAE encoding
 *
 * @param bridge Bridge instance
 * @param input Input tensor to validate
 * @param result Output validation result
 * @return VAE_BBB_RESULT_PASS if valid, other result on issues
 */
vae_bbb_result_t vae_bbb_validate_input(vae_bbb_bridge_t* bridge,
                                         const nimcp_tensor_t* input,
                                         vae_bbb_validation_t* result);

/**
 * @brief Validate latent representation
 *
 * @param bridge Bridge instance
 * @param latent Latent tensor to validate
 * @param result Output validation result
 * @return VAE_BBB_RESULT_PASS if valid
 */
vae_bbb_result_t vae_bbb_validate_latent(vae_bbb_bridge_t* bridge,
                                          const nimcp_tensor_t* latent,
                                          vae_bbb_validation_t* result);

/**
 * @brief Validate decoder output
 *
 * @param bridge Bridge instance
 * @param output Output tensor to validate
 * @param result Output validation result
 * @return VAE_BBB_RESULT_PASS if valid
 */
vae_bbb_result_t vae_bbb_validate_output(vae_bbb_bridge_t* bridge,
                                          const nimcp_tensor_t* output,
                                          vae_bbb_validation_t* result);

/**
 * @brief Validate gradients
 *
 * @param bridge Bridge instance
 * @param gradients Gradient tensor to validate
 * @param result Output validation result
 * @return VAE_BBB_RESULT_PASS if valid
 */
vae_bbb_result_t vae_bbb_validate_gradients(vae_bbb_bridge_t* bridge,
                                             const nimcp_tensor_t* gradients,
                                             vae_bbb_validation_t* result);

/**
 * @brief Full pipeline validation (input -> latent -> output)
 *
 * @param bridge Bridge instance
 * @param input Input tensor
 * @param latent Latent tensor
 * @param output Output tensor
 * @param result Output validation result
 * @return VAE_BBB_RESULT_PASS if all valid
 */
vae_bbb_result_t vae_bbb_validate_pipeline(vae_bbb_bridge_t* bridge,
                                            const nimcp_tensor_t* input,
                                            const nimcp_tensor_t* latent,
                                            const nimcp_tensor_t* output,
                                            vae_bbb_validation_t* result);

/* ============================================================================
 * Sanitization API
 * ============================================================================ */

/**
 * @brief Sanitize input tensor
 *
 * @param bridge Bridge instance
 * @param input Input tensor (modified in place if sanitized)
 * @return 0 on success, -1 on error (unsanitizable)
 */
int vae_bbb_sanitize_input(vae_bbb_bridge_t* bridge, nimcp_tensor_t* input);

/**
 * @brief Sanitize output tensor
 *
 * @param bridge Bridge instance
 * @param output Output tensor (modified in place)
 * @return 0 on success, -1 on error
 */
int vae_bbb_sanitize_output(vae_bbb_bridge_t* bridge, nimcp_tensor_t* output);

/**
 * @brief Clamp tensor values to bounds
 *
 * @param bridge Bridge instance
 * @param tensor Tensor to clamp (modified in place)
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @return Number of values clamped
 */
uint32_t vae_bbb_clamp_tensor(vae_bbb_bridge_t* bridge, nimcp_tensor_t* tensor,
                               float min_val, float max_val);

/**
 * @brief Replace NaN/Inf values in tensor
 *
 * @param bridge Bridge instance
 * @param tensor Tensor to clean (modified in place)
 * @param replacement Value to replace NaN/Inf with
 * @return Number of values replaced
 */
uint32_t vae_bbb_replace_invalid(vae_bbb_bridge_t* bridge, nimcp_tensor_t* tensor,
                                  float replacement);

/* ============================================================================
 * Adversarial Detection API
 * ============================================================================ */

/**
 * @brief Check if input is potentially adversarial
 *
 * @param bridge Bridge instance
 * @param input Input tensor
 * @param reference Reference/clean input (optional)
 * @param adversarial_score Output adversarial score (0-1)
 * @return true if adversarial detected
 */
bool vae_bbb_detect_adversarial(vae_bbb_bridge_t* bridge,
                                 const nimcp_tensor_t* input,
                                 const nimcp_tensor_t* reference,
                                 float* adversarial_score);

/**
 * @brief Compute perturbation magnitude
 *
 * @param input Input tensor
 * @param reference Reference tensor
 * @return L2 norm of perturbation
 */
float vae_bbb_perturbation_magnitude(const nimcp_tensor_t* input,
                                      const nimcp_tensor_t* reference);

/* ============================================================================
 * BBB Integration API
 * ============================================================================ */

/**
 * @brief Report threat to BBB system
 *
 * @param bridge Bridge instance
 * @param threat Threat type
 * @param severity Severity level
 * @param data Threatening data (for quarantine)
 * @param data_size Size of threatening data
 * @return 0 on success, -1 on error
 */
int vae_bbb_report_threat(vae_bbb_bridge_t* bridge,
                           vae_bbb_threat_t threat,
                           bbb_severity_t severity,
                           const void* data,
                           size_t data_size);

/**
 * @brief Quarantine suspicious tensor
 *
 * @param bridge Bridge instance
 * @param tensor Tensor to quarantine
 * @param reason Reason for quarantine
 * @return Quarantine ID on success, 0 on failure
 */
uint32_t vae_bbb_quarantine_tensor(vae_bbb_bridge_t* bridge,
                                    const nimcp_tensor_t* tensor,
                                    const char* reason);

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
int vae_bbb_bridge_get_stats(const vae_bbb_bridge_t* bridge,
                              vae_bbb_bridge_stats_t* stats);

/**
 * @brief Get bridge health
 *
 * @param bridge Bridge instance
 * @param health Output health metrics
 * @return 0 on success, -1 on error
 */
int vae_bbb_bridge_get_health(const vae_bbb_bridge_t* bridge,
                               vae_bbb_bridge_health_t* health);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @return Current state
 */
vae_bbb_bridge_state_t vae_bbb_bridge_get_state(const vae_bbb_bridge_t* bridge);

/**
 * @brief Get last validation result
 *
 * @param bridge Bridge instance
 * @param result Output validation result
 * @return 0 on success, -1 on error
 */
int vae_bbb_bridge_get_last_validation(const vae_bbb_bridge_t* bridge,
                                        vae_bbb_validation_t* result);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Set validation bounds
 *
 * @param bridge Bridge instance
 * @param bounds New bounds
 * @return 0 on success, -1 on error
 */
int vae_bbb_set_bounds(vae_bbb_bridge_t* bridge, const vae_bbb_bounds_t* bounds);

/**
 * @brief Get current bounds
 *
 * @param bridge Bridge instance
 * @param bounds Output bounds
 * @return 0 on success, -1 on error
 */
int vae_bbb_get_bounds(const vae_bbb_bridge_t* bridge, vae_bbb_bounds_t* bounds);

/**
 * @brief Set validation mask
 *
 * @param bridge Bridge instance
 * @param mask Validation mask (VAE_BBB_VALIDATE_*)
 * @return 0 on success, -1 on error
 */
int vae_bbb_set_validation_mask(vae_bbb_bridge_t* bridge, uint32_t mask);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert VAE threat to BBB threat type
 *
 * @param threat VAE threat type
 * @return BBB threat type
 */
bbb_threat_type_t vae_threat_to_bbb_threat(vae_bbb_threat_t threat);

/**
 * @brief Convert VAE threat type to string
 *
 * @param threat Threat type
 * @return String representation
 */
const char* vae_bbb_threat_to_string(vae_bbb_threat_t threat);

/**
 * @brief Convert validation result to string
 *
 * @param result Result code
 * @return String representation
 */
const char* vae_bbb_result_to_string(vae_bbb_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_BBB_BRIDGE_H */
