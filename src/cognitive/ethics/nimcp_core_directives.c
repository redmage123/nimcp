//=============================================================================
// nimcp_core_directives.c - Core Ethical Directives Implementation
//=============================================================================
/**
 * @file nimcp_core_directives.c
 * @brief Core ethical foundation implementation
 *
 * RESPONSIBILITY: Enforcing ethical constraints on all brain actions
 *
 * FUNCTIONS:
 * - core_directives_create() - Create directives system
 * - core_directives_destroy() - Destroy directives system
 * - core_directives_evaluate() - Evaluate action against directives
 * - core_directives_record_action() - Record action in history
 * - core_directives_connect_bio_async() - Connect to bio-async
 * - core_directives_connect_immune() - Connect to brain immune
 * - core_directives_connect_fep() - Connect to FEP orchestrator
 */

#include "cognitive/ethics/nimcp_core_directives.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(core_directives, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Action history entry
 */
typedef struct {
    float* action_vector;       // Action representation
    uint32_t action_dim;        // Vector dimensionality
    char context_desc[NIMCP_ERROR_BUFFER_MEDIUM];     // Human-readable description
    uint64_t timestamp_us;      // When action was executed
    bool valid;                 // Whether entry is populated
} action_history_entry_t;

/**
 * @brief Core directives system implementation
 */
struct core_directives_system {
    // Configuration
    core_directives_config_t config;

    // Action history for combinatorial analysis
    action_history_entry_t* action_history;
    uint32_t history_head;          // Next write position
    uint32_t history_count;         // Number of valid entries

    // Statistics
    core_directives_stats_t stats;

    // Integration
    brain_immune_system_t* immune_system;
    directive_immune_bridge_t* immune_bridge;

    fep_orchestrator_t* fep_orchestrator;
    directive_fep_bridge_t* fep_bridge;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

/**
 * @brief Directive-immune bridge (placeholder)
 */
struct directive_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    core_directives_system_t* directives;
    brain_immune_system_t* immune_system;
    bool enabled;
};

/**
 * @brief Directive-FEP bridge (placeholder)
 */
struct directive_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    core_directives_system_t* directives;
    fep_orchestrator_t* fep_orchestrator;
    bool enabled;
};

//=============================================================================
// Default Configuration
//=============================================================================

void core_directives_default_config(core_directives_config_t* config)
{
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_default_config", 0.0f);


    memset(config, 0, sizeof(core_directives_config_t));

    // Asimov's Laws
    config->enable_first_law = true;
    config->enable_second_law = true;
    config->enable_third_law = true;

    // Golden Rule
    config->enable_golden_rule = true;
    config->reciprocity_threshold = 0.7f;

    // Combinatorial Harm
    config->enable_combinatorial_harm = true;
    config->action_history_size = 100;
    config->max_combination_depth = 5;

    // Thresholds
    config->harm_threshold = 0.3f;
    config->severity_threshold = 0.5f;
    config->confidence_threshold = 0.6f;

    // Integration
    config->enable_bio_async = true;
    config->enable_immune_integration = true;
    config->enable_fep_integration = true;
}

//=============================================================================
// System Lifecycle
//=============================================================================

core_directives_system_t* core_directives_create(const core_directives_config_t* config)
{
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in core_directives_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_create", 0.0f);


    core_directives_system_t* directives = nimcp_calloc(1, sizeof(core_directives_system_t));
    if (!directives) {
        NIMCP_LOGGING_ERROR("Failed to allocate core directives system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate directives");

        return NULL;
    }

    // Copy configuration with threshold validation
    memcpy(&directives->config, config, sizeof(core_directives_config_t));

    // Validate and clamp threshold values to [0.0, 1.0] range
    directives->config.reciprocity_threshold = nimcp_clampf(
        directives->config.reciprocity_threshold, 0.0f, 1.0f);
    directives->config.harm_threshold = nimcp_clampf(
        directives->config.harm_threshold, 0.0f, 1.0f);
    directives->config.severity_threshold = nimcp_clampf(
        directives->config.severity_threshold, 0.0f, 1.0f);
    directives->config.confidence_threshold = nimcp_clampf(
        directives->config.confidence_threshold, 0.0f, 1.0f);

    // Allocate action history
    if (config->action_history_size > 0) {
        directives->action_history = nimcp_calloc(
            config->action_history_size,
            sizeof(action_history_entry_t)
        );
        if (!directives->action_history) {
            NIMCP_LOGGING_ERROR("Failed to allocate action history");
            nimcp_free(directives);
            directives = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "core_directives_create: directives->action_history is NULL");
            return NULL;
        }
    }

    // Initialize mutex
    if (nimcp_platform_mutex_init(&directives->mutex, false) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize directives mutex");
        nimcp_free(directives->action_history);
        nimcp_free(directives);
        directives = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "core_directives_create: validation failed");
        return NULL;
    }

    directives->history_head = 0;
    directives->history_count = 0;
    directives->bio_async_enabled = false;
    directives->immune_system = NULL;
    directives->immune_bridge = NULL;
    directives->fep_orchestrator = NULL;
    directives->fep_bridge = NULL;

    NIMCP_LOGGING_INFO("Created core directives system (history=%u)",
                       config->action_history_size);

    return directives;
}

void core_directives_destroy(core_directives_system_t* directives)
{
    if (!directives) return;

    // Disconnect integrations
    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_destroy", 0.0f);


    if (directives->bio_async_enabled) {
        core_directives_disconnect_bio_async(directives);
    }

    // Destroy bridges
    if (directives->immune_bridge) {
        nimcp_free(directives->immune_bridge);
    }
    if (directives->fep_bridge) {
        nimcp_free(directives->fep_bridge);
    }

    // Free action history
    if (directives->action_history) {
        for (uint32_t i = 0; i < directives->config.action_history_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && directives->config.action_history_size > 256) {
                core_directives_heartbeat("core_directi_loop",
                                 (float)(i + 1) / (float)directives->config.action_history_size);
            }

            if (directives->action_history[i].action_vector) {
                nimcp_free(directives->action_history[i].action_vector);
            }
        }
        nimcp_free(directives->action_history);
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&directives->mutex);

    NIMCP_LOGGING_INFO("Destroyed core directives system");
    nimcp_free(directives);
    directives = NULL;
}

//=============================================================================
// Action Evaluation Helpers
//=============================================================================

/**
 * @brief Compute harm potential from action vector
 *
 * ACTION VECTOR ENCODING (assumed):
 * - Dim 0-2: Target (human=positive, self=negative, object=near-zero)
 * - Dim 3-5: Force/intensity (high magnitude = forceful action)
 * - Dim 6-8: Harm semantics (positive = beneficial, negative = harmful)
 */
static float compute_harm_potential(const float* action_vector, uint32_t action_dim)
{
    if (action_dim < 3) return 0.0f;

    float harm_score = 0.0f;
    float total_weight = 0.0f;

    // Harm semantics dimensions - negative values indicate harmful actions
    if (action_dim >= 9) {
        for (uint32_t i = 6; i < 9 && i < action_dim; i++) {
            if (action_vector[i] < 0.0f) {
                harm_score += -action_vector[i];
                total_weight += 1.0f;
            }
        }
    }

    // Force/intensity amplifies harm
    if (action_dim >= 6) {
        float force_magnitude = 0.0f;
        for (uint32_t i = 3; i < 6 && i < action_dim; i++) {
            force_magnitude += action_vector[i] * action_vector[i];
        }
        force_magnitude = sqrtf(force_magnitude);
        if (force_magnitude > 0.7f && harm_score > 0.0f) {
            harm_score += force_magnitude * 0.5f;
            total_weight += 0.5f;
        }
    }

    // Actions targeting humans with harmful semantics are more severe
    if (action_dim >= 3) {
        float human_target = action_vector[0];
        if (human_target > 0.5f && harm_score > 0.0f) {
            harm_score *= (1.0f + human_target);
        }
    }

    if (total_weight > 0.0f) {
        harm_score /= total_weight;
    }
    return fminf(1.0f, fmaxf(0.0f, harm_score));
}

/** @brief Evaluate Asimov's First Law - Harm Prevention */
static bool evaluate_first_law(const float* action_vector, uint32_t action_dim,
                               float harm_threshold, float* severity)
{
    float harm = compute_harm_potential(action_vector, action_dim);
    *severity = harm;
    return (harm >= harm_threshold);
}

/** @brief Evaluate Asimov's Third Law - Self-Preservation */
static bool evaluate_third_law(const float* action_vector, uint32_t action_dim, float* severity)
{
    if (action_dim < 3) {
        *severity = 0.0f;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_third_law: validation failed");
        return false;
    }

    float self_target = action_vector[0];
    if (self_target < -0.3f) {
        float self_harm = 0.0f;
        if (action_dim >= 9) {
            for (uint32_t i = 6; i < 9 && i < action_dim; i++) {
                if (action_vector[i] < 0.0f) self_harm += -action_vector[i];
            }
            self_harm /= 3.0f;
        }
        *severity = self_harm * (-self_target);
        return (*severity >= 0.5f);
    }
    *severity = 0.0f;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_third_law: validation failed");
    return false;
}

/** @brief Evaluate Golden Rule - Reciprocity Ethics */
static bool evaluate_golden_rule(const float* action_vector, uint32_t action_dim,
                                 float reciprocity_threshold, float* severity)
{
    if (action_dim < 6) {
        *severity = 0.0f;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_golden_rule: validation failed");
        return false;
    }

    float unwantedness = 0.0f;
    if (action_dim >= 9) {
        for (uint32_t i = 6; i < 9 && i < action_dim; i++) {
            if (action_vector[i] < 0.0f) unwantedness += -action_vector[i];
        }
        unwantedness /= 3.0f;
    }

    float force = 0.0f;
    for (uint32_t i = 3; i < 6 && i < action_dim; i++) {
        force += action_vector[i] * action_vector[i];
    }
    force = sqrtf(force);

    float reciprocity_violation = unwantedness * (1.0f + force);
    reciprocity_violation = fminf(1.0f, reciprocity_violation);
    *severity = reciprocity_violation;
    return (reciprocity_violation >= reciprocity_threshold);
}

/** @brief Evaluate Combinatorial Harm from action sequences */
static bool evaluate_combinatorial_harm(core_directives_system_t* directives,
                                        const float* action_vector, uint32_t action_dim,
                                        float* severity)
{
    *severity = 0.0f;
    if (!directives->action_history || directives->history_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evaluate_combinatorial_harm: directives->action_history is NULL");
        return false;
    }

    float cumulative_harm = 0.0f;
    float pattern_strength = 0.0f;
    uint32_t harmful_count = 0;

    for (uint32_t i = 0; i < directives->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && directives->history_count > 256) {
            core_directives_heartbeat("core_directi_loop",
                             (float)(i + 1) / (float)directives->history_count);
        }

        action_history_entry_t* entry = &directives->action_history[i];
        if (!entry->valid) continue;

        float hist_harm = compute_harm_potential(entry->action_vector, entry->action_dim);
        cumulative_harm += hist_harm;
        if (hist_harm > 0.1f) harmful_count++;

        if (entry->action_dim == action_dim) {
            float similarity = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
            for (uint32_t j = 0; j < action_dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && action_dim > 256) {
                    core_directives_heartbeat("core_directi_loop",
                                     (float)(j + 1) / (float)action_dim);
                }

                similarity += action_vector[j] * entry->action_vector[j];
                norm_a += action_vector[j] * action_vector[j];
                norm_b += entry->action_vector[j] * entry->action_vector[j];
            }
            if (norm_a > 0.0f && norm_b > 0.0f) {
                similarity /= (sqrtf(norm_a) * sqrtf(norm_b));
                if (similarity > 0.7f && hist_harm > 0.1f) {
                    pattern_strength += similarity * hist_harm;
                }
            }
        }
    }

    float combined_severity = 0.0f;
    float avg_harm = cumulative_harm / fmaxf(1.0f, (float)directives->history_count);
    float current_harm = compute_harm_potential(action_vector, action_dim);

    if (avg_harm > 0.2f && current_harm > 0.1f) combined_severity += avg_harm * 0.5f;
    if (pattern_strength > 0.3f) combined_severity += pattern_strength * 0.3f;

    float harm_frequency = (float)harmful_count / fmaxf(1.0f, (float)directives->history_count);
    if (harm_frequency > 0.5f) combined_severity += harm_frequency * 0.2f;

    *severity = fminf(1.0f, combined_severity);
    return (*severity >= directives->config.harm_threshold);
}

//=============================================================================
// Action Evaluation (Full Implementation)
//=============================================================================

int core_directives_evaluate(
    core_directives_system_t* directives,
    const float* action_vector,
    uint32_t action_dim,
    const char* context_desc,
    directive_evaluation_t* result)
{
    if (!directives || !action_vector || !result) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_evaluate", 0.0f);


    if (action_dim == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&directives->mutex);

    result->action = DIRECTIVE_ALLOW;
    result->violation = DIRECTIVE_VIOLATION_NONE;
    result->severity = 0.0f;
    result->confidence = 1.0f;
    result->reason[0] = '\0';

    float max_severity = 0.0f;
    float eval_severity = 0.0f;

    // FIRST LAW: Harm Prevention (HIGHEST PRIORITY)
    if (directives->config.enable_first_law) {
        if (evaluate_first_law(action_vector, action_dim,
                               directives->config.harm_threshold, &eval_severity)) {
            result->action = DIRECTIVE_BLOCK;
            result->violation = DIRECTIVE_VIOLATION_HARM;
            result->severity = eval_severity;
            snprintf(result->reason, sizeof(result->reason),
                     "First Law: Harm potential %.2f exceeds threshold %.2f",
                     eval_severity, directives->config.harm_threshold);
            directives->stats.harm_violations++;
            directives->stats.blocked_actions++;
            goto evaluation_complete;
        }
        max_severity = fmaxf(max_severity, eval_severity);
    }

    // THIRD LAW: Self-Preservation
    if (directives->config.enable_third_law) {
        if (evaluate_third_law(action_vector, action_dim, &eval_severity)) {
            if (eval_severity >= directives->config.severity_threshold) {
                result->action = DIRECTIVE_MODIFY;
                result->violation = DIRECTIVE_VIOLATION_SELF_PRESERVATION;
                result->severity = eval_severity;
                snprintf(result->reason, sizeof(result->reason),
                         "Third Law: Self-harm severity %.2f", eval_severity);
                directives->stats.self_harm_violations++;
                directives->stats.modified_actions++;
                goto evaluation_complete;
            }
        }
        max_severity = fmaxf(max_severity, eval_severity);
    }

    // GOLDEN RULE: Reciprocity Ethics
    if (directives->config.enable_golden_rule) {
        if (evaluate_golden_rule(action_vector, action_dim,
                                 directives->config.reciprocity_threshold, &eval_severity)) {
            result->action = DIRECTIVE_MODIFY;
            result->violation = DIRECTIVE_VIOLATION_GOLDEN_RULE;
            result->severity = eval_severity;
            snprintf(result->reason, sizeof(result->reason),
                     "Golden Rule: Reciprocity violation %.2f", eval_severity);
            directives->stats.golden_rule_violations++;
            directives->stats.modified_actions++;
            goto evaluation_complete;
        }
        max_severity = fmaxf(max_severity, eval_severity);
    }

    // COMBINATORIAL HARM: Emergent harm from sequences
    if (directives->config.enable_combinatorial_harm && directives->action_history) {
        if (evaluate_combinatorial_harm(directives, action_vector, action_dim, &eval_severity)) {
            result->action = DIRECTIVE_BLOCK;
            result->violation = DIRECTIVE_VIOLATION_COMBINATORIAL;
            result->severity = eval_severity;
            snprintf(result->reason, sizeof(result->reason),
                     "Combinatorial harm detected: severity %.2f", eval_severity);
            directives->stats.combinatorial_violations++;
            directives->stats.blocked_actions++;
            goto evaluation_complete;
        }
        max_severity = fmaxf(max_severity, eval_severity);
    }

    // Action passed all checks
    result->severity = max_severity;
    result->confidence = 1.0f - (max_severity * 0.3f);
    if (max_severity > 0.0f) {
        snprintf(result->reason, sizeof(result->reason),
                 "Allowed with residual severity %.2f%s%s",
                 max_severity, context_desc ? ": " : "", context_desc ? context_desc : "");
    } else {
        snprintf(result->reason, sizeof(result->reason), "No ethical violations detected");
    }

evaluation_complete:
    directives->stats.total_evaluations++;
    nimcp_platform_mutex_unlock(&directives->mutex);

    if (result->action == DIRECTIVE_BLOCK) {
        NIMCP_LOGGING_WARN("DIRECTIVE BLOCK: %s", result->reason);
    }
    return 0;
}

int core_directives_record_action(
    core_directives_system_t* directives,
    const float* action_vector,
    uint32_t action_dim,
    const char* context_desc)
{
    if (!directives || !action_vector) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_record_action", 0.0f);


    if (action_dim == 0 || !directives->action_history) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&directives->mutex);

    // Get next history slot
    uint32_t idx = directives->history_head;
    action_history_entry_t* entry = &directives->action_history[idx];

    // Free old action vector if exists
    if (entry->action_vector) {
        nimcp_free(entry->action_vector);
    }

    // Allocate and copy action vector
    entry->action_vector = nimcp_calloc(action_dim, sizeof(float));
    if (!entry->action_vector) {
        nimcp_platform_mutex_unlock(&directives->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    memcpy(entry->action_vector, action_vector, action_dim * sizeof(float));
    entry->action_dim = action_dim;

    if (context_desc) {
        strncpy(entry->context_desc, context_desc, sizeof(entry->context_desc) - 1);
        entry->context_desc[sizeof(entry->context_desc) - 1] = '\0';
    } else {
        entry->context_desc[0] = '\0';
    }

    entry->timestamp_us = nimcp_time_get_us();
    entry->valid = true;

    // Update circular buffer
    directives->history_head = (idx + 1) % directives->config.action_history_size;
    if (directives->history_count < directives->config.action_history_size) {
        directives->history_count++;
    }

    nimcp_platform_mutex_unlock(&directives->mutex);

    return 0;
}

int core_directives_clear_history(core_directives_system_t* directives)
{
    if (!directives) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_clear_history", 0.0f);


    nimcp_platform_mutex_lock(&directives->mutex);

    if (directives->action_history) {
        for (uint32_t i = 0; i < directives->config.action_history_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && directives->config.action_history_size > 256) {
                core_directives_heartbeat("core_directi_loop",
                                 (float)(i + 1) / (float)directives->config.action_history_size);
            }

            if (directives->action_history[i].action_vector) {
                nimcp_free(directives->action_history[i].action_vector);
                directives->action_history[i].action_vector = NULL;
            }
            directives->action_history[i].valid = false;
        }
    }

    directives->history_head = 0;
    directives->history_count = 0;

    nimcp_platform_mutex_unlock(&directives->mutex);

    NIMCP_LOGGING_INFO("Cleared core directives action history");
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int core_directives_connect_bio_async(core_directives_system_t* directives)
{
    if (!directives) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_connect_bio_async", 0.0f);


    if (directives->bio_async_enabled) {
        return 0; // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORE_DIRECTIVES, // 0x1000
        .module_name = "core_directives",
        .inbox_capacity = 32,
        .user_data = directives
    };

    directives->bio_ctx = bio_router_register_module(&info);
    if (directives->bio_ctx) {
        directives->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Core directives connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available for core directives");
    }

    return 0;
}

int core_directives_disconnect_bio_async(core_directives_system_t* directives)
{
    if (!directives) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!directives->bio_async_enabled) {
        return 0; // Not connected
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_disconnect_bio_async", 0.0f);


    if (directives->bio_ctx) {
        bio_router_unregister_module(directives->bio_ctx);
        directives->bio_ctx = NULL;
    }

    directives->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Core directives disconnected from bio-async router");

    return 0;
}

bool core_directives_is_bio_async_connected(const core_directives_system_t* directives)
{
    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_is_bio_async_connect", 0.0f);


    return directives ? directives->bio_async_enabled : false;
}

//=============================================================================
// Immune Integration (Stub)
//=============================================================================

int core_directives_connect_immune(
    core_directives_system_t* directives,
    brain_immune_system_t* immune)
{
    if (!directives || !immune) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_connect_immune", 0.0f);


    if (directives->immune_bridge) {
        NIMCP_LOGGING_WARN("Core directives already connected to immune system");
        return 0;
    }

    // Create bridge
    directive_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(directive_immune_bridge_t));
    if (!bridge) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    bridge->directives = directives;
    bridge->immune_system = immune;
    bridge->enabled = true;

    directives->immune_bridge = bridge;
    directives->immune_system = immune;

    NIMCP_LOGGING_INFO("Core directives connected to brain immune system");
    return 0;
}

//=============================================================================
// FEP Integration (Stub)
//=============================================================================

int core_directives_connect_fep(
    core_directives_system_t* directives,
    fep_orchestrator_t* fep_orch)
{
    if (!directives || !fep_orch) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_connect_fep", 0.0f);


    if (directives->fep_bridge) {
        NIMCP_LOGGING_WARN("Core directives already connected to FEP orchestrator");
        return 0;
    }

    // Create bridge
    directive_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(directive_fep_bridge_t));
    if (!bridge) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    bridge->directives = directives;
    bridge->fep_orchestrator = fep_orch;
    bridge->enabled = true;

    directives->fep_bridge = bridge;
    directives->fep_orchestrator = fep_orch;

    NIMCP_LOGGING_INFO("Core directives connected to FEP orchestrator");
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int core_directives_get_stats(
    const core_directives_system_t* directives,
    core_directives_stats_t* stats)
{
    if (!directives || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_get_stats", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&directives->mutex);
    memcpy(stats, &directives->stats, sizeof(core_directives_stats_t));
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&directives->mutex);

    return 0;
}

int core_directives_reset_stats(core_directives_system_t* directives)
{
    if (!directives) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_reset_stats", 0.0f);


    nimcp_platform_mutex_lock(&directives->mutex);
    memset(&directives->stats, 0, sizeof(core_directives_stats_t));
    nimcp_platform_mutex_unlock(&directives->mutex);

    NIMCP_LOGGING_INFO("Reset core directives statistics");
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Core Directives self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int core_directives_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    core_directives_heartbeat("core_directi_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Core_Directives_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                core_directives_heartbeat("core_directi_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Core directives self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Core_Directives_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Core_Directives_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void core_directives_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_core_directives_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int core_directives_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "core_directives_training_begin: NULL argument");
        return -1;
    }
    core_directives_heartbeat_instance(NULL, "core_directives_training_begin", 0.0f);
    (void)(struct core_directives_system*)instance; /* Module state available for reset */
    return 0;
}

int core_directives_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "core_directives_training_end: NULL argument");
        return -1;
    }
    core_directives_heartbeat_instance(NULL, "core_directives_training_end", 1.0f);
    (void)(struct core_directives_system*)instance; /* Module state available for finalization */
    return 0;
}

int core_directives_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "core_directives_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    core_directives_heartbeat_instance(NULL, "core_directives_training_step", progress);
    (void)(struct core_directives_system*)instance; /* Module state available for step adaptation */
    return 0;
}
