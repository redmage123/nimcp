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

// Substrate / thalamic motor bridges: use the real headers. The old local
// duplicate config structs had a different field layout from the compiled
// bridges (an ABI mismatch on the by-value default_config return).
#include "core/motor/nimcp_motor_substrate_bridge.h"
#include "core/motor/nimcp_motor_thalamic_bridge.h"

//=============================================================================
// Internal Helper Functions
//=============================================================================

/* Bio-async: motor_create() registers BIO_MODULE_MOTOR_CORTEX itself (with
 * its real handlers and KG wiring callback) when cfg.enable_bio_async is set,
 * and motor_destroy() unregisters it. The former connect_motor_to_bio_async()
 * here registered the same module id a second time with TODO stub handlers;
 * it was removed when this file was wired (2026-11-24). */

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

    /* Neural substrate (metabolic state). The bridge rejects a NULL
     * substrate, so skip it on brains created without one. */
    neural_substrate_t* substrate = (neural_substrate_t*)brain->substrate;
    if (!substrate) {
        LOG_DEBUG(LOG_MODULE, "No neural substrate; skipping motor substrate bridge");
        return true;
    }

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

    /* Thalamic router (VA/VL relay). Skip the bridge when absent. */
    thalamic_router_t* router = (thalamic_router_t*)brain->thalamic_router;
    if (!router) {
        LOG_DEBUG(LOG_MODULE, "No thalamic router; skipping motor thalamic bridge");
        return true;
    }

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

    /* Trajectory execution (motor_update_execution) is advanced every
     * training step by brain_tick_sensorimotor(); doing it here as well
     * would double-step the motor cortex. This update drains the motor
     * bio-router inbox (nothing else does) and refreshes the metabolic
     * (substrate) modulation. */
    (void)dt_us;

    /* Process bio-async messages if enabled (bounded batch). */
    if (brain->bio_async_enabled) {
        motor_process_bio_messages(brain->motor, 10);
    }

    /* Update substrate bridge periodically */
    if (brain->motor_substrate_bridge) {
        motor_substrate_bridge_update(brain->motor_substrate_bridge);
    }

    /* Update timestamp */
    brain->last_motor_update_us = brain->current_time_us;

    return true;
}
