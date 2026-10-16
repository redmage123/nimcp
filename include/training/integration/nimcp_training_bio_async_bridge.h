/**
 * @file nimcp_training_bio_async_bridge.h
 * @brief Bio-Async Integration Bridge for Training Module
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bidirectional bridge connecting training modules with bio-async system
 * WHY:  Bio-async provides coordination, futures, and neuromodulator-based signaling
 *       for distributed training coordination
 * HOW:  Full bridge pattern with message handlers, gradient synchronization,
 *       and checkpoint coordination
 *
 * BIOLOGICAL BASIS:
 * Training processes in biological neural networks involve:
 * - Dopamine signals learning rate modulation and reward-based updates
 * - Norepinephrine signals priority gradients and urgent synchronization
 * - Acetylcholine modulates attention to specific parameter updates
 * - Serotonin reflects overall training state and convergence
 * - Phase coupling synchronizes distributed gradient updates
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * |   TRAINING MODULE    |                    |     BIO-ASYNC        |
 * |                      |                    |                      |
 * | - Optimizer          |<-- neuromodulator->| - Message Router     |
 * | - Gradient Manager   |    channels        | - Future Manager     |
 * | - LR Scheduler       |                    | - Phase Sync         |
 * | - Checkpoint Manager |<-- phase coupling->| - Glial Waves        |
 * | - Distributed Coord  |                    | - Oscillators        |
 * +----------------------+                    +----------------------+
 *           |                                           |
 *           +---------------- BRIDGE -------------------+
 *                      (bidirectional flow)
 * ```
 *
 * MODULE ID ALLOCATION:
 * - 0x2100: Training Bio-Async Bridge
 * - 0x2101: Gradient Synchronization
 * - 0x2102: Checkpoint Coordination
 * - 0x2103: Loss Broadcasting
 * - 0x2104: Distributed Training Coordination
 * - 0x2105: Learning Rate Signaling
 *
 * INTEGRATION POINTS:
 * - training_integration_hub: Event routing
 * - distributed_training: Multi-node coordination
 * - gradient_manager: Gradient synchronization
 * - optimizers: Weight update coordination
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRAINING_BIO_ASYNC_BRIDGE_H
#define NIMCP_TRAINING_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "training/integration/nimcp_training_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct nimcp_bio_async;
struct nimcp_bio_future;
struct nimcp_phase_sync;
struct nimcp_glial_wave;
struct nimcp_gradient_manager_ctx;
struct dist_ctx_s;
struct training_integration_hub_struct;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Module ID for training bio-async bridge */
#define TRAIN_BIO_MODULE_BRIDGE             0x2100

/** Module ID for gradient synchronization */
#define TRAIN_BIO_MODULE_GRADIENT_SYNC      0x2101

/** Module ID for checkpoint coordination */
#define TRAIN_BIO_MODULE_CHECKPOINT         0x2102

/** Module ID for loss broadcasting */
#define TRAIN_BIO_MODULE_LOSS               0x2103

/** Module ID for distributed training */
#define TRAIN_BIO_MODULE_DISTRIBUTED        0x2104

/** Module ID for learning rate signaling */
#define TRAIN_BIO_MODULE_LR                 0x2105

/** Default dopamine release on epoch completion */
#define TRAIN_BIO_DEFAULT_EPOCH_DOPAMINE    0.4f

/** Default norepinephrine for gradient explosion warning */
#define TRAIN_BIO_DEFAULT_GRAD_EXPLODE_NE   0.9f

/** Default coherence threshold for distributed sync */
#define TRAIN_BIO_DEFAULT_SYNC_COHERENCE    0.85f

/** Maximum number of distributed workers */
#define TRAIN_BIO_MAX_WORKERS               256

/** Maximum number of message handlers */
#define TRAIN_BIO_MAX_HANDLERS              64

/** Default message queue size */
#define TRAIN_BIO_DEFAULT_QUEUE_SIZE        512

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

/**
 * @brief Training bio-async bridge error codes
 */
typedef enum {
    TRAIN_BIO_OK                        = 0,      /**< Success */
    TRAIN_BIO_ERROR_NULL_POINTER        = -1,     /**< NULL pointer argument */
    TRAIN_BIO_ERROR_NOT_CONNECTED       = -2,     /**< Bridge not connected */
    TRAIN_BIO_ERROR_INVALID_CONFIG      = -3,     /**< Invalid configuration */
    TRAIN_BIO_ERROR_HANDLERS_FULL       = -4,     /**< Handler table full */
    TRAIN_BIO_ERROR_SYNC_TIMEOUT        = -5,     /**< Synchronization timeout */
    TRAIN_BIO_ERROR_GRADIENT_OVERFLOW   = -6,     /**< Gradient overflow detected */
    TRAIN_BIO_ERROR_CHECKPOINT_FAILED   = -7,     /**< Checkpoint operation failed */
    TRAIN_BIO_ERROR_WORKER_DISCONNECT   = -8,     /**< Worker disconnected */
    TRAIN_BIO_ERROR_QUEUE_FULL          = -9,     /**< Message queue full */
    TRAIN_BIO_ERROR_ALLOC_FAILED        = -10,    /**< Memory allocation failed */
    TRAIN_BIO_ERROR_ALREADY_CONNECTED   = -11,    /**< Already connected */
    TRAIN_BIO_ERROR_INVALID_WORKER_ID   = -12,    /**< Invalid worker ID */
    TRAIN_BIO_ERROR_PHASE_INCOHERENT    = -13,    /**< Phase synchronization failed */
    TRAIN_BIO_ERROR_WAVE_TIMEOUT        = -14     /**< Glial wave timeout */
} training_bio_error_t;

/*=============================================================================
 * MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief Bio-async message types for training coordination
 */
typedef enum {
    /* Batch/Epoch messages (0x2100xx) */
    TRAIN_MSG_BATCH_START           = 0x210001,  /**< Batch processing started */
    TRAIN_MSG_BATCH_COMPLETE        = 0x210002,  /**< Batch processing completed */
    TRAIN_MSG_EPOCH_START           = 0x210003,  /**< Epoch started */
    TRAIN_MSG_EPOCH_DONE            = 0x210004,  /**< Epoch completed */
    TRAIN_MSG_TRAINING_START        = 0x210005,  /**< Training session started */
    TRAIN_MSG_TRAINING_STOP         = 0x210006,  /**< Training session stopped */

    /* Gradient messages (0x2101xx) */
    TRAIN_MSG_GRADIENT_COMPUTED     = 0x210101,  /**< Gradients computed locally */
    TRAIN_MSG_GRADIENT_UPDATE       = 0x210102,  /**< Gradient update available */
    TRAIN_MSG_GRADIENT_SYNC_START   = 0x210103,  /**< Gradient sync initiated */
    TRAIN_MSG_GRADIENT_SYNC_DONE    = 0x210104,  /**< Gradient sync completed */
    TRAIN_MSG_GRADIENT_EXPLOSION    = 0x210105,  /**< Gradient explosion detected */
    TRAIN_MSG_GRADIENT_VANISHING    = 0x210106,  /**< Vanishing gradients detected */

    /* Checkpoint messages (0x2102xx) */
    TRAIN_MSG_CHECKPOINT_START      = 0x210201,  /**< Checkpoint save initiated */
    TRAIN_MSG_CHECKPOINT_COMPLETE   = 0x210202,  /**< Checkpoint save completed */
    TRAIN_MSG_CHECKPOINT_LOAD       = 0x210203,  /**< Checkpoint load initiated */
    TRAIN_MSG_CHECKPOINT_LOADED     = 0x210204,  /**< Checkpoint load completed */
    TRAIN_MSG_BEST_MODEL_UPDATE     = 0x210205,  /**< New best model saved */

    /* Loss messages (0x2103xx) */
    TRAIN_MSG_LOSS_COMPUTED         = 0x210301,  /**< Loss value computed */
    TRAIN_MSG_LOSS_BROADCAST        = 0x210302,  /**< Loss broadcast to workers */
    TRAIN_MSG_LOSS_AGGREGATE        = 0x210303,  /**< Aggregated loss computed */
    TRAIN_MSG_VALIDATION_LOSS       = 0x210304,  /**< Validation loss computed */
    TRAIN_MSG_LOSS_IMPROVED         = 0x210305,  /**< Loss improvement detected */
    TRAIN_MSG_LOSS_PLATEAU          = 0x210306,  /**< Loss plateau detected */

    /* Distributed coordination messages (0x2104xx) */
    TRAIN_MSG_WORKER_READY          = 0x210401,  /**< Worker ready for training */
    TRAIN_MSG_WORKER_DONE           = 0x210402,  /**< Worker completed iteration */
    TRAIN_MSG_SYNC_BARRIER          = 0x210403,  /**< Synchronization barrier */
    TRAIN_MSG_PARAM_BROADCAST       = 0x210404,  /**< Parameter broadcast */
    TRAIN_MSG_ALL_REDUCE_START      = 0x210405,  /**< All-reduce initiated */
    TRAIN_MSG_ALL_REDUCE_DONE       = 0x210406,  /**< All-reduce completed */
    TRAIN_MSG_WORKER_STRAGGLER      = 0x210407,  /**< Straggler worker detected */

    /* Learning rate messages (0x2105xx) */
    TRAIN_MSG_LR_ADJUSTED           = 0x210501,  /**< Learning rate adjusted */
    TRAIN_MSG_LR_WARMUP_DONE        = 0x210502,  /**< LR warmup completed */
    TRAIN_MSG_LR_DECAY_STEP         = 0x210503,  /**< LR decay step applied */
    TRAIN_MSG_LR_SCHEDULE_CHANGE    = 0x210504   /**< LR schedule changed */
} training_bio_message_type_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Effects flowing from training to bio-async
 *
 * WHAT: Signals generated by training for system-wide coordination
 * WHY:  Other brain modules need to know about training state
 */
typedef struct {
    /* Dopamine channel - reward/progress */
    float dopamine_release;              /**< Training progress reward [0.0-1.0] */
    uint32_t completed_epochs;           /**< Number of epochs completed */
    uint32_t completed_batches;          /**< Number of batches completed */
    float loss_improvement;              /**< Recent loss improvement */

    /* Norepinephrine channel - priority/alerts */
    float norepinephrine_level;          /**< Alert level [0.0-1.0] */
    bool gradient_explosion_alert;       /**< Gradient explosion detected */
    bool gradient_vanishing_alert;       /**< Vanishing gradients detected */
    bool sync_timeout_alert;             /**< Sync timeout warning */

    /* Acetylcholine channel - attention/focus */
    float acetylcholine_level;           /**< Attention level [0.0-1.0] */
    uint32_t focused_layer;              /**< Currently focused layer index */
    float gradient_attention;            /**< Attention on gradient updates */

    /* Serotonin channel - state/convergence */
    float serotonin_level;               /**< Training state level [0.0-1.0] */
    bool convergence_detected;           /**< Model appears to be converging */
    bool early_stopping_triggered;       /**< Early stopping activated */
    float training_progress;             /**< Overall training progress [0.0-1.0] */

    /* Phase coupling requests */
    bool request_gradient_sync;          /**< Request gradient synchronization */
    uint32_t workers_to_sync;            /**< Number of workers to synchronize */
    float desired_sync_coherence;        /**< Desired synchronization coherence */

    /* Glial wave triggers */
    bool trigger_checkpoint_wave;        /**< Trigger checkpoint save wave */
    bool trigger_lr_adjustment_wave;     /**< Trigger LR adjustment wave */
} training_to_bio_async_effects_t;

/**
 * @brief Effects flowing from bio-async to training
 *
 * WHAT: Signals from bio-async that modulate training
 * WHY:  Training should adapt to system-wide state
 */
typedef struct {
    /* Global state effects */
    float global_arousal;                /**< Global arousal level [0.0-1.0] */
    float global_valence;                /**< Global valence [-1.0 to 1.0] */
    bool system_overload;                /**< System is overloaded */

    /* Gradient synchronization status */
    bool gradient_sync_achieved;         /**< Gradient sync completed */
    float current_sync_coherence;        /**< Current sync coherence [0.0-1.0] */
    uint32_t synchronized_workers;       /**< Number of synchronized workers */
    uint32_t total_workers;              /**< Total number of workers */

    /* Oscillation band recommendations */
    uint8_t recommended_band;            /**< Recommended oscillation band */
    float band_power;                    /**< Power in recommended band */

    /* Glial wave effects */
    bool glial_wave_active;              /**< Glial wave is propagating */
    float wave_intensity;                /**< Intensity of current wave */
    uint32_t wave_origin_module;         /**< Module that initiated wave */

    /* Resource availability */
    float available_compute;             /**< Available compute capacity [0.0-1.0] */
    float available_memory;              /**< Available memory capacity [0.0-1.0] */
    bool throttling_active;              /**< Whether throttling is active */

    /* Timing signals */
    uint64_t current_phase_ms;           /**< Current phase in milliseconds */
    float circadian_factor;              /**< Circadian modulation factor */

    /* Distributed training status */
    uint32_t active_workers;             /**< Currently active workers */
    uint32_t straggler_count;            /**< Number of straggler workers */
    float avg_worker_throughput;         /**< Average worker throughput */
} bio_async_to_training_effects_t;

/*=============================================================================
 * GRADIENT SYNC PAYLOAD
 *===========================================================================*/

/**
 * @brief Gradient synchronization message payload
 */
typedef struct {
    uint32_t worker_id;                  /**< Worker that computed gradients */
    uint32_t batch_id;                   /**< Batch ID */
    uint32_t epoch;                      /**< Current epoch */
    float gradient_norm;                 /**< L2 norm of gradients */
    float* gradients;                    /**< Gradient buffer (optional) */
    size_t gradient_count;               /**< Number of gradient elements */
    bool compressed;                     /**< Whether gradients are compressed */
    float compression_ratio;             /**< Compression ratio if compressed */
} training_gradient_sync_payload_t;

/* training_loss_payload_t and training_checkpoint_payload_t are defined in
 * nimcp_training_event_types.h (included above) — not redefined here. */

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bio-async bridge configuration for training
 */
typedef struct {
    /* Neuromodulator sensitivity */
    float dopamine_sensitivity;          /**< Sensitivity to dopamine signals */
    float norepinephrine_sensitivity;    /**< Sensitivity to NE signals */
    float acetylcholine_sensitivity;     /**< Sensitivity to ACh signals */
    float serotonin_sensitivity;         /**< Sensitivity to 5-HT signals */

    /* Gradient synchronization */
    float sync_coherence_threshold;      /**< Threshold for gradient sync */
    uint32_t sync_timeout_ms;            /**< Timeout for synchronization */
    bool enable_gradient_compression;    /**< Enable gradient compression */
    float compression_threshold;         /**< Gradient compression threshold */

    /* Phase coupling parameters */
    float coupling_strength;             /**< Strength of phase coupling */
    uint8_t default_oscillation_band;    /**< Default oscillation band */

    /* Message routing */
    bool enable_message_logging;         /**< Log all bio-async messages */
    uint32_t message_queue_size;         /**< Size of message queue */

    /* Distributed training */
    uint32_t max_workers;                /**< Maximum number of workers */
    uint32_t straggler_timeout_ms;       /**< Timeout for straggler detection */
    bool enable_async_gradient_sync;     /**< Enable async gradient sync */

    /* Checkpoint coordination */
    bool enable_checkpoint_waves;        /**< Enable checkpoint via glial waves */
    float checkpoint_wave_threshold;     /**< Threshold to trigger checkpoint */

    /* Glial wave parameters */
    float glial_wave_decay;              /**< Decay rate of glial waves */
} training_bio_bridge_config_t;

/*=============================================================================
 * BRIDGE HANDLE
 *===========================================================================*/

/**
 * @brief Training bio-async bridge opaque handle
 */
typedef struct training_bio_async_bridge training_bio_async_bridge_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Message handler callback type
 */
typedef void (*training_bio_message_handler_t)(
    training_bio_message_type_t type,
    const void* payload,
    size_t payload_size,
    void* user_data
);

/**
 * @brief Gradient sync callback type
 */
typedef void (*training_gradient_sync_callback_t)(
    const training_gradient_sync_payload_t* payload,
    void* user_data
);

/**
 * @brief Loss broadcast callback type
 */
typedef void (*training_loss_callback_t)(
    const training_loss_payload_t* payload,
    void* user_data
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message statistics */
    uint64_t messages_sent;              /**< Total messages sent */
    uint64_t messages_received;          /**< Total messages received */
    uint64_t messages_dropped;           /**< Messages dropped (queue full) */

    /* Gradient sync statistics */
    uint64_t gradient_syncs;             /**< Total gradient syncs */
    uint64_t gradient_sync_timeouts;     /**< Gradient sync timeouts */
    float avg_gradient_norm;             /**< Average gradient norm */
    float avg_sync_time_ms;              /**< Average sync time */

    /* Loss statistics */
    uint64_t loss_broadcasts;            /**< Total loss broadcasts */
    float avg_loss;                      /**< Running average loss */
    float best_loss;                     /**< Best loss seen */

    /* Checkpoint statistics */
    uint64_t checkpoints_saved;          /**< Checkpoints saved */
    uint64_t checkpoints_loaded;         /**< Checkpoints loaded */

    /* Distributed statistics */
    uint64_t worker_syncs;               /**< Total worker synchronizations */
    uint64_t straggler_events;           /**< Straggler detections */
    float avg_worker_throughput;         /**< Average worker throughput */

    /* Phase coupling statistics */
    uint64_t phase_syncs_created;        /**< Phase syncs created */
    uint64_t phase_syncs_achieved;       /**< Phase syncs achieved */
    float avg_coherence;                 /**< Average coherence achieved */

    /* Glial wave statistics */
    uint64_t glial_waves_initiated;      /**< Glial waves initiated */
    float avg_wave_intensity;            /**< Average wave intensity */

    /* Timing */
    uint64_t total_update_time_us;       /**< Total update time (microseconds) */
} training_bio_bridge_stats_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Get default configuration
 * @return Default configuration
 */
training_bio_bridge_config_t training_bio_bridge_default_config(void);

/**
 * @brief Create bio-async bridge with configuration
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
training_bio_async_bridge_t* training_bio_bridge_create(
    const training_bio_bridge_config_t* config
);

/**
 * @brief Create bridge with default configuration
 * @return Bridge handle or NULL on error
 */
training_bio_async_bridge_t* training_bio_bridge_create_default(void);

/**
 * @brief Destroy bio-async bridge
 * @param bridge Bridge handle (NULL safe)
 */
void training_bio_bridge_destroy(training_bio_async_bridge_t* bridge);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect bridge to bio-async system
 * @param bridge Bridge handle
 * @param bio_async Bio-async system handle
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_connect(
    training_bio_async_bridge_t* bridge,
    struct nimcp_bio_async* bio_async
);

/**
 * @brief Connect bridge to training integration hub
 * @param bridge Bridge handle
 * @param hub Training integration hub handle
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_connect_hub(
    training_bio_async_bridge_t* bridge,
    struct training_integration_hub_struct* hub
);

/**
 * @brief Connect bridge to gradient manager
 * @param bridge Bridge handle
 * @param grad_mgr Gradient manager handle
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_connect_gradient_manager(
    training_bio_async_bridge_t* bridge,
    struct nimcp_gradient_manager_ctx* grad_mgr
);

/**
 * @brief Connect bridge to distributed training context
 * @param bridge Bridge handle
 * @param dist_ctx Distributed training context
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_connect_distributed(
    training_bio_async_bridge_t* bridge,
    struct dist_ctx_s* dist_ctx
);

/**
 * @brief Disconnect from bio-async system
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_disconnect(training_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 * @param bridge Bridge handle
 * @return true if connected
 */
bool training_bio_bridge_is_connected(const training_bio_async_bridge_t* bridge);

/*=============================================================================
 * UPDATE
 *===========================================================================*/

/**
 * @brief Update bridge state (call each frame/tick)
 * @param bridge Bridge handle
 * @param delta_time_ms Time since last update in milliseconds
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_update(
    training_bio_async_bridge_t* bridge,
    float delta_time_ms
);

/*=============================================================================
 * MESSAGING
 *===========================================================================*/

/**
 * @brief Send message through bio-async
 * @param bridge Bridge handle
 * @param message_type Message type
 * @param payload Message payload
 * @param payload_size Payload size
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_send_message(
    training_bio_async_bridge_t* bridge,
    training_bio_message_type_t message_type,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Register message handler callback
 * @param bridge Bridge handle
 * @param message_type Message type to handle
 * @param handler Handler function
 * @param user_data User data for callback
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_register_handler(
    training_bio_async_bridge_t* bridge,
    training_bio_message_type_t message_type,
    training_bio_message_handler_t handler,
    void* user_data
);

/**
 * @brief Unregister message handler
 * @param bridge Bridge handle
 * @param message_type Message type to unregister
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_unregister_handler(
    training_bio_async_bridge_t* bridge,
    training_bio_message_type_t message_type
);

/*=============================================================================
 * TRAINING COORDINATION
 *===========================================================================*/

/**
 * @brief Signal batch completion
 * @param bridge Bridge handle
 * @param epoch Current epoch
 * @param batch Batch index
 * @param loss Loss value for batch
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_signal_batch_complete(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    uint32_t batch,
    float loss
);

/**
 * @brief Signal epoch completion
 * @param bridge Bridge handle
 * @param epoch Completed epoch number
 * @param avg_loss Average loss for epoch
 * @param val_loss Validation loss (or -1 if not available)
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_signal_epoch_complete(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    float avg_loss,
    float val_loss
);

/**
 * @brief Signal learning rate adjustment
 * @param bridge Bridge handle
 * @param old_lr Previous learning rate
 * @param new_lr New learning rate
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_signal_lr_adjusted(
    training_bio_async_bridge_t* bridge,
    float old_lr,
    float new_lr
);

/**
 * @brief Signal early stopping
 * @param bridge Bridge handle
 * @param epoch Epoch at which stopping occurred
 * @param best_loss Best loss achieved
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_signal_early_stopping(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    float best_loss
);

/*=============================================================================
 * GRADIENT SYNCHRONIZATION
 *===========================================================================*/

/**
 * @brief Initiate gradient synchronization
 * @param bridge Bridge handle
 * @param worker_id This worker's ID
 * @param gradients Gradient buffer
 * @param gradient_count Number of gradient elements
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_sync_gradients(
    training_bio_async_bridge_t* bridge,
    uint32_t worker_id,
    const float* gradients,
    size_t gradient_count
);

/**
 * @brief Wait for gradient synchronization to complete
 * @param bridge Bridge handle
 * @param timeout_ms Timeout in milliseconds
 * @param out_gradients Output buffer for synchronized gradients
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_await_gradient_sync(
    training_bio_async_bridge_t* bridge,
    uint32_t timeout_ms,
    float* out_gradients
);

/**
 * @brief Create phase synchronization for gradient update
 * @param bridge Bridge handle
 * @param worker_ids Array of worker IDs
 * @param worker_count Number of workers
 * @param sync Output phase sync handle
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_create_gradient_phase_sync(
    training_bio_async_bridge_t* bridge,
    const uint32_t* worker_ids,
    size_t worker_count,
    struct nimcp_phase_sync** sync
);

/**
 * @brief Wait for gradient phase coherence
 * @param bridge Bridge handle
 * @param sync Phase sync handle
 * @param coherence_threshold Required coherence level
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_wait_gradient_coherent(
    training_bio_async_bridge_t* bridge,
    struct nimcp_phase_sync* sync,
    float coherence_threshold,
    uint32_t timeout_ms
);

/**
 * @brief Register gradient sync callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_on_gradient_sync(
    training_bio_async_bridge_t* bridge,
    training_gradient_sync_callback_t callback,
    void* user_data
);

/*=============================================================================
 * LOSS BROADCASTING
 *===========================================================================*/

/**
 * @brief Broadcast loss to all workers
 * @param bridge Bridge handle
 * @param payload Loss payload
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_broadcast_loss(
    training_bio_async_bridge_t* bridge,
    const training_loss_payload_t* payload
);

/**
 * @brief Aggregate loss from all workers
 * @param bridge Bridge handle
 * @param timeout_ms Timeout in milliseconds
 * @param out_avg_loss Output average loss
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_aggregate_loss(
    training_bio_async_bridge_t* bridge,
    uint32_t timeout_ms,
    float* out_avg_loss
);

/**
 * @brief Register loss broadcast callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_on_loss_broadcast(
    training_bio_async_bridge_t* bridge,
    training_loss_callback_t callback,
    void* user_data
);

/*=============================================================================
 * CHECKPOINT COORDINATION
 *===========================================================================*/

/**
 * @brief Signal checkpoint save initiation
 * @param bridge Bridge handle
 * @param epoch Current epoch
 * @param global_step Global training step
 * @param is_best Whether this is the best model
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_signal_checkpoint_start(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    uint32_t global_step,
    bool is_best
);

/**
 * @brief Signal checkpoint save completion
 * @param bridge Bridge handle
 * @param payload Checkpoint payload
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_signal_checkpoint_complete(
    training_bio_async_bridge_t* bridge,
    const training_checkpoint_payload_t* payload
);

/**
 * @brief Initiate checkpoint via glial wave
 * @param bridge Bridge handle
 * @param epoch Current epoch
 * @param wave Output wave handle
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_initiate_checkpoint_wave(
    training_bio_async_bridge_t* bridge,
    uint32_t epoch,
    struct nimcp_glial_wave** wave
);

/**
 * @brief Wait for checkpoint wave to propagate
 * @param bridge Bridge handle
 * @param wave Wave handle
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_await_checkpoint_wave(
    training_bio_async_bridge_t* bridge,
    struct nimcp_glial_wave* wave,
    uint32_t timeout_ms
);

/*=============================================================================
 * DISTRIBUTED TRAINING COORDINATION
 *===========================================================================*/

/**
 * @brief Register worker with distributed training
 * @param bridge Bridge handle
 * @param worker_id Worker ID
 * @param is_coordinator Whether this worker is the coordinator
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_register_worker(
    training_bio_async_bridge_t* bridge,
    uint32_t worker_id,
    bool is_coordinator
);

/**
 * @brief Signal worker ready for synchronization
 * @param bridge Bridge handle
 * @param worker_id Worker ID
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_signal_worker_ready(
    training_bio_async_bridge_t* bridge,
    uint32_t worker_id
);

/**
 * @brief Wait for all workers to be ready (barrier)
 * @param bridge Bridge handle
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_barrier(
    training_bio_async_bridge_t* bridge,
    uint32_t timeout_ms
);

/**
 * @brief Broadcast parameters to all workers
 * @param bridge Bridge handle
 * @param source_worker_id Source worker ID
 * @param params Parameter buffer
 * @param param_count Number of parameters
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_broadcast_params(
    training_bio_async_bridge_t* bridge,
    uint32_t source_worker_id,
    const float* params,
    size_t param_count
);

/**
 * @brief Get number of active workers
 * @param bridge Bridge handle
 * @return Number of active workers, or 0 on error
 */
uint32_t training_bio_bridge_get_active_workers(
    const training_bio_async_bridge_t* bridge
);

/**
 * @brief Check for straggler workers
 * @param bridge Bridge handle
 * @param straggler_ids Output array for straggler IDs
 * @param max_stragglers Maximum stragglers to return
 * @return Number of stragglers found
 */
uint32_t training_bio_bridge_check_stragglers(
    training_bio_async_bridge_t* bridge,
    uint32_t* straggler_ids,
    uint32_t max_stragglers
);

/*=============================================================================
 * NEUROMODULATOR CONTROL
 *===========================================================================*/

/**
 * @brief Release dopamine (training progress signal)
 * @param bridge Bridge handle
 * @param amount Amount to release [0.0-1.0]
 * @param trigger Type of trigger (epoch complete, loss improved, etc.)
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_release_dopamine(
    training_bio_async_bridge_t* bridge,
    float amount,
    training_bio_message_type_t trigger
);

/**
 * @brief Signal priority escalation via norepinephrine
 * @param bridge Bridge handle
 * @param priority Priority level [0.0-1.0]
 * @param alert_type Type of alert
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_signal_priority(
    training_bio_async_bridge_t* bridge,
    float priority,
    training_bio_message_type_t alert_type
);

/**
 * @brief Modulate attention via acetylcholine
 * @param bridge Bridge handle
 * @param attention Attention level [0.0-1.0]
 * @param layer_idx Layer to focus on (or -1 for all)
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_modulate_attention(
    training_bio_async_bridge_t* bridge,
    float attention,
    int32_t layer_idx
);

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

/**
 * @brief Get current effects from training to bio-async
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_get_outgoing_effects(
    const training_bio_async_bridge_t* bridge,
    training_to_bio_async_effects_t* effects
);

/**
 * @brief Get current effects from bio-async to training
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_get_incoming_effects(
    const training_bio_async_bridge_t* bridge,
    bio_async_to_training_effects_t* effects
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int training_bio_bridge_get_stats(
    const training_bio_async_bridge_t* bridge,
    training_bio_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge handle
 */
void training_bio_bridge_reset_stats(training_bio_async_bridge_t* bridge);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get message type name as string
 * @param type Message type
 * @return String name or "UNKNOWN"
 */
const char* training_bio_message_type_name(training_bio_message_type_t type);

/**
 * @brief Get error name as string
 * @param error Error code
 * @return String name or "UNKNOWN"
 */
const char* training_bio_error_name(training_bio_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_BIO_ASYNC_BRIDGE_H */
