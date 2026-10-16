/**
 * @file nimcp_vae_substrate_bridge.h
 * @brief Bridge between VAE and Neural Substrate for Metabolic-Aware Encoding
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE with neural substrate for energy-aware processing
 *
 * WHY:  Substrate-aware VAE enables:
 *       - ATP-dependent latent complexity (compress when low energy)
 *       - Temperature-modulated learning rates
 *       - Metabolic stress → increased uncertainty encoding
 *       - Energy-efficient inference through adaptive compression
 *       - Ion balance effects on encoding precision
 *
 * HOW:  Bridge modulates VAE based on substrate state:
 *       - Low ATP → reduced latent dimensions, higher compression
 *       - Temperature → learning rate Q10 scaling
 *       - O2 saturation → encoding fidelity
 *       - Glucose → reconstruction detail level
 *
 * METABOLIC PARAMETERS:
 * ====================
 * - ATP level: 0.0-1.0 (critical < 0.3)
 * - O2 saturation: 0.0-1.0 (critical < 0.5)
 * - Glucose level: 0.0-1.0 (critical < 0.4)
 * - Ion balance: 0.0-1.0 (critical < 0.5)
 * - Temperature: 32-40°C (normal 37°C)
 *
 * BIO_MODULE: 0x1F1E (VAE-Substrate Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_SUBSTRATE_BRIDGE_H
#define NIMCP_VAE_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VAE_SUBSTRATE_BRIDGE_VERSION    "1.0.0"
#define BIO_MODULE_VAE_SUBSTRATE_BRIDGE 0x1F1E

/** Metabolic thresholds */
#define VAE_SUBSTRATE_CRITICAL_ATP      0.3f
#define VAE_SUBSTRATE_CRITICAL_O2       0.5f
#define VAE_SUBSTRATE_CRITICAL_GLUCOSE  0.4f
#define VAE_SUBSTRATE_CRITICAL_ION      0.5f

/** Temperature constants */
#define VAE_SUBSTRATE_NORMAL_TEMP       37.0f
#define VAE_SUBSTRATE_HYPOTHERMIA       32.0f
#define VAE_SUBSTRATE_HYPERTHERMIA      40.0f

/** Q10 coefficients */
#define VAE_SUBSTRATE_Q10_ENCODING      2.0f
#define VAE_SUBSTRATE_Q10_LEARNING      2.2f
#define VAE_SUBSTRATE_Q10_INFERENCE     1.8f

/** Error code range (32550-32559) */
#define NIMCP_ERROR_VAE_SUB_BASE          32550
#define NIMCP_ERROR_VAE_SUB_NULL          32551
#define NIMCP_ERROR_VAE_SUB_NOT_CONNECTED 32552
#define NIMCP_ERROR_VAE_SUB_CRITICAL      32553
#define NIMCP_ERROR_VAE_SUB_TEMP_RANGE    32554
#define NIMCP_ERROR_VAE_SUB_NO_MEMORY     32555

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Substrate health level
 */
typedef enum {
    VAE_SUBSTRATE_OPTIMAL = 0,    /**< All parameters normal */
    VAE_SUBSTRATE_NORMAL,          /**< Slight deviations */
    VAE_SUBSTRATE_STRESSED,        /**< Approaching critical */
    VAE_SUBSTRATE_CRITICAL,        /**< Critical - emergency mode */
    VAE_SUBSTRATE_FAILURE          /**< System failure */
} vae_substrate_health_t;

/**
 * @brief Adaptation strategy under stress
 */
typedef enum {
    VAE_SUBSTRATE_ADAPT_COMPRESS = 0, /**< Reduce latent dims */
    VAE_SUBSTRATE_ADAPT_SPARSE,        /**< Sparse encoding */
    VAE_SUBSTRATE_ADAPT_QUANTIZE,      /**< Quantize latent values */
    VAE_SUBSTRATE_ADAPT_THROTTLE,      /**< Reduce update frequency */
    VAE_SUBSTRATE_ADAPT_HYBRID         /**< Combination of above */
} vae_substrate_adaptation_t;

/**
 * @brief Energy cost category
 */
typedef enum {
    VAE_ENERGY_ENCODE = 0,        /**< Encoding cost */
    VAE_ENERGY_DECODE,             /**< Decoding cost */
    VAE_ENERGY_SAMPLE,             /**< Sampling cost */
    VAE_ENERGY_TRAIN,              /**< Training cost */
    VAE_ENERGY_TOTAL               /**< Total cost */
} vae_energy_category_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_SUBSTRATE_STATE_DISCONNECTED = 0,
    VAE_SUBSTRATE_STATE_CONNECTED,
    VAE_SUBSTRATE_STATE_MONITORING,
    VAE_SUBSTRATE_STATE_ADAPTING,
    VAE_SUBSTRATE_STATE_EMERGENCY,
    VAE_SUBSTRATE_STATE_ERROR
} vae_substrate_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Metabolic state snapshot
 */
typedef struct {
    float atp_level;              /**< [0, 1] ATP availability */
    float o2_saturation;          /**< [0, 1] Oxygen saturation */
    float glucose_level;          /**< [0, 1] Glucose availability */
    float ion_balance;            /**< [0, 1] Ion homeostasis */
    float temperature_c;          /**< Temperature in Celsius */
    uint64_t timestamp_us;
} vae_substrate_metabolic_state_t;

/**
 * @brief Energy budget configuration
 */
typedef struct {
    float max_atp_per_encode;     /**< Max ATP cost per encode */
    float max_atp_per_decode;     /**< Max ATP cost per decode */
    float max_atp_per_sample;     /**< Max ATP cost per sample */
    float atp_per_latent_dim;     /**< ATP cost per latent dimension */
    float reserve_fraction;       /**< ATP fraction to reserve */
} vae_substrate_energy_budget_t;

/**
 * @brief Compression configuration under stress
 */
typedef struct {
    uint32_t min_latent_dim;      /**< Minimum latent dimensions */
    uint32_t normal_latent_dim;   /**< Normal latent dimensions */
    float compression_rate;       /**< How fast to compress under stress */
    bool enable_pruning;          /**< Prune inactive latent dims */
    float pruning_threshold;      /**< Activity threshold for pruning */
} vae_substrate_compression_config_t;

/**
 * @brief Q10 temperature scaling configuration
 */
typedef struct {
    float q10_encoding;           /**< Q10 for encoding operations */
    float q10_decoding;           /**< Q10 for decoding operations */
    float q10_learning;           /**< Q10 for learning rate */
    float reference_temp_c;       /**< Reference temperature */
    bool enable_temp_scaling;
} vae_substrate_temp_config_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_substrate_adaptation_t adaptation_strategy;
    vae_substrate_energy_budget_t energy_budget;
    vae_substrate_compression_config_t compression;
    vae_substrate_temp_config_t temperature;

    /* Monitoring */
    float monitor_interval_ms;    /**< Substrate check interval */
    bool continuous_monitoring;

    /* Stress response */
    float stress_onset_threshold; /**< When to start adapting */
    float emergency_threshold;    /**< When to enter emergency mode */
    float recovery_threshold;     /**< When to exit emergency mode */

    /* Uncertainty encoding */
    bool encode_uncertainty;      /**< Increase variance under stress */
    float stress_variance_scale;  /**< Variance multiplier under stress */

    bool enable_logging;
} vae_substrate_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Current modulation state
 */
typedef struct {
    vae_substrate_health_t health;
    float encoding_modulation;    /**< Encoding rate modifier */
    float learning_modulation;    /**< Learning rate modifier */
    float inference_modulation;   /**< Inference rate modifier */
    uint32_t effective_latent_dim; /**< Current latent dimension */
    float variance_scale;         /**< Uncertainty scaling factor */
    float energy_efficiency;      /**< Current efficiency [0, 1] */
} vae_substrate_modulation_t;

/**
 * @brief Energy usage result
 */
typedef struct {
    float atp_used;
    float atp_remaining;
    float glucose_consumed;
    float efficiency_ratio;       /**< Output quality / energy */
    uint64_t operation_time_us;
} vae_substrate_energy_result_t;

/**
 * @brief Adaptation result
 */
typedef struct {
    vae_substrate_adaptation_t strategy_used;
    uint32_t latent_dim_before;
    uint32_t latent_dim_after;
    float compression_ratio;
    float quality_loss_estimate;
    uint64_t adaptation_time_us;
} vae_substrate_adaptation_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_operations;
    uint64_t emergency_events;
    uint64_t adaptations_triggered;
    float total_atp_consumed;
    float avg_efficiency;
    float min_atp_observed;
    float max_temp_observed;
    float min_temp_observed;
    float time_in_emergency_us;
    uint64_t creation_time_us;
    uint64_t last_update_us;
} vae_substrate_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_substrate_bridge {
    vae_substrate_bridge_config_t config;
    vae_system_t* vae;
    void* substrate_system;       /**< Neural substrate context */
    vae_substrate_bridge_state_t state;
    bool is_initialized;

    /* Current substrate state */
    vae_substrate_metabolic_state_t current_state;
    vae_substrate_health_t current_health;
    vae_substrate_modulation_t current_modulation;

    /* History for trend detection */
    float* atp_history;
    float* temp_history;
    uint32_t history_head;
    uint32_t history_size;

    /* Adaptation state */
    uint32_t current_latent_dim;
    float current_variance_scale;
    bool in_emergency_mode;
    uint64_t emergency_start_us;

    /* Energy tracking */
    float atp_consumed_session;
    float glucose_consumed_session;

    /* Working buffer */
    float* modulation_buffer;

    /* Statistics */
    vae_substrate_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_substrate_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_substrate_bridge_default_config(vae_substrate_bridge_config_t* config);
vae_substrate_bridge_t* vae_substrate_bridge_create(const vae_substrate_bridge_config_t* config);
void vae_substrate_bridge_destroy(vae_substrate_bridge_t* bridge);
int vae_substrate_bridge_connect_vae(vae_substrate_bridge_t* bridge, vae_system_t* vae);
int vae_substrate_bridge_connect_substrate(vae_substrate_bridge_t* bridge, void* substrate);
int vae_substrate_bridge_disconnect(vae_substrate_bridge_t* bridge);
bool vae_substrate_bridge_is_connected(const vae_substrate_bridge_t* bridge);

/* ============================================================================
 * Monitoring API
 * ============================================================================ */

int vae_substrate_update_state(vae_substrate_bridge_t* bridge);

int vae_substrate_get_metabolic_state(const vae_substrate_bridge_t* bridge,
                                       vae_substrate_metabolic_state_t* state);

vae_substrate_health_t vae_substrate_assess_health(const vae_substrate_bridge_t* bridge);

int vae_substrate_get_modulation(const vae_substrate_bridge_t* bridge,
                                  vae_substrate_modulation_t* modulation);

/* ============================================================================
 * Adaptation API
 * ============================================================================ */

int vae_substrate_adapt(vae_substrate_bridge_t* bridge,
                         vae_substrate_adaptation_result_t* result);

int vae_substrate_enter_emergency(vae_substrate_bridge_t* bridge);

int vae_substrate_exit_emergency(vae_substrate_bridge_t* bridge);

int vae_substrate_set_latent_dim(vae_substrate_bridge_t* bridge, uint32_t dim);

/* ============================================================================
 * Energy API
 * ============================================================================ */

float vae_substrate_estimate_cost(const vae_substrate_bridge_t* bridge,
                                   vae_energy_category_t category);

bool vae_substrate_can_afford(const vae_substrate_bridge_t* bridge,
                               vae_energy_category_t category);

int vae_substrate_consume_energy(vae_substrate_bridge_t* bridge,
                                  vae_energy_category_t category,
                                  vae_substrate_energy_result_t* result);

float vae_substrate_get_efficiency(const vae_substrate_bridge_t* bridge);

/* ============================================================================
 * Temperature API
 * ============================================================================ */

float vae_substrate_q10_scale(const vae_substrate_bridge_t* bridge,
                               float base_rate,
                               float q10_coefficient);

float vae_substrate_get_temp_modulated_lr(const vae_substrate_bridge_t* bridge,
                                           float base_lr);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_substrate_bridge_state_t vae_substrate_bridge_get_state(const vae_substrate_bridge_t* bridge);
int vae_substrate_bridge_get_stats(const vae_substrate_bridge_t* bridge,
                                    vae_substrate_bridge_stats_t* stats);
const char* vae_substrate_health_to_string(vae_substrate_health_t health);
const char* vae_substrate_adaptation_to_string(vae_substrate_adaptation_t adapt);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_substrate_energy_result_free(vae_substrate_energy_result_t* result);
void vae_substrate_adaptation_result_free(vae_substrate_adaptation_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_SUBSTRATE_BRIDGE_H */
