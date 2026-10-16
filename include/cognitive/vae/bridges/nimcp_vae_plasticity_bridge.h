/**
 * @file nimcp_vae_plasticity_bridge.h
 * @brief Bridge between VAE and Plasticity Systems for Learning Modulation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE with neural plasticity for VAE-modulated learning
 *
 * WHY:  VAE provides learning signals for plasticity:
 *       - Reconstruction error → prediction error signal
 *       - KL divergence → complexity/surprise signal
 *       - Posterior variance → uncertainty for precision weighting
 *       - Latent dynamics → eligibility trace modulation
 *       - Generative samples → replay for consolidation
 *
 * HOW:  Bridge maps VAE metrics to plasticity parameters:
 *       - Recon error → STDP learning rate modulation
 *       - KL divergence → BCM threshold adjustment
 *       - Precision → synaptic weight confidence
 *       - Novelty → homeostatic scaling trigger
 *
 * PLASTICITY MECHANISMS:
 * =====================
 * - STDP: Spike-timing dependent plasticity
 * - BCM: Bienenstock-Cooper-Munro sliding threshold
 * - Homeostatic: Synaptic scaling and intrinsic plasticity
 * - Structural: Synapse formation/elimination
 * - Metaplasticity: Plasticity of plasticity
 *
 * BIO_MODULE: 0x1F1C (VAE-Plasticity Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_PLASTICITY_BRIDGE_H
#define NIMCP_VAE_PLASTICITY_BRIDGE_H

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

#define VAE_PLASTICITY_BRIDGE_VERSION    "1.0.0"
#define BIO_MODULE_VAE_PLASTICITY_BRIDGE 0x1F1C

/** Maximum synapses tracked */
#define VAE_PLASTICITY_MAX_SYNAPSES      65536

/** Maximum plasticity mechanisms */
#define VAE_PLASTICITY_MAX_MECHANISMS    16

/** Error code range (32530-32539) */
#define NIMCP_ERROR_VAE_PLAST_BASE          32530
#define NIMCP_ERROR_VAE_PLAST_NULL          32531
#define NIMCP_ERROR_VAE_PLAST_NOT_CONNECTED 32532
#define NIMCP_ERROR_VAE_PLAST_UPDATE_FAILED 32533
#define NIMCP_ERROR_VAE_PLAST_NO_MEMORY     32534
#define NIMCP_ERROR_VAE_PLAST_INVALID_MODE  32535

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Plasticity mechanism types
 */
typedef enum {
    VAE_PLAST_STDP = 0,           /**< Spike-timing dependent plasticity */
    VAE_PLAST_RSTDP,               /**< Reward-modulated STDP */
    VAE_PLAST_BCM,                 /**< BCM sliding threshold */
    VAE_PLAST_HOMEOSTATIC,         /**< Synaptic scaling */
    VAE_PLAST_STRUCTURAL,          /**< Synapse creation/elimination */
    VAE_PLAST_METAPLASTICITY,      /**< Plasticity of plasticity */
    VAE_PLAST_STP,                 /**< Short-term plasticity */
    VAE_PLAST_COUNT
} vae_plasticity_mechanism_t;

/**
 * @brief Modulation signal source from VAE
 */
typedef enum {
    VAE_PLAST_SIG_RECON_ERROR = 0, /**< Reconstruction error */
    VAE_PLAST_SIG_KL_DIVERGENCE,   /**< KL divergence */
    VAE_PLAST_SIG_PRECISION,       /**< Posterior precision (1/var) */
    VAE_PLAST_SIG_NOVELTY,         /**< Latent space novelty */
    VAE_PLAST_SIG_ELBO,            /**< Evidence lower bound */
    VAE_PLAST_SIG_LATENT_CHANGE,   /**< Rate of latent change */
    VAE_PLAST_SIG_COLLAPSE_RISK    /**< Posterior collapse risk */
} vae_plasticity_signal_t;

/**
 * @brief Modulation mode
 */
typedef enum {
    VAE_PLAST_MOD_MULTIPLICATIVE = 0, /**< Multiply learning rate */
    VAE_PLAST_MOD_ADDITIVE,           /**< Add to learning rate */
    VAE_PLAST_MOD_GATING,             /**< Gate on/off */
    VAE_PLAST_MOD_THRESHOLD           /**< Adjust threshold */
} vae_plasticity_mod_mode_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_PLAST_STATE_DISCONNECTED = 0,
    VAE_PLAST_STATE_CONNECTED,
    VAE_PLAST_STATE_MODULATING,
    VAE_PLAST_STATE_CONSOLIDATING,
    VAE_PLAST_STATE_ERROR
} vae_plasticity_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Modulation mapping configuration
 */
typedef struct {
    vae_plasticity_signal_t signal;
    vae_plasticity_mechanism_t target_mechanism;
    vae_plasticity_mod_mode_t mode;
    float scale;                  /**< Scaling factor */
    float offset;                 /**< Offset for additive mode */
    float min_value;              /**< Minimum modulated value */
    float max_value;              /**< Maximum modulated value */
    bool invert;                  /**< Invert signal effect */
} vae_plasticity_mapping_t;

/**
 * @brief STDP modulation configuration
 */
typedef struct {
    bool modulate_a_plus;         /**< Modulate LTP amplitude */
    bool modulate_a_minus;        /**< Modulate LTD amplitude */
    bool modulate_tau;            /**< Modulate time constants */
    float recon_error_gain;       /**< How much recon error affects LR */
    float precision_gain;         /**< How much precision affects LR */
    float novelty_threshold;      /**< Novelty level to boost learning */
} vae_plasticity_stdp_config_t;

/**
 * @brief BCM modulation configuration
 */
typedef struct {
    bool modulate_threshold;      /**< Modulate sliding threshold */
    bool modulate_rate;           /**< Modulate threshold adaptation rate */
    float kl_divergence_gain;     /**< KL effect on threshold */
    float target_activity;        /**< Target firing rate */
} vae_plasticity_bcm_config_t;

/**
 * @brief Homeostatic modulation configuration
 */
typedef struct {
    bool enable_scaling;          /**< Enable synaptic scaling */
    bool enable_intrinsic;        /**< Enable intrinsic plasticity */
    float collapse_threshold;     /**< KL threshold for emergency scaling */
    float scaling_rate;           /**< Scaling adaptation rate */
} vae_plasticity_homeo_config_t;

/**
 * @brief Consolidation configuration (for replay)
 */
typedef struct {
    bool enable_replay;           /**< Enable VAE-generated replay */
    uint32_t replay_batch_size;   /**< Samples per replay batch */
    float replay_temperature;     /**< Sampling temperature for replay */
    float consolidation_rate;     /**< Learning rate during consolidation */
    bool prioritized_replay;      /**< Prioritize high-error samples */
} vae_plasticity_consolidation_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    /* Mechanism-specific configs */
    vae_plasticity_stdp_config_t stdp_config;
    vae_plasticity_bcm_config_t bcm_config;
    vae_plasticity_homeo_config_t homeo_config;
    vae_plasticity_consolidation_t consolidation;

    /* Modulation mappings */
    vae_plasticity_mapping_t* mappings;
    uint32_t num_mappings;

    /* Global parameters */
    float global_learning_rate;   /**< Base learning rate */
    float modulation_smoothing;   /**< Temporal smoothing of signals */
    float min_learning_rate;      /**< Minimum after modulation */
    float max_learning_rate;      /**< Maximum after modulation */

    /* Update timing */
    float update_interval_ms;     /**< How often to update modulation */
    bool continuous_modulation;   /**< Update every timestep */

    bool enable_logging;
} vae_plasticity_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Current modulation state
 */
typedef struct {
    float learning_rate_mod;      /**< Current LR modifier */
    float threshold_mod;          /**< BCM threshold modifier */
    float scaling_factor;         /**< Homeostatic scaling factor */
    float recon_error;            /**< Current reconstruction error */
    float kl_divergence;          /**< Current KL divergence */
    float precision_avg;          /**< Average precision */
    float novelty_score;          /**< Current novelty */
    bool plasticity_enabled;      /**< Whether plasticity is active */
    uint64_t last_update_us;
} vae_plasticity_modulation_state_t;

/**
 * @brief Replay result
 */
typedef struct {
    float* generated_samples;     /**< VAE-generated replay samples */
    uint32_t num_samples;
    uint32_t sample_dim;
    float avg_reconstruction_error;
    float replay_time_us;
} vae_plasticity_replay_result_t;

/**
 * @brief Consolidation result
 */
typedef struct {
    uint32_t synapses_updated;
    float avg_weight_change;
    float max_weight_change;
    float consolidation_error;
    uint64_t consolidation_time_us;
} vae_plasticity_consolidation_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_updates;
    uint64_t total_consolidations;
    uint64_t total_replays;
    float avg_learning_rate_mod;
    float avg_reconstruction_error;
    float avg_kl_divergence;
    float min_kl_observed;
    float max_kl_observed;
    uint32_t collapse_events;     /**< Times posterior collapse detected */
    uint64_t creation_time_us;
    uint64_t last_update_us;
} vae_plasticity_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_plasticity_bridge {
    vae_plasticity_bridge_config_t config;
    vae_system_t* vae;
    void* plasticity_coordinator; /**< Plasticity coordinator context */
    vae_plasticity_bridge_state_t state;
    bool is_initialized;

    /* Current modulation state */
    vae_plasticity_modulation_state_t current_modulation;

    /* Signal history (for smoothing) */
    float* recon_error_history;
    float* kl_history;
    float* precision_history;
    uint32_t history_head;
    uint32_t history_length;

    /* Replay buffer */
    float* replay_buffer;
    uint32_t replay_buffer_size;
    uint32_t replay_buffer_head;

    /* Working buffers */
    float* latent_buffer;
    float* signal_buffer;

    /* Statistics */
    vae_plasticity_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_plasticity_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_plasticity_bridge_default_config(vae_plasticity_bridge_config_t* config);
vae_plasticity_bridge_t* vae_plasticity_bridge_create(const vae_plasticity_bridge_config_t* config);
void vae_plasticity_bridge_destroy(vae_plasticity_bridge_t* bridge);
int vae_plasticity_bridge_connect_vae(vae_plasticity_bridge_t* bridge, vae_system_t* vae);
int vae_plasticity_bridge_connect_plasticity(vae_plasticity_bridge_t* bridge, void* coordinator);
int vae_plasticity_bridge_disconnect(vae_plasticity_bridge_t* bridge);
bool vae_plasticity_bridge_is_connected(const vae_plasticity_bridge_t* bridge);

/* ============================================================================
 * Modulation API
 * ============================================================================ */

int vae_plasticity_update_modulation(vae_plasticity_bridge_t* bridge);

int vae_plasticity_compute_signals(vae_plasticity_bridge_t* bridge,
                                    const float* input, uint32_t input_dim);

int vae_plasticity_apply_modulation(vae_plasticity_bridge_t* bridge,
                                     vae_plasticity_mechanism_t mechanism);

int vae_plasticity_get_modulation(const vae_plasticity_bridge_t* bridge,
                                   vae_plasticity_modulation_state_t* state);

/* ============================================================================
 * Signal Extraction API
 * ============================================================================ */

float vae_plasticity_get_recon_error(const vae_plasticity_bridge_t* bridge);
float vae_plasticity_get_kl_divergence(const vae_plasticity_bridge_t* bridge);
float vae_plasticity_get_precision(const vae_plasticity_bridge_t* bridge);
float vae_plasticity_get_novelty(const vae_plasticity_bridge_t* bridge);
float vae_plasticity_get_elbo(const vae_plasticity_bridge_t* bridge);
bool vae_plasticity_detect_collapse(const vae_plasticity_bridge_t* bridge);

/* ============================================================================
 * Consolidation API (VAE-based replay)
 * ============================================================================ */

int vae_plasticity_generate_replay(vae_plasticity_bridge_t* bridge,
                                    uint32_t num_samples,
                                    vae_plasticity_replay_result_t* result);

int vae_plasticity_consolidate(vae_plasticity_bridge_t* bridge,
                                const vae_plasticity_replay_result_t* replay,
                                vae_plasticity_consolidation_result_t* result);

int vae_plasticity_add_to_replay_buffer(vae_plasticity_bridge_t* bridge,
                                         const float* sample, uint32_t dim);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int vae_plasticity_add_mapping(vae_plasticity_bridge_t* bridge,
                                const vae_plasticity_mapping_t* mapping);

int vae_plasticity_set_global_rate(vae_plasticity_bridge_t* bridge, float rate);

int vae_plasticity_enable_mechanism(vae_plasticity_bridge_t* bridge,
                                     vae_plasticity_mechanism_t mechanism,
                                     bool enabled);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_plasticity_bridge_state_t vae_plasticity_bridge_get_state(const vae_plasticity_bridge_t* bridge);
int vae_plasticity_bridge_get_stats(const vae_plasticity_bridge_t* bridge,
                                     vae_plasticity_bridge_stats_t* stats);
const char* vae_plasticity_mechanism_to_string(vae_plasticity_mechanism_t mech);
const char* vae_plasticity_signal_to_string(vae_plasticity_signal_t signal);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_plasticity_replay_result_free(vae_plasticity_replay_result_t* result);
void vae_plasticity_consolidation_result_free(vae_plasticity_consolidation_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_PLASTICITY_BRIDGE_H */
