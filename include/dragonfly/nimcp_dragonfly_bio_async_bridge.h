/**
 * @file nimcp_dragonfly_bio_async_bridge.h
 * @brief Dragonfly-to-Bio-Async Integration Bridge
 *
 * WHAT: Bridges dragonfly target tracking to bio-async system
 * WHY:  Enable biologically-inspired async operations for tracking
 * HOW:  Neuromodulator channels for priority, phase sync for coordinated pursuit
 *
 * BIOLOGICAL BASIS:
 * - Dopamine signals for successful interception (reward)
 * - Norepinephrine for alert/pursuit escalation
 * - Acetylcholine for attention focusing on target
 * - Phase coupling for coordinated TSDN responses
 *
 * @author NIMCP Development Team
 * @date 2025-12-28
 */

#ifndef NIMCP_DRAGONFLY_BIO_ASYNC_BRIDGE_H
#define NIMCP_DRAGONFLY_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct dragonfly_system_s;
typedef struct dragonfly_system_s dragonfly_system_t;

//=============================================================================
// Constants
//=============================================================================

#define DRAGONFLY_BIO_MAX_FUTURES 16        /**< Max concurrent futures */
#define DRAGONFLY_BIO_DEFAULT_PRIORITY 0.5f /**< Default priority level */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Neuromodulator channels for dragonfly operations
 */
typedef enum {
    DRAGONFLY_CHANNEL_DOPAMINE = 0,     /**< Reward/success signals */
    DRAGONFLY_CHANNEL_NOREPINEPHRINE,   /**< Alert/pursuit escalation */
    DRAGONFLY_CHANNEL_ACETYLCHOLINE,    /**< Attention focus */
    DRAGONFLY_CHANNEL_SEROTONIN         /**< State coordination */
} dragonfly_neuromod_channel_t;

/**
 * @brief Async operation types
 */
typedef enum {
    DRAGONFLY_ASYNC_TSDN_UPDATE = 0,    /**< TSDN population update */
    DRAGONFLY_ASYNC_TRACKING,           /**< Target tracking step */
    DRAGONFLY_ASYNC_PREDICTION,         /**< Trajectory prediction */
    DRAGONFLY_ASYNC_INTERCEPT,          /**< Interception calculation */
    DRAGONFLY_ASYNC_MODE_SWITCH,        /**< Mode transition */
    DRAGONFLY_ASYNC_FULL_CYCLE          /**< Complete tracking cycle */
} dragonfly_async_op_t;

/**
 * @brief Phase synchronization mode
 */
typedef enum {
    DRAGONFLY_PHASE_GAMMA = 0,          /**< High-frequency (30-100 Hz) */
    DRAGONFLY_PHASE_BETA,               /**< Mid-frequency (12-30 Hz) */
    DRAGONFLY_PHASE_ALPHA,              /**< Low-frequency (8-12 Hz) */
    DRAGONFLY_PHASE_THETA               /**< Very low (4-8 Hz) */
} dragonfly_phase_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Future handle for async operations
 */
typedef struct dragonfly_bio_future_s dragonfly_bio_future_t;

/**
 * @brief Async operation result
 */
typedef struct {
    dragonfly_async_op_t operation;     /**< Operation type */
    float result_data[16];              /**< Result values */
    uint32_t result_size;               /**< Number of result values */
    float confidence;                   /**< Result confidence [0-1] */
    float latency_ms;                   /**< Operation latency */
    bool success;                       /**< Operation succeeded */
} dragonfly_async_result_t;

/**
 * @brief Configuration for bio-async bridge
 */
typedef struct {
    /* Neuromodulator settings */
    float dopamine_decay_rate;          /**< Decay rate for dopamine channel */
    float norepinephrine_threshold;     /**< Alert threshold */
    float acetylcholine_focus_gain;     /**< Attention focus gain */

    /* Phase coupling */
    dragonfly_phase_mode_t default_phase; /**< Default oscillation band */
    float phase_coherence_threshold;    /**< Coherence threshold for sync */

    /* Priority */
    float base_priority;                /**< Base operation priority */
    float pursuit_priority_boost;       /**< Priority boost during pursuit */
    float intercept_priority_boost;     /**< Priority boost during intercept */

    /* Timeouts */
    float default_timeout_ms;           /**< Default operation timeout */
    bool enable_timeout_escalation;     /**< Escalate priority on timeout */
} dragonfly_bio_async_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t operations_started;
    uint64_t operations_completed;
    uint64_t operations_failed;
    uint64_t timeouts;
    float avg_latency_ms;
    float avg_confidence;
    float dopamine_level;
    float norepinephrine_level;
    float coherence_level;
} dragonfly_bio_async_stats_t;

/**
 * @brief Bio-async bridge handle
 */
typedef struct dragonfly_bio_async_bridge_s dragonfly_bio_async_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_bio_async_bridge_default_config(dragonfly_bio_async_config_t* config);
int dragonfly_bio_async_bridge_validate_config(const dragonfly_bio_async_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_bio_async_bridge_t* dragonfly_bio_async_bridge_create(
    dragonfly_system_t* dragonfly,
    void* bio_async_system,
    const dragonfly_bio_async_config_t* config
);

void dragonfly_bio_async_bridge_destroy(dragonfly_bio_async_bridge_t* bridge);
int dragonfly_bio_async_bridge_reset(dragonfly_bio_async_bridge_t* bridge);

//=============================================================================
// Async Operations
//=============================================================================

dragonfly_bio_future_t* dragonfly_bio_async_start(
    dragonfly_bio_async_bridge_t* bridge,
    dragonfly_async_op_t operation,
    const float* input,
    uint32_t input_size
);

int dragonfly_bio_future_wait(
    dragonfly_bio_future_t* future,
    dragonfly_async_result_t* result,
    float timeout_ms
);

bool dragonfly_bio_future_is_ready(const dragonfly_bio_future_t* future);
float dragonfly_bio_future_get_confidence(const dragonfly_bio_future_t* future);
void dragonfly_bio_future_destroy(dragonfly_bio_future_t* future);

//=============================================================================
// Neuromodulator Signaling
//=============================================================================

int dragonfly_bio_async_signal_reward(
    dragonfly_bio_async_bridge_t* bridge,
    float reward_magnitude
);

int dragonfly_bio_async_signal_alert(
    dragonfly_bio_async_bridge_t* bridge,
    float alert_level
);

int dragonfly_bio_async_signal_focus(
    dragonfly_bio_async_bridge_t* bridge,
    float focus_level
);

float dragonfly_bio_async_get_dopamine(const dragonfly_bio_async_bridge_t* bridge);
float dragonfly_bio_async_get_norepinephrine(const dragonfly_bio_async_bridge_t* bridge);
float dragonfly_bio_async_get_acetylcholine(const dragonfly_bio_async_bridge_t* bridge);

//=============================================================================
// Phase Synchronization
//=============================================================================

int dragonfly_bio_async_set_phase_mode(
    dragonfly_bio_async_bridge_t* bridge,
    dragonfly_phase_mode_t mode
);

float dragonfly_bio_async_get_coherence(const dragonfly_bio_async_bridge_t* bridge);

int dragonfly_bio_async_sync_futures(
    dragonfly_bio_async_bridge_t* bridge,
    dragonfly_bio_future_t** futures,
    uint32_t num_futures,
    float coherence_threshold
);

//=============================================================================
// Integration
//=============================================================================

int dragonfly_bio_async_connect_dragonfly(
    dragonfly_bio_async_bridge_t* bridge,
    dragonfly_system_t* dragonfly
);

int dragonfly_bio_async_connect_system(
    dragonfly_bio_async_bridge_t* bridge,
    void* bio_async_system
);

int dragonfly_bio_async_update(dragonfly_bio_async_bridge_t* bridge, float dt_ms);

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_bio_async_bridge_get_stats(
    const dragonfly_bio_async_bridge_t* bridge,
    dragonfly_bio_async_stats_t* stats
);

int dragonfly_bio_async_bridge_reset_stats(dragonfly_bio_async_bridge_t* bridge);

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_bio_async_channel_name(dragonfly_neuromod_channel_t channel);
const char* dragonfly_bio_async_op_name(dragonfly_async_op_t op);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_BIO_ASYNC_BRIDGE_H */
