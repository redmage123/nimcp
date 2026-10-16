/**
 * @file nimcp_snn_medulla_bridge.h
 * @brief SNN-Medulla integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and medulla oblongata
 * WHY:  Enable spike-based autonomic regulation and brainstem modulation
 * HOW:  Convert medulla states to spike patterns, modulate neural activity
 *
 * BIOLOGICAL BASIS:
 * - Medulla oblongata regulates vital autonomic functions
 * - Arousal state modulates cortical excitability via reticular activating system
 * - Protection levels gate neural activity during emergencies
 * - Circadian rhythm modulates baseline neural excitability
 * - High neural activity can trigger protective cutoffs
 *
 * INTEGRATION:
 * - Medulla → SNN: Arousal modulates firing rates, protection restricts activity
 * - SNN → Medulla: High activity can trigger protective responses
 * - Circadian phase adjusts baseline excitability
 * - Emergency shutdown suppresses all SNN activity
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_MEDULLA_BRIDGE_H
#define NIMCP_SNN_MEDULLA_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "core/medulla/nimcp_medulla.h"
#include "async/nimcp_bio_async.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Configuration for SNN-medulla bridge
 */
typedef struct snn_medulla_config_s {
    /* Arousal modulation parameters */
    float arousal_min_rate_factor;   /**< Rate factor at minimum arousal (default 0.1) */
    float arousal_max_rate_factor;   /**< Rate factor at maximum arousal (default 2.0) */
    float arousal_excitability_gain; /**< How much arousal affects excitability (default 0.5) */

    /* Protection level effects */
    float protection_throttle_factor;/**< Activity reduction at THROTTLE level (default 0.5) */
    float protection_shed_factor;    /**< Activity reduction at SHED_LOAD (default 0.2) */
    float protection_safe_factor;    /**< Activity reduction at SAFE_MODE (default 0.05) */
    bool enable_emergency_shutdown;  /**< Allow emergency shutdown to halt SNN (default true) */

    /* Circadian modulation */
    float circadian_amplitude;       /**< Amplitude of circadian modulation (default 0.3) */
    float circadian_peak_phase;      /**< Phase of peak activity (default MORNING=1) */
    bool enable_circadian_modulation;/**< Enable time-of-day effects (default true) */

    /* SNN → Medulla feedback */
    float activity_threat_threshold; /**< Firing rate threshold for threat (Hz, default 100) */
    float activity_emergency_threshold; /**< Rate for emergency trigger (Hz, default 200) */
    bool enable_activity_feedback;   /**< Allow SNN to trigger protection (default true) */

    /* Population configuration */
    uint32_t arousal_sensing_pop_id; /**< Population sensing arousal (0 = all) */
    uint32_t motor_output_pop_id;    /**< Population for motor output (0 = all) */

    /* Update timing */
    float update_interval_ms;        /**< How often to sync (default 10.0) */

    /* Bio-async */
    bool enable_bio_async;           /**< Enable bio-async messaging (default true) */
} snn_medulla_config_t;

/**
 * @brief Current state of SNN-medulla integration
 */
typedef struct snn_medulla_state_s {
    /* Current medulla readings */
    float current_arousal;           /**< Current arousal level [0, 1] */
    arousal_level_t arousal_level;   /**< Discrete arousal level */
    protection_level_t protection_level; /**< Current protection level */
    circadian_phase_t circadian_phase;   /**< Current circadian phase */

    /* Computed modulation factors */
    float arousal_rate_factor;       /**< Firing rate multiplier from arousal */
    float protection_gate;           /**< Activity gate from protection [0, 1] */
    float circadian_modulation;      /**< Circadian adjustment [-0.3, +0.3] */
    float combined_modulation;       /**< Combined modulation factor */

    /* SNN activity metrics */
    float avg_firing_rate_hz;        /**< Average population firing rate */
    float max_firing_rate_hz;        /**< Maximum population firing rate */
    bool activity_threat_triggered;  /**< High activity triggered threat */
    bool emergency_triggered;        /**< SNN triggered emergency shutdown */

    /* Statistics */
    uint32_t sync_count;             /**< Number of syncs performed */
    uint32_t emergency_count;        /**< Emergency shutdowns triggered */
    uint32_t protection_activations; /**< Protection level escalations */
    float avg_modulation;            /**< Running average modulation */
} snn_medulla_state_t;

/**
 * @brief SNN-medulla bridge structure
 */
typedef struct snn_medulla_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;              /**< SNN network reference */
    medulla_t medulla;               /**< Medulla reference */
    snn_medulla_config_t config;     /**< Bridge configuration */
    snn_medulla_state_t state;       /**< Current state */
    float last_update_time;          /**< Last update timestamp */
    bool bio_async_enabled;          /**< Bio-async connected */
    bio_module_context_t bio_ctx;    /**< Bio-async context */
    void* mutex;                     /**< Thread safety mutex */
} snn_medulla_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize configuration with defaults
 *
 * @param config Configuration to initialize
 */
void snn_medulla_config_default(snn_medulla_config_t* config);

/**
 * @brief Create SNN-medulla bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param snn SNN network (required)
 * @param medulla Medulla handle (can be NULL, set later)
 * @return Bridge handle or NULL on failure
 */
snn_medulla_bridge_t* snn_medulla_bridge_create(
    const snn_medulla_config_t* config,
    snn_network_t* snn,
    medulla_t medulla
);

/**
 * @brief Destroy SNN-medulla bridge
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void snn_medulla_bridge_destroy(snn_medulla_bridge_t* bridge);

//=============================================================================
// Bio-async Integration
//=============================================================================

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, negative on error
 */
int snn_medulla_bridge_connect_bio_async(snn_medulla_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, negative on error
 */
int snn_medulla_bridge_disconnect_bio_async(snn_medulla_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool snn_medulla_bridge_is_bio_async_connected(const snn_medulla_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect or update medulla reference
 *
 * @param bridge Bridge handle
 * @param medulla Medulla handle
 * @return 0 on success, negative on error
 */
int snn_medulla_bridge_connect_medulla(
    snn_medulla_bridge_t* bridge,
    medulla_t medulla
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update bridge state (sync medulla and SNN)
 *
 * WHAT: Main update function - read medulla, modulate SNN, check activity
 * WHY:  Keep SNN and medulla synchronized
 * HOW:
 *   1. Read arousal/protection/circadian from medulla
 *   2. Compute modulation factors
 *   3. Apply modulation to SNN populations
 *   4. Monitor SNN activity for threat detection
 *   5. Trigger protection if needed
 *
 * @param bridge Bridge handle
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int snn_medulla_bridge_update(snn_medulla_bridge_t* bridge, float dt);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Compute arousal-based firing rate modulation
 *
 * @param bridge Bridge handle
 * @return Modulation factor [arousal_min_rate_factor, arousal_max_rate_factor]
 */
float snn_medulla_compute_arousal_modulation(const snn_medulla_bridge_t* bridge);

/**
 * @brief Compute protection-based activity gate
 *
 * @param bridge Bridge handle
 * @return Gate factor [0, 1] where 0 = fully blocked
 */
float snn_medulla_compute_protection_gate(const snn_medulla_bridge_t* bridge);

/**
 * @brief Compute circadian-based modulation
 *
 * @param bridge Bridge handle
 * @return Modulation offset [-amplitude, +amplitude]
 */
float snn_medulla_compute_circadian_modulation(const snn_medulla_bridge_t* bridge);

/**
 * @brief Apply combined modulation to SNN populations
 *
 * @param bridge Bridge handle
 * @return 0 on success, negative on error
 */
int snn_medulla_apply_modulation(snn_medulla_bridge_t* bridge);

//=============================================================================
// Feedback Functions
//=============================================================================

/**
 * @brief Check SNN activity and trigger protection if needed
 *
 * @param bridge Bridge handle
 * @return 0 on success, positive if protection triggered, negative on error
 */
int snn_medulla_check_activity_threat(snn_medulla_bridge_t* bridge);

/**
 * @brief Trigger emergency shutdown via medulla
 *
 * @param bridge Bridge handle
 * @param reason Reason for emergency
 * @return 0 on success, negative on error
 */
int snn_medulla_trigger_emergency(
    snn_medulla_bridge_t* bridge,
    const char* reason
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state (can be NULL to just get return code)
 * @return 0 on success, negative on error
 */
int snn_medulla_bridge_get_state(
    const snn_medulla_bridge_t* bridge,
    snn_medulla_state_t* state
);

/**
 * @brief Get current arousal level from medulla
 *
 * @param bridge Bridge handle
 * @return Arousal level [0, 1] or -1 on error
 */
float snn_medulla_get_arousal(const snn_medulla_bridge_t* bridge);

/**
 * @brief Get current protection level from medulla
 *
 * @param bridge Bridge handle
 * @return Protection level enum
 */
protection_level_t snn_medulla_get_protection_level(const snn_medulla_bridge_t* bridge);

/**
 * @brief Get current circadian phase from medulla
 *
 * @param bridge Bridge handle
 * @return Circadian phase enum
 */
circadian_phase_t snn_medulla_get_circadian_phase(const snn_medulla_bridge_t* bridge);

/**
 * @brief Get combined modulation factor
 *
 * @param bridge Bridge handle
 * @return Combined modulation factor
 */
float snn_medulla_get_combined_modulation(const snn_medulla_bridge_t* bridge);

/**
 * @brief Check if SNN is currently activity-restricted
 *
 * @param bridge Bridge handle
 * @return true if protection level is restricting activity
 */
bool snn_medulla_is_activity_restricted(const snn_medulla_bridge_t* bridge);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param sync_count Output sync count
 * @param emergency_count Output emergency count
 * @param avg_modulation Output average modulation
 * @return 0 on success, negative on error
 */
int snn_medulla_get_stats(
    const snn_medulla_bridge_t* bridge,
    uint32_t* sync_count,
    uint32_t* emergency_count,
    float* avg_modulation
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 */
void snn_medulla_reset_stats(snn_medulla_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_MEDULLA_BRIDGE_H */
