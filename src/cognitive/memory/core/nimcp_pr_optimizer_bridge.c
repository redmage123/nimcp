//=============================================================================
// nimcp_pr_optimizer_bridge.c - Prime Resonant Memory Optimizer Bridge
//=============================================================================
/**
 * @file nimcp_pr_optimizer_bridge.c
 * @brief Implementation of Prime Resonant memory optimizer bridge
 *
 * WHAT: Bridge implementation connecting PR memory to optimization backends
 * WHY:  Enable resonance-aware learning with quaternion-specific optimization
 * HOW:  Implements all optimization algorithms with PR-specific extensions
 *
 * IMPLEMENTATION NOTES:
 *
 *   Optimization Algorithm Details:
 *   +-----------------------------------------------------------------------+
 *   |  RESONANCE ADAM:                                                      |
 *   |  - Standard Adam with LR modulated by resonance score                 |
 *   |  - Higher resonance = faster learning (attention gates plasticity)    |
 *   |  - Uses bias correction for accurate early updates                    |
 *   |                                                                       |
 *   |  QUATERNION MOMENTUM:                                                  |
 *   |  - Momentum-based optimization on quaternion manifold                 |
 *   |  - Projects gradients to tangent space at current point               |
 *   |  - Uses exponential map for manifold-preserving updates               |
 *   |  - Parallel transports velocity when base point moves                 |
 *   |                                                                       |
 *   |  CONSOLIDATION-GATED SGD:                                              |
 *   |  - Updates blocked for highly consolidated memories                   |
 *   |  - Gate function: max(0, 1 - consolidation / threshold)               |
 *   |  - Prevents overwriting stable long-term memories                     |
 *   |                                                                       |
 *   |  TIER-ADAPTIVE SGD:                                                    |
 *   |  - Different learning rates per Z-ladder tier                         |
 *   |  - Z0 (working): Fast plasticity for immediate learning               |
 *   |  - Z3 (permanent): Near-zero LR for stability                         |
 *   +-----------------------------------------------------------------------+
 *
 *   Thread Safety:
 *   +-----------------------------------------------------------------------+
 *   |  - All optimization steps are mutex-protected                         |
 *   |  - Statistics use atomic operations for thread-safe reads             |
 *   |  - Configuration updates are atomic (copy-then-swap)                  |
 *   +-----------------------------------------------------------------------+
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_pr_optimizer_bridge.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_optimizer_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_optimizer_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_optimizer_bridge_mesh_registry = NULL;

nimcp_error_t pr_optimizer_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_optimizer_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_optimizer_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_optimizer_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_optimizer_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_optimizer_bridge_mesh_registry = registry;
    return err;
}

void pr_optimizer_bridge_mesh_unregister(void) {
    if (g_pr_optimizer_bridge_mesh_registry && g_pr_optimizer_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_optimizer_bridge_mesh_registry, g_pr_optimizer_bridge_mesh_id);
        g_pr_optimizer_bridge_mesh_id = 0;
        g_pr_optimizer_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_optimizer_bridge module (instance-level) */
static inline void pr_optimizer_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_optimizer_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_optimizer_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_optimizer_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_optimizer_bridge, struct pr_optimizer_bridge_struct)

//=============================================================================
// Private Constants
//=============================================================================

/** Module name for logging */
#define PR_OPT_MODULE_NAME "pr_optimizer_bridge"

/** Minimum value for gradient norm (prevent division by zero) */
#define GRAD_NORM_MIN 1e-10f

/** Maximum number of retries for mutex acquisition */
#define MUTEX_MAX_RETRIES 3

//=============================================================================
// Private Helper Functions - Forward Declarations
//=============================================================================

static uint64_t get_time_ns(void);
static float compute_grad_norm(const float* gradients, size_t count);
static void scale_gradients(float* gradients, size_t count, float scale);

static pr_optimizer_error_t init_adam_state(
    pr_optimizer_bridge_t bridge,
    size_t num_params
);

static pr_optimizer_error_t init_quat_momentum_state(
    pr_optimizer_bridge_t bridge,
    size_t num_quats
);

static void free_optimizer_state(pr_optimizer_state_t* state);

static void update_stats_gradient(
    pr_optimizer_stats_t* stats,
    float grad_norm
);

static void update_stats_lr(
    pr_optimizer_stats_t* stats,
    float effective_lr
);

//=============================================================================
// Configuration Functions
//=============================================================================

pr_optimizer_config_t pr_optimizer_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_config_", 0.0f);


    pr_optimizer_config_t config;
    memset(&config, 0, sizeof(config));

    // Basic parameters
    config.base_lr = PR_OPT_DEFAULT_LR;

    // Resonance modulation
    config.resonance_scale = PR_OPT_DEFAULT_RESONANCE_SCALE;

    // Consolidation gating
    config.consolidation_gate = PR_OPT_DEFAULT_CONSOL_GATE;

    // Tier-specific learning rates
    config.tier_lr_scale[0] = PR_OPT_TIER_LR_Z0;  // Z0 - working
    config.tier_lr_scale[1] = PR_OPT_TIER_LR_Z1;  // Z1 - short-term
    config.tier_lr_scale[2] = PR_OPT_TIER_LR_Z2;  // Z2 - long-term
    config.tier_lr_scale[3] = PR_OPT_TIER_LR_Z3;  // Z3 - permanent

    // Adam parameters
    config.beta1 = PR_OPT_DEFAULT_BETA1;
    config.beta2 = PR_OPT_DEFAULT_BETA2;
    config.epsilon = PR_OPT_DEFAULT_EPSILON;

    // Quaternion momentum
    config.quat_momentum = PR_OPT_DEFAULT_QUAT_MOMENTUM;

    // Gradient clipping
    config.max_grad_norm = PR_OPT_MAX_GRAD_NORM;
    config.entanglement_scale = PR_OPT_ENTANGLEMENT_SCALE;

    // Feature flags - all enabled by default
    config.enable_resonance_scaling = true;
    config.enable_consolidation_gating = true;
    config.enable_tier_adaptation = true;
    config.enable_entanglement_clipping = true;
    config.enable_weight_decay = false;
    config.weight_decay = 0.0f;

    return config;
}

pr_optimizer_config_t pr_optimizer_config_aggressive(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_config_", 0.0f);


    pr_optimizer_config_t config = pr_optimizer_config_default();

    config.base_lr = NIMCP_LEARNING_RATE_DEFAULT;                    // Higher base LR
    config.resonance_scale = 1.0f;             // Maximum resonance effect
    config.consolidation_gate = 0.99f;         // Almost no gating
    config.tier_lr_scale[0] = 2.0f;            // Double LR for Z0
    config.tier_lr_scale[1] = 1.5f;            // Higher for Z1
    config.max_grad_norm = 20.0f;              // Allow larger gradients

    return config;
}

pr_optimizer_config_t pr_optimizer_config_conservative(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_config_", 0.0f);


    pr_optimizer_config_t config = pr_optimizer_config_default();

    config.base_lr = NIMCP_LEARNING_RATE_MICRO;                  // Very low base LR
    config.resonance_scale = 0.1f;             // Minimal resonance effect
    config.consolidation_gate = 0.3f;          // Strong gating
    config.tier_lr_scale[0] = 0.5f;            // Lower even for Z0
    config.tier_lr_scale[1] = 0.2f;
    config.tier_lr_scale[2] = 0.05f;
    config.tier_lr_scale[3] = 0.001f;          // Near-zero for Z3
    config.max_grad_norm = 1.0f;               // Strict clipping
    config.enable_weight_decay = true;
    config.weight_decay = 0.01f;

    return config;
}

bool pr_optimizer_config_validate(const pr_optimizer_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_optimizer_config_validate: config is NULL");
        return false;
    }

    // Check base learning rate
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_config_", 0.0f);


    if (config->base_lr <= 0.0f) {
        NIMCP_LOGGING_WARN("Invalid base_lr: %f (must be > 0)", config->base_lr);
        return false;
    }

    // Check Adam betas
    if (config->beta1 < 0.0f || config->beta1 >= 1.0f) {
        NIMCP_LOGGING_WARN("Invalid beta1: %f (must be in [0, 1))", config->beta1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_optimizer_config_validate: capacity exceeded");
        return false;
    }

    if (config->beta2 < 0.0f || config->beta2 >= 1.0f) {
        NIMCP_LOGGING_WARN("Invalid beta2: %f (must be in [0, 1))", config->beta2);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_optimizer_config_validate: capacity exceeded");
        return false;
    }

    // Check epsilon
    if (config->epsilon <= 0.0f) {
        NIMCP_LOGGING_WARN("Invalid epsilon: %f (must be > 0)", config->epsilon);
        return false;
    }

    // Check resonance scale
    if (config->resonance_scale < 0.0f || config->resonance_scale > 10.0f) {
        NIMCP_LOGGING_WARN("Invalid resonance_scale: %f (should be in [0, 10])",
                          config->resonance_scale);
        return false;
    }

    // Check consolidation gate
    if (config->consolidation_gate < 0.0f || config->consolidation_gate > 1.0f) {
        NIMCP_LOGGING_WARN("Invalid consolidation_gate: %f (must be in [0, 1])",
                          config->consolidation_gate);
        return false;
    }

    // Check tier scales
    for (int i = 0; i < PR_OPT_NUM_TIERS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OPT_NUM_TIERS > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)PR_OPT_NUM_TIERS);
        }

        if (config->tier_lr_scale[i] < 0.0f) {
            NIMCP_LOGGING_WARN("Invalid tier_lr_scale[%d]: %f (must be >= 0)",
                              i, config->tier_lr_scale[i]);
            return false;
        }
    }

    // Check quaternion momentum
    if (config->quat_momentum < 0.0f || config->quat_momentum >= 1.0f) {
        NIMCP_LOGGING_WARN("Invalid quat_momentum: %f (must be in [0, 1))",
                          config->quat_momentum);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_optimizer_config_validate: capacity exceeded");
        return false;
    }

    // Check max gradient norm
    if (config->max_grad_norm <= 0.0f) {
        NIMCP_LOGGING_WARN("Invalid max_grad_norm: %f (must be > 0)",
                          config->max_grad_norm);
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pr_optimizer_bridge_t pr_optimizer_bridge_create(
    const pr_optimizer_config_t* config
) {
    // Allocate bridge structure
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_create", 0.0f);


    pr_optimizer_bridge_t bridge = nimcp_malloc(sizeof(struct pr_optimizer_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate optimizer bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_optimizer_bridge_create: bridge is NULL");
        return NULL;
    }

    // Zero initialize
    memset(bridge, 0, sizeof(struct pr_optimizer_bridge_struct));

    // Initialize base bridge
    if (bridge_base_init(&bridge->base, PR_OPT_MODULE_ID, PR_OPT_MODULE_NAME) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize base bridge");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_optimizer_bridge_create: validation failed");
        return NULL;
    }

    // Set configuration
    if (config) {
        if (!pr_optimizer_config_validate(config)) {
            NIMCP_LOGGING_WARN("Invalid config provided, using defaults");
            bridge->config = pr_optimizer_config_default();
        } else {
            bridge->config = *config;
        }
    } else {
        bridge->config = pr_optimizer_config_default();
    }

    // Initialize state
    bridge->active_type = PR_OPT_RESONANCE_ADAM;
    bridge->state.type = PR_OPT_RESONANCE_ADAM;
    bridge->state.initialized = false;

    // Initialize statistics
    atomic_store(&bridge->stats.total_steps, 0);
    atomic_store(&bridge->stats.resonance_adam_steps, 0);
    atomic_store(&bridge->stats.quat_momentum_steps, 0);
    atomic_store(&bridge->stats.consolidation_gated_steps, 0);
    atomic_store(&bridge->stats.tier_adaptive_steps, 0);
    atomic_store(&bridge->stats.gradient_clips, 0);
    atomic_store(&bridge->stats.gated_updates, 0);
    atomic_store(&bridge->stats.allowed_updates, 0);

    bridge->stats.total_gradient_norm = 0.0f;
    bridge->stats.max_gradient_norm = 0.0f;
    bridge->stats.min_gradient_norm = INFINITY;
    bridge->stats.avg_gradient_norm = 0.0f;

    bridge->stats.total_effective_lr = 0.0f;
    bridge->stats.max_effective_lr = 0.0f;
    bridge->stats.min_effective_lr = INFINITY;
    bridge->stats.avg_effective_lr = 0.0f;

    bridge->stats.total_resonance = 0.0f;
    bridge->stats.avg_resonance = 0.0f;

    bridge->stats.total_time_ns = 0;
    bridge->stats.last_step_time_ns = 0;

    // Set current LR
    bridge->current_lr = bridge->config.base_lr;

    // Mark as initialized
    bridge->initialized = true;

    NIMCP_LOGGING_INFO("PR optimizer bridge created with base_lr=%.6f",
                       bridge->config.base_lr);

    return bridge;
}

void pr_optimizer_bridge_destroy(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        return;
    }

    // Free optimizer state
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_destroy", 0.0f);


    free_optimizer_state(&bridge->state);

    // Cleanup base bridge
    bridge_base_cleanup(&bridge->base);

    // Free bridge structure
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("PR optimizer bridge destroyed");
}

pr_optimizer_error_t pr_optimizer_bridge_connect(
    pr_optimizer_bridge_t bridge,
    nimcp_optimizer_context_t* optimizer
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_connect", 0.0f);


    BRIDGE_LOCK(bridge);

    bridge->base_optimizer = optimizer;
    if (optimizer) {
        bridge_base_connect_a(&bridge->base, optimizer);
    }

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

pr_optimizer_error_t pr_optimizer_bridge_init_state(
    pr_optimizer_bridge_t bridge,
    pr_optimizer_type_t type,
    size_t num_params
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_init_state", 0.0f);


    if (type >= PR_OPT_TYPE_COUNT) {
        return PR_OPT_ERROR_INVALID_TYPE;
    }

    if (num_params == 0) {
        return PR_OPT_ERROR_INVALID_CONFIG;
    }

    if (num_params > PR_OPT_MAX_BATCH_SIZE) {
        return PR_OPT_ERROR_BATCH_TOO_LARGE;
    }

    BRIDGE_LOCK(bridge);

    // Free existing state if different type
    if (bridge->state.initialized && bridge->state.type != type) {
        free_optimizer_state(&bridge->state);
    }

    pr_optimizer_error_t result = PR_OPT_SUCCESS;

    switch (type) {
        case PR_OPT_RESONANCE_ADAM:
        case PR_OPT_CONSOLIDATION_GATED:
        case PR_OPT_TIER_ADAPTIVE:
            result = init_adam_state(bridge, num_params);
            break;

        case PR_OPT_QUAT_SGD:
        case PR_OPT_QUAT_MOMENTUM:
            result = init_quat_momentum_state(bridge, num_params);
            break;

        default:
            result = PR_OPT_ERROR_INVALID_TYPE;
            break;
    }

    if (result == PR_OPT_SUCCESS) {
        bridge->state.type = type;
        bridge->active_type = type;
        bridge->state.initialized = true;
        NIMCP_LOGGING_INFO("Initialized %s state for %zu parameters",
                          pr_optimizer_type_name(type), num_params);
    }

    BRIDGE_UNLOCK(bridge);

    return result;
}

//=============================================================================
// Core Optimization Functions
//=============================================================================

pr_optimizer_error_t pr_optimizer_resonance_adam_step(
    pr_optimizer_bridge_t bridge,
    float* params,
    const float* gradients,
    size_t count,
    float resonance
) {
    if (!bridge || !params || !gradients) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    if (!bridge->initialized) {
        return PR_OPT_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_resonan", 0.0f);


    if (count == 0) {
        return PR_OPT_SUCCESS;  // Nothing to do
    }

    uint64_t start_time = get_time_ns();

    BRIDGE_LOCK(bridge);

    // Ensure state is initialized
    if (!bridge->state.initialized ||
        bridge->state.type != PR_OPT_RESONANCE_ADAM) {
        pr_optimizer_error_t init_result = init_adam_state(bridge, count);
        if (init_result != PR_OPT_SUCCESS) {
            BRIDGE_UNLOCK(bridge);
            return init_result;
        }
        bridge->state.type = PR_OPT_RESONANCE_ADAM;
        bridge->state.initialized = true;
    }

    // Check size match
    if (bridge->state.state.adam.count != count) {
        BRIDGE_UNLOCK(bridge);
        return PR_OPT_ERROR_SIZE_MISMATCH;
    }

    pr_adam_state_t* adam = &bridge->state.state.adam;
    const pr_optimizer_config_t* cfg = &bridge->config;

    // Compute gradient norm before update
    float grad_norm = compute_grad_norm(gradients, count);
    update_stats_gradient(&bridge->stats, grad_norm);

    // Compute effective learning rate with resonance scaling
    float resonance_clamped = nimcp_myelin_clamp(resonance, 0.0f, 1.0f);
    float lr_scale = 1.0f;
    if (cfg->enable_resonance_scaling) {
        lr_scale = 1.0f + cfg->resonance_scale * resonance_clamped;
    }
    float effective_lr = cfg->base_lr * lr_scale;
    update_stats_lr(&bridge->stats, effective_lr);

    // Update resonance stats
    bridge->stats.total_resonance += resonance_clamped;

    // Increment step
    adam->step++;
    uint64_t t = adam->step;

    // Compute bias correction factors
    float beta1_t = powf(cfg->beta1, (float)t);
    float beta2_t = powf(cfg->beta2, (float)t);
    float bias_correction1 = 1.0f - beta1_t;
    float bias_correction2 = 1.0f - beta2_t;

    // Adam update for each parameter
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)count);
        }

        float g = gradients[i];

        // Update first moment (mean)
        adam->m[i] = cfg->beta1 * adam->m[i] + (1.0f - cfg->beta1) * g;

        // Update second moment (variance)
        adam->v[i] = cfg->beta2 * adam->v[i] + (1.0f - cfg->beta2) * g * g;

        // Bias-corrected estimates
        float m_hat = adam->m[i] / bias_correction1;
        float v_hat = adam->v[i] / bias_correction2;

        // Weight decay (if enabled)
        float wd = 0.0f;
        if (cfg->enable_weight_decay) {
            wd = cfg->weight_decay * params[i];
        }

        // Update parameter
        params[i] -= effective_lr * (m_hat / (sqrtf(v_hat) + cfg->epsilon) + wd);
    }

    // Update statistics
    atomic_fetch_add(&bridge->stats.total_steps, 1);
    atomic_fetch_add(&bridge->stats.resonance_adam_steps, 1);
    atomic_fetch_add(&bridge->stats.allowed_updates, count);

    uint64_t elapsed = get_time_ns() - start_time;
    bridge->stats.total_time_ns += elapsed;
    bridge->stats.last_step_time_ns = elapsed;

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

pr_optimizer_error_t pr_optimizer_quat_momentum_step(
    pr_optimizer_bridge_t bridge,
    nimcp_quaternion_t* quat,
    const nimcp_quaternion_t* grad,
    nimcp_quaternion_t* velocity
) {
    if (!bridge || !quat || !grad || !velocity) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    if (!bridge->initialized) {
        return PR_OPT_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_quat_mo", 0.0f);


    uint64_t start_time = get_time_ns();

    BRIDGE_LOCK(bridge);

    const pr_optimizer_config_t* cfg = &bridge->config;

    // Step 1: Project gradient to tangent space at quat
    // grad_proj = grad - (grad . quat) * quat
    nimcp_quaternion_t grad_proj = pr_optimizer_project_to_tangent(*quat, *grad);

    // Step 2: Update velocity with momentum
    // velocity = momentum * velocity + grad_proj
    velocity->w = cfg->quat_momentum * velocity->w + grad_proj.w;
    velocity->x = cfg->quat_momentum * velocity->x + grad_proj.x;
    velocity->y = cfg->quat_momentum * velocity->y + grad_proj.y;
    velocity->z = cfg->quat_momentum * velocity->z + grad_proj.z;

    // Step 3: Scale velocity by learning rate
    nimcp_quaternion_t scaled_vel = quat_scale(*velocity, cfg->base_lr);

    // Step 4: Apply exponential map to get new quaternion
    *quat = pr_optimizer_exp_map(*quat, scaled_vel);

    // Step 5: Normalize result (safety check)
    *quat = quat_normalize(*quat);

    // Update statistics
    atomic_fetch_add(&bridge->stats.total_steps, 1);
    atomic_fetch_add(&bridge->stats.quat_momentum_steps, 1);

    uint64_t elapsed = get_time_ns() - start_time;
    bridge->stats.total_time_ns += elapsed;
    bridge->stats.last_step_time_ns = elapsed;

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

pr_optimizer_error_t pr_optimizer_quat_momentum_step_batch(
    pr_optimizer_bridge_t bridge,
    nimcp_quaternion_t* quats,
    const nimcp_quaternion_t* grads,
    nimcp_quaternion_t* velocities,
    size_t count
) {
    if (!bridge || !quats || !grads || !velocities) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_quat_mo", 0.0f);


    if (count == 0) {
        return PR_OPT_SUCCESS;
    }

    if (count > PR_OPT_MAX_BATCH_SIZE) {
        return PR_OPT_ERROR_BATCH_TOO_LARGE;
    }

    uint64_t start_time = get_time_ns();

    BRIDGE_LOCK(bridge);

    const pr_optimizer_config_t* cfg = &bridge->config;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)count);
        }

        // Project gradient to tangent space
        nimcp_quaternion_t grad_proj = pr_optimizer_project_to_tangent(
            quats[i], grads[i]);

        // Update velocity with momentum
        velocities[i].w = cfg->quat_momentum * velocities[i].w + grad_proj.w;
        velocities[i].x = cfg->quat_momentum * velocities[i].x + grad_proj.x;
        velocities[i].y = cfg->quat_momentum * velocities[i].y + grad_proj.y;
        velocities[i].z = cfg->quat_momentum * velocities[i].z + grad_proj.z;

        // Scale and apply exponential map
        nimcp_quaternion_t scaled_vel = quat_scale(velocities[i], cfg->base_lr);
        quats[i] = pr_optimizer_exp_map(quats[i], scaled_vel);
        quats[i] = quat_normalize(quats[i]);
    }

    // Update statistics
    atomic_fetch_add(&bridge->stats.total_steps, 1);
    atomic_fetch_add(&bridge->stats.quat_momentum_steps, count);

    uint64_t elapsed = get_time_ns() - start_time;
    bridge->stats.total_time_ns += elapsed;
    bridge->stats.last_step_time_ns = elapsed;

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

pr_optimizer_error_t pr_optimizer_consolidation_gated_step(
    pr_optimizer_bridge_t bridge,
    float* params,
    const float* gradients,
    const pr_memory_node_t* const* nodes,
    size_t count
) {
    if (!bridge || !params || !gradients || !nodes) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    if (!bridge->initialized) {
        return PR_OPT_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_consoli", 0.0f);


    if (count == 0) {
        return PR_OPT_SUCCESS;
    }

    uint64_t start_time = get_time_ns();

    BRIDGE_LOCK(bridge);

    const pr_optimizer_config_t* cfg = &bridge->config;
    uint64_t gated = 0;
    uint64_t allowed = 0;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)count);
        }

        if (!nodes[i]) {
            // No node info, apply full update
            params[i] -= cfg->base_lr * gradients[i];
            allowed++;
            continue;
        }

        // Get consolidation strength from quaternion w component
        nimcp_quaternion_t state = pr_memory_node_get_state(nodes[i]);
        float consolidation = nimcp_myelin_clamp(state.w, 0.0f, 1.0f);

        // Compute gate factor
        float gate = 0.0f;
        if (cfg->enable_consolidation_gating) {
            if (consolidation >= cfg->consolidation_gate) {
                // Fully gated - no update
                gate = 0.0f;
                gated++;
            } else {
                // Partial gating
                gate = 1.0f - (consolidation / cfg->consolidation_gate);
                allowed++;
            }
        } else {
            gate = 1.0f;
            allowed++;
        }

        // Apply gated update
        params[i] -= cfg->base_lr * gate * gradients[i];
    }

    // Update statistics
    atomic_fetch_add(&bridge->stats.total_steps, 1);
    atomic_fetch_add(&bridge->stats.consolidation_gated_steps, 1);
    atomic_fetch_add(&bridge->stats.gated_updates, gated);
    atomic_fetch_add(&bridge->stats.allowed_updates, allowed);

    uint64_t elapsed = get_time_ns() - start_time;
    bridge->stats.total_time_ns += elapsed;
    bridge->stats.last_step_time_ns = elapsed;

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

pr_optimizer_error_t pr_optimizer_tier_adaptive_step(
    pr_optimizer_bridge_t bridge,
    float* params,
    const float* gradients,
    const pr_memory_node_t* const* nodes,
    size_t count
) {
    if (!bridge || !params || !gradients || !nodes) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    if (!bridge->initialized) {
        return PR_OPT_ERROR_NOT_INITIALIZED;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_tier_ad", 0.0f);


    if (count == 0) {
        return PR_OPT_SUCCESS;
    }

    uint64_t start_time = get_time_ns();

    BRIDGE_LOCK(bridge);

    const pr_optimizer_config_t* cfg = &bridge->config;
    float total_lr = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)count);
        }

        float effective_lr = cfg->base_lr;

        if (nodes[i] && cfg->enable_tier_adaptation) {
            // Get tier from node
            pr_memory_tier_t tier = pr_memory_node_get_tier(nodes[i]);
            if (tier < PR_OPT_NUM_TIERS) {
                effective_lr *= cfg->tier_lr_scale[tier];
            }
        }

        total_lr += effective_lr;

        // Apply update
        params[i] -= effective_lr * gradients[i];
    }

    // Update statistics
    atomic_fetch_add(&bridge->stats.total_steps, 1);
    atomic_fetch_add(&bridge->stats.tier_adaptive_steps, 1);
    atomic_fetch_add(&bridge->stats.allowed_updates, count);

    update_stats_lr(&bridge->stats, total_lr / (float)count);

    uint64_t elapsed = get_time_ns() - start_time;
    bridge->stats.total_time_ns += elapsed;
    bridge->stats.last_step_time_ns = elapsed;

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

//=============================================================================
// Learning Rate Functions
//=============================================================================

float pr_optimizer_compute_effective_lr(
    pr_optimizer_bridge_t bridge,
    float base_lr,
    const pr_memory_node_t* node,
    float resonance
) {
    if (!bridge) {
        return base_lr;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_compute", 0.0f);


    const pr_optimizer_config_t* cfg = &bridge->config;
    float effective_lr = base_lr;

    // Resonance scaling
    if (cfg->enable_resonance_scaling) {
        float r = nimcp_myelin_clamp(resonance, 0.0f, 1.0f);
        effective_lr *= (1.0f + cfg->resonance_scale * r);
    }

    if (node) {
        // Tier adaptation
        if (cfg->enable_tier_adaptation) {
            pr_memory_tier_t tier = pr_memory_node_get_tier(node);
            if (tier < PR_OPT_NUM_TIERS) {
                effective_lr *= cfg->tier_lr_scale[tier];
            }
        }

        // Consolidation gating
        if (cfg->enable_consolidation_gating) {
            nimcp_quaternion_t state = pr_memory_node_get_state(node);
            float consolidation = nimcp_myelin_clamp(state.w, 0.0f, 1.0f);
            if (consolidation >= cfg->consolidation_gate) {
                effective_lr = 0.0f;  // Fully gated
            } else {
                float gate = 1.0f - (consolidation / cfg->consolidation_gate);
                effective_lr *= gate;
            }
        }
    }

    return effective_lr;
}

pr_optimizer_error_t pr_optimizer_set_lr(
    pr_optimizer_bridge_t bridge,
    float lr
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_set_lr", 0.0f);


    if (lr <= 0.0f) {
        return PR_OPT_ERROR_INVALID_CONFIG;
    }

    BRIDGE_LOCK(bridge);

    bridge->config.base_lr = lr;
    bridge->current_lr = lr;

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

float pr_optimizer_get_lr(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_get_lr", 0.0f);


    return bridge->config.base_lr;
}

//=============================================================================
// Gradient Functions
//=============================================================================

float pr_optimizer_clip_by_entanglement(
    pr_optimizer_bridge_t bridge,
    float* gradients,
    size_t count,
    const uint32_t* entanglement_counts
) {
    if (!bridge || !gradients || count == 0) {
        return 0.0f;
        BRIDGE_BBB_VALIDATE(bridge, gradients, sizeof(*gradients));
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_clip_by", 0.0f);


    const pr_optimizer_config_t* cfg = &bridge->config;

    // Compute original gradient norm
    float original_norm = compute_grad_norm(gradients, count);

    if (!cfg->enable_entanglement_clipping || !entanglement_counts) {
        return original_norm;
    }

    BRIDGE_LOCK(bridge);

    // Compute total entanglement
    uint64_t total_entanglement = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)count);
        }

        total_entanglement += entanglement_counts[i];
    }

    // Compute clip norm based on entanglement
    float clip_norm = cfg->max_grad_norm /
        (1.0f + (float)total_entanglement * cfg->entanglement_scale);

    // Clip if necessary
    if (original_norm > clip_norm && original_norm > GRAD_NORM_MIN) {
        float scale = clip_norm / (fabsf(original_norm) > 1e-7f ? original_norm : 1e-7f);
        scale_gradients(gradients, count, scale);
        atomic_fetch_add(&bridge->stats.gradient_clips, 1);
    }

    BRIDGE_UNLOCK(bridge);

    return original_norm;
}

float pr_optimizer_clip_by_norm(
    pr_optimizer_bridge_t bridge,
    float* gradients,
    size_t count,
    float max_norm
) {
    BRIDGE_BBB_VALIDATE(bridge, gradients, sizeof(*gradients));
    if (!gradients || count == 0 || max_norm <= 0.0f) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_clip_by", 0.0f);


    float original_norm = compute_grad_norm(gradients, count);

    if (original_norm > max_norm && original_norm > GRAD_NORM_MIN) {
        float scale = max_norm / (fabsf(original_norm) > 1e-7f ? original_norm : 1e-7f);
        scale_gradients(gradients, count, scale);

        if (bridge) {
            BRIDGE_LOCK(bridge);
            atomic_fetch_add(&bridge->stats.gradient_clips, 1);
            BRIDGE_UNLOCK(bridge);
        }
    }

    return original_norm;
}

void pr_optimizer_zero_grad(float* gradients, size_t count) {
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_zero_gr", 0.0f);


    if (gradients && count > 0) {
        memset(gradients, 0, count * sizeof(float));
    }
}

pr_optimizer_error_t pr_optimizer_zero_state(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_zero_st", 0.0f);


    BRIDGE_LOCK(bridge);

    if (!bridge->state.initialized) {
        BRIDGE_UNLOCK(bridge);
        return PR_OPT_ERROR_NOT_INITIALIZED;
    }

    switch (bridge->state.type) {
        case PR_OPT_RESONANCE_ADAM:
        case PR_OPT_CONSOLIDATION_GATED:
        case PR_OPT_TIER_ADAPTIVE:
            if (bridge->state.state.adam.m) {
                memset(bridge->state.state.adam.m, 0,
                       bridge->state.state.adam.count * sizeof(float));
            }
            if (bridge->state.state.adam.v) {
                memset(bridge->state.state.adam.v, 0,
                       bridge->state.state.adam.count * sizeof(float));
            }
            if (bridge->state.state.adam.v_max) {
                memset(bridge->state.state.adam.v_max, 0,
                       bridge->state.state.adam.count * sizeof(float));
            }
            bridge->state.state.adam.step = 0;
            break;

        case PR_OPT_QUAT_SGD:
        case PR_OPT_QUAT_MOMENTUM:
            if (bridge->state.state.quat.velocities) {
                for (size_t i = 0; i < bridge->state.state.quat.count; i++) {
                    bridge->state.state.quat.velocities[i] = quat_create(0, 0, 0, 0);
                }
            }
            break;

        default:
            break;
    }

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

//=============================================================================
// State Query Functions
//=============================================================================

const pr_optimizer_state_t* pr_optimizer_get_state(
    pr_optimizer_bridge_t bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_get_sta", 0.0f);


    return &bridge->state;
}

uint64_t pr_optimizer_get_step(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_get_ste", 0.0f);


    return atomic_load(&bridge->stats.total_steps);
}

pr_optimizer_type_t pr_optimizer_get_type(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        return PR_OPT_RESONANCE_ADAM;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_get_typ", 0.0f);


    return bridge->active_type;
}

pr_optimizer_error_t pr_optimizer_set_type(
    pr_optimizer_bridge_t bridge,
    pr_optimizer_type_t type
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_set_typ", 0.0f);


    if (type >= PR_OPT_TYPE_COUNT) {
        return PR_OPT_ERROR_INVALID_TYPE;
    }

    BRIDGE_LOCK(bridge);

    bridge->active_type = type;

    // Note: May need to reinitialize state if type changed
    if (bridge->state.initialized && bridge->state.type != type) {
        bridge->state.initialized = false;
    }

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

//=============================================================================
// Statistics Functions
//=============================================================================

pr_optimizer_error_t pr_optimizer_get_stats(
    pr_optimizer_bridge_t bridge,
    pr_optimizer_stats_t* stats
) {
    if (!bridge || !stats) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    // Copy atomic values
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_get_sta", 0.0f);


    stats->total_steps = atomic_load(&bridge->stats.total_steps);
    stats->resonance_adam_steps = atomic_load(&bridge->stats.resonance_adam_steps);
    stats->quat_momentum_steps = atomic_load(&bridge->stats.quat_momentum_steps);
    stats->consolidation_gated_steps = atomic_load(&bridge->stats.consolidation_gated_steps);
    stats->tier_adaptive_steps = atomic_load(&bridge->stats.tier_adaptive_steps);
    stats->gradient_clips = atomic_load(&bridge->stats.gradient_clips);
    stats->gated_updates = atomic_load(&bridge->stats.gated_updates);
    stats->allowed_updates = atomic_load(&bridge->stats.allowed_updates);

    // Copy non-atomic values (under lock for consistency)
    BRIDGE_LOCK(bridge);

    stats->total_gradient_norm = bridge->stats.total_gradient_norm;
    stats->max_gradient_norm = bridge->stats.max_gradient_norm;
    stats->min_gradient_norm = bridge->stats.min_gradient_norm;
    stats->avg_gradient_norm = bridge->stats.avg_gradient_norm;

    stats->total_effective_lr = bridge->stats.total_effective_lr;
    stats->max_effective_lr = bridge->stats.max_effective_lr;
    stats->min_effective_lr = bridge->stats.min_effective_lr;
    stats->avg_effective_lr = bridge->stats.avg_effective_lr;

    stats->total_resonance = bridge->stats.total_resonance;
    stats->avg_resonance = bridge->stats.avg_resonance;

    stats->total_time_ns = bridge->stats.total_time_ns;
    stats->last_step_time_ns = bridge->stats.last_step_time_ns;

    // Compute averages
    uint64_t steps = stats->total_steps;
    if (steps > 0) {
        stats->avg_gradient_norm = stats->total_gradient_norm / (float)steps;
        stats->avg_effective_lr = stats->total_effective_lr / (float)steps;
        stats->avg_resonance = stats->total_resonance / (float)steps;
    }

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

pr_optimizer_error_t pr_optimizer_reset_stats(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_reset_s", 0.0f);


    BRIDGE_LOCK(bridge);

    atomic_store(&bridge->stats.total_steps, 0);
    atomic_store(&bridge->stats.resonance_adam_steps, 0);
    atomic_store(&bridge->stats.quat_momentum_steps, 0);
    atomic_store(&bridge->stats.consolidation_gated_steps, 0);
    atomic_store(&bridge->stats.tier_adaptive_steps, 0);
    atomic_store(&bridge->stats.gradient_clips, 0);
    atomic_store(&bridge->stats.gated_updates, 0);
    atomic_store(&bridge->stats.allowed_updates, 0);

    bridge->stats.total_gradient_norm = 0.0f;
    bridge->stats.max_gradient_norm = 0.0f;
    bridge->stats.min_gradient_norm = INFINITY;
    bridge->stats.avg_gradient_norm = 0.0f;

    bridge->stats.total_effective_lr = 0.0f;
    bridge->stats.max_effective_lr = 0.0f;
    bridge->stats.min_effective_lr = INFINITY;
    bridge->stats.avg_effective_lr = 0.0f;

    bridge->stats.total_resonance = 0.0f;
    bridge->stats.avg_resonance = 0.0f;

    bridge->stats.total_time_ns = 0;
    bridge->stats.last_step_time_ns = 0;

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

const char* pr_optimizer_type_name(pr_optimizer_type_t type) {
    switch (type) {
        case PR_OPT_RESONANCE_ADAM:
            return "Resonance-Adam";
        case PR_OPT_QUAT_SGD:
            return "Quaternion-SGD";
        case PR_OPT_QUAT_MOMENTUM:
            return "Quaternion-Momentum";
        case PR_OPT_CONSOLIDATION_GATED:
            return "Consolidation-Gated";
        case PR_OPT_TIER_ADAPTIVE:
            return "Tier-Adaptive";
        default:
            return "Unknown";
    }
}

const char* pr_optimizer_error_string(pr_optimizer_error_t error) {
    switch (error) {
        case PR_OPT_SUCCESS:
            return "Success";
        case PR_OPT_ERROR_NULL_POINTER:
            return "Null pointer argument";
        case PR_OPT_ERROR_INVALID_CONFIG:
            return "Invalid configuration";
        case PR_OPT_ERROR_INVALID_TYPE:
            return "Invalid optimizer type";
        case PR_OPT_ERROR_NO_MEMORY:
            return "Memory allocation failed";
        case PR_OPT_ERROR_NOT_INITIALIZED:
            return "Bridge not initialized";
        case PR_OPT_ERROR_SIZE_MISMATCH:
            return "Array size mismatch";
        case PR_OPT_ERROR_INVALID_QUATERNION:
            return "Invalid quaternion state";
        case PR_OPT_ERROR_MUTEX_FAILED:
            return "Mutex operation failed";
        case PR_OPT_ERROR_BATCH_TOO_LARGE:
            return "Batch size exceeds limit";
        case PR_OPT_ERROR_INVALID_TIER:
            return "Invalid memory tier";
        default:
            return "Unknown error";
    }
}

//=============================================================================
// Configuration Update Functions
//=============================================================================

pr_optimizer_error_t pr_optimizer_update_config(
    pr_optimizer_bridge_t bridge,
    const pr_optimizer_config_t* config
) {
    if (!bridge || !config) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    if (!pr_optimizer_config_validate(config)) {
        return PR_OPT_ERROR_INVALID_CONFIG;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_update_", 0.0f);


    BRIDGE_LOCK(bridge);

    bridge->config = *config;
    bridge->current_lr = config->base_lr;

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

const pr_optimizer_config_t* pr_optimizer_get_config(
    pr_optimizer_bridge_t bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_get_con", 0.0f);


    return &bridge->config;
}

pr_optimizer_error_t pr_optimizer_set_tier_scale(
    pr_optimizer_bridge_t bridge,
    pr_memory_tier_t tier,
    float scale
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_set_tie", 0.0f);


    if (tier >= PR_OPT_NUM_TIERS) {
        return PR_OPT_ERROR_INVALID_TIER;
    }

    if (scale < 0.0f) {
        return PR_OPT_ERROR_INVALID_CONFIG;
    }

    BRIDGE_LOCK(bridge);

    bridge->config.tier_lr_scale[tier] = scale;

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

pr_optimizer_error_t pr_optimizer_set_resonance_scale(
    pr_optimizer_bridge_t bridge,
    float scale
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_set_res", 0.0f);


    if (scale < 0.0f || scale > 10.0f) {
        return PR_OPT_ERROR_INVALID_CONFIG;
    }

    BRIDGE_LOCK(bridge);

    bridge->config.resonance_scale = scale;

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

pr_optimizer_error_t pr_optimizer_set_consolidation_gate(
    pr_optimizer_bridge_t bridge,
    float threshold
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_set_con", 0.0f);


    if (threshold < 0.0f || threshold > 1.0f) {
        return PR_OPT_ERROR_INVALID_CONFIG;
    }

    BRIDGE_LOCK(bridge);

    bridge->config.consolidation_gate = threshold;

    BRIDGE_UNLOCK(bridge);

    return PR_OPT_SUCCESS;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

pr_optimizer_error_t pr_optimizer_connect_bio_async(
    pr_optimizer_bridge_t bridge
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_connect", 0.0f);


    int result = bridge_base_connect_bio_async(&bridge->base);
    return (result == 0) ? PR_OPT_SUCCESS : PR_OPT_ERROR_MUTEX_FAILED;
}

pr_optimizer_error_t pr_optimizer_disconnect_bio_async(
    pr_optimizer_bridge_t bridge
) {
    if (!bridge) {
        return PR_OPT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_disconn", 0.0f);


    int result = bridge_base_disconnect_bio_async(&bridge->base);
    return (result == 0) ? PR_OPT_SUCCESS : PR_OPT_ERROR_MUTEX_FAILED;
}

bool pr_optimizer_is_bio_async_connected(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_is_bio_", 0.0f);


    return bridge_base_is_bio_async_connected(&bridge->base);
}

//=============================================================================
// Utility Functions - Quaternion Manifold Operations
//=============================================================================

nimcp_quaternion_t pr_optimizer_project_to_tangent(
    nimcp_quaternion_t q,
    nimcp_quaternion_t grad
) {
    // Project gradient to tangent space at q
    // grad_proj = grad - (grad . q) * q

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_project", 0.0f);


    float dot = quat_dot(grad, q);

    nimcp_quaternion_t result;
    result.w = grad.w - dot * q.w;
    result.x = grad.x - dot * q.x;
    result.y = grad.y - dot * q.y;
    result.z = grad.z - dot * q.z;

    return result;
}

nimcp_quaternion_t pr_optimizer_exp_map(
    nimcp_quaternion_t q,
    nimcp_quaternion_t tangent_vec
) {
    // Apply exponential map to move along geodesic
    // q_new = q * exp(tangent_vec)

    // First, compute exp(tangent_vec)
    // For small tangent vectors: exp(v) ≈ (1, v_xyz)
    // For general: exp(v) = (cos(|v|), v/|v| * sin(|v|))

    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_exp_map", 0.0f);


    float norm = quat_magnitude(tangent_vec);

    nimcp_quaternion_t exp_v;
    if (norm < 1e-6f) {
        // Small angle approximation
        exp_v.w = 1.0f;
        exp_v.x = tangent_vec.x;
        exp_v.y = tangent_vec.y;
        exp_v.z = tangent_vec.z;
    } else {
        float s = sinf(norm) / (fabsf(norm) > 1e-7f ? norm : 1e-7f);
        exp_v.w = cosf(norm);
        exp_v.x = tangent_vec.x * s;
        exp_v.y = tangent_vec.y * s;
        exp_v.z = tangent_vec.z * s;
    }

    // Multiply q * exp(v)
    nimcp_quaternion_t result = quat_hamilton_product(q, exp_v);

    return result;
}

nimcp_quaternion_t pr_optimizer_parallel_transport(
    nimcp_quaternion_t velocity,
    nimcp_quaternion_t q_from,
    nimcp_quaternion_t q_to
) {
    // Parallel transport velocity from tangent space at q_from to q_to
    // Uses the quaternion parallel transport formula

    // Compute rotation from q_from to q_to
    /* Phase 8: Heartbeat at operation start */
    pr_optimizer_bridge_heartbeat("pr_optimizer_pr_optimizer_paralle", 0.0f);


    nimcp_quaternion_t q_from_inv = quat_conjugate(q_from);
    nimcp_quaternion_t rotation = quat_hamilton_product(q_to, q_from_inv);

    // Apply half rotation to velocity
    // This is the correct parallel transport on the unit sphere

    // First, project velocity to ensure it's in tangent space at q_from
    velocity = pr_optimizer_project_to_tangent(q_from, velocity);

    // Apply rotation
    nimcp_quaternion_t result = quat_hamilton_product(
        quat_hamilton_product(rotation, velocity),
        quat_conjugate(rotation)
    );

    // Project result to tangent space at q_to
    result = pr_optimizer_project_to_tangent(q_to, result);

    return result;
}

//=============================================================================
// Private Helper Functions - Implementation
//=============================================================================

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Compute L2 norm of gradient array
 */
static float compute_grad_norm(const float* gradients, size_t count) {
    if (!gradients || count == 0) {
        return 0.0f;
    }

    double sum_sq = 0.0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)count);
        }

        sum_sq += (double)gradients[i] * (double)gradients[i];
    }

    return (float)sqrt(sum_sq);
}

/**
 * @brief Scale all gradients by a factor
 */
static void scale_gradients(float* gradients, size_t count, float scale) {
    if (!gradients || count == 0) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)count);
        }

        gradients[i] *= scale;
    }
}

/**
 * @brief Initialize Adam optimizer state
 */
static pr_optimizer_error_t init_adam_state(
    pr_optimizer_bridge_t bridge,
    size_t num_params
) {
    // Allocate first moment estimates
    bridge->state.state.adam.m = nimcp_calloc(num_params, sizeof(float));
    if (!bridge->state.state.adam.m) {
        return PR_OPT_ERROR_NO_MEMORY;
    }

    // Allocate second moment estimates
    bridge->state.state.adam.v = nimcp_calloc(num_params, sizeof(float));
    if (!bridge->state.state.adam.v) {
        nimcp_free(bridge->state.state.adam.m);
        bridge->state.state.adam.m = NULL;
        return PR_OPT_ERROR_NO_MEMORY;
    }

    // Allocate max v for AMSGrad (optional, but pre-allocate)
    bridge->state.state.adam.v_max = nimcp_calloc(num_params, sizeof(float));
    if (!bridge->state.state.adam.v_max) {
        nimcp_free(bridge->state.state.adam.m);
        nimcp_free(bridge->state.state.adam.v);
        bridge->state.state.adam.m = NULL;
        bridge->state.state.adam.v = NULL;
        return PR_OPT_ERROR_NO_MEMORY;
    }

    bridge->state.state.adam.count = num_params;
    bridge->state.state.adam.step = 0;

    return PR_OPT_SUCCESS;
}

/**
 * @brief Initialize quaternion momentum state
 */
static pr_optimizer_error_t init_quat_momentum_state(
    pr_optimizer_bridge_t bridge,
    size_t num_quats
) {
    bridge->state.state.quat.velocities = nimcp_calloc(
        num_quats, sizeof(nimcp_quaternion_t));

    if (!bridge->state.state.quat.velocities) {
        return PR_OPT_ERROR_NO_MEMORY;
    }

    // Initialize velocities to zero quaternion
    for (size_t i = 0; i < num_quats; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_quats > 256) {
            pr_optimizer_bridge_heartbeat("pr_optimizer_loop",
                             (float)(i + 1) / (float)num_quats);
        }

        bridge->state.state.quat.velocities[i] = quat_create(0, 0, 0, 0);
    }

    bridge->state.state.quat.count = num_quats;

    return PR_OPT_SUCCESS;
}

/**
 * @brief Free optimizer state memory
 */
static void free_optimizer_state(pr_optimizer_state_t* state) {
    if (!state || !state->initialized) {
        return;
    }

    switch (state->type) {
        case PR_OPT_RESONANCE_ADAM:
        case PR_OPT_CONSOLIDATION_GATED:
        case PR_OPT_TIER_ADAPTIVE:
            if (state->state.adam.m) {
                nimcp_free(state->state.adam.m);
                state->state.adam.m = NULL;
            }
            if (state->state.adam.v) {
                nimcp_free(state->state.adam.v);
                state->state.adam.v = NULL;
            }
            if (state->state.adam.v_max) {
                nimcp_free(state->state.adam.v_max);
                state->state.adam.v_max = NULL;
            }
            state->state.adam.count = 0;
            state->state.adam.step = 0;
            break;

        case PR_OPT_QUAT_SGD:
        case PR_OPT_QUAT_MOMENTUM:
            if (state->state.quat.velocities) {
                nimcp_free(state->state.quat.velocities);
                state->state.quat.velocities = NULL;
            }
            state->state.quat.count = 0;
            break;

        default:
            break;
    }

    state->initialized = false;
}

/**
 * @brief Update gradient statistics
 */
static void update_stats_gradient(
    pr_optimizer_stats_t* stats,
    float grad_norm
) {
    if (!stats) return;

    stats->total_gradient_norm += grad_norm;

    if (grad_norm > stats->max_gradient_norm) {
        stats->max_gradient_norm = grad_norm;
    }

    if (grad_norm < stats->min_gradient_norm) {
        stats->min_gradient_norm = grad_norm;
    }
}

/**
 * @brief Update learning rate statistics
 */
static void update_stats_lr(
    pr_optimizer_stats_t* stats,
    float effective_lr
) {
    if (!stats) return;

    stats->total_effective_lr += effective_lr;

    if (effective_lr > stats->max_effective_lr) {
        stats->max_effective_lr = effective_lr;
    }

    if (effective_lr < stats->min_effective_lr) {
        stats->min_effective_lr = effective_lr;
    }
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_optimizer_bridge_set_instance_health_agent(
    pr_optimizer_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_optimizer_bridge_training_begin(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_optimizer_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_optimizer_bridge_heartbeat_instance(bridge->health_agent, "pr_optimizer_bridge_training_begin", 0.0f);
    return 0;
}

int pr_optimizer_bridge_training_end(pr_optimizer_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_optimizer_bridge_training_end: NULL argument");
        return -1;
    }
    pr_optimizer_bridge_heartbeat_instance(bridge->health_agent, "pr_optimizer_bridge_training_end", 1.0f);
    return 0;
}

int pr_optimizer_bridge_training_step(pr_optimizer_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_optimizer_bridge_training_step: NULL argument");
        return -1;
    }
    pr_optimizer_bridge_heartbeat_instance(bridge->health_agent, "pr_optimizer_bridge_training_step", progress);
    return 0;
}
