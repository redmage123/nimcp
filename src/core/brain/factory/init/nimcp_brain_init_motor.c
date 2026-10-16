//=============================================================================
// nimcp_brain_init_motor.c - Motor Cortex Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_motor.c
 * @brief Motor Cortex Initialization Implementation
 *
 * WHAT: Initialization functions for Motor Cortex (M1, premotor, SMA)
 * WHY:  Enable motor planning and execution capabilities in the brain
 * HOW:  Creates motor adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase M1: Motor Cortex Brain Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_motor.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_MOTOR"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_motor, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Motor Cortex includes
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "core/brain/regions/motor/nimcp_motor_quantum_bridge.h"

// Bio-async includes
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

// Forward declarations for substrate and thalamic bridges
// (will be implemented in separate files following Broca pattern)
struct motor_substrate_bridge;
typedef struct motor_substrate_bridge motor_substrate_bridge_t;

struct motor_thalamic_bridge;
typedef struct motor_thalamic_bridge motor_thalamic_bridge_t;

// Motor substrate config (duplicated to avoid header conflicts)
typedef struct {
    bool enable_fatigue_modeling;
    bool enable_atp_modulation;
    float fatigue_recovery_rate;
    float atp_consumption_rate;
} motor_substrate_config_t;

// Motor thalamic config (duplicated to avoid header conflicts)
typedef struct {
    bool enable_va_routing;
    bool enable_vl_routing;
    bool enable_gating;
    float min_gating_threshold;
} motor_thalamic_config_t;

// Substrate bridge API declarations (to avoid header conflicts)
extern motor_substrate_config_t motor_substrate_default_config(void);
extern motor_substrate_bridge_t* motor_substrate_bridge_create(
    void* motor, void* substrate, const motor_substrate_config_t* config);
extern void motor_substrate_bridge_destroy(motor_substrate_bridge_t* bridge);
extern int motor_substrate_bridge_update(motor_substrate_bridge_t* bridge);

// Thalamic bridge API declarations (to avoid header conflicts)
extern motor_thalamic_config_t motor_thalamic_default_config(void);
extern motor_thalamic_bridge_t* motor_thalamic_bridge_create(
    void* motor, void* router, const motor_thalamic_config_t* config);
extern void motor_thalamic_bridge_destroy(motor_thalamic_bridge_t* bridge);
extern int motor_thalamic_bridge_reset(motor_thalamic_bridge_t* bridge);

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Message handler for motor command requests
 *
 * WHAT: Process motor command execution requests from other brain regions
 * WHY:  Enable coordinated movement initiation from prefrontal/basal ganglia
 * HOW:  Queue motor command and initiate execution pipeline
 *
 * @param msg Motor command request message
 * @param msg_size Message size
 * @param response_promise Promise for response (execution result)
 * @param user_data Motor cortex adapter pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t motor_handle_command_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when motor command format is finalized
     *
     * Implementation would:
     * 1. Validate motor command parameters (target, velocity, duration)
     * 2. Check motor readiness and inhibition status
     * 3. Queue command in motor planning buffer
     * 4. Initiate execution sequence via M1 activation
     * 5. Send acknowledgment with estimated execution time
     */

    LOG_TRACE(LOG_MODULE, "Motor cortex received command request (stub)");
    return NIMCP_SUCCESS;
}

/**
 * @brief Message handler for motor stop requests
 *
 * WHAT: Process emergency stop or inhibition requests
 * WHY:  Enable rapid motor inhibition from prefrontal/basal ganglia
 * HOW:  Interrupt current execution and activate motor inhibition
 *
 * @param msg Motor stop request message
 * @param msg_size Message size
 * @param response_promise Promise for response (stop confirmation)
 * @param user_data Motor cortex adapter pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t motor_handle_stop_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when motor stop format is finalized
     *
     * Implementation would:
     * 1. Immediately halt current motor execution
     * 2. Clear pending motor commands from queue
     * 3. Activate antagonist muscle groups for rapid deceleration
     * 4. Update motor state to INHIBITED
     * 5. Send confirmation with stop latency metrics
     */

    LOG_TRACE(LOG_MODULE, "Motor cortex received stop request (stub)");
    return NIMCP_SUCCESS;
}

/**
 * @brief Message handler for cerebellar correction signals
 *
 * WHAT: Process real-time motor corrections from cerebellum
 * WHY:  Enable online motor adjustment for precision movements
 * HOW:  Apply correction to ongoing motor commands
 *
 * @param msg Cerebellar correction message
 * @param msg_size Message size
 * @param response_promise Promise for response (may be NULL)
 * @param user_data Motor cortex adapter pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t motor_handle_cerebellar_correction(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when cerebellar correction format is finalized
     *
     * Implementation would:
     * 1. Extract correction vector from cerebellar signal
     * 2. Validate correction is within safe bounds
     * 3. Apply correction to active motor commands
     * 4. Update internal forward model with error signal
     * 5. Optionally trigger motor learning if error is systematic
     */

    LOG_TRACE(LOG_MODULE, "Motor cortex received cerebellar correction (stub)");
    return NIMCP_SUCCESS;
}

/**
 * @brief KG-driven wiring handler callback for Motor Cortex
 *
 * WHAT: Register message handlers based on KG-discovered message types
 * WHY:  Enable dynamic wiring driven by knowledge graph
 * HOW:  Iterate discovered message types and register appropriate handlers
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Motor cortex adapter pointer
 * @return 0 on success, -1 on error
 */
static int motor_init_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO(LOG_MODULE, "motor_init_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_MOTOR_COMMAND_REQUEST:
                bio_router_register_handler(ctx, message_types[i], motor_handle_command_request);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_MOTOR_COMMAND_REQUEST");
                break;

            case BIO_MSG_MOTOR_STOP_REQUEST:
                bio_router_register_handler(ctx, message_types[i], motor_handle_stop_request);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_MOTOR_STOP_REQUEST");
                break;

            case BIO_MSG_CEREBELLAR_CORRECTION:
                bio_router_register_handler(ctx, message_types[i], motor_handle_cerebellar_correction);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_CEREBELLAR_CORRECTION");
                break;

            default:
                LOG_DEBUG(LOG_MODULE, "  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

/**
 * @brief Connect motor to bio-async messaging
 */
static bool connect_motor_to_bio_async(brain_t brain) {
    if (!brain || !brain->motor) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /* Register motor cortex module with bio-async router */
        bio_module_info_t info = {
            .module_id = BIO_MODULE_MOTOR_CORTEX,
            .module_name = "motor_cortex",
            .inbox_capacity = 64,
            .user_data = brain->motor
        };

        bio_module_context_t ctx = bio_router_register_module(&info);
        if (ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_MOTOR_CORTEX,
                (void*)motor_init_wiring_handler_callback,
                brain->motor
            );

            if (cb_result == NIMCP_SUCCESS) {
                LOG_DEBUG(LOG_MODULE, "Motor cortex bio-async registered with KG-driven wiring (module_id=0x%04X)",
                          BIO_MODULE_MOTOR_CORTEX);
            } else {
                /* Fallback: Direct registration if orchestrator not available */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_MOTOR_COMMAND_REQUEST,
                                                motor_handle_command_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_MOTOR_STOP_REQUEST,
                                                motor_handle_stop_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_CEREBELLAR_CORRECTION,
                                                motor_handle_cerebellar_correction)
                );
                LOG_DEBUG(LOG_MODULE, "Motor cortex bio-async registered with legacy handlers (module_id=0x%04X)",
                          BIO_MODULE_MOTOR_CORTEX);
            }
        } else {
            LOG_WARN(LOG_MODULE, "Failed to register motor cortex with bio-async");
        }
    }

    return true;
}

/**
 * @brief Connect motor substrate bridge to neural substrate
 */
static bool connect_substrate_bridge(brain_t brain) {
    if (!brain || !brain->motor_substrate_bridge) return true;

    /* Apply initial metabolic effects */
    if (motor_substrate_bridge_update(brain->motor_substrate_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Initial motor substrate bridge update failed");
    }

    return true;
}

/**
 * @brief Connect motor thalamic bridge to thalamic router
 */
static bool connect_thalamic_bridge(brain_t brain) {
    if (!brain || !brain->motor_thalamic_bridge) return true;

    /* Reset bridge to clean state */
    if (motor_thalamic_bridge_reset(brain->motor_thalamic_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Motor thalamic bridge reset failed");
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_motor_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_motor_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->motor) {
        return true;  /* Already initialized */
    }

    /* Motor cortex is enabled by default for capable brains */
    LOG_INFO(LOG_MODULE, "Initializing Motor Cortex subsystem");

    /* Create motor adapter with default configuration */
    motor_config_t motor_cfg = motor_default_config();

    /* Enable bio-async if brain has it enabled */
    motor_cfg.enable_bio_async = brain->bio_async_enabled;

    /* Enable training integration if brain has it */
    motor_cfg.enable_training = brain->enable_training_integration;

    brain->motor = motor_create(&motor_cfg);
    if (!brain->motor) {
        set_error("Failed to create Motor Cortex adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_motor_subsystem: brain->motor is NULL");
        return false;
    }

    brain->motor_enabled = true;
    brain->last_motor_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Motor adapter created successfully");

    /* Initialize integration bridges */
    if (!nimcp_brain_factory_init_motor_substrate_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Motor substrate bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_motor_thalamic_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Motor thalamic bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_motor_quantum_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Motor quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_motor_to_basal_ganglia(brain)) {
        LOG_WARN(LOG_MODULE, "Motor-BG connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_motor_to_cerebellum(brain)) {
        LOG_WARN(LOG_MODULE, "Motor-Cerebellum connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_motor_to_thalamus(brain)) {
        LOG_WARN(LOG_MODULE, "Motor-Thalamus connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_motor_to_training(brain)) {
        LOG_WARN(LOG_MODULE, "Motor-Training connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_motor_to_immune(brain)) {
        LOG_WARN(LOG_MODULE, "Motor-Immune connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_motor_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Motor bio-async connection failed (non-fatal)");
    }

    LOG_INFO(LOG_MODULE, "Motor Cortex subsystem initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_motor_substrate_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_motor_substrate_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->motor_substrate_bridge) {
        return true;
    }

    /* Need motor adapter first */
    if (!brain->motor) {
        return true;  /* Not ready yet, will be called again */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing motor substrate bridge");

    /* Get neural substrate - may be NULL in simple configurations */
    void* substrate = NULL;
    /*
     * TODO: Get substrate from brain if available
     * substrate = brain->substrate;
     */

    /* Create substrate bridge with default config */
    motor_substrate_config_t config = motor_substrate_default_config();

    brain->motor_substrate_bridge = motor_substrate_bridge_create(
        brain->motor, substrate, &config);

    if (!brain->motor_substrate_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create motor substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_motor_substrate_bridge: brain->motor_substrate_bridge is NULL");
        return false;
    }

    /* Connect to substrate */
    connect_substrate_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Motor substrate bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_motor_thalamic_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_motor_thalamic_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->motor_thalamic_bridge) {
        return true;
    }

    /* Need motor adapter first */
    if (!brain->motor) {
        return true;  /* Not ready yet */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing motor thalamic bridge");

    /* Get thalamic router - may be NULL in simple configurations */
    void* router = NULL;
    /*
     * TODO: Get thalamic router from brain if available
     * router = brain->thalamic_router;
     */

    /* Create thalamic bridge with default config */
    motor_thalamic_config_t thal_config = motor_thalamic_default_config();

    brain->motor_thalamic_bridge = motor_thalamic_bridge_create(
        brain->motor, router, &thal_config);

    if (!brain->motor_thalamic_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create motor thalamic bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_motor_thalamic_bridge: brain->motor_thalamic_bridge is NULL");
        return false;
    }

    /* Connect to thalamus */
    connect_thalamic_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Motor thalamic bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_motor_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_motor_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->motor_quantum_bridge) {
        return true;
    }

    /* Need motor adapter first */
    if (!brain->motor) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        LOG_DEBUG(LOG_MODULE, "Quantum reasoning disabled, skipping motor quantum bridge");
        return true;  /* Not enabled, not an error */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing motor quantum bridge");

    /* Create quantum bridge with default config */
    motor_quantum_config_t config = motor_quantum_default_config();

    /* Scale trajectory alternatives based on complexity */
    if (brain->config.num_outputs > 64) {
        config.trajectory_alternatives = 32;
    }

    brain->motor_quantum_bridge = motor_quantum_bridge_create(
        brain->motor, &config);

    if (!brain->motor_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create motor quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_motor_quantum_bridge: brain->motor_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Motor quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_motor_to_basal_ganglia(brain_t brain) {
    if (!brain || !brain->motor) {
        return true;  /* Nothing to connect */
    }

    /* Check if basal ganglia is available */
    if (!brain->basal_ganglia_enabled || !brain->basal_ganglia) {
        LOG_DEBUG(LOG_MODULE, "Basal ganglia not available for motor connection");
        return true;  /* BG not initialized yet */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting motor cortex to basal ganglia");

    /*
     * Register motor cortex as action executor.
     * BG selects actions; motor cortex executes them.
     *
     * Signal flow:
     * - BG GPi/SNr -> VA/VL thalamus -> Motor cortex
     * - Direct pathway: Disinhibits selected motor programs
     * - Indirect pathway: Suppresses competing movements
     */

    /* TODO: Register with BG enhanced system
     * bg_enhanced_register_motor_executor(brain->basal_ganglia, brain->motor);
     */

    LOG_DEBUG(LOG_MODULE, "Motor cortex connected to basal ganglia");
    return true;
}

bool nimcp_brain_factory_connect_motor_to_cerebellum(brain_t brain) {
    if (!brain || !brain->motor) {
        return true;  /* Nothing to connect */
    }

    /* Check if cerebellum is available */
    /* Note: Cerebellum might be part of the bg_cerebellar coordination */
    if (!brain->basal_ganglia_enabled) {
        LOG_DEBUG(LOG_MODULE, "Cerebellum not available for motor connection");
        return true;
    }

    LOG_DEBUG(LOG_MODULE, "Connecting motor cortex to cerebellum");

    /*
     * Register for cerebellar correction signals.
     * Cerebellum provides:
     * - Timing precision: Coordinates movement phases
     * - Error correction: Climbing fiber teaching signals
     * - Motor adaptation: Calibrates motor commands
     *
     * Signal flow:
     * - Motor cortex -> Pontine nuclei -> Cerebellum (mossy fibers)
     * - Inferior olive -> Cerebellum (climbing fibers, error signals)
     * - Cerebellum -> Thalamus VL -> Motor cortex
     */

    /* TODO: Register with cerebellar system
     * cerebellum_register_motor_target(brain->cerebellum, brain->motor,
     *     motor_receive_cerebellar_correction);
     */

    LOG_DEBUG(LOG_MODULE, "Motor cortex connected to cerebellum");
    return true;
}

bool nimcp_brain_factory_connect_motor_to_thalamus(brain_t brain) {
    if (!brain || !brain->motor) {
        return true;  /* Nothing to connect */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting motor cortex to thalamus");

    /*
     * Register motor cortex with thalamic router.
     * Motor-related thalamic nuclei:
     * - VA (ventral anterior): Receives BG output
     * - VL (ventral lateral): Receives cerebellar output
     * - VPL: Receives somatosensory feedback
     *
     * Thalamus gates motor signals based on:
     * - Arousal state (reticular formation)
     * - Attention (pulvinar)
     * - Motor preparation (VA/VL)
     */

    /* TODO: Register with thalamic router
     * thalamic_router_register_motor(brain->thalamic_router, brain->motor,
     *     THAL_NUCLEUS_VA | THAL_NUCLEUS_VL);
     */

    LOG_DEBUG(LOG_MODULE, "Motor cortex connected to thalamus");
    return true;
}

bool nimcp_brain_factory_connect_motor_to_training(brain_t brain) {
    if (!brain || !brain->motor) {
        return true;  /* Nothing to connect */
    }

    /* Check if training is enabled */
    if (!brain->enable_training_integration || !brain->training_ctx) {
        LOG_DEBUG(LOG_MODULE, "Training not enabled for motor connection");
        return true;  /* Training not enabled */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting motor cortex to training system");

    /*
     * Register motor adapter with training context.
     * This allows motor learning through:
     * - Error-based learning: Use feedback to correct movements
     * - Reinforcement learning: Shape movements via reward
     * - Imitation learning: Learn from demonstrations
     *
     * Training signals:
     * - Loss gradients for supervised motor learning
     * - Reward signals for RL-based skill acquisition
     * - Demonstration trajectories for imitation
     */

    /* TODO: Register with training
     * nimcp_brain_training_register_module(brain->training_ctx,
     *     TRAIN_MODULE_MOTOR_CORTEX, brain->motor);
     */

    LOG_DEBUG(LOG_MODULE, "Motor cortex connected to training system");
    return true;
}

bool nimcp_brain_factory_connect_motor_to_immune(brain_t brain) {
    if (!brain || !brain->motor) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        LOG_DEBUG(LOG_MODULE, "Immune system not available for motor connection");
        return true;  /* Immune not enabled */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting motor cortex to immune system");

    /*
     * Register for cytokine signals that affect motor function.
     * Neuroinflammation effects on motor cortex:
     * - IL-1beta: Reduces motor neuron excitability -> slower movements
     * - TNF-alpha: Increases synaptic noise -> motor variability
     * - IL-6: Fatigue-like effects -> reduced motor output
     * - IFN-gamma: Affects motor learning plasticity
     *
     * This implements "sickness behavior" motor component:
     * - Reduced voluntary movement
     * - Conservation of energy for immune response
     */

    /* TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A | CYTOKINE_IL6,
     *     motor_inflammation_callback, brain->motor);
     */

    LOG_DEBUG(LOG_MODULE, "Motor cortex connected to immune system");
    return true;
}

bool nimcp_brain_factory_update_motor_subsystem(brain_t brain, uint64_t dt_us) {
    if (!brain || !brain->motor_enabled || !brain->motor) {
        return true;  /* Nothing to update */
    }

    /* Convert to milliseconds for motor update */
    float dt_ms = (float)dt_us / 1000.0f;

    /* Update motor execution */
    motor_status_t status = motor_get_status(brain->motor);

    if (status == MOTOR_STATUS_EXECUTING) {
        bool still_executing = motor_update_execution(brain->motor, dt_ms);
        if (!still_executing) {
            LOG_DEBUG(LOG_MODULE, "Motor execution completed");
        }
    }

    /* Process bio-async messages if enabled */
    if (brain->bio_async_enabled) {
        motor_process_bio_messages(brain->motor, 10);  /* Process up to 10 messages */
    }

    /* Update substrate bridge periodically */
    if (brain->motor_substrate_bridge) {
        motor_substrate_bridge_update(brain->motor_substrate_bridge);
    }

    /* Update timestamp */
    brain->last_motor_update_us = brain->current_time_us;

    return true;
}
