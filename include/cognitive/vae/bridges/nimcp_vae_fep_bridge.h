/**
 * @file nimcp_vae_fep_bridge.h
 * @brief Bridge between VAE and Free Energy Principle systems
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Bidirectional integration between VAE latent space and FEP beliefs
 *
 * WHY:  VAE provides learned recognition/generative models that can serve as
 *       the implementation substrate for FEP's variational inference.
 *       - VAE encoder = FEP recognition density q(s|o)
 *       - VAE decoder = FEP generative model p(o|s)
 *       - VAE latent = FEP hidden state beliefs
 *       - VAE ELBO = FEP variational free energy
 *
 * HOW:  Bridge synchronizes:
 *       VAE → FEP: Latent μ/σ become belief means/precisions
 *       FEP → VAE: Prediction errors guide VAE training
 *       Bidirectional: Free energy computation shared
 *
 * MAPPING:
 * ```
 *   VAE Component        | FEP Component
 *   ---------------------|--------------------
 *   q(z|x)               | Recognition density q(s|o)
 *   p(x|z)               | Generative model p(o|s)
 *   z ~ N(μ, σ²)         | Posterior belief about s
 *   Reconstruction error | Prediction error (inaccuracy)
 *   KL(q||p)             | Model complexity
 *   -ELBO                | Variational free energy
 *   1/σ²                 | Precision weighting
 * ```
 *
 * ARCHITECTURE:
 * ```
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │                       VAE-FEP BRIDGE                                    │
 *   │                                                                         │
 *   │   ┌───────────────┐                          ┌───────────────┐         │
 *   │   │   VAE System  │                          │  FEP System   │         │
 *   │   │               │                          │               │         │
 *   │   │  ┌─────────┐  │    latent_to_belief     │  ┌─────────┐  │         │
 *   │   │  │ Latent  │  │  ─────────────────────▶ │  │ Beliefs │  │         │
 *   │   │  │ μ, σ²   │  │                          │  │ μ, Π    │  │         │
 *   │   │  └─────────┘  │    belief_to_latent     │  └─────────┘  │         │
 *   │   │               │  ◀─────────────────────  │               │         │
 *   │   │  ┌─────────┐  │                          │  ┌─────────┐  │         │
 *   │   │  │ Recon   │  │    prediction_error     │  │ Pred    │  │         │
 *   │   │  │ Error   │  │  ─────────────────────▶ │  │ Error   │  │         │
 *   │   │  └─────────┘  │                          │  └─────────┘  │         │
 *   │   │               │                          │               │         │
 *   │   │  ┌─────────┐  │    free_energy_sync     │  ┌─────────┐  │         │
 *   │   │  │  ELBO   │  │  ◀────────────────────▶ │  │   F     │  │         │
 *   │   │  └─────────┘  │                          │  └─────────┘  │         │
 *   │   └───────────────┘                          └───────────────┘         │
 *   │                                                                         │
 *   └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIO_MODULE: 0x1F10 (VAE-FEP Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_FEP_BRIDGE_H
#define NIMCP_VAE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bridge version */
#define VAE_FEP_BRIDGE_VERSION          "1.0.0"

/** Maximum hierarchy levels for mapping */
#define VAE_FEP_MAX_HIERARCHY_LEVELS    8

/** Bio-async module ID */
#define BIO_MODULE_VAE_FEP_BRIDGE       0x1F10

/** Default sync interval (milliseconds) */
#define VAE_FEP_DEFAULT_SYNC_INTERVAL   10

/** Default message buffer size */
#define VAE_FEP_DEFAULT_MSG_BUFFER      256

/** Precision mapping bounds */
#define VAE_FEP_MIN_PRECISION           0.001f
#define VAE_FEP_MAX_PRECISION           1000.0f

/* ============================================================================
 * Error Codes (32420-32429 range for FEP bridge)
 * ============================================================================ */

#define NIMCP_ERROR_VAE_FEP_BASE            32420
#define NIMCP_ERROR_VAE_FEP_NULL_BRIDGE     32421
#define NIMCP_ERROR_VAE_FEP_NOT_CONNECTED   32422
#define NIMCP_ERROR_VAE_FEP_SYNC_FAILED     32423
#define NIMCP_ERROR_VAE_FEP_DIM_MISMATCH    32424
#define NIMCP_ERROR_VAE_FEP_NO_VAE          32425
#define NIMCP_ERROR_VAE_FEP_NO_FEP          32426
#define NIMCP_ERROR_VAE_FEP_PRECISION_ERR   32427

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Synchronization direction
 */
typedef enum {
    VAE_FEP_SYNC_VAE_TO_FEP = 0,     /**< VAE latent → FEP belief */
    VAE_FEP_SYNC_FEP_TO_VAE,          /**< FEP belief → VAE latent */
    VAE_FEP_SYNC_BIDIRECTIONAL        /**< Both directions */
} vae_fep_sync_direction_t;

/**
 * @brief Mapping mode between latent and belief dimensions
 */
typedef enum {
    VAE_FEP_MAP_DIRECT = 0,          /**< Direct 1:1 mapping (dims must match) */
    VAE_FEP_MAP_LINEAR,               /**< Linear transformation */
    VAE_FEP_MAP_PROJECTION,           /**< Learned projection */
    VAE_FEP_MAP_HIERARCHICAL          /**< Map to FEP hierarchy */
} vae_fep_mapping_mode_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_FEP_STATE_DISCONNECTED = 0,  /**< Not connected */
    VAE_FEP_STATE_CONNECTED,          /**< Connected but idle */
    VAE_FEP_STATE_SYNCING,            /**< Currently synchronizing */
    VAE_FEP_STATE_ERROR               /**< Error state */
} vae_fep_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief VAE-FEP bridge configuration
 */
typedef struct {
    /* Synchronization settings */
    vae_fep_sync_direction_t sync_direction;  /**< Default sync direction */
    uint32_t sync_interval_ms;                 /**< Auto-sync interval (0=disabled) */
    bool auto_sync_enabled;                    /**< Enable automatic synchronization */

    /* Mapping configuration */
    vae_fep_mapping_mode_t mapping_mode;       /**< How to map dimensions */
    uint32_t target_hierarchy_level;           /**< FEP level to map to (0=lowest) */

    /* Precision settings */
    bool share_precision;                      /**< Share precision between systems */
    float precision_scale;                     /**< Scale factor for precision mapping */
    float min_precision;                       /**< Minimum precision value */
    float max_precision;                       /**< Maximum precision value */

    /* Free energy settings */
    bool share_free_energy;                    /**< Share free energy computation */
    float free_energy_weight_vae;              /**< Weight for VAE contribution */
    float free_energy_weight_fep;              /**< Weight for FEP contribution */

    /* Integration flags */
    bool enable_bio_async;                     /**< Enable bio-async messaging */
    bool enable_immune_reporting;              /**< Report anomalies to immune */
    bool enable_logging;                       /**< Detailed logging */

    /* Message buffer */
    uint32_t message_buffer_size;              /**< Bio-async message buffer size */
} vae_fep_bridge_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Mapping state between VAE latent and FEP beliefs
 */
typedef struct {
    /* Dimension mapping */
    uint32_t latent_dim;                       /**< VAE latent dimension */
    uint32_t belief_dim;                       /**< FEP belief dimension */

    /* Mapping matrices (if using linear/projection mode) */
    float* latent_to_belief_matrix;            /**< [belief_dim x latent_dim] */
    float* belief_to_latent_matrix;            /**< [latent_dim x belief_dim] */

    /* Current mapped values */
    float* mapped_mean;                        /**< Mapped mean values */
    float* mapped_precision;                   /**< Mapped precision values */

    /* Shared free energy state */
    float shared_free_energy;                  /**< Combined free energy */
    float vae_free_energy;                     /**< VAE contribution */
    float fep_free_energy;                     /**< FEP contribution */
    float inaccuracy;                          /**< Shared inaccuracy term */
    float complexity;                          /**< Shared complexity term */

    /* Timing */
    uint64_t last_vae_to_fep_us;              /**< Last VAE→FEP sync time */
    uint64_t last_fep_to_vae_us;              /**< Last FEP→VAE sync time */
    uint64_t last_free_energy_us;              /**< Last free energy update */
} vae_fep_mapping_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Sync counts */
    uint64_t total_syncs;                      /**< Total synchronizations */
    uint64_t latent_to_belief_syncs;           /**< VAE → FEP syncs */
    uint64_t belief_to_latent_syncs;           /**< FEP → VAE syncs */
    uint64_t bidirectional_syncs;              /**< Full bidirectional syncs */

    /* Data transfer */
    uint64_t prediction_errors_sent;           /**< Errors sent to FEP */
    uint64_t belief_updates_received;          /**< Updates from FEP */
    uint64_t precision_updates;                /**< Precision synchronizations */

    /* Free energy tracking */
    float avg_free_energy;                     /**< Average shared free energy */
    float min_free_energy;                     /**< Minimum free energy seen */
    float max_free_energy;                     /**< Maximum free energy seen */
    float avg_inaccuracy;                      /**< Average inaccuracy */
    float avg_complexity;                      /**< Average complexity */

    /* Precision tracking */
    float avg_precision;                       /**< Average precision */
    float min_precision;                       /**< Minimum precision */
    float max_precision;                       /**< Maximum precision */

    /* Performance */
    float avg_sync_latency_us;                 /**< Average sync latency */
    uint64_t sync_failures;                    /**< Number of failed syncs */

    /* Timing */
    uint64_t last_update_us;                   /**< Last statistics update */
    uint64_t uptime_us;                        /**< Bridge uptime */
} vae_fep_bridge_stats_t;

/**
 * @brief Health metrics for the bridge
 */
typedef struct {
    float bridge_health;                       /**< Overall health (0-1) */
    float sync_reliability;                    /**< Sync success rate */
    float latency_health;                      /**< Latency acceptability */
    bool is_healthy;                           /**< Quick health check */
    uint32_t consecutive_failures;             /**< Consecutive sync failures */
    uint32_t last_error_code;                  /**< Last error code */
} vae_fep_bridge_health_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief VAE-FEP bridge instance
 */
typedef struct vae_fep_bridge {
    /* Configuration */
    vae_fep_bridge_config_t config;

    /* Connected systems */
    vae_system_t* vae;                         /**< Connected VAE system */
    fep_system_t* fep;                         /**< Connected FEP system */

    /* State */
    vae_fep_bridge_state_t state;              /**< Current bridge state */
    vae_fep_mapping_state_t mapping;           /**< Mapping state */
    bool is_initialized;                       /**< Initialization flag */

    /* Statistics */
    vae_fep_bridge_stats_t stats;

    /* Health */
    vae_fep_bridge_health_t health;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;                 /**< Bridge creation time */
    uint64_t last_sync_time_us;                /**< Last sync time */
} vae_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_default_config(vae_fep_bridge_config_t* config);

/**
 * @brief Create VAE-FEP bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on error (throws to immune)
 */
vae_fep_bridge_t* vae_fep_bridge_create(const vae_fep_bridge_config_t* config);

/**
 * @brief Destroy VAE-FEP bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void vae_fep_bridge_destroy(vae_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state (keep connections)
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_reset(vae_fep_bridge_t* bridge);

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
int vae_fep_bridge_connect_vae(vae_fep_bridge_t* bridge, vae_system_t* vae);

/**
 * @brief Connect FEP system to bridge
 *
 * @param bridge Bridge instance
 * @param fep FEP system to connect
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_connect_fep(vae_fep_bridge_t* bridge, fep_system_t* fep);

/**
 * @brief Disconnect both systems
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_disconnect(vae_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge instance
 * @return true if both VAE and FEP are connected
 */
bool vae_fep_bridge_is_connected(const vae_fep_bridge_t* bridge);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @return Current state
 */
vae_fep_bridge_state_t vae_fep_bridge_get_state(const vae_fep_bridge_t* bridge);

/* ============================================================================
 * Synchronization API
 * ============================================================================ */

/**
 * @brief Sync VAE latent state to FEP beliefs
 *
 * Transfers latent mean to belief mean, latent precision to belief precision.
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_sync_latent_to_belief(vae_fep_bridge_t* bridge);

/**
 * @brief Sync FEP beliefs to VAE latent state
 *
 * Transfers belief mean to latent mean, belief precision to latent precision.
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_sync_belief_to_latent(vae_fep_bridge_t* bridge);

/**
 * @brief Full bidirectional synchronization
 *
 * Performs both latent→belief and belief→latent syncs, reconciling differences.
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_sync(vae_fep_bridge_t* bridge);

/**
 * @brief Sync with specified direction
 *
 * @param bridge Bridge instance
 * @param direction Sync direction
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_sync_direction(vae_fep_bridge_t* bridge,
                                   vae_fep_sync_direction_t direction);

/* ============================================================================
 * Free Energy API
 * ============================================================================ */

/**
 * @brief Compute shared free energy from VAE ELBO
 *
 * Combines VAE free energy (negative ELBO) with FEP free energy.
 *
 * @param bridge Bridge instance
 * @param free_energy Output shared free energy
 * @return 0 on success, -1 on error
 */
int vae_fep_compute_free_energy(vae_fep_bridge_t* bridge, float* free_energy);

/**
 * @brief Get free energy decomposition
 *
 * @param bridge Bridge instance
 * @param total Output total free energy
 * @param inaccuracy Output inaccuracy component
 * @param complexity Output complexity component
 * @return 0 on success, -1 on error
 */
int vae_fep_get_free_energy_decomposition(vae_fep_bridge_t* bridge,
                                           float* total,
                                           float* inaccuracy,
                                           float* complexity);

/**
 * @brief Update free energy from VAE loss
 *
 * Called after VAE forward pass to update shared free energy.
 *
 * @param bridge Bridge instance
 * @param vae_loss VAE loss structure
 * @return 0 on success, -1 on error
 */
int vae_fep_update_free_energy_from_vae(vae_fep_bridge_t* bridge,
                                         const vae_loss_t* vae_loss);

/* ============================================================================
 * Precision API
 * ============================================================================ */

/**
 * @brief Get precision weights from VAE latent variance
 *
 * Computes precision = 1/variance from VAE latent log_var.
 *
 * @param bridge Bridge instance
 * @param precision Output precision array [dim]
 * @param dim Expected dimension
 * @return 0 on success, -1 on error
 */
int vae_fep_get_precision(vae_fep_bridge_t* bridge, float* precision, uint32_t dim);

/**
 * @brief Set FEP precision from VAE latent variance
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_sync_precision(vae_fep_bridge_t* bridge);

/**
 * @brief Get average precision
 *
 * @param bridge Bridge instance
 * @return Average precision, or NAN on error
 */
float vae_fep_get_avg_precision(const vae_fep_bridge_t* bridge);

/* ============================================================================
 * Prediction Error API
 * ============================================================================ */

/**
 * @brief Report prediction error from VAE to FEP
 *
 * Sends VAE reconstruction error to FEP as sensory prediction error.
 *
 * @param bridge Bridge instance
 * @param error_magnitude Error magnitude (L2 norm)
 * @return 0 on success, -1 on error
 */
int vae_fep_report_prediction_error(vae_fep_bridge_t* bridge, float error_magnitude);

/**
 * @brief Report prediction error tensor
 *
 * @param bridge Bridge instance
 * @param error Error tensor
 * @return 0 on success, -1 on error
 */
int vae_fep_report_prediction_error_tensor(vae_fep_bridge_t* bridge,
                                            const nimcp_tensor_t* error);

/**
 * @brief Get prediction error from FEP
 *
 * @param bridge Bridge instance
 * @param error Output error tensor
 * @return 0 on success, -1 on error
 */
int vae_fep_get_fep_prediction_error(vae_fep_bridge_t* bridge,
                                      nimcp_tensor_t* error);

/* ============================================================================
 * Active Inference API
 * ============================================================================ */

/**
 * @brief Compute expected free energy for action in latent space
 *
 * Uses VAE generative model to predict outcomes and compute EFE.
 *
 * @param bridge Bridge instance
 * @param action_latent Action encoded in latent space
 * @param expected_fe Output expected free energy
 * @return 0 on success, -1 on error
 */
int vae_fep_compute_expected_free_energy(vae_fep_bridge_t* bridge,
                                          const nimcp_tensor_t* action_latent,
                                          float* expected_fe);

/**
 * @brief Sample action from latent space
 *
 * Uses FEP active inference to select action, returns in VAE latent space.
 *
 * @param bridge Bridge instance
 * @param action_latent Output action in latent space
 * @return 0 on success, -1 on error
 */
int vae_fep_sample_action_latent(vae_fep_bridge_t* bridge,
                                  nimcp_tensor_t* action_latent);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main bridge update (call each processing cycle)
 *
 * Performs auto-synchronization if enabled, updates statistics.
 *
 * @param bridge Bridge instance
 * @param delta_ms Milliseconds since last update
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_update(vae_fep_bridge_t* bridge, uint64_t delta_ms);

/**
 * @brief Process pending messages (bio-async)
 *
 * @param bridge Bridge instance
 * @return Number of messages processed, -1 on error
 */
int vae_fep_bridge_process_messages(vae_fep_bridge_t* bridge);

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
int vae_fep_bridge_get_stats(const vae_fep_bridge_t* bridge,
                              vae_fep_bridge_stats_t* stats);

/**
 * @brief Get mapping state
 *
 * @param bridge Bridge instance
 * @param mapping Output mapping state
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_get_mapping(const vae_fep_bridge_t* bridge,
                                vae_fep_mapping_state_t* mapping);

/**
 * @brief Get bridge health
 *
 * @param bridge Bridge instance
 * @param health Output health metrics
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_get_health(const vae_fep_bridge_t* bridge,
                               vae_fep_bridge_health_t* health);

/**
 * @brief Get bridge configuration
 *
 * @param bridge Bridge instance
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_get_config(const vae_fep_bridge_t* bridge,
                               vae_fep_bridge_config_t* config);

/* ============================================================================
 * Dimension Query
 * ============================================================================ */

/**
 * @brief Get VAE latent dimension
 *
 * @param bridge Bridge instance
 * @return Latent dimension, or 0 if not connected
 */
uint32_t vae_fep_bridge_get_latent_dim(const vae_fep_bridge_t* bridge);

/**
 * @brief Get FEP belief dimension
 *
 * @param bridge Bridge instance
 * @return Belief dimension, or 0 if not connected
 */
uint32_t vae_fep_bridge_get_belief_dim(const vae_fep_bridge_t* bridge);

/**
 * @brief Check dimension compatibility
 *
 * @param bridge Bridge instance
 * @return true if dimensions are compatible for mapping
 */
bool vae_fep_bridge_dims_compatible(const vae_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async messaging system
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_connect_bio_async(vae_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vae_fep_bridge_disconnect_bio_async(vae_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if bio-async is connected
 */
bool vae_fep_bridge_is_bio_async_connected(const vae_fep_bridge_t* bridge);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for the bridge
 *
 * @param bridge Bridge instance
 * @param agent Health agent
 */
void vae_fep_bridge_set_health_agent(vae_fep_bridge_t* bridge,
                                      nimcp_health_agent_t* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_FEP_BRIDGE_H */
