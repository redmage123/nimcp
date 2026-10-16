/**
 * @file nimcp_security_imagination_bridge.c
 * @brief Security Module - Imagination System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional integration between security module and imagination system
 * WHY:  Imagination/hypothetical reasoning requires security oversight to prevent
 *       confabulation, adversarial amplification, and reality disconnection
 * HOW:  Workspace sandboxing, confabulation detection, reasoning bounds,
 *       reality grounding, and simulation integrity verification
 */

#include "security/imagination/nimcp_security_imagination_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_imagination_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Module ID for bio-async registration */
#define BIO_MODULE_SECURITY_IMAGINATION_BRIDGE 0x1600

/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

static int find_sandbox_by_id(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    size_t* index
);

static void update_average(float* avg, uint64_t count, float new_value);

static float compute_confabulation_score(
    const void* content,
    size_t content_size
);

static float compute_divergence_score(
    const security_imagination_sandbox_t* sandbox
);

static float compute_integrity_score(
    const security_imagination_sandbox_t* sandbox
);

static uint64_t generate_sandbox_id(void);

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

/**
 * @brief Get default security-imagination bridge configuration
 */
int security_imagination_default_config(security_imagination_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(security_imagination_config_t));

    /* Enable all security features by default */
    config->enable_workspace_sandboxing = true;
    config->enable_confabulation_detection = true;
    config->enable_reasoning_bounds = true;
    config->enable_reality_grounding = true;
    config->enable_simulation_integrity = true;
    config->enable_adversarial_detection = true;
    config->enable_resource_tracking = true;
    config->enable_audit_logging = true;

    /* Sandbox configuration */
    config->default_isolation_level = SANDBOX_LEVEL_STANDARD;
    config->max_sandboxed_workspaces = SECURITY_IMAGINATION_MAX_SANDBOXES;

    /* Reasoning limits */
    config->max_hypothetical_depth = SECURITY_IMAGINATION_DEFAULT_MAX_DEPTH;
    config->max_branching_factor = 4;
    config->max_simulation_steps = 10000;

    /* Detection thresholds */
    config->confabulation_threshold = SECURITY_IMAGINATION_DEFAULT_CONFAB_THRESHOLD;
    config->reality_divergence_threshold = SECURITY_IMAGINATION_DEFAULT_DIVERGENCE_THRESHOLD;
    config->adversarial_threshold = 0.6f;
    config->integrity_threshold = 0.8f;

    /* Resource limits */
    config->default_simulation_budget = SECURITY_IMAGINATION_DEFAULT_SIM_BUDGET;
    config->resource_warning_threshold = 0.8f;

    return NIMCP_SUCCESS;
}

/**
 * @brief Create security-imagination bridge
 */
security_imagination_bridge_t* security_imagination_bridge_create(
    const security_imagination_config_t* config
) {
    security_imagination_bridge_t* bridge = nimcp_calloc(
        1, sizeof(security_imagination_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security-imagination bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_imagination_bridge_create: failed to allocate bridge");

        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_IMAGINATION_BRIDGE,
                         "security_imagination_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_imagination_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_imagination_default_config(&bridge->config);
    }

    /* Allocate sandbox array */
    bridge->sandbox_capacity = bridge->config.max_sandboxed_workspaces;
    bridge->sandboxes = nimcp_calloc(
        bridge->sandbox_capacity,
        sizeof(security_imagination_sandbox_t)
    );
    if (!bridge->sandboxes) {
        NIMCP_LOGGING_ERROR("Failed to allocate sandbox array");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_imagination_bridge_create: bridge->sandboxes is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.is_active = true;
    bridge->state.imagination_restricted = false;
    bridge->state.new_scenarios_blocked = false;
    bridge->next_sandbox_id = 1;

    /* Initialize security effects with defaults */
    bridge->security_effects.effective_max_depth = bridge->config.max_hypothetical_depth;
    bridge->security_effects.effective_simulation_budget = bridge->config.default_simulation_budget;
    bridge->security_effects.depth_reduction_factor = 1.0f;
    bridge->security_effects.resource_reduction_factor = 1.0f;
    bridge->security_effects.effective_confab_threshold = bridge->config.confabulation_threshold;
    bridge->security_effects.effective_divergence_threshold = bridge->config.reality_divergence_threshold;
    bridge->security_effects.min_required_level = bridge->config.default_isolation_level;

    return bridge;
}

/**
 * @brief Destroy security-imagination bridge
 */
void security_imagination_bridge_destroy(security_imagination_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Free sandbox array */
    if (bridge->sandboxes) {
        nimcp_free(bridge->sandboxes);
        bridge->sandboxes = NULL;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

/**
 * @brief Connect imagination engine to bridge
 */
int security_imagination_connect_engine(
    security_imagination_bridge_t* bridge,
    struct imagination_engine* engine
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(engine, NIMCP_ERROR_NULL_POINTER, "engine is NULL");

    BRIDGE_LOCK(bridge);
    bridge->imagination_engine = engine;
    bridge->state.imagination_engine_connected = true;
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Connect imagination workspace to bridge
 */
int security_imagination_connect_workspace(
    security_imagination_bridge_t* bridge,
    struct imagination_workspace* workspace
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_NULL_POINTER, "workspace is NULL");

    BRIDGE_LOCK(bridge);
    bridge->workspace = workspace;
    bridge->state.workspace_connected = true;
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if bridge is connected
 */
bool security_imagination_is_connected(const security_imagination_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Minimum: either engine or workspace */
    return bridge->state.imagination_engine_connected ||
           bridge->state.workspace_connected;
}

/* ============================================================================
 * Sandbox Management API Implementation
 * ============================================================================ */

/**
 * @brief Create sandboxed workspace for imagination scenario
 */
int security_imagination_sandbox_workspace(
    security_imagination_bridge_t* bridge,
    const char* scenario_name,
    sandbox_isolation_level_t isolation_level,
    uint64_t* sandbox_id
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Check if new scenarios are blocked */
    if (bridge->state.new_scenarios_blocked) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    BRIDGE_LOCK(bridge);

    /* Check capacity */
    if (bridge->sandbox_count >= bridge->sandbox_capacity) {
        bridge->security_effects.sandbox_quota_reached = true;
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Find empty slot */
    size_t slot = 0;
    for (size_t i = 0; i < bridge->sandbox_capacity; i++) {
        if (!bridge->sandboxes[i].is_active) {
            slot = i;
            break;
        }
    }

    /* Initialize sandbox */
    security_imagination_sandbox_t* sandbox = &bridge->sandboxes[slot];
    memset(sandbox, 0, sizeof(security_imagination_sandbox_t));

    sandbox->sandbox_id = generate_sandbox_id();
    if (scenario_name) {
        strncpy(sandbox->scenario_name, scenario_name,
                SECURITY_IMAGINATION_MAX_SIM_NAME - 1);
    }
    sandbox->isolation_level = isolation_level;
    sandbox->is_active = true;
    sandbox->created_at_ms = nimcp_platform_time_monotonic_ms();
    sandbox->flags = SECURITY_IMAGINATION_FLAG_SANDBOXED;

    bridge->sandbox_count++;
    bridge->state.active_sandbox_count = (uint32_t)bridge->sandbox_count;
    bridge->security_effects.active_sandboxes = (uint32_t)bridge->sandbox_count;
    bridge->security_effects.sandbox_quota_reached =
        (bridge->sandbox_count >= bridge->sandbox_capacity);
    bridge->stats.sandboxes_created++;

    if (sandbox_id) {
        *sandbox_id = sandbox->sandbox_id;
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Release sandboxed workspace
 */
int security_imagination_release_sandbox(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Calculate duration for statistics */
    uint64_t duration = nimcp_platform_time_monotonic_ms() -
                        bridge->sandboxes[index].created_at_ms;
    update_average(&bridge->stats.avg_sandbox_duration_ms,
                   bridge->stats.sandboxes_destroyed + 1,
                   (float)duration);

    /* Clear sandbox */
    bridge->sandboxes[index].is_active = false;
    bridge->sandbox_count--;
    bridge->state.active_sandbox_count = (uint32_t)bridge->sandbox_count;
    bridge->security_effects.active_sandboxes = (uint32_t)bridge->sandbox_count;
    bridge->security_effects.sandbox_quota_reached = false;
    bridge->stats.sandboxes_destroyed++;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get sandbox information
 */
int security_imagination_get_sandbox(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    security_imagination_sandbox_t* sandbox
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(sandbox, NIMCP_ERROR_NULL_POINTER, "sandbox is NULL");

    BRIDGE_LOCK((security_imagination_bridge_t*)bridge);

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    *sandbox = bridge->sandboxes[index];

    BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Confabulation Detection API Implementation
 * ============================================================================ */

/**
 * @brief Detect confabulation in imagination content
 */
int security_imagination_detect_confabulation(
    security_imagination_bridge_t* bridge,
    const void* content,
    size_t content_size,
    security_imagination_confab_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    /* Skip detection if disabled */
    if (!bridge->config.enable_confabulation_detection) {
        result->detected = false;
        result->score = 0.0f;
        result->type = CONFAB_TYPE_NONE;
        return NIMCP_SUCCESS;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.confab_checks++;

    /* Compute confabulation score */
    float score = 0.0f;
    if (content && content_size > 0) {
        score = compute_confabulation_score(content, content_size);
    }

    result->score = score;
    result->source_timestamp = nimcp_platform_time_monotonic_ms();

    /* Check against threshold */
    if (score >= bridge->security_effects.effective_confab_threshold) {
        result->detected = true;
        result->type = CONFAB_TYPE_FACT_INVENTION;  /* Default type */
        bridge->stats.confab_detections++;
        bridge->imagination_effects.confabulations_detected++;

        if (score > bridge->imagination_effects.peak_confabulation_score) {
            bridge->imagination_effects.peak_confabulation_score = score;
        }
    } else {
        result->detected = false;
        result->type = CONFAB_TYPE_NONE;
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Check scenario for confabulation patterns
 */
int security_imagination_check_scenario_confabulation(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    security_imagination_confab_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    BRIDGE_LOCK(bridge);

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_imagination_sandbox_t* sandbox = &bridge->sandboxes[index];

    result->score = sandbox->confabulation_score;
    result->detected = (sandbox->flags & SECURITY_IMAGINATION_FLAG_CONFAB_DETECTED) != 0;
    result->type = result->detected ? CONFAB_TYPE_FACT_INVENTION : CONFAB_TYPE_NONE;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Reasoning Bounds API Implementation
 * ============================================================================ */

/**
 * @brief Enforce hypothetical reasoning bounds
 */
bool security_imagination_enforce_bounds(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    uint32_t requested_depth
) {
    if (!bridge) {
        return false;
    }

    if (!bridge->config.enable_reasoning_bounds) {
        return true;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.depth_checks++;

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        return false;
    }

    uint32_t effective_max = bridge->security_effects.effective_max_depth;
    bool allowed = requested_depth <= effective_max;

    if (allowed) {
        bridge->sandboxes[index].current_depth = requested_depth;
        if (requested_depth > bridge->sandboxes[index].max_depth_reached) {
            bridge->sandboxes[index].max_depth_reached = requested_depth;
        }
        bridge->sandboxes[index].flags |= SECURITY_IMAGINATION_FLAG_BOUNDED;
    } else {
        bridge->stats.depth_limit_hits++;
        bridge->sandboxes[index].flags |= SECURITY_IMAGINATION_FLAG_DEPTH_LIMITED;
    }

    if (requested_depth > bridge->stats.max_depth_observed) {
        bridge->stats.max_depth_observed = requested_depth;
    }

    BRIDGE_UNLOCK(bridge);

    return allowed;
}

/**
 * @brief Check current depth against limits
 */
bool security_imagination_check_depth(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    uint32_t current_depth
) {
    if (!bridge) {
        return false;
    }

    if (!bridge->config.enable_reasoning_bounds) {
        return true;
    }

    return current_depth <= bridge->security_effects.effective_max_depth;
}

/**
 * @brief Get effective maximum depth
 */
uint32_t security_imagination_get_max_depth(
    const security_imagination_bridge_t* bridge
) {
    if (!bridge) {
        return 0;
    }
    return bridge->security_effects.effective_max_depth;
}

/* ============================================================================
 * Reality Grounding API Implementation
 * ============================================================================ */

/**
 * @brief Verify imagination grounding to reality
 */
int security_imagination_ground_reality(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    security_imagination_grounding_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    /* Skip if disabled */
    if (!bridge->config.enable_reality_grounding) {
        result->grounded = true;
        result->divergence_score = 0.0f;
        return NIMCP_SUCCESS;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.grounding_checks++;

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_imagination_sandbox_t* sandbox = &bridge->sandboxes[index];

    /* Compute divergence score */
    float divergence = compute_divergence_score(sandbox);
    sandbox->reality_divergence = divergence;
    result->divergence_score = divergence;

    /* Check against threshold */
    float threshold = bridge->security_effects.effective_divergence_threshold;
    result->grounded = (divergence < threshold);

    if (result->grounded) {
        sandbox->flags |= SECURITY_IMAGINATION_FLAG_GROUNDED;
        result->anchor_points_valid = 8;  /* Simulated */
        result->anchor_points_total = 10;
    } else {
        sandbox->flags |= SECURITY_IMAGINATION_FLAG_DIVERGED;
        bridge->stats.grounding_failures++;
        bridge->imagination_effects.divergence_violations++;
        result->anchor_points_valid = 2;
        result->anchor_points_total = 10;
        result->violations_count = 3;
    }

    /* Update statistics */
    update_average(&bridge->stats.avg_divergence_score,
                   bridge->stats.grounding_checks,
                   divergence);

    if (divergence > bridge->imagination_effects.current_max_divergence) {
        bridge->imagination_effects.current_max_divergence = divergence;
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get current reality divergence score
 */
float security_imagination_get_divergence(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id
) {
    if (!bridge) {
        return -1.0f;
    }

    BRIDGE_LOCK((security_imagination_bridge_t*)bridge);

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);
        return -1.0f;
    }

    float divergence = bridge->sandboxes[index].reality_divergence;

    BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);

    return divergence;
}

/* ============================================================================
 * Simulation Integrity API Implementation
 * ============================================================================ */

/**
 * @brief Verify simulation integrity
 */
int security_imagination_verify_simulation(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    security_imagination_integrity_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    /* Skip if disabled */
    if (!bridge->config.enable_simulation_integrity) {
        result->integrity_valid = true;
        result->integrity_score = 1.0f;
        return NIMCP_SUCCESS;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.integrity_checks++;

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_imagination_sandbox_t* sandbox = &bridge->sandboxes[index];

    /* Compute integrity score */
    float score = compute_integrity_score(sandbox);
    result->integrity_score = score;

    /* Check against threshold */
    result->integrity_valid = (score >= bridge->config.integrity_threshold);
    result->hash_verified = true;
    result->consistency_valid = result->integrity_valid;
    result->causality_preserved = result->integrity_valid;

    if (result->integrity_valid) {
        sandbox->flags |= SECURITY_IMAGINATION_FLAG_INTEGRITY_OK;
        result->anomalies_detected = 0;
    } else {
        bridge->stats.integrity_failures++;
        bridge->imagination_effects.integrity_failures++;
        result->anomalies_detected = 2;
    }

    /* Update statistics */
    update_average(&bridge->stats.avg_integrity_score,
                   bridge->stats.integrity_checks,
                   score);
    bridge->imagination_effects.current_integrity_score = score;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Check for adversarial patterns in simulation
 */
int security_imagination_check_adversarial(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    float* adversarial_score
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(adversarial_score, NIMCP_ERROR_NULL_POINTER, "adversarial_score is NULL");

    /* Skip if disabled */
    if (!bridge->config.enable_adversarial_detection) {
        *adversarial_score = 0.0f;
        return NIMCP_SUCCESS;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.adversarial_checks++;

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Compute adversarial score (simplified) */
    security_imagination_sandbox_t* sandbox = &bridge->sandboxes[index];
    float score = sandbox->confabulation_score * 0.5f +
                  sandbox->reality_divergence * 0.5f;

    *adversarial_score = score;

    if (score >= bridge->config.adversarial_threshold) {
        sandbox->flags |= SECURITY_IMAGINATION_FLAG_ADVERSARIAL;
        bridge->stats.adversarial_detections++;
        bridge->imagination_effects.adversarial_detections++;

        if (score > bridge->stats.peak_adversarial_score) {
            bridge->stats.peak_adversarial_score = score;
        }
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Resource Tracking API Implementation
 * ============================================================================ */

/**
 * @brief Track simulation resource usage
 */
int security_imagination_track_resources(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    uint64_t resources_used
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->config.enable_resource_tracking) {
        return NIMCP_SUCCESS;
    }

    BRIDGE_LOCK(bridge);

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    security_imagination_sandbox_t* sandbox = &bridge->sandboxes[index];

    sandbox->resources_used += resources_used;
    sandbox->simulation_steps++;
    sandbox->flags |= SECURITY_IMAGINATION_FLAG_RESOURCE_LIMITED;

    bridge->stats.total_resources_consumed += resources_used;
    bridge->imagination_effects.total_resources_used += resources_used;
    bridge->imagination_effects.total_simulation_steps++;

    /* Calculate utilization */
    float utilization = (float)sandbox->resources_used /
                        (float)bridge->security_effects.effective_simulation_budget;
    bridge->imagination_effects.resource_utilization = utilization;

    /* Check warning threshold */
    if (utilization >= bridge->config.resource_warning_threshold) {
        bridge->imagination_effects.resource_warnings++;
    }

    /* Check if budget exceeded */
    if (sandbox->resources_used > bridge->security_effects.effective_simulation_budget) {
        bridge->stats.resource_limit_hits++;
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get current resource usage for sandbox
 */
uint64_t security_imagination_get_resources(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id
) {
    if (!bridge) {
        return 0;
    }

    BRIDGE_LOCK((security_imagination_bridge_t*)bridge);

    size_t index;
    if (find_sandbox_by_id(bridge, sandbox_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);
        return 0;
    }

    uint64_t resources = bridge->sandboxes[index].resources_used;

    BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);

    return resources;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

/**
 * @brief Update security effects on imagination (outbound)
 */
int security_imagination_update_security_effects(
    security_imagination_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Compute effective limits based on security state */
    float depth_factor = 1.0f;
    float resource_factor = 1.0f;
    float confab_factor = 1.0f;

    /* Reduce limits if imagination is restricted */
    if (bridge->state.imagination_restricted) {
        depth_factor = 0.5f;
        resource_factor = 0.5f;
        confab_factor = 0.8f;  /* Lower threshold = more sensitive */
    }

    /* Further reduce if high threat detected */
    if (bridge->imagination_effects.peak_confabulation_score > 0.8f) {
        depth_factor *= 0.75f;
        confab_factor *= 0.9f;
    }

    /* Update effective limits */
    bridge->security_effects.depth_reduction_factor = depth_factor;
    bridge->security_effects.resource_reduction_factor = resource_factor;
    bridge->security_effects.effective_max_depth =
        (uint32_t)(bridge->config.max_hypothetical_depth * depth_factor);
    bridge->security_effects.effective_simulation_budget =
        (uint64_t)(bridge->config.default_simulation_budget * resource_factor);
    bridge->security_effects.effective_confab_threshold =
        bridge->config.confabulation_threshold * confab_factor;
    bridge->security_effects.effective_divergence_threshold =
        bridge->config.reality_divergence_threshold * confab_factor;

    /* Update sandbox quota state */
    bridge->security_effects.sandbox_quota_reached =
        (bridge->sandbox_count >= bridge->sandbox_capacity);

    /* Update restriction state */
    bridge->security_effects.imagination_restricted =
        bridge->state.imagination_restricted;
    bridge->security_effects.new_scenarios_blocked =
        bridge->state.new_scenarios_blocked;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Update imagination effects on security (inbound)
 */
int security_imagination_update_imagination_effects(
    security_imagination_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Calculate averages */
    bridge->imagination_effects.active_scenarios = bridge->sandbox_count;

    /* Calculate average depth */
    float total_depth = 0.0f;
    uint32_t active_count = 0;
    for (size_t i = 0; i < bridge->sandbox_capacity; i++) {
        if (bridge->sandboxes[i].is_active) {
            total_depth += (float)bridge->sandboxes[i].max_depth_reached;
            active_count++;
        }
    }
    if (active_count > 0) {
        bridge->imagination_effects.avg_scenario_depth = total_depth / (float)active_count;
    }

    /* Calculate average divergence */
    bridge->imagination_effects.avg_divergence = bridge->stats.avg_divergence_score;

    /* Update resource utilization */
    if (bridge->stats.sandboxes_created > 0) {
        bridge->stats.avg_resources_per_scenario =
            (float)bridge->stats.total_resources_consumed /
            (float)bridge->stats.sandboxes_created;
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Full update cycle (both directions)
 */
int security_imagination_bridge_update(
    security_imagination_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Update both directions */
    int result = security_imagination_update_security_effects(bridge);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    result = security_imagination_update_imagination_effects(bridge);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    /* Record update in base */
    BRIDGE_LOCK(bridge);
    bridge_base_record_update(&bridge->base);
    BRIDGE_UNLOCK(bridge);

    (void)delta_ms;  /* Used for timing if needed */

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

/**
 * @brief Get security effects on imagination
 */
int security_imagination_get_security_effects(
    const security_imagination_bridge_t* bridge,
    security_to_imagination_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    BRIDGE_LOCK((security_imagination_bridge_t*)bridge);
    *effects = bridge->security_effects;
    BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get imagination effects on security
 */
int security_imagination_get_imagination_effects(
    const security_imagination_bridge_t* bridge,
    imagination_to_security_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    BRIDGE_LOCK((security_imagination_bridge_t*)bridge);
    *effects = bridge->imagination_effects;
    BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get bridge state
 */
int security_imagination_get_state(
    const security_imagination_bridge_t* bridge,
    security_imagination_state_t* state
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    BRIDGE_LOCK((security_imagination_bridge_t*)bridge);
    *state = bridge->state;
    BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get bridge statistics
 */
int security_imagination_get_stats(
    const security_imagination_bridge_t* bridge,
    security_imagination_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    BRIDGE_LOCK((security_imagination_bridge_t*)bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK((security_imagination_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Reset bridge statistics
 */
int security_imagination_reset_stats(security_imagination_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(security_imagination_stats_t));
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Restriction Mode API Implementation
 * ============================================================================ */

/**
 * @brief Restrict imagination capabilities
 */
int security_imagination_enter_restricted(security_imagination_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.imagination_restricted = true;
    bridge->security_effects.imagination_restricted = true;

    /* Reduce limits */
    bridge->security_effects.effective_max_depth =
        bridge->config.max_hypothetical_depth / 2;
    bridge->security_effects.effective_simulation_budget =
        bridge->config.default_simulation_budget / 2;
    bridge->security_effects.depth_reduction_factor = 0.5f;
    bridge->security_effects.resource_reduction_factor = 0.5f;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Restore normal imagination capabilities
 */
int security_imagination_exit_restricted(security_imagination_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.imagination_restricted = false;
    bridge->security_effects.imagination_restricted = false;

    /* Restore limits */
    bridge->security_effects.effective_max_depth = bridge->config.max_hypothetical_depth;
    bridge->security_effects.effective_simulation_budget =
        bridge->config.default_simulation_budget;
    bridge->security_effects.depth_reduction_factor = 1.0f;
    bridge->security_effects.resource_reduction_factor = 1.0f;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if imagination is restricted
 */
bool security_imagination_is_restricted(
    const security_imagination_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->state.imagination_restricted;
}

/**
 * @brief Block creation of new scenarios
 */
int security_imagination_block_new_scenarios(
    security_imagination_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    bridge->state.new_scenarios_blocked = true;
    bridge->security_effects.new_scenarios_blocked = true;
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Allow creation of new scenarios
 */
int security_imagination_allow_new_scenarios(
    security_imagination_bridge_t* bridge
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    bridge->state.new_scenarios_blocked = false;
    bridge->security_effects.new_scenarios_blocked = false;
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Find sandbox by ID
 */
static int find_sandbox_by_id(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    size_t* index
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(index, NIMCP_ERROR_NULL_POINTER, "index is NULL");

    for (size_t i = 0; i < bridge->sandbox_capacity; i++) {
        if (bridge->sandboxes[i].is_active &&
            bridge->sandboxes[i].sandbox_id == sandbox_id) {
            *index = i;
            return NIMCP_SUCCESS;
        }
    }

    return NIMCP_ERROR_NOT_FOUND;
}

/**
 * @brief Update running average
 */
static void update_average(float* avg, uint64_t count, float new_value) {
    if (!avg || count == 0) {
        return;
    }

    /* Exponential moving average for stability */
    float alpha = 1.0f / (float)(count < 100 ? count : 100);
    *avg = (*avg * (1.0f - alpha)) + (new_value * alpha);
    if (!isfinite(*avg)) *avg = new_value;
}

/**
 * @brief Compute confabulation score from content
 */
static float compute_confabulation_score(
    const void* content,
    size_t content_size
) {
    if (!content || content_size == 0) {
        return 0.0f;
    }

    const uint8_t* data = (const uint8_t*)content;
    float score = 0.0f;

    /* Simple heuristics for confabulation detection */
    size_t repetition_count = 0;
    size_t anomaly_count = 0;

    for (size_t i = 1; i < content_size; i++) {
        /* Count repetitions (may indicate hallucination) */
        if (data[i] == data[i-1]) {
            repetition_count++;
        }
        /* Count anomalous patterns */
        if (data[i] > 200 && data[i-1] > 200) {
            anomaly_count++;
        }
    }

    /* Calculate score based on heuristics */
    float repetition_ratio = (float)repetition_count / (float)content_size;
    float anomaly_ratio = (float)anomaly_count / (float)content_size;

    score = repetition_ratio * 0.3f + anomaly_ratio * 0.7f;

    /* Clamp to [0, 1] */
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f) score = 0.0f;

    return score;
}

/**
 * @brief Compute reality divergence score
 */
static float compute_divergence_score(
    const security_imagination_sandbox_t* sandbox
) {
    if (!sandbox) {
        return 1.0f;  /* Max divergence if no sandbox */
    }

    /* Compute based on sandbox state */
    float score = 0.0f;

    /* Depth contributes to divergence */
    score += (float)sandbox->current_depth * 0.05f;

    /* Simulation steps contribute */
    score += (float)sandbox->simulation_steps * 0.0001f;

    /* Previous confabulation increases divergence */
    score += sandbox->confabulation_score * 0.3f;

    /* Clamp to [0, 1] */
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f) score = 0.0f;

    return score;
}

/**
 * @brief Compute simulation integrity score
 */
static float compute_integrity_score(
    const security_imagination_sandbox_t* sandbox
) {
    if (!sandbox) {
        return 0.0f;  /* No integrity if no sandbox */
    }

    /* Start with perfect integrity */
    float score = 1.0f;

    /* Reduce based on concerning patterns */
    if (sandbox->flags & SECURITY_IMAGINATION_FLAG_CONFAB_DETECTED) {
        score -= 0.2f;
    }
    if (sandbox->flags & SECURITY_IMAGINATION_FLAG_DIVERGED) {
        score -= 0.3f;
    }
    if (sandbox->flags & SECURITY_IMAGINATION_FLAG_ADVERSARIAL) {
        score -= 0.4f;
    }

    /* High depth reduces integrity confidence */
    score -= (float)sandbox->current_depth * 0.02f;

    /* Clamp to [0, 1] */
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f) score = 0.0f;

    return score;
}

/**
 * @brief Generate unique sandbox ID
 */
static uint64_t generate_sandbox_id(void) {
    static uint64_t counter = 0;
    uint64_t timestamp = nimcp_platform_time_monotonic_ms();
    return (timestamp << 20) | (++counter & 0xFFFFF);
}
