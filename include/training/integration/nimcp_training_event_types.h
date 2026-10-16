/**
 * @file nimcp_training_event_types.h
 * @brief Training event type definitions for cross-module communication
 * @version 1.0.0
 * @date 2025
 *
 * WHAT: Type definitions for training events used throughout the integration hub
 * WHY:  Standardize event communication between training modules
 * HOW:  Define enums, structs, and callback types for event-driven architecture
 *
 * DESIGN PATTERNS:
 * - Observer: Event callbacks for inter-module communication
 * - Strategy: Different event types trigger different processing strategies
 * - Command: Events encapsulate operations and their data
 *
 * TRAINING CATEGORIES:
 * - CURRICULUM: Difficulty scheduling, sample ordering
 * - OPTIMIZATION: Meta-learning, hyperparameter optimization, optimizers
 * - ARCHITECTURE: Auto-architecture search, NAS
 * - COMPRESSION: Quantization, distillation, mixed precision
 * - DISTRIBUTED: Distributed training, gradient scaling
 * - ROBUSTNESS: Adversarial training, data augmentation
 * - CONTINUAL: Continual learning, multi-task learning
 * - DATA: Data pipeline, checkpoints, data loading
 *
 * THREAD SAFETY: All types are designed for multi-threaded environments.
 * Event data should be copied before async processing.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRAINING_EVENT_TYPES_H
#define NIMCP_TRAINING_EVENT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * TRAINING CATEGORY DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Categories of training processing modules
 * WHY: Classify modules for routing and filtering events
 * HOW: Enum with distinct training domain categories
 *
 * CATEGORIES:
 * - CURRICULUM: Difficulty scheduling, sample ordering (curriculum_learning)
 * - OPTIMIZATION: Meta-learning, hyperparameter optimization, optimizers
 * - ARCHITECTURE: Auto-architecture search, NAS
 * - COMPRESSION: Quantization, distillation, mixed precision
 * - DISTRIBUTED: Distributed training, gradient scaling
 * - ROBUSTNESS: Adversarial training, data augmentation
 * - CONTINUAL: Continual learning, multi-task learning, catastrophic forgetting
 * - DATA: Data pipeline, checkpoints, data loading
 */
typedef enum {
    TRAINING_CATEGORY_CURRICULUM = 0,     /* Difficulty scheduling, sample ordering */
    TRAINING_CATEGORY_OPTIMIZATION,       /* Meta-learning, hyperparameter optimization */
    TRAINING_CATEGORY_ARCHITECTURE,       /* Auto-architecture, NAS */
    TRAINING_CATEGORY_COMPRESSION,        /* Quantization, distillation, mixed precision */
    TRAINING_CATEGORY_DISTRIBUTED,        /* Distributed training, gradient scaling */
    TRAINING_CATEGORY_ROBUSTNESS,         /* Adversarial training, data augmentation */
    TRAINING_CATEGORY_CONTINUAL,          /* Continual learning, multi-task */
    TRAINING_CATEGORY_DATA,               /* Data pipeline, checkpoints */
    TRAINING_CATEGORY_COUNT               /* Number of categories (sentinel) */
} training_category_t;

/* ========================================================================
 * TRAINING EVENT TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Types of events that can occur in training processing
 * WHY: Distinguish different event semantics for proper handling
 * HOW: Enum with comprehensive training event type coverage
 *
 * EVENT TYPES:
 * - DIFFICULTY_UPDATED: Curriculum difficulty level changed
 * - EPOCH_COMPLETE: Training epoch finished
 * - BATCH_COMPLETE: Training batch finished
 * - LOSS_COMPUTED: Loss value available
 * - GRADIENT_READY: Gradients computed and ready
 * - WEIGHTS_UPDATED: Model weights updated
 * - LR_ADJUSTED: Learning rate changed
 * - TASK_SAMPLED: New meta-learning task sampled
 * - CHECKPOINT_SAVED: Checkpoint created
 * - HYPERPARAMS_TUNED: Hyperparameters adjusted
 * - QUANTIZATION_APPLIED: Quantization calibration updated
 * - KNOWLEDGE_DISTILLED: Distillation step completed
 * - ADVERSARIAL_UPDATED: Adversarial training step completed
 * - TASK_SWITCHED: Multi-task learning switched tasks
 * - MEMORY_REPLAYED: Continual learning memory replay
 */
typedef enum {
    TRAINING_EVENT_DIFFICULTY_UPDATED = 0,  /* Curriculum difficulty changed */
    TRAINING_EVENT_EPOCH_COMPLETE,          /* Training epoch finished */
    TRAINING_EVENT_BATCH_COMPLETE,          /* Training batch finished */
    TRAINING_EVENT_LOSS_COMPUTED,           /* Loss value available */
    TRAINING_EVENT_GRADIENT_READY,          /* Gradients computed */
    TRAINING_EVENT_WEIGHTS_UPDATED,         /* Model weights updated */
    TRAINING_EVENT_LR_ADJUSTED,             /* Learning rate changed */
    TRAINING_EVENT_TASK_SAMPLED,            /* Meta-learning task sampled */
    TRAINING_EVENT_CHECKPOINT_SAVED,        /* Checkpoint created */
    TRAINING_EVENT_HYPERPARAMS_TUNED,       /* Hyperparameters adjusted */
    TRAINING_EVENT_QUANTIZATION_APPLIED,    /* Quantization updated */
    TRAINING_EVENT_KNOWLEDGE_DISTILLED,     /* Distillation step done */
    TRAINING_EVENT_ADVERSARIAL_UPDATED,     /* Adversarial training step */
    TRAINING_EVENT_TASK_SWITCHED,           /* Multi-task switched */
    TRAINING_EVENT_MEMORY_REPLAYED,         /* Continual learning replay */
    TRAINING_EVENT_ARCHITECTURE_UPDATED,    /* NAS architecture changed */
    TRAINING_EVENT_VALIDATION_COMPLETE,     /* Validation finished */
    TRAINING_EVENT_EARLY_STOPPING,          /* Early stopping triggered */
    TRAINING_EVENT_COUNT                    /* Number of event types (sentinel) */
} training_event_type_t;

/* ========================================================================
 * TRAINING QUERY TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Types of queries modules can make to each other
 * WHY: Enable structured inter-module information requests
 * HOW: Enum covering common training query patterns
 *
 * QUERY TYPES:
 * - DIFFICULTY_SCORE: Get sample difficulty score
 * - LEARNING_RATE: Get current learning rate
 * - LOSS_HISTORY: Get loss history
 * - GRADIENT_STATS: Get gradient statistics
 * - TASK_PROGRESS: Get meta-learning task progress
 * - COMPRESSION_RATIO: Get compression metrics
 * - RESOURCE_USAGE: Get training resource usage
 * - SAMPLE_WEIGHTS: Get curriculum sample weights
 * - ARCHITECTURE_STATE: Get current NAS architecture
 */
typedef enum {
    TRAINING_QUERY_STATUS = 0,            /* Module operational status */
    TRAINING_QUERY_DIFFICULTY_SCORE,      /* Sample difficulty score */
    TRAINING_QUERY_LEARNING_RATE,         /* Current learning rate */
    TRAINING_QUERY_LOSS_HISTORY,          /* Loss history */
    TRAINING_QUERY_GRADIENT_STATS,        /* Gradient statistics */
    TRAINING_QUERY_TASK_PROGRESS,         /* Meta-learning progress */
    TRAINING_QUERY_COMPRESSION_RATIO,     /* Compression metrics */
    TRAINING_QUERY_RESOURCE_USAGE,        /* Resource usage */
    TRAINING_QUERY_SAMPLE_WEIGHTS,        /* Curriculum sample weights */
    TRAINING_QUERY_ARCHITECTURE_STATE,    /* NAS architecture state */
    TRAINING_QUERY_CHECKPOINT_INFO,       /* Checkpoint information */
    TRAINING_QUERY_HYPERPARAMS,           /* Current hyperparameters */
    TRAINING_QUERY_COUNT                  /* Number of query types (sentinel) */
} training_query_type_t;

/* ========================================================================
 * EVENT PRIORITY DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Priority levels for training events
 * WHY: Enable prioritized event processing
 * HOW: Enum from lowest to highest priority
 */
typedef enum {
    TRAINING_PRIORITY_LOW = 0,            /* Background, non-urgent */
    TRAINING_PRIORITY_NORMAL,             /* Standard priority */
    TRAINING_PRIORITY_HIGH,               /* Elevated priority */
    TRAINING_PRIORITY_CRITICAL            /* Immediate processing required */
} training_event_priority_t;

/* ========================================================================
 * EVENT DATA STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Container for training event data
 * WHY: Standardize event information passing between modules
 * HOW: Struct with type, source, timestamp, and payload
 *
 * FIELDS:
 * - event_type: Type of event (training_event_type_t)
 * - source_module_id: ID of module that generated event
 * - timestamp: Event generation time (microseconds since epoch)
 * - priority: Event priority level
 * - epoch: Current training epoch (if applicable)
 * - batch: Current batch index (if applicable)
 * - payload: Pointer to event-specific data
 * - payload_size: Size of payload in bytes
 *
 * MEMORY: Payload ownership depends on context:
 * - Synchronous: Caller owns payload
 * - Asynchronous: Hub copies payload, caller can free immediately
 */
typedef struct {
    training_event_type_t event_type;       /* Type of event */
    uint32_t source_module_id;              /* Source module identifier */
    uint64_t timestamp;                     /* Event timestamp (microseconds) */
    training_event_priority_t priority;     /* Event priority */
    uint32_t epoch;                         /* Current training epoch */
    uint32_t batch;                         /* Current batch index */
    float loss_value;                       /* Loss value (if applicable) */
    float learning_rate;                    /* Learning rate (if applicable) */
    void* payload;                          /* Event-specific data */
    size_t payload_size;                    /* Size of payload in bytes */
} training_event_data_t;

/* ========================================================================
 * CALLBACK TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Callback function type for event notifications
 * WHY: Enable modules to receive asynchronous event notifications
 * HOW: Function pointer with event data and user context
 *
 * PARAMETERS:
 * - event: The event data (read-only, do not modify)
 * - user_data: User-provided context from subscription
 *
 * RETURN: 0 on success, -1 on error
 *
 * THREAD SAFETY: Callbacks may be invoked from any thread.
 * Implementations must be thread-safe.
 *
 * BLOCKING: Callbacks should not block. Long operations should
 * be queued for later processing.
 */
typedef int (*training_event_callback_t)(const training_event_data_t* event,
                                          void* user_data);

/* ========================================================================
 * QUERY DATA STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Container for module query requests
 * WHY: Standardize inter-module query format
 * HOW: Struct with query type and parameters
 */
typedef struct {
    training_query_type_t query_type;       /* Type of query */
    void* query_params;                     /* Query-specific parameters */
    size_t params_size;                     /* Size of parameters */
} training_query_t;

/**
 * WHAT: Container for query results
 * WHY: Standardize query response format
 * HOW: Struct with result data and status
 */
typedef struct {
    int status;                             /* 0 = success, -1 = error */
    void* result_data;                      /* Query result data */
    size_t result_size;                     /* Size of result data */
    char error_message[128];                /* Error message if status != 0 */
} training_query_result_t;

/* ========================================================================
 * COMMON EVENT PAYLOADS
 * ======================================================================== */

/**
 * WHAT: Payload for difficulty update events
 * WHY: Communicate curriculum difficulty changes
 */
typedef struct {
    float old_difficulty;                   /* Previous difficulty level */
    float new_difficulty;                   /* New difficulty level */
    uint32_t samples_at_level;              /* Samples at this difficulty */
    float progression_rate;                 /* Rate of difficulty increase */
} training_difficulty_payload_t;

/**
 * WHAT: Payload for loss computed events
 * WHY: Communicate training loss information
 */
typedef struct {
    uint32_t worker_id;                     /* Worker that computed loss */
    uint32_t batch_id;                      /* Batch ID */
    uint32_t epoch;                         /* Current epoch */
    float loss_value;                       /* Loss value (alias: total_loss) */
    float total_loss;                       /* Total combined loss */
    float primary_loss;                     /* Primary task loss */
    float regularization_loss;              /* Regularization loss */
    float auxiliary_loss;                   /* Auxiliary losses */
    uint32_t batch_size;                    /* Batch size */
} training_loss_payload_t;

/**
 * WHAT: Payload for gradient ready events
 * WHY: Communicate gradient statistics
 */
typedef struct {
    float gradient_norm;                    /* L2 norm of gradients */
    float gradient_max;                     /* Maximum gradient value */
    float gradient_min;                     /* Minimum gradient value */
    bool gradient_clipped;                  /* Whether gradients were clipped */
    float clip_threshold;                   /* Clipping threshold used */
} training_gradient_payload_t;

/**
 * WHAT: Payload for learning rate adjustment events
 * WHY: Communicate LR schedule changes
 */
typedef struct {
    float old_lr;                           /* Previous learning rate */
    float new_lr;                           /* New learning rate */
    const char* scheduler_name;             /* Name of LR scheduler */
    uint32_t step;                          /* Scheduler step number */
} training_lr_payload_t;

/**
 * WHAT: Payload for meta-learning task sampled events
 * WHY: Communicate meta-learning task information
 */
typedef struct {
    uint32_t task_id;                       /* Task identifier */
    uint32_t n_way;                         /* N-way classification */
    uint32_t k_shot;                        /* K-shot learning */
    uint32_t query_size;                    /* Query set size */
    const char* task_name;                  /* Optional task name */
} training_meta_task_payload_t;

/**
 * WHAT: Payload for checkpoint saved events
 * WHY: Communicate checkpoint information
 */
typedef struct {
    uint32_t epoch;                         /* Epoch at checkpoint */
    uint32_t global_step;                   /* Global training step */
    float best_metric;                      /* Best metric value */
    bool is_best;                           /* Whether this is best model */
    const char* checkpoint_path;            /* Path to checkpoint */
} training_checkpoint_payload_t;

/* ========================================================================
 * UTILITY MACROS
 * ======================================================================== */

/**
 * WHAT: Helper macro to check if category is valid
 */
#define TRAINING_CATEGORY_IS_VALID(cat) \
    ((cat) >= TRAINING_CATEGORY_CURRICULUM && (cat) < TRAINING_CATEGORY_COUNT)

/**
 * WHAT: Helper macro to check if event type is valid
 */
#define TRAINING_EVENT_TYPE_IS_VALID(type) \
    ((type) >= TRAINING_EVENT_DIFFICULTY_UPDATED && (type) < TRAINING_EVENT_COUNT)

/**
 * WHAT: Helper macro to check if query type is valid
 */
#define TRAINING_QUERY_TYPE_IS_VALID(type) \
    ((type) >= TRAINING_QUERY_STATUS && (type) < TRAINING_QUERY_COUNT)

/* ========================================================================
 * STRING CONVERSION UTILITIES
 * ======================================================================== */

/**
 * WHAT: Get string name for training category
 * WHY: Debug output and logging
 * HOW: Return static string for category
 *
 * @param category Training category
 * @return String name or "UNKNOWN" if invalid
 */
const char* training_category_to_string(training_category_t category);

/**
 * WHAT: Get string name for event type
 * WHY: Debug output and logging
 * HOW: Return static string for event type
 *
 * @param event_type Event type
 * @return String name or "UNKNOWN" if invalid
 */
const char* training_event_type_to_string(training_event_type_t event_type);

/**
 * WHAT: Get string name for query type
 * WHY: Debug output and logging
 * HOW: Return static string for query type
 *
 * @param query_type Query type
 * @return String name or "UNKNOWN" if invalid
 */
const char* training_query_type_to_string(training_query_type_t query_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_EVENT_TYPES_H */
