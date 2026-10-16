/**
 * @file nimcp_motor_adapter.c
 * @brief Implementation of Motor Cortex brain adapter
 *
 * WHAT: Unified adapter connecting Motor Cortex sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, training, and event system
 * HOW:  Orchestrates M1, premotor, and SMA processors
 *
 * @version Phase M1: Motor Cortex Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(motor_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define MOTOR_LOG_MODULE "MOTOR"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Stored motor program
 */
typedef struct motor_program {
    motor_program_info_t info;
    motor_command_t* commands;       /**< Command sequence */
    struct motor_program* next;      /**< Hash chain */
} motor_program_node_t;

/**
 * @brief Command queue entry
 */
typedef struct {
    motor_command_t command;
    bool is_valid;
} command_queue_entry_t;

/**
 * @brief Trajectory plan
 */
typedef struct {
    trajectory_waypoint_t* waypoints;
    uint32_t num_waypoints;
    uint32_t current_waypoint;
    float elapsed_ms;
    motor_region_t region;
    bool is_active;
} trajectory_plan_t;

/**
 * @brief Internal adapter structure
 */
struct motor_adapter {
    /* Configuration */
    motor_config_t config;

    /* Effector states */
    motor_effector_state_t* effectors;
    uint32_t num_effectors;

    /* Motor program storage (hash table) */
    motor_program_node_t** programs;
    uint32_t program_capacity;
    uint32_t program_count;
    uint32_t next_program_id;

    /* Trajectory planning */
    trajectory_plan_t* trajectories;
    uint32_t num_trajectories;
    trajectory_plan_t* active_trajectory;

    /* Command output queue */
    command_queue_entry_t* command_queue;
    uint32_t queue_capacity;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;
    nimcp_mutex_t* queue_mutex;  /**< Mutex protecting queue operations */

    /* Callback re-entrance guard (atomic for thread safety)
     * Uses atomic CAS to prevent race conditions where two threads
     * could both see in_callback=false and both invoke the callback */
    nimcp_atomic_bool_t in_callback;

    /* Current movement state */
    motor_goal_t current_goal;
    motor_result_t last_result;
    bool has_active_goal;
    float execution_time_ms;

    /* Callbacks */
    motor_command_callback_t command_callback;
    void* command_user_data;
    motor_complete_callback_t complete_callback;
    void* complete_user_data;
    motor_event_callback_t event_callback;
    void* event_user_data;

    /* State */
    motor_status_t status;
    motor_error_t last_error;
    double current_time_ms;

    /* Memory pool for trajectory waypoints */
    memory_pool_t waypoint_pool;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* Statistics */
    motor_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Hash program ID for storage
 */
static uint32_t hash_program_id(uint32_t program_id, uint32_t capacity) {
    return program_id % capacity;
}

/**
 * @brief Emit event to callback
 *
 * THREAD SAFETY: Acquires queue_mutex to safely read callback pointer, then releases
 * mutex before invoking callback (to avoid deadlock). Uses atomic CAS on in_callback
 * flag to prevent callback re-entrance.
 */
static void emit_event(motor_adapter_t* adapter, uint32_t event_type, const void* data) {
    if (!adapter->config.enable_events) {
        return;
    }

    /* Capture callback pointer and user data under mutex protection */
    motor_event_callback_t callback = NULL;
    void* user_data = NULL;

    nimcp_mutex_lock(adapter->queue_mutex);
    callback = adapter->event_callback;
    user_data = adapter->event_user_data;
    nimcp_mutex_unlock(adapter->queue_mutex);

    if (!callback) {
        return;
    }

    /* Atomic CAS: only proceed if we successfully change in_callback from false to true */
    bool expected = false;
    if (nimcp_atomic_compare_exchange_bool(&adapter->in_callback, &expected, true,
                                            NIMCP_MEMORY_ORDER_ACQ_REL)) {
        callback(event_type, data, user_data);
        nimcp_atomic_store_bool(&adapter->in_callback, false, NIMCP_MEMORY_ORDER_RELEASE);
    }
}

/**
 * @brief Set error state
 */
static void set_error(motor_adapter_t* adapter, motor_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != MOTOR_ERROR_NONE) {
        adapter->status = MOTOR_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", MOTOR_LOG_MODULE, error);
    }
}

/**
 * @brief Compute distance between two points
 */
static float compute_distance(const motor_vec3_t* a, const motor_vec3_t* b) {
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief Linear interpolation between points
 */
static motor_vec3_t lerp_vec3(const motor_vec3_t* a, const motor_vec3_t* b, float t) {
    motor_vec3_t result;
    result.x = a->x + (b->x - a->x) * t;
    result.y = a->y + (b->y - a->y) * t;
    result.z = a->z + (b->z - a->z) * t;
    return result;
}

/**
 * @brief Enqueue a motor command (internal, caller must hold queue_mutex)
 *
 * THREAD SAFETY: This helper expects the caller to already hold queue_mutex.
 *                Use enqueue_command() for the public thread-safe version.
 */
static bool enqueue_command_unlocked(motor_adapter_t* adapter, const motor_command_t* command) {
    if (!adapter || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enqueue_command_unlocked: required parameter is NULL (adapter, command)");
        return false;
    }

    if (adapter->queue_count >= adapter->queue_capacity) {
        LOG_WARN("[%s] Command queue full", MOTOR_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "enqueue_command_unlocked: capacity exceeded");
        return false;
    }

    adapter->command_queue[adapter->queue_tail].command = *command;
    adapter->command_queue[adapter->queue_tail].is_valid = true;
    adapter->queue_tail = (adapter->queue_tail + 1) % adapter->queue_capacity;
    adapter->queue_count++;

    return true;
}

/**
 * @brief Enqueue a motor command (thread-safe)
 *
 * THREAD SAFETY: Acquires queue_mutex to protect queue state.
 *                Releases mutex before invoking callback to avoid deadlock.
 *                Uses in_callback flag to prevent callback re-entrance.
 */
static bool enqueue_command(motor_adapter_t* adapter, const motor_command_t* command) {
    if (!adapter || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enqueue_command: required parameter is NULL (adapter, command)");
        return false;
    }

    /* Copy callback info while holding lock, invoke after releasing */
    motor_command_callback_t callback = NULL;
    void* user_data = NULL;
    motor_command_t cmd_copy;
    bool success = false;

    nimcp_mutex_lock(adapter->queue_mutex);

    success = enqueue_command_unlocked(adapter, command);
    if (success) {
        /* Capture callback and command for invocation outside lock */
        callback = adapter->command_callback;
        user_data = adapter->command_user_data;
        cmd_copy = *command;
    }

    nimcp_mutex_unlock(adapter->queue_mutex);

    /* Invoke callback outside of lock to avoid deadlock
     * Use atomic CAS on in_callback flag to prevent re-entrance race condition */
    if (success && callback) {
        bool expected = false;
        if (nimcp_atomic_compare_exchange_bool(&adapter->in_callback, &expected, true,
                                                NIMCP_MEMORY_ORDER_ACQ_REL)) {
            callback(&cmd_copy, user_data);
            nimcp_atomic_store_bool(&adapter->in_callback, false, NIMCP_MEMORY_ORDER_RELEASE);
        }
    }

    return success;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Forward declarations)
 *===========================================================================*/

static nimcp_error_t handle_motor_command_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_motor_stop_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_bg_action_selection(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_cerebellar_correction(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/*=============================================================================
 * KG-DRIVEN WIRING CALLBACK
 *===========================================================================*/

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Motor adapter pointer
 * @return 0 on success, -1 on error
 */
static int motor_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO("[%s] motor_wiring_handler_callback: registering %u handlers from KG",
             MOTOR_LOG_MODULE, message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_MOTOR_COMMAND_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_motor_command_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_MOTOR_COMMAND_REQUEST", MOTOR_LOG_MODULE);
                break;

            case BIO_MSG_MOTOR_STOP_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_motor_stop_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_MOTOR_STOP_REQUEST", MOTOR_LOG_MODULE);
                break;

            case BIO_MSG_BG_ACTION_SELECTION:
                bio_router_register_handler(ctx, message_types[i], handle_bg_action_selection);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_BG_ACTION_SELECTION", MOTOR_LOG_MODULE);
                break;

            case BIO_MSG_CEREBELLAR_CORRECTION:
                bio_router_register_handler(ctx, message_types[i], handle_cerebellar_correction);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_CEREBELLAR_CORRECTION", MOTOR_LOG_MODULE);
                break;

            default:
                LOG_DEBUG("[%s]   Unknown message type %u - skipping", MOTOR_LOG_MODULE, message_types[i]);
                break;
        }
    }

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

motor_config_t motor_default_config(void) {
    motor_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_motor_programs = MOTOR_DEFAULT_MAX_PROGRAMS;
    config.max_effectors = MOTOR_DEFAULT_MAX_EFFECTORS;
    config.max_trajectories = MOTOR_DEFAULT_MAX_TRAJECTORIES;
    config.planning_horizon_ms = MOTOR_DEFAULT_PLANNING_HORIZON_MS;
    config.execution_rate_hz = MOTOR_DEFAULT_EXECUTION_RATE_HZ;
    config.reaction_time_ms = 150.0f;  /* Typical human reaction time */

    config.enable_premotor = true;
    config.enable_sma = true;
    config.enable_trajectory_opt = true;
    config.enable_feedforward = true;
    config.enable_feedback = true;

    config.enable_basal_ganglia = true;
    config.enable_cerebellum = true;
    config.enable_thalamus = true;

    config.enable_events = true;
    config.enable_training = false;
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;

    /* Bio-async: enabled by default, use acetylcholine for motor control */
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_ACETYLCHOLINE;

    return config;
}

motor_adapter_t* motor_create(const motor_config_t* config) {
    LOG_INFO("[%s] Creating motor cortex adapter", MOTOR_LOG_MODULE);

    motor_adapter_t* adapter = (motor_adapter_t*)nimcp_calloc(1, sizeof(motor_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", MOTOR_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG("[%s] Using provided configuration", MOTOR_LOG_MODULE);
    } else {
        adapter->config = motor_default_config();
        LOG_DEBUG("[%s] Using default configuration", MOTOR_LOG_MODULE);
    }

    /* Initialize effector states */
    LOG_DEBUG("[%s] Initializing effectors (max=%u)", MOTOR_LOG_MODULE,
              adapter->config.max_effectors);
    adapter->effectors = (motor_effector_state_t*)nimcp_calloc(
        adapter->config.max_effectors, sizeof(motor_effector_state_t));
    if (!adapter->effectors) {
        LOG_ERROR("[%s] Failed to allocate effector array", MOTOR_LOG_MODULE);
        motor_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_create: adapter->effectors is NULL");
        return NULL;
    }

    /* Initialize effector IDs by region */
    for (uint32_t i = 0; i < MOTOR_REGION_COUNT && i < adapter->config.max_effectors; i++) {
        adapter->effectors[i].effector_id = i;
        adapter->effectors[i].region = (motor_region_t)i;
        adapter->effectors[i].stiffness = 0.5f;
        adapter->num_effectors++;
    }

    /* Initialize motor program storage */
    LOG_DEBUG("[%s] Initializing motor program storage (capacity=%u)", MOTOR_LOG_MODULE,
              adapter->config.max_motor_programs);
    adapter->program_capacity = adapter->config.max_motor_programs;
    adapter->programs = (motor_program_node_t**)nimcp_calloc(
        adapter->program_capacity, sizeof(motor_program_node_t*));
    if (!adapter->programs) {
        LOG_ERROR("[%s] Failed to allocate program storage", MOTOR_LOG_MODULE);
        motor_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_create: adapter->programs is NULL");
        return NULL;
    }
    adapter->next_program_id = 1;

    /* Initialize trajectory planning */
    LOG_DEBUG("[%s] Initializing trajectory plans (max=%u)", MOTOR_LOG_MODULE,
              adapter->config.max_trajectories);
    adapter->trajectories = (trajectory_plan_t*)nimcp_calloc(
        adapter->config.max_trajectories, sizeof(trajectory_plan_t));
    if (!adapter->trajectories) {
        LOG_ERROR("[%s] Failed to allocate trajectory array", MOTOR_LOG_MODULE);
        motor_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_create: adapter->trajectories is NULL");
        return NULL;
    }

    /* Initialize command queue */
    uint32_t queue_size = 256;  /* Commands per execution cycle */
    LOG_DEBUG("[%s] Initializing command queue (capacity=%u)", MOTOR_LOG_MODULE, queue_size);
    adapter->queue_capacity = queue_size;
    adapter->command_queue = (command_queue_entry_t*)nimcp_calloc(
        queue_size, sizeof(command_queue_entry_t));
    if (!adapter->command_queue) {
        LOG_ERROR("[%s] Failed to allocate command queue", MOTOR_LOG_MODULE);
        motor_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_create: adapter->command_queue is NULL");
        return NULL;
    }

    /* Initialize queue mutex for thread safety */
    adapter->queue_mutex = nimcp_mutex_create(NULL);
    if (!adapter->queue_mutex) {
        LOG_ERROR("[%s] Failed to create command queue mutex", MOTOR_LOG_MODULE);
        motor_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_create: adapter->queue_mutex is NULL");
        return NULL;
    }

    /* Initialize callback re-entrance guard (atomic) */
    nimcp_atomic_init_bool(&adapter->in_callback, false);

    /* Initialize memory pool for waypoints */
    LOG_DEBUG("[%s] Creating waypoint memory pool", MOTOR_LOG_MODULE);
    memory_pool_config_t pool_config = {
        .block_size = 64 * sizeof(trajectory_waypoint_t),  /* 64 waypoints per block */
        .num_blocks = 4,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    adapter->waypoint_pool = memory_pool_create(&pool_config);
    if (!adapter->waypoint_pool) {
        LOG_ERROR("[%s] Failed to create waypoint memory pool", MOTOR_LOG_MODULE);
        motor_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_create: adapter->waypoint_pool is NULL");
        return NULL;
    }

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    adapter->default_channel = adapter->config.default_channel;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", MOTOR_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_MOTOR_CORTEX,
            .module_name = "motor_cortex",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_MOTOR_CORTEX,
                (void*)motor_wiring_handler_callback,
                adapter
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_MOTOR_COMMAND_REQUEST, handle_motor_command_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_MOTOR_STOP_REQUEST, handle_motor_stop_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_BG_ACTION_SELECTION, handle_bg_action_selection)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_CEREBELLAR_CORRECTION, handle_cerebellar_correction)
                );
                LOG_INFO("[%s] Bio-async enabled (legacy direct registration)", MOTOR_LOG_MODULE);
            } else {
                LOG_INFO("[%s] Bio-async enabled (KG-driven wiring callback registered)", MOTOR_LOG_MODULE);
            }
        } else {
            LOG_WARNING("[%s] Failed to register with bio-async router", MOTOR_LOG_MODULE);
        }
    }

    /* Initialize state */
    adapter->status = MOTOR_STATUS_IDLE;
    adapter->last_error = MOTOR_ERROR_NONE;
    adapter->current_time_ms = 0.0;
    adapter->has_active_goal = false;

    LOG_INFO("[%s] Motor cortex adapter created successfully", MOTOR_LOG_MODULE);
    return adapter;
}

void motor_destroy(motor_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying motor cortex adapter", MOTOR_LOG_MODULE);

    /* Unregister from bio-async router */
    if (adapter->bio_ctx) {
        LOG_DEBUG("[%s] Unregistering from bio-async router", MOTOR_LOG_MODULE);
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Free motor programs */
    if (adapter->programs) {
        LOG_DEBUG("[%s] Freeing motor programs", MOTOR_LOG_MODULE);
        for (uint32_t i = 0; i < adapter->program_capacity; i++) {
            motor_program_node_t* node = adapter->programs[i];
            while (node) {
                motor_program_node_t* next = node->next;
                if (node->commands) {
                    nimcp_free(node->commands);
                }
                nimcp_free(node);
                node = next;
            }
        }
        nimcp_free(adapter->programs);
    }

    /* Free trajectories */
    if (adapter->trajectories) {
        LOG_DEBUG("[%s] Freeing trajectories", MOTOR_LOG_MODULE);
        for (uint32_t i = 0; i < adapter->config.max_trajectories; i++) {
            if (adapter->trajectories[i].waypoints) {
                nimcp_free(adapter->trajectories[i].waypoints);
            }
        }
        nimcp_free(adapter->trajectories);
    }

    /* Free effectors */
    if (adapter->effectors) {
        LOG_DEBUG("[%s] Freeing effectors", MOTOR_LOG_MODULE);
        nimcp_free(adapter->effectors);
    }

    /* Free command queue and its mutex */
    if (adapter->queue_mutex) {
        LOG_DEBUG("[%s] Destroying command queue mutex", MOTOR_LOG_MODULE);
        nimcp_mutex_free(adapter->queue_mutex);

        adapter->queue_mutex = NULL;
    }
    if (adapter->command_queue) {
        LOG_DEBUG("[%s] Freeing command queue", MOTOR_LOG_MODULE);
        nimcp_free(adapter->command_queue);
    }

    /* Destroy memory pool */
    if (adapter->waypoint_pool) {
        LOG_DEBUG("[%s] Destroying waypoint memory pool", MOTOR_LOG_MODULE);
        memory_pool_destroy(adapter->waypoint_pool);
    }

    LOG_DEBUG("[%s] Motor cortex adapter destroyed", MOTOR_LOG_MODULE);
    nimcp_free(adapter);
}

bool motor_reset(motor_adapter_t* adapter) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    LOG_DEBUG("[%s] Resetting adapter state", MOTOR_LOG_MODULE);

    /* Clear command queue (thread-safe) */
    nimcp_mutex_lock(adapter->queue_mutex);
    adapter->queue_head = 0;
    adapter->queue_tail = 0;
    adapter->queue_count = 0;
    nimcp_mutex_unlock(adapter->queue_mutex);

    /* Clear active trajectory */
    adapter->active_trajectory = NULL;
    for (uint32_t i = 0; i < adapter->config.max_trajectories; i++) {
        adapter->trajectories[i].is_active = false;
    }

    /* Clear movement state */
    adapter->has_active_goal = false;
    adapter->execution_time_ms = 0.0f;
    memset(&adapter->current_goal, 0, sizeof(motor_goal_t));
    memset(&adapter->last_result, 0, sizeof(motor_result_t));

    /* Reset effector states to origin */
    for (uint32_t i = 0; i < adapter->num_effectors; i++) {
        memset(&adapter->effectors[i].position, 0, sizeof(motor_vec3_t));
        memset(&adapter->effectors[i].velocity, 0, sizeof(motor_vec3_t));
    }

    /* Reset state */
    adapter->status = MOTOR_STATUS_IDLE;
    adapter->last_error = MOTOR_ERROR_NONE;

    LOG_DEBUG("[%s] Adapter reset complete", MOTOR_LOG_MODULE);
    return true;
}

/*=============================================================================
 * MOTOR PROGRAM MANAGEMENT
 *===========================================================================*/

uint32_t motor_store_program(motor_adapter_t* adapter,
                              const char* name,
                              const motor_command_t* commands,
                              uint32_t num_commands,
                              movement_type_t type) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        return 0;
    }
    if (!name) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: name");
        return 0;
    }
    if (!commands) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: commands");
        return 0;
    }
    if (num_commands == 0) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: num_commands must be > 0");
        return 0;
    }

    if (adapter->program_count >= adapter->program_capacity) {
        LOG_WARN("[%s] Program storage full", MOTOR_LOG_MODULE);
        return 0;
    }

    /* Create new program node */
    motor_program_node_t* node = (motor_program_node_t*)nimcp_calloc(
        1, sizeof(motor_program_node_t));
    if (!node) return 0;

    /* Copy commands */
    node->commands = (motor_command_t*)nimcp_calloc(num_commands, sizeof(motor_command_t));
    if (!node->commands) {
        nimcp_free(node);
        return 0;
    }
    memcpy(node->commands, commands, num_commands * sizeof(motor_command_t));

    /* Fill program info */
    node->info.program_id = adapter->next_program_id++;
    strncpy(node->info.name, name, sizeof(node->info.name) - 1);
    node->info.type = type;
    node->info.num_commands = num_commands;
    node->info.is_learned = true;

    /* Calculate duration and complexity */
    float duration = 0.0f;
    for (uint32_t i = 0; i < num_commands; i++) {
        duration = fmaxf(duration, commands[i].timestamp_ms + commands[i].duration_ms);
    }
    node->info.total_duration_ms = duration;
    node->info.complexity = fminf(1.0f, (float)num_commands / 100.0f);

    /* Get primary region from first command */
    if (num_commands > 0 && commands[0].effector_id < adapter->num_effectors) {
        node->info.primary_region = adapter->effectors[commands[0].effector_id].region;
    }

    /* Insert into hash table */
    uint32_t idx = hash_program_id(node->info.program_id, adapter->program_capacity);
    node->next = adapter->programs[idx];
    adapter->programs[idx] = node;
    adapter->program_count++;

    LOG_DEBUG("[%s] Stored motor program '%s' (ID=%u, commands=%u)",
              MOTOR_LOG_MODULE, name, node->info.program_id, num_commands);

    return node->info.program_id;
}

bool motor_get_program(const motor_adapter_t* adapter,
                        uint32_t program_id,
                        motor_program_info_t* info) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(info, "info");
    if (program_id == 0) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: program_id cannot be 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_reset: program_id is zero");
        return false;
    }

    uint32_t idx = hash_program_id(program_id, adapter->program_capacity);
    motor_program_node_t* node = adapter->programs[idx];

    while (node) {
        if (node->info.program_id == program_id) {
            *info = node->info;
            return true;
        }
        node = node->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_reset: validation failed");
    return false;
}

bool motor_delete_program(motor_adapter_t* adapter, uint32_t program_id) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    if (program_id == 0) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: program_id cannot be 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_delete_program: program_id is zero");
        return false;
    }

    uint32_t idx = hash_program_id(program_id, adapter->program_capacity);
    motor_program_node_t** pp = &adapter->programs[idx];

    while (*pp) {
        if ((*pp)->info.program_id == program_id) {
            motor_program_node_t* node = *pp;
            *pp = node->next;
            if (node->commands) nimcp_free(node->commands);
            nimcp_free(node);
            adapter->program_count--;
            LOG_DEBUG("[%s] Deleted motor program ID=%u", MOTOR_LOG_MODULE, program_id);
            return true;
        }
        pp = &(*pp)->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_delete_program: validation failed");
    return false;
}

/*=============================================================================
 * MOTOR PLANNING
 *===========================================================================*/

bool motor_plan_movement(motor_adapter_t* adapter, const motor_goal_t* goal) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_plan_movement: adapter is NULL");
        return false;
    }
    if (!goal) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: goal");
        set_error(adapter, MOTOR_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_plan_movement: goal is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Planning movement to region %d", MOTOR_LOG_MODULE, goal->region);

    /* Validate region first before changing state */
    uint32_t effector_id = (uint32_t)goal->region;
    if (effector_id >= adapter->num_effectors) {
        set_error(adapter, MOTOR_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_plan_movement: capacity exceeded");
        return false;
    }

    adapter->status = MOTOR_STATUS_PLANNING;
    adapter->current_goal = *goal;
    adapter->has_active_goal = true;
    adapter->stats.movements_planned++;

    motor_effector_state_t* effector = &adapter->effectors[effector_id];

    /* Calculate movement parameters */
    float distance = compute_distance(&effector->position, &goal->target_position);
    float duration = goal->max_duration_ms;

    /* Apply Fitts' law for duration estimation if not specified */
    if (duration <= 0.0f) {
        float precision = fmaxf(0.01f, goal->precision_required);
        float a = 50.0f;   /* Intercept (ms) */
        float b = 150.0f;  /* Slope (ms) */
        duration = a + b * log2f(distance / precision + 1.0f);
        duration = fmaxf(duration, adapter->config.reaction_time_ms);
    }

    /* Create simple trajectory with start and end */
    if (adapter->num_trajectories >= adapter->config.max_trajectories) {
        set_error(adapter, MOTOR_ERROR_BUFFER_OVERFLOW);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "motor_plan_movement: capacity exceeded");
        return false;
    }

    trajectory_plan_t* traj = &adapter->trajectories[adapter->num_trajectories];
    traj->waypoints = (trajectory_waypoint_t*)nimcp_calloc(2, sizeof(trajectory_waypoint_t));
    if (!traj->waypoints) {
        set_error(adapter, MOTOR_ERROR_INTERNAL);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_plan_movement: traj->waypoints is NULL");
        return false;
    }
    /* Only increment after successful allocation to avoid memory leak on error path */
    adapter->num_trajectories++;

    /* Start waypoint */
    traj->waypoints[0].position = effector->position;
    traj->waypoints[0].velocity = effector->velocity;
    traj->waypoints[0].time_ms = 0.0f;

    /* End waypoint */
    traj->waypoints[1].position = goal->target_position;
    traj->waypoints[1].velocity = goal->target_velocity;
    traj->waypoints[1].time_ms = duration;

    traj->num_waypoints = 2;
    traj->current_waypoint = 0;
    traj->elapsed_ms = 0.0f;
    traj->region = goal->region;
    traj->is_active = true;

    adapter->active_trajectory = traj;
    adapter->status = MOTOR_STATUS_PREPARING;

    emit_event(adapter, 1 /* MOTOR_EVENT_PLAN_COMPLETE */, goal);

    LOG_DEBUG("[%s] Movement planned: distance=%.2f, duration=%.2fms",
              MOTOR_LOG_MODULE, distance, duration);

    return true;
}

bool motor_plan_trajectory(motor_adapter_t* adapter,
                            motor_region_t region,
                            const trajectory_waypoint_t* waypoints,
                            uint32_t num_waypoints) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_plan_movement: adapter is NULL");
        return false;
    }
    if (!waypoints) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: waypoints");
        set_error(adapter, MOTOR_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_plan_movement: waypoints is NULL");
        return false;
    }
    if (num_waypoints < 2) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: num_waypoints must be >= 2");
        set_error(adapter, MOTOR_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_plan_movement: validation failed");
        return false;
    }

    if (adapter->num_trajectories >= adapter->config.max_trajectories) {
        set_error(adapter, MOTOR_ERROR_BUFFER_OVERFLOW);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "motor_plan_movement: capacity exceeded");
        return false;
    }

    LOG_DEBUG("[%s] Planning trajectory with %u waypoints", MOTOR_LOG_MODULE, num_waypoints);

    adapter->status = MOTOR_STATUS_PLANNING;
    adapter->stats.movements_planned++;

    trajectory_plan_t* traj = &adapter->trajectories[adapter->num_trajectories];
    traj->waypoints = (trajectory_waypoint_t*)nimcp_calloc(
        num_waypoints, sizeof(trajectory_waypoint_t));
    if (!traj->waypoints) {
        set_error(adapter, MOTOR_ERROR_INTERNAL);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_plan_movement: traj->waypoints is NULL");
        return false;
    }
    /* Only increment after successful allocation to avoid memory leak on error path */
    adapter->num_trajectories++;

    memcpy(traj->waypoints, waypoints, num_waypoints * sizeof(trajectory_waypoint_t));
    traj->num_waypoints = num_waypoints;
    traj->current_waypoint = 0;
    traj->elapsed_ms = 0.0f;
    traj->region = region;
    traj->is_active = true;

    adapter->active_trajectory = traj;
    adapter->status = MOTOR_STATUS_PREPARING;
    adapter->has_active_goal = true;

    return true;
}

bool motor_plan_program_execution(motor_adapter_t* adapter,
                                   uint32_t program_id,
                                   float speed_factor) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_plan_movement: adapter is NULL");
        return false;
    }
    if (program_id == 0) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: program_id cannot be 0");
        set_error(adapter, MOTOR_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_plan_movement: program_id is zero");
        return false;
    }

    /* Find program */
    uint32_t idx = hash_program_id(program_id, adapter->program_capacity);
    motor_program_node_t* node = adapter->programs[idx];

    while (node) {
        if (node->info.program_id == program_id) break;
        node = node->next;
    }

    if (!node) {
        set_error(adapter, MOTOR_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_plan_movement: node is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Planning execution of program '%s' (speed=%.2fx)",
              MOTOR_LOG_MODULE, node->info.name, speed_factor);

    adapter->status = MOTOR_STATUS_PLANNING;
    adapter->stats.movements_planned++;

    /* Clear command queue (thread-safe) */
    nimcp_mutex_lock(adapter->queue_mutex);
    adapter->queue_head = 0;
    adapter->queue_tail = 0;
    adapter->queue_count = 0;
    nimcp_mutex_unlock(adapter->queue_mutex);

    /* Scale commands by speed factor and enqueue */
    float scale = (speed_factor > 0.0f) ? (1.0f / speed_factor) : 1.0f;

    for (uint32_t i = 0; i < node->info.num_commands; i++) {
        motor_command_t cmd = node->commands[i];
        cmd.timestamp_ms *= scale;
        cmd.duration_ms *= scale;

        if (!enqueue_command(adapter, &cmd)) {
            set_error(adapter, MOTOR_ERROR_BUFFER_OVERFLOW);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_plan_movement: enqueue_command is NULL");
            return false;
        }
    }

    adapter->status = MOTOR_STATUS_PREPARING;
    adapter->has_active_goal = true;

    return true;
}

/*=============================================================================
 * MOTOR EXECUTION
 *===========================================================================*/

bool motor_begin_execution(motor_adapter_t* adapter) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    if (adapter->status != MOTOR_STATUS_PREPARING) {
        LOG_WARN("[%s] Cannot begin execution from status %d", MOTOR_LOG_MODULE, adapter->status);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_begin_execution: validation failed");
        return false;
    }

    LOG_DEBUG("[%s] Beginning movement execution", MOTOR_LOG_MODULE);

    adapter->status = MOTOR_STATUS_EXECUTING;
    adapter->execution_time_ms = 0.0f;
    adapter->stats.movements_executed++;

    emit_event(adapter, 2 /* MOTOR_EVENT_EXECUTION_STARTED */, NULL);

    return true;
}

bool motor_update_execution(motor_adapter_t* adapter, float dt_ms) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    if (adapter->status != MOTOR_STATUS_EXECUTING &&
        adapter->status != MOTOR_STATUS_CORRECTING) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_update_execution: operation failed");
        return false;
    }

    adapter->execution_time_ms += dt_ms;
    adapter->current_time_ms += dt_ms;

    /* Process active trajectory */
    if (adapter->active_trajectory && adapter->active_trajectory->is_active) {
        trajectory_plan_t* traj = adapter->active_trajectory;
        traj->elapsed_ms += dt_ms;

        /* Find current segment */
        uint32_t seg = 0;
        while (seg + 1 < traj->num_waypoints &&
               traj->elapsed_ms > traj->waypoints[seg + 1].time_ms) {
            seg++;
        }

        /* Check if complete */
        if (seg + 1 >= traj->num_waypoints) {
            traj->is_active = false;
            adapter->status = MOTOR_STATUS_COMPLETE;

            /* Fill result */
            adapter->last_result.success = true;
            adapter->last_result.final_position = traj->waypoints[traj->num_waypoints - 1].position;
            adapter->last_result.actual_duration_ms = traj->elapsed_ms;
            adapter->last_result.accuracy = 1.0f;

            /* Update effector state to final position */
            uint32_t eff_id = (uint32_t)traj->region;
            if (eff_id < adapter->num_effectors) {
                adapter->effectors[eff_id].position = adapter->last_result.final_position;
                adapter->effectors[eff_id].velocity.x = 0.0f;
                adapter->effectors[eff_id].velocity.y = 0.0f;
                adapter->effectors[eff_id].velocity.z = 0.0f;
            }

            /* Capture completion callback under mutex to prevent race */
            motor_complete_callback_t complete_cb = NULL;
            void* complete_data = NULL;
            motor_result_t result_copy = adapter->last_result;

            nimcp_mutex_lock(adapter->queue_mutex);
            complete_cb = adapter->complete_callback;
            complete_data = adapter->complete_user_data;
            nimcp_mutex_unlock(adapter->queue_mutex);

            /* Invoke completion callback with atomic CAS re-entrance guard */
            if (complete_cb) {
                bool expected = false;
                if (nimcp_atomic_compare_exchange_bool(&adapter->in_callback, &expected, true,
                                                        NIMCP_MEMORY_ORDER_ACQ_REL)) {
                    complete_cb(&result_copy, complete_data);
                    nimcp_atomic_store_bool(&adapter->in_callback, false, NIMCP_MEMORY_ORDER_RELEASE);
                }
            }

            emit_event(adapter, 3 /* MOTOR_EVENT_EXECUTION_COMPLETE */, &adapter->last_result);
            adapter->stats.successful_movements++;

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_update_execution: operation failed");
            return false;  /* Execution complete */
        }

        /* Interpolate position within segment */
        float seg_start = traj->waypoints[seg].time_ms;
        float seg_end = traj->waypoints[seg + 1].time_ms;
        float t = (traj->elapsed_ms - seg_start) / (seg_end - seg_start);
        t = fmaxf(0.0f, fminf(1.0f, t));

        motor_vec3_t pos = lerp_vec3(&traj->waypoints[seg].position,
                                      &traj->waypoints[seg + 1].position, t);

        /* Generate motor command */
        uint32_t effector_id = (uint32_t)traj->region;
        if (effector_id < adapter->num_effectors) {
            /* Update effector state during execution */
            adapter->effectors[effector_id].position = pos;

            motor_command_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.effector_id = effector_id;
            cmd.target_position = pos;
            cmd.duration_ms = dt_ms;
            cmd.timestamp_ms = adapter->current_time_ms;

            enqueue_command(adapter, &cmd);
            adapter->stats.commands_generated++;
        }

        return true;  /* Still executing */
    }

    /* Process queued commands */
    if (adapter->queue_count > 0) {
        /* Just dequeue - actual output happens through get_next_command */
        return true;
    }

    /* No more work */
    adapter->status = MOTOR_STATUS_COMPLETE;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_update_execution: validation failed");
    return false;
}

bool motor_stop_execution(motor_adapter_t* adapter) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    LOG_DEBUG("[%s] Stopping movement execution", MOTOR_LOG_MODULE);

    /* Clear active trajectory */
    if (adapter->active_trajectory) {
        adapter->active_trajectory->is_active = false;
        adapter->active_trajectory = NULL;
    }

    /* Clear command queue (thread-safe) */
    nimcp_mutex_lock(adapter->queue_mutex);
    adapter->queue_head = 0;
    adapter->queue_tail = 0;
    adapter->queue_count = 0;
    nimcp_mutex_unlock(adapter->queue_mutex);

    /* Generate stop commands for all active effectors */
    for (uint32_t i = 0; i < adapter->num_effectors; i++) {
        if (adapter->effectors[i].is_active) {
            motor_command_t stop_cmd;
            memset(&stop_cmd, 0, sizeof(stop_cmd));
            stop_cmd.effector_id = i;
            stop_cmd.target_position = adapter->effectors[i].position;
            stop_cmd.target_velocity.x = 0.0f;
            stop_cmd.target_velocity.y = 0.0f;
            stop_cmd.target_velocity.z = 0.0f;
            stop_cmd.duration_ms = 0.0f;
            stop_cmd.timestamp_ms = adapter->current_time_ms;

            enqueue_command(adapter, &stop_cmd);
        }
    }

    adapter->status = MOTOR_STATUS_IDLE;
    adapter->has_active_goal = false;

    emit_event(adapter, 4 /* MOTOR_EVENT_STOPPED */, NULL);

    return true;
}

bool motor_get_next_command(motor_adapter_t* adapter, motor_command_t* command) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(command, "command");

    bool success = false;

    nimcp_mutex_lock(adapter->queue_mutex);

    if (adapter->queue_count > 0) {
        *command = adapter->command_queue[adapter->queue_head].command;
        adapter->command_queue[adapter->queue_head].is_valid = false;
        adapter->queue_head = (adapter->queue_head + 1) % adapter->queue_capacity;
        adapter->queue_count--;
        success = true;
    }

    nimcp_mutex_unlock(adapter->queue_mutex);

    return success;
}

bool motor_get_result(const motor_adapter_t* adapter, motor_result_t* result) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(result, "result");

    if (adapter->status != MOTOR_STATUS_COMPLETE &&
        adapter->status != MOTOR_STATUS_ERROR) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_get_result: operation failed");
        return false;
    }

    *result = adapter->last_result;
    return true;
}

/*=============================================================================
 * SENSORY FEEDBACK
 *===========================================================================*/

bool motor_update_feedback(motor_adapter_t* adapter,
                            uint32_t effector_id,
                            const motor_effector_state_t* state) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(state, "state");
    if (effector_id >= adapter->num_effectors) {
        NIMCP_ERROR_SET(NIMCP_ERROR_OUT_OF_RANGE, "Out of bounds: effector_id (%u >= %u)",
                       effector_id, adapter->num_effectors);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_get_result: capacity exceeded");
        return false;
    }

    /* Update internal state */
    adapter->effectors[effector_id] = *state;

    /* Apply corrections if executing with feedback enabled */
    if (adapter->status == MOTOR_STATUS_EXECUTING &&
        adapter->config.enable_feedback &&
        adapter->has_active_goal) {

        /* Compute position error */
        float error = compute_distance(&state->position,
                                        &adapter->current_goal.target_position);

        if (error > 0.1f) {  /* Error threshold */
            adapter->status = MOTOR_STATUS_CORRECTING;
            adapter->stats.corrections_applied++;

            /* Generate corrective command */
            motor_command_t correction;
            memset(&correction, 0, sizeof(correction));
            correction.effector_id = effector_id;
            correction.target_position = adapter->current_goal.target_position;
            correction.duration_ms = error * 10.0f;  /* Simple proportional */
            correction.timestamp_ms = adapter->current_time_ms;

            enqueue_command(adapter, &correction);
        }
    }

    return true;
}

bool motor_update_visual_feedback(motor_adapter_t* adapter,
                                   uint32_t effector_id,
                                   const motor_vec3_t* visual_position,
                                   float confidence) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(visual_position, "visual_position");
    if (effector_id >= adapter->num_effectors) {
        NIMCP_ERROR_SET(NIMCP_ERROR_OUT_OF_RANGE, "Out of bounds: effector_id (%u >= %u)",
                       effector_id, adapter->num_effectors);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_get_result: capacity exceeded");
        return false;
    }

    /* Blend visual feedback with proprioception based on confidence */
    motor_effector_state_t* eff = &adapter->effectors[effector_id];

    float blend = confidence * 0.3f;  /* Visual feedback weight */
    eff->position.x = eff->position.x * (1.0f - blend) + visual_position->x * blend;
    eff->position.y = eff->position.y * (1.0f - blend) + visual_position->y * blend;
    eff->position.z = eff->position.z * (1.0f - blend) + visual_position->z * blend;

    return true;
}

/*=============================================================================
 * INTEGRATION INTERFACES
 *===========================================================================*/

bool motor_receive_bg_selection(motor_adapter_t* adapter,
                                 uint32_t action_id,
                                 float vigor) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    LOG_DEBUG("[%s] Received BG action selection: action=%u, vigor=%.2f",
              MOTOR_LOG_MODULE, action_id, vigor);

    /* Scale movement parameters by vigor */
    if (adapter->has_active_goal) {
        adapter->current_goal.urgency = vigor;

        /* Vigor affects movement speed */
        if (adapter->active_trajectory && adapter->active_trajectory->is_active) {
            /* Would scale trajectory timing by vigor here */
        }
    }

    return true;
}

bool motor_receive_cerebellar_correction(motor_adapter_t* adapter,
                                          uint32_t effector_id,
                                          float timing_correction,
                                          float amplitude_correction) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    if (effector_id >= adapter->num_effectors) {
        NIMCP_ERROR_SET(NIMCP_ERROR_OUT_OF_RANGE, "Out of bounds: effector_id (%u >= %u)",
                       effector_id, adapter->num_effectors);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_get_result: capacity exceeded");
        return false;
    }

    LOG_DEBUG("[%s] Received cerebellar correction: effector=%u, timing=%.2fms, amp=%.2f",
              MOTOR_LOG_MODULE, effector_id, timing_correction, amplitude_correction);

    /* Apply correction to current trajectory if active */
    if (adapter->active_trajectory && adapter->active_trajectory->is_active) {
        trajectory_plan_t* traj = adapter->active_trajectory;

        /* Adjust timing */
        traj->elapsed_ms += timing_correction;

        /* Amplitude correction would scale velocity/force */
        adapter->stats.corrections_applied++;
    }

    return true;
}

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

bool motor_train_movement(motor_adapter_t* adapter,
                           const motor_vec3_t* target_position,
                           float learning_rate) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(target_position, "target_position");
    if (!adapter->config.enable_training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_get_result: adapter->config is NULL");
        return false;
    }

    if (learning_rate <= 0.0f) {
        learning_rate = adapter->config.learning_rate;
    }

    /* Compute error from last movement */
    if (adapter->status == MOTOR_STATUS_COMPLETE) {
        motor_vec3_t error;
        error.x = target_position->x - adapter->last_result.final_position.x;
        error.y = target_position->y - adapter->last_result.final_position.y;
        error.z = target_position->z - adapter->last_result.final_position.z;

        float error_magnitude = sqrtf(error.x * error.x +
                                      error.y * error.y +
                                      error.z * error.z);

        adapter->stats.training_iterations++;
        adapter->stats.training_loss = error_magnitude;

        LOG_DEBUG("[%s] Training: error=%.4f, lr=%.4f",
                  MOTOR_LOG_MODULE, error_magnitude, learning_rate);
    }

    return true;
}

uint32_t motor_train_from_demonstration(motor_adapter_t* adapter,
                                         const trajectory_waypoint_t* waypoints,
                                         uint32_t num_waypoints,
                                         const char* program_name) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        return 0;
    }
    if (!waypoints) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: waypoints");
        return 0;
    }
    if (num_waypoints < 2) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: num_waypoints must be >= 2");
        return 0;
    }
    if (!program_name) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: program_name");
        return 0;
    }

    LOG_DEBUG("[%s] Learning from demonstration: '%s' (%u waypoints)",
              MOTOR_LOG_MODULE, program_name, num_waypoints);

    /* Convert waypoints to motor commands */
    motor_command_t* commands = (motor_command_t*)nimcp_calloc(
        num_waypoints, sizeof(motor_command_t));
    if (!commands) return 0;

    for (uint32_t i = 0; i < num_waypoints; i++) {
        commands[i].effector_id = 0;  /* Default effector */
        commands[i].target_position = waypoints[i].position;
        commands[i].target_velocity = waypoints[i].velocity;
        commands[i].timestamp_ms = waypoints[i].time_ms;

        if (i + 1 < num_waypoints) {
            commands[i].duration_ms = waypoints[i + 1].time_ms - waypoints[i].time_ms;
        } else {
            commands[i].duration_ms = 0.0f;
        }
    }

    /* Store as motor program */
    uint32_t program_id = motor_store_program(adapter, program_name,
                                               commands, num_waypoints,
                                               MOVEMENT_TYPE_CONTINUOUS);

    nimcp_free(commands);

    return program_id;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

motor_status_t motor_get_status(const motor_adapter_t* adapter) {
    if (!adapter) return MOTOR_STATUS_ERROR;
    return adapter->status;
}

motor_error_t motor_get_last_error(const motor_adapter_t* adapter) {
    if (!adapter) return MOTOR_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* motor_error_string(motor_error_t error) {
    switch (error) {
        case MOTOR_ERROR_NONE: return "No error";
        case MOTOR_ERROR_INVALID_INPUT: return "Invalid input";
        case MOTOR_ERROR_PLANNING_FAILURE: return "Planning failed";
        case MOTOR_ERROR_EXECUTION_FAILURE: return "Execution failed";
        case MOTOR_ERROR_TRAJECTORY_INFEASIBLE: return "Trajectory infeasible";
        case MOTOR_ERROR_EFFECTOR_CONFLICT: return "Effector conflict";
        case MOTOR_ERROR_TIMING_VIOLATION: return "Timing violation";
        case MOTOR_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case MOTOR_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* motor_status_string(motor_status_t status) {
    switch (status) {
        case MOTOR_STATUS_IDLE: return "Idle";
        case MOTOR_STATUS_PLANNING: return "Planning";
        case MOTOR_STATUS_PREPARING: return "Preparing";
        case MOTOR_STATUS_EXECUTING: return "Executing";
        case MOTOR_STATUS_CORRECTING: return "Correcting";
        case MOTOR_STATUS_COMPLETE: return "Complete";
        case MOTOR_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool motor_get_stats(const motor_adapter_t* adapter, motor_stats_t* stats) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(stats, "stats");
    *stats = adapter->stats;
    return true;
}

bool motor_get_config(const motor_adapter_t* adapter, motor_config_t* config) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(config, "config");
    *config = adapter->config;
    return true;
}

bool motor_get_effector_state(const motor_adapter_t* adapter,
                               uint32_t effector_id,
                               motor_effector_state_t* state) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(state, "state");
    if (effector_id >= adapter->num_effectors) {
        NIMCP_ERROR_SET(NIMCP_ERROR_OUT_OF_RANGE, "Out of bounds: effector_id (%u >= %u)",
                       effector_id, adapter->num_effectors);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_get_config: capacity exceeded");
        return false;
    }
    *state = adapter->effectors[effector_id];
    return true;
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

bool motor_set_command_callback(motor_adapter_t* adapter,
                                 motor_command_callback_t callback,
                                 void* user_data) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    /* THREAD SAFETY: Acquire mutex to prevent race with callback invocation */
    nimcp_mutex_lock(adapter->queue_mutex);
    adapter->command_callback = callback;
    adapter->command_user_data = user_data;
    nimcp_mutex_unlock(adapter->queue_mutex);

    return true;
}

bool motor_set_complete_callback(motor_adapter_t* adapter,
                                  motor_complete_callback_t callback,
                                  void* user_data) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    /* THREAD SAFETY: Acquire mutex to prevent race with callback invocation */
    nimcp_mutex_lock(adapter->queue_mutex);
    adapter->complete_callback = callback;
    adapter->complete_user_data = user_data;
    nimcp_mutex_unlock(adapter->queue_mutex);

    return true;
}

bool motor_set_event_callback(motor_adapter_t* adapter,
                               motor_event_callback_t callback,
                               void* user_data) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    /* THREAD SAFETY: Acquire mutex to prevent race with callback invocation */
    nimcp_mutex_lock(adapter->queue_mutex);
    adapter->event_callback = callback;
    adapter->event_user_data = user_data;
    nimcp_mutex_unlock(adapter->queue_mutex);

    return true;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION API
 *===========================================================================*/

bio_module_context_t motor_get_bio_context(motor_adapter_t* adapter) {
    NIMCP_CHECK_NULL_PTR(adapter, "adapter");
    return adapter->bio_ctx;
}

uint32_t motor_process_bio_messages(motor_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        return 0;
    }
    if (!adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG("[%s] Processed %u bio-async messages", MOTOR_LOG_MODULE, processed);
    }
    return processed;
}

nimcp_bio_future_t motor_request_movement_async(
    motor_adapter_t* adapter,
    const motor_goal_t* goal) {

    NIMCP_CHECK_NULL_PTR(adapter, "adapter");
    NIMCP_CHECK_NULL_PTR(goal, "goal");
    if (!adapter->bio_ctx) {
        LOG_WARNING("[%s] Cannot request movement: bio_ctx not initialized", MOTOR_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_process_bio_messages: adapter->bio_ctx is NULL");
        return NULL;
    }

    LOG_DEBUG("[%s] Requesting movement async to region %d", MOTOR_LOG_MODULE, goal->region);

    /* Create motor command request message */
    bio_msg_motor_command_request_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_MOTOR_COMMAND_REQUEST;
    msg.header.source_module = BIO_MODULE_MOTOR_CORTEX;
    msg.header.target_module = BIO_MODULE_MOTOR_CORTEX;
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;

    /* Encode goal in message */
    msg.phoneme = (uint8_t)goal->region;  /* Repurpose phoneme field for region */
    msg.duration_ms = goal->max_duration_ms;
    msg.intensity = goal->urgency;

    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->default_channel);

    if (!promise) {
        LOG_ERROR("[%s] Failed to send movement request", MOTOR_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "promise is NULL");


        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_error_t motor_broadcast_completion(
    motor_adapter_t* adapter,
    const motor_result_t* result) {

    NIMCP_CHECK_NULL(adapter, "adapter");
    NIMCP_CHECK_NULL(result, "result");

    if (!adapter->bio_ctx) {
        LOG_DEBUG("[%s] Cannot broadcast: bio-async not available", MOTOR_LOG_MODULE);
        return NIMCP_SUCCESS;
    }

    LOG_INFO("[%s] Broadcasting movement completion (success=%d, accuracy=%.2f)",
             MOTOR_LOG_MODULE, result->success, result->accuracy);

    bio_msg_motor_command_result_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_MOTOR_COMMAND_RESULT;
    msg.header.source_module = BIO_MODULE_MOTOR_CORTEX;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    /* Encode result */
    msg.timestamp_ms = adapter->current_time_ms;

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Implementation)
 *===========================================================================*/

static nimcp_error_t handle_motor_command_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    motor_adapter_t* adapter = (motor_adapter_t*)user_data;
    const bio_msg_motor_command_request_t* req = (const bio_msg_motor_command_request_t*)msg;

    if (!adapter || !req || msg_size < sizeof(bio_msg_motor_command_request_t)) {
        LOG_ERROR("[%s] Invalid motor command request", MOTOR_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling motor command request", MOTOR_LOG_MODULE);

    /* Create goal from request */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = (motor_region_t)req->phoneme;
    goal.max_duration_ms = req->duration_ms;
    goal.urgency = req->intensity;
    goal.precision_required = 0.1f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    /* Plan and begin execution */
    bool success = motor_plan_movement(adapter, &goal);
    if (success) {
        motor_begin_execution(adapter);
    }

    /* Build response */
    bio_msg_motor_command_result_t response;
    memset(&response, 0, sizeof(response));

    response.header.type = BIO_MSG_MOTOR_COMMAND_RESULT;
    response.header.source_module = BIO_MODULE_MOTOR_CORTEX;
    response.header.target_module = req->header.source_module;
    response.header.payload_size = sizeof(response);
    response.header.channel = req->header.channel;

    response.timestamp_ms = adapter->current_time_ms;

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_motor_stop_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    motor_adapter_t* adapter = (motor_adapter_t*)user_data;

    if (!adapter) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling motor stop request", MOTOR_LOG_MODULE);

    motor_stop_execution(adapter);

    (void)msg;
    (void)msg_size;
    (void)response_promise;

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_bg_action_selection(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    motor_adapter_t* adapter = (motor_adapter_t*)user_data;
    const bio_msg_bg_action_selection_t* selection = (const bio_msg_bg_action_selection_t*)msg;

    if (!adapter || !selection || msg_size < sizeof(bio_msg_bg_action_selection_t)) {
        LOG_ERROR("[%s] Invalid BG action selection message", MOTOR_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling BG action selection: action=%u, vigor=%.2f, confidence=%.2f",
              MOTOR_LOG_MODULE, selection->action_id, selection->vigor, selection->confidence);

    /* Apply action selection to motor cortex */
    bool success = motor_receive_bg_selection(adapter, selection->action_id, selection->vigor);

    /* Update goal urgency based on BG confidence and urgency signals */
    if (adapter->has_active_goal && success) {
        /* Combine BG confidence with current urgency */
        float combined_urgency = adapter->current_goal.urgency * 0.5f +
                                  selection->urgency * 0.3f +
                                  selection->confidence * 0.2f;
        adapter->current_goal.urgency = fminf(1.0f, combined_urgency);

        /* If habitual, we can potentially speed up execution */
        if (selection->is_habit && adapter->active_trajectory) {
            LOG_DEBUG("[%s] Habitual action detected, applying speedup", MOTOR_LOG_MODULE);
        }
    }

    (void)response_promise;

    return success ? NIMCP_SUCCESS : NIMCP_BIO_ERROR_NOT_INITIALIZED;
}

static nimcp_error_t handle_cerebellar_correction(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    motor_adapter_t* adapter = (motor_adapter_t*)user_data;
    const bio_msg_cerebellar_correction_t* correction = (const bio_msg_cerebellar_correction_t*)msg;

    if (!adapter || !correction || msg_size < sizeof(bio_msg_cerebellar_correction_t)) {
        LOG_ERROR("[%s] Invalid cerebellar correction message", MOTOR_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling cerebellar correction: effector=%u, timing=%.2fms, amp=%.2f",
              MOTOR_LOG_MODULE, correction->effector_id, correction->timing_correction_ms,
              correction->amplitude_correction);

    /* Apply cerebellar correction to motor output */
    bool success = motor_receive_cerebellar_correction(
        adapter,
        correction->effector_id,
        correction->timing_correction_ms,
        correction->amplitude_correction
    );

    /* Apply additional cerebellar refinement parameters */
    if (success && adapter->active_trajectory && adapter->active_trajectory->is_active) {
        /* Apply coordination weight to multi-joint movements */
        if (correction->coordination_weight != 0.0f && correction->coordination_weight != 1.0f) {
            /* Coordination weight modulates inter-joint coupling */
            LOG_DEBUG("[%s] Applying coordination weight: %.2f", MOTOR_LOG_MODULE,
                      correction->coordination_weight);
        }

        /* Apply phase correction for rhythmic movements */
        if (fabsf(correction->phase_correction) > 0.001f) {
            trajectory_plan_t* traj = adapter->active_trajectory;
            /* Phase correction adjusts position within movement cycle */
            float phase_time_adjustment = correction->phase_correction *
                                          (traj->waypoints[traj->num_waypoints - 1].time_ms / (NIMCP_TWO_PI_F));
            traj->elapsed_ms += phase_time_adjustment;
            LOG_DEBUG("[%s] Phase correction applied: %.2f rad -> %.2fms",
                      MOTOR_LOG_MODULE, correction->phase_correction, phase_time_adjustment);
        }
    }

    (void)response_promise;

    return success ? NIMCP_SUCCESS : NIMCP_BIO_ERROR_NOT_INITIALIZED;
}
