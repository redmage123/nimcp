//=============================================================================
// nimcp_brain_init_cerebellum.c - Cerebellum Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_cerebellum.c
 * @brief Cerebellum Initialization Implementation
 *
 * WHAT: Initialization functions for Cerebellum (motor coordination)
 * WHY:  Enable motor coordination capabilities in the brain
 * HOW:  Creates Cerebellum adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase B4: Cerebellum Brain Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_cerebellum.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_CEREBELLUM"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_cerebellum, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Cerebellum includes
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

// Forward declarations for substrate and thalamic bridges
// (similar pattern to Broca to avoid header conflicts)
struct cerebellum_substrate_bridge;
typedef struct cerebellum_substrate_bridge cerebellum_substrate_bridge_t;

struct cerebellum_thalamic_bridge;
typedef struct cerebellum_thalamic_bridge cerebellum_thalamic_bridge_t;

// Substrate bridge config
typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_precision;
} cerebellum_substrate_config_t;

// Thalamic bridge config
typedef struct {
    bool enable_attention_gating;
    bool enable_motor_priority;
    bool enable_timing_routing;
    float min_urgency_threshold;
    float motor_boost;
    float attention_decay_rate;
} cerebellum_thalamic_config_t;

// External API declarations (to be implemented if bridges are added later)
// For now, we implement stubs

static cerebellum_substrate_config_t cerebellum_substrate_default_config(void) {
    cerebellum_substrate_config_t config = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = true,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_precision = 0.3f
    };
    return config;
}

static cerebellum_thalamic_config_t cerebellum_thalamic_default_config(void) {
    cerebellum_thalamic_config_t config = {
        .enable_attention_gating = true,
        .enable_motor_priority = true,
        .enable_timing_routing = true,
        .min_urgency_threshold = 0.3f,
        .motor_boost = 1.5f,
        .attention_decay_rate = 0.1f
    };
    return config;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Connect Cerebellum to bio-async messaging
 */
static bool connect_cerebellum_to_bio_async(brain_t brain) {
    if (!brain || !brain->cerebellum) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /*
         * TODO: Register Cerebellum message handlers
         * bio_router_register_module(router, BIO_MODULE_CEREBELLUM, brain->cerebellum);
         */
        LOG_DEBUG(LOG_MODULE, "Cerebellum bio-async connection pending");
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_cerebellum_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_cerebellum_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->cerebellum) {
        return true;  /* Already initialized */
    }

    /* Cerebellum is enabled by default for motor-capable brains */
    LOG_INFO(LOG_MODULE, "Initializing Cerebellum subsystem");

    /* Create Cerebellum adapter with default configuration */
    cerebellum_config_t cb_cfg = cerebellum_default_config();

    brain->cerebellum = cerebellum_create(&cb_cfg);
    if (!brain->cerebellum) {
        set_error("Failed to create Cerebellum adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_cerebellum_subsystem: brain->cerebellum is NULL");
        return false;
    }

    brain->cerebellum_enabled = true;
    brain->last_cerebellum_update_us = 0;

    /* Initialize integration bridges */
    if (!nimcp_brain_factory_init_cerebellum_substrate_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum substrate bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_cerebellum_thalamic_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum thalamic bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_cerebellum_quantum_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_cerebellum_to_motor_cortex(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum-motor cortex connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cerebellum_to_basal_ganglia(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum-BG connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cerebellum_to_brainstem(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum-brainstem connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cerebellum_to_training(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum-Training connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_cerebellum_to_immune(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum-Immune connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_cerebellum_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Cerebellum bio-async connection failed (non-fatal)");
    }

    LOG_INFO(LOG_MODULE, "Cerebellum initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_cerebellum_substrate_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_cerebellum_substrate_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->cerebellum_substrate_bridge) {
        return true;
    }

    /* Need Cerebellum adapter first */
    if (!brain->cerebellum) {
        return true;  /* Not ready yet, will be called again */
    }

    /* Get neural substrate - may be NULL in simple configurations */
    /* void* substrate = brain->substrate; */

    /* Create substrate bridge with default config */
    cerebellum_substrate_config_t config = cerebellum_substrate_default_config();

    /* Note: Substrate bridge implementation would go here
     * For now, we just log and return success */
    LOG_DEBUG(LOG_MODULE, "Cerebellum substrate bridge pending implementation");

    /* brain->cerebellum_substrate_bridge = cerebellum_substrate_bridge_create(
        brain->cerebellum, substrate, &config); */

    return true;
}

bool nimcp_brain_factory_init_cerebellum_thalamic_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_cerebellum_thalamic_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->cerebellum_thalamic_bridge) {
        return true;
    }

    /* Need Cerebellum adapter first */
    if (!brain->cerebellum) {
        return true;  /* Not ready yet */
    }

    /* Get thalamic router - may be NULL in simple configurations */
    /* void* router = brain->thalamic_router; */

    /* Create thalamic bridge with default config */
    cerebellum_thalamic_config_t thal_config = cerebellum_thalamic_default_config();

    /* Note: Thalamic bridge implementation would go here */
    LOG_DEBUG(LOG_MODULE, "Cerebellum thalamic bridge pending implementation");

    /* brain->cerebellum_thalamic_bridge = cerebellum_thalamic_bridge_create(
        brain->cerebellum, router, &thal_config); */

    return true;
}

bool nimcp_brain_factory_init_cerebellum_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_cerebellum_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->cerebellum_quantum_bridge) {
        return true;
    }

    /* Need Cerebellum adapter first */
    if (!brain->cerebellum) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        LOG_DEBUG(LOG_MODULE, "Quantum reasoning not enabled, skipping quantum bridge");
        return true;  /* Not enabled, not an error */
    }

    /* Create quantum bridge with default config */
    cerebellum_quantum_config_t config = cerebellum_quantum_default_config();

    /* Scale Grover iterations based on motor complexity */
    if (brain->config.num_outputs > 10) {
        config.timing_search_depth = brain->config.num_outputs * 10;
    }

    brain->cerebellum_quantum_bridge = cerebellum_quantum_bridge_create(
        brain->cerebellum, &config);

    if (!brain->cerebellum_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create Cerebellum quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_cerebellum_quantum_bridge: brain->cerebellum_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Cerebellum quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_cerebellum_to_motor_cortex(brain_t brain) {
    if (!brain || !brain->cerebellum) {
        return true;  /* Nothing to connect */
    }

    /*
     * Connect Cerebellum output to motor cortex.
     * The cerebellum provides timing and coordination adjustments
     * that modulate motor cortex output.
     */

    /* TODO: Register with motor cortex
     * motor_cortex_register_modulator(brain->motor_cortex,
     *     MOTOR_MODULATOR_CEREBELLUM, brain->cerebellum);
     */

    LOG_DEBUG(LOG_MODULE, "Cerebellum connected to motor cortex (pending implementation)");
    return true;
}

bool nimcp_brain_factory_connect_cerebellum_to_basal_ganglia(brain_t brain) {
    if (!brain || !brain->cerebellum) {
        return true;  /* Nothing to connect */
    }

    /* Check if basal ganglia is available */
    if (!brain->basal_ganglia_enabled || !brain->basal_ganglia) {
        LOG_DEBUG(LOG_MODULE, "Basal ganglia not available for connection");
        return true;  /* BG not enabled */
    }

    /*
     * Connect Cerebellum to Basal Ganglia.
     * These systems cooperate in motor control:
     * - BG: Action selection (what to do)
     * - Cerebellum: Motor execution (how to do it)
     *
     * Recent research shows bidirectional connections via thalamus.
     */

    /* TODO: Register bidirectional connection
     * bg_register_cerebellar_input(brain->basal_ganglia, brain->cerebellum);
     * cerebellum_register_bg_input(brain->cerebellum, brain->basal_ganglia);
     */

    LOG_DEBUG(LOG_MODULE, "Cerebellum connected to basal ganglia (pending implementation)");
    return true;
}

bool nimcp_brain_factory_connect_cerebellum_to_brainstem(brain_t brain) {
    if (!brain || !brain->cerebellum) {
        return true;  /* Nothing to connect */
    }

    /* Check if medulla/brainstem is available */
    if (!brain->medulla_enabled || !brain->medulla) {
        LOG_DEBUG(LOG_MODULE, "Brainstem not available for connection");
        return true;  /* Brainstem not enabled */
    }

    /*
     * Connect Cerebellum (fastigial nucleus) to brainstem.
     * This connection is essential for:
     * - Balance and posture control
     * - Vestibular reflexes
     * - Eye movement coordination
     */

    /* TODO: Register brainstem connection
     * medulla_register_cerebellar_input(brain->medulla, brain->cerebellum);
     */

    LOG_DEBUG(LOG_MODULE, "Cerebellum connected to brainstem (pending implementation)");
    return true;
}

bool nimcp_brain_factory_connect_cerebellum_to_training(brain_t brain) {
    if (!brain || !brain->cerebellum) {
        return true;  /* Nothing to connect */
    }

    /* Check if training is enabled */
    if (!brain->enable_training_integration || !brain->training_ctx) {
        LOG_DEBUG(LOG_MODULE, "Training not enabled for Cerebellum connection");
        return true;  /* Training not enabled */
    }

    /*
     * Register Cerebellum adapter with training context.
     * This allows motor learning through:
     * - Error-based learning (climbing fiber signals)
     * - Forward model updates
     * - Gain adaptation
     */

    /* TODO: Register with training
     * nimcp_brain_training_register_module(brain->training_ctx,
     *     TRAIN_MODULE_CEREBELLUM, brain->cerebellum);
     */

    LOG_DEBUG(LOG_MODULE, "Cerebellum connected to training system");
    return true;
}

bool nimcp_brain_factory_connect_cerebellum_to_immune(brain_t brain) {
    if (!brain || !brain->cerebellum) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        LOG_DEBUG(LOG_MODULE, "Immune system not available for Cerebellum connection");
        return true;  /* Immune not enabled */
    }

    /*
     * Register for cytokine signals that affect motor coordination.
     * Neuroinflammation can cause:
     * - Reduced motor precision
     * - Impaired timing
     * - Tremor and ataxia
     */

    /* TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A, cerebellum_inflammation_callback, brain->cerebellum);
     */

    LOG_DEBUG(LOG_MODULE, "Cerebellum connected to immune system");
    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy cerebellum subsystem
 *
 * WHAT: Clean up all cerebellum resources and bridges
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_cerebellum_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_cerebellum_subsystem: brain is NULL"); return; }

    LOG_DEBUG(LOG_MODULE, "Destroying cerebellum subsystem");

    /* Destroy quantum bridge first (depends on cerebellum) */
    if (brain->cerebellum_quantum_bridge) {
        cerebellum_quantum_bridge_destroy(brain->cerebellum_quantum_bridge);
        brain->cerebellum_quantum_bridge = NULL;
    }

    /* Destroy thalamic bridge */
    if (brain->cerebellum_thalamic_bridge) {
        /* cerebellum_thalamic_bridge_destroy(brain->cerebellum_thalamic_bridge); */
        brain->cerebellum_thalamic_bridge = NULL;
    }

    /* Destroy substrate bridge */
    if (brain->cerebellum_substrate_bridge) {
        /* cerebellum_substrate_bridge_destroy(brain->cerebellum_substrate_bridge); */
        brain->cerebellum_substrate_bridge = NULL;
    }

    /* Destroy cerebellum adapter */
    if (brain->cerebellum) {
        cerebellum_destroy(brain->cerebellum);
        brain->cerebellum = NULL;
    }

    brain->cerebellum_enabled = false;
    brain->last_cerebellum_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Cerebellum subsystem destroyed");
}
