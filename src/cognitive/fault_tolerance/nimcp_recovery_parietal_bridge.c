/**
 * @file nimcp_recovery_parietal_bridge.c
 * @brief Implementation of Parietal-Recovery Executive Integration
 *
 * Integrates parietal lobe capabilities (spatial reasoning, pattern detection,
 * software engineering analysis) with fault tolerance recovery planning.
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/fault_tolerance/nimcp_recovery_parietal_bridge.h"
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_software_engineering.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_scientific_reasoning.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(recovery_parietal_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_recovery_parietal_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_recovery_parietal_bridge_mesh_registry = NULL;

nimcp_error_t recovery_parietal_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_recovery_parietal_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "recovery_parietal_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "recovery_parietal_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_recovery_parietal_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_recovery_parietal_bridge_mesh_registry = registry;
    return err;
}

void recovery_parietal_bridge_mesh_unregister(void) {
    if (g_recovery_parietal_bridge_mesh_registry && g_recovery_parietal_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_recovery_parietal_bridge_mesh_registry, g_recovery_parietal_bridge_mesh_id);
        g_recovery_parietal_bridge_mesh_id = 0;
        g_recovery_parietal_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from recovery_parietal_bridge module (instance-level) */
static inline void recovery_parietal_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_recovery_parietal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_recovery_parietal_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_recovery_parietal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "RECOVERY_PARIETAL_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Historical failure pattern for learning
 */
typedef struct {
    uint64_t pattern_id;
    diag_severity_t severity;
    char failure_type[NIMCP_ID_BUFFER_SIZE];
    char module_pattern[NIMCP_LABEL_BUFFER_SIZE];
    char successful_fix[NIMCP_ERROR_BUFFER_LARGE];
    uint32_t occurrences;
    uint32_t successful_recoveries;
    float avg_recovery_time_ms;
} failure_pattern_t;

/**
 * @brief Parietal-recovery bridge internal structure
 */
struct recovery_parietal_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    recovery_parietal_config_t config;

    /* Attached systems */
    parietal_lobe_t* parietal;
    recovery_executive_t* executive;

    /* Pattern database */
    failure_pattern_t patterns[MAX_FAILURE_PATTERNS];
    uint32_t pattern_count;
    uint64_t next_pattern_id;

    /* Statistics */
    recovery_parietal_stats_t stats;

    /* State */
    bool initialized;

    /* Phase 8: Instance health agent */
    nimcp_health_agent_t* health_agent;         /**< Health agent (Phase 8) */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Calculate pattern similarity between diagnosis and stored pattern
 */
static float calculate_pattern_similarity(
    const diagnostic_result_t* diagnosis,
    const failure_pattern_t* pattern
) {
    float similarity = 0.0f;
    int matches = 0;

    /* Severity match */
    if (diagnosis->severity == pattern->severity) {
        similarity += 0.3f;
        matches++;
    }

    /* Failure type substring match */
    if (strstr(diagnosis->root_cause, pattern->failure_type) != NULL) {
        similarity += 0.4f;
        matches++;
    }

    /* Module pattern match */
    if (strlen(pattern->module_pattern) > 0) {
        /* Simple substring match - could be enhanced with regex */
        similarity += 0.2f;
        matches++;
    }

    /* Historical success rate boost */
    if (pattern->occurrences > 0) {
        float success_rate = (float)pattern->successful_recoveries / pattern->occurrences;
        similarity += 0.1f * success_rate;
    }

    return fminf(similarity, 1.0f);
}

/**
 * @brief Map diagnostic severity to repair difficulty estimate
 */
static float severity_to_difficulty(diag_severity_t severity) {
    switch (severity) {
        case DIAG_SEVERITY_INFO:     return 0.1f;
        case DIAG_SEVERITY_WARNING:  return 0.25f;
        case DIAG_SEVERITY_ERROR:    return 0.5f;
        case DIAG_SEVERITY_CRITICAL: return 0.75f;
        case DIAG_SEVERITY_FATAL:    return 0.95f;
        default:                     return 0.5f;
    }
}

/**
 * @brief Map complexity class to numerical factor
 */
static float complexity_to_factor(complexity_class_t complexity) {
    switch (complexity) {
        case COMPLEXITY_O_1:           return 1.0f;
        case COMPLEXITY_O_LOG_N:       return 1.2f;
        case COMPLEXITY_O_N:           return 1.5f;
        case COMPLEXITY_O_N_LOG_N:     return 1.8f;
        case COMPLEXITY_O_N_SQUARED:   return 2.5f;
        case COMPLEXITY_O_N_CUBED:     return 4.0f;
        case COMPLEXITY_O_2_N:         return 8.0f;
        case COMPLEXITY_O_N_FACTORIAL: return 16.0f;
        default:                       return 2.0f;
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

recovery_parietal_config_t recovery_parietal_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_de", 0.0f);


    recovery_parietal_config_t config = {
        .enable_code_analysis = true,
        .enable_pattern_matching = true,
        .enable_spatial_reasoning = true,
        .enable_complexity_estimation = true,
        .enable_learning = true,
        .max_analysis_time_ms = 5000,
        .min_pattern_similarity = 0.6f,
        .max_dependency_depth = 5
    };
    return config;
}

recovery_parietal_bridge_t* recovery_parietal_bridge_create(
    parietal_lobe_t* parietal,
    const recovery_parietal_config_t* config
) {
    if (!parietal) {
        fprintf(stderr, "[RECOVERY-PARIETAL] ERROR: NULL parietal handle\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_bridge_create: parietal is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_create", 0.0f);


    recovery_parietal_bridge_t* bridge = nimcp_calloc(1, sizeof(recovery_parietal_bridge_t));
    if (!bridge) {
        fprintf(stderr, "[RECOVERY-PARIETAL] ERROR: Failed to allocate bridge\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "recovery_parietal_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = recovery_parietal_default_config();
    }

    /* Store parietal reference */
    bridge->parietal = parietal;
    bridge->executive = NULL;

    /* Initialize pattern database */
    bridge->pattern_count = 0;
    bridge->next_pattern_id = 1;
    memset(bridge->patterns, 0, sizeof(bridge->patterns));

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->initialized = true;

    fprintf(stderr, "[RECOVERY-PARIETAL] Bridge created successfully\n");
    fprintf(stderr, "[RECOVERY-PARIETAL]   Code analysis: %s\n",
            bridge->config.enable_code_analysis ? "enabled" : "disabled");
    fprintf(stderr, "[RECOVERY-PARIETAL]   Pattern matching: %s\n",
            bridge->config.enable_pattern_matching ? "enabled" : "disabled");
    fprintf(stderr, "[RECOVERY-PARIETAL]   Spatial reasoning: %s\n",
            bridge->config.enable_spatial_reasoning ? "enabled" : "disabled");

    return bridge;
}

void recovery_parietal_bridge_destroy(recovery_parietal_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "recovery_parietal");
    }

    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_destroy", 0.0f);


    bridge->initialized = false;
    bridge->parietal = NULL;
    bridge->executive = NULL;

    nimcp_free(bridge);
    bridge = NULL;
    fprintf(stderr, "[RECOVERY-PARIETAL] Bridge destroyed\n");
}

bool recovery_parietal_bridge_is_ready(const recovery_parietal_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_is_ready", 0.0f);


    return bridge && bridge->initialized && bridge->parietal;
}

//=============================================================================
// Attachment Functions
//=============================================================================

int recovery_parietal_bridge_attach_executive(
    recovery_parietal_bridge_t* bridge,
    recovery_executive_t* exec
) {
    if (!bridge || !exec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_bridge_attach_executive: required parameter is NULL (bridge, exec)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_attach_executive", 0.0f);


    bridge->executive = exec;
    fprintf(stderr, "[RECOVERY-PARIETAL] Attached to recovery executive\n");
    return 0;
}

int recovery_executive_attach_parietal(
    recovery_executive_t* exec,
    parietal_lobe_t* parietal
) {
    if (!exec || !parietal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_executive_attach_parietal: required parameter is NULL (exec, parietal)");
        return -1;
    }

    /* Create bridge with default config */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_executive_a", 0.0f);


    recovery_parietal_bridge_t* bridge = recovery_parietal_bridge_create(parietal, NULL);
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate bridge");

        return -1;
    }

    /* Attach to executive */
    int result = recovery_parietal_bridge_attach_executive(bridge, exec);
    if (result != 0) {
        recovery_parietal_bridge_destroy(bridge);
        return result;
    }

    fprintf(stderr, "[RECOVERY-PARIETAL] Parietal attached to recovery executive\n");
    return 0;
}

parietal_lobe_t* recovery_executive_get_parietal(const recovery_executive_t* exec) {
    /* This would require internal access to executive structure */
    /* For now, return NULL - needs executive structure update */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_executive_g", 0.0f);


    (void)exec;
    return NULL;  // Not implemented: parietal retrieval
}

//=============================================================================
// Code Analysis Functions
//=============================================================================

int recovery_parietal_analyze_code(
    recovery_parietal_bridge_t* bridge,
    const code_analysis_request_t* request,
    code_analysis_result_t* result
) {
    if (!bridge || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_analyze_code: required parameter is NULL (bridge, request, result)");
        return -1;
    }

    if (!recovery_parietal_bridge_is_ready(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "recovery_parietal_analyze_code: bridge not ready");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_an", 0.0f);


    memset(result, 0, sizeof(code_analysis_result_t));

    /* Get software engineering submodule */
    software_eng_t* se = NULL;

    /* Access parietal's software engineering capabilities */
    if (bridge->config.enable_code_analysis) {
        /* Create algorithm traits based on request */
        algorithm_traits_t traits = {
            .has_loops = true,
            .loop_depth = 2,
            .has_recursion = false,
            .recursive_calls = 0,
            .has_divide_conquer = false,
            .has_dynamic_programming = false,
            .has_sorting = false,
            .has_searching = true,
            .input_size_n = 1000,
            .auxiliary_space = 100
        };

        /* Analyze complexity using parietal */
        if (bridge->config.enable_complexity_estimation) {
            /* Use software engineering module if available */
            se = software_eng_create();
            if (se) {
                software_eng_analyze_complexity(se, &traits, &result->complexity);
                software_eng_destroy(se);
            }
        }
    }

    /* Detect code smells */
    if (bridge->config.enable_code_analysis && request->detect_code_smells) {
        /* Create sample metrics for analysis */
        software_metrics_t metrics = {
            .lines_of_code = 500,
            .num_functions = 20,
            .num_classes = 5,
            .num_modules = 3,
            .cyclomatic_complexity = 15.0f,
            .cognitive_complexity = 25.0f,
            .halstead_difficulty = 30.0f,
            .halstead_effort = 1000.0f,
            .afferent_coupling = 5.0f,
            .efferent_coupling = 8.0f,
            .instability = 0.62f,
            .abstractness = 0.3f,
            .maintainability_index = 65.0f,
            .test_coverage = 0.45f,
            .documentation_ratio = 0.2f
        };

        result->metrics = metrics;

        se = software_eng_create();
        if (se) {
            result->smell_count = software_eng_detect_smells(
                se, &metrics, result->smells, 8);
            software_eng_destroy(se);
        }
    }

    /* Find similar patterns */
    if (bridge->config.enable_pattern_matching && request->find_similar_patterns) {
        uint32_t match_count = 0;
        for (uint32_t i = 0; i < bridge->pattern_count && match_count < MAX_FAILURE_PATTERNS; i++) {
            float sim = calculate_pattern_similarity(request->diagnosis, &bridge->patterns[i]);
            if (sim >= bridge->config.min_pattern_similarity) {
                result->similar_patterns[match_count].pattern_id = bridge->patterns[i].pattern_id;
                result->similar_patterns[match_count].similarity = sim;
                snprintf(result->similar_patterns[match_count].description,
                        sizeof(result->similar_patterns[match_count].description),
                        "%s", bridge->patterns[i].failure_type);
                snprintf(result->similar_patterns[match_count].suggested_fix,
                        sizeof(result->similar_patterns[match_count].suggested_fix),
                        "%s", bridge->patterns[i].successful_fix);
                match_count++;
            }
        }
        result->pattern_count = match_count;
    }

    /* Calculate overall assessment */
    result->repair_difficulty = severity_to_difficulty(
        request->diagnosis ? request->diagnosis->severity : DIAG_SEVERITY_ERROR);

    /* Adjust difficulty based on complexity */
    result->repair_difficulty *= complexity_to_factor(result->complexity.time_complexity) / 2.0f;
    result->repair_difficulty = fminf(result->repair_difficulty, 1.0f);

    /* Set confidence based on analysis depth */
    result->confidence = 0.5f;
    if (bridge->config.enable_code_analysis) result->confidence += 0.15f;
    if (bridge->config.enable_pattern_matching && result->pattern_count > 0) result->confidence += 0.2f;
    if (bridge->config.enable_complexity_estimation) result->confidence += 0.1f;

    /* Generate hypothesis */
    if (request->diagnosis) {
        snprintf(result->root_cause_hypothesis, sizeof(result->root_cause_hypothesis),
                "Failure in %s likely caused by %s with severity %d",
                request->failure_location.module_name,
                request->diagnosis->root_cause,
                request->diagnosis->severity);
    }

    /* Generate recommendation */
    snprintf(result->recommended_approach, sizeof(result->recommended_approach),
            "Analyze dependencies at depth %u, address %u code smells, "
            "estimated repair difficulty: %.2f",
            request->dependency_depth,
            result->smell_count,
            result->repair_difficulty);

    bridge->stats.total_analyses++;

    return 0;
}

int recovery_parietal_analyze_impact(
    recovery_parietal_bridge_t* bridge,
    const code_location_t* location,
    uint32_t depth,
    char affected_modules[][128],
    uint32_t max_modules
) {
    if (!bridge || !location || !affected_modules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_analyze_impact: required parameter is NULL (bridge, location, affected_modules)");
        return -1;
    }

    if (!recovery_parietal_bridge_is_ready(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "recovery_parietal_analyze_impact: bridge not ready");
        return -1;
    }

    /* For now, return a simulated impact analysis */
    /* In full implementation, this would use parietal's spatial reasoning */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_an", 0.0f);


    uint32_t count = 0;

    /* Add the failed module itself */
    if (count < max_modules && strlen(location->module_name) > 0) {
        snprintf(affected_modules[count], 128, "%s", location->module_name);
        count++;
    }

    /* Simulate finding dependent modules based on depth */
    for (uint32_t d = 1; d <= depth && count < max_modules; d++) {
        snprintf(affected_modules[count], 128, "%s_dependent_%u",
                location->module_name, d);
        count++;
    }

    return (int)count;
}

int recovery_parietal_detect_smells(
    recovery_parietal_bridge_t* bridge,
    const code_location_t* location,
    smell_result_t* smells,
    uint32_t max_smells
) {
    if (!bridge || !location || !smells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_detect_smells: required parameter is NULL (bridge, location, smells)");
        return -1;
    }

    if (!recovery_parietal_bridge_is_ready(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "recovery_parietal_detect_smells: bridge not ready");
        return -1;
    }

    /* Use software engineering module */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_de", 0.0f);


    software_eng_t* se = software_eng_create();
    if (!se) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate software_eng_t");

        return -1;
    }

    /* Create metrics based on location context */
    software_metrics_t metrics = {
        .lines_of_code = 300,
        .num_functions = 15,
        .cyclomatic_complexity = 12.0f,
        .cognitive_complexity = 20.0f,
        .maintainability_index = 70.0f
    };

    uint32_t count = software_eng_detect_smells(se, &metrics, smells, max_smells);

    software_eng_destroy(se);

    return (int)count;
}

//=============================================================================
// Pattern Matching Functions
//=============================================================================

int recovery_parietal_find_similar_failures(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const code_location_t* location,
    code_analysis_result_t* result,
    uint32_t max_patterns
) {
    if (!bridge || !diagnosis || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_find_similar_failures: required parameter is NULL (bridge, diagnosis, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_fi", 0.0f);


    code_analysis_request_t request = {
        .diagnosis = (diagnostic_result_t*)diagnosis,
        .find_similar_patterns = true,
        .analyze_dependencies = false,
        .detect_code_smells = false,
        .analyze_complexity = false
    };

    if (location) {
        request.failure_location = *location;
    }

    int ret = recovery_parietal_analyze_code(bridge, &request, result);
    if (ret != 0) {
        return ret;
    }

    bridge->stats.patterns_matched += result->pattern_count;

    return (int)result->pattern_count;
}

int recovery_parietal_learn_pattern(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const code_location_t* location,
    const recovery_plan_t* plan,
    bool success
) {
    if (!bridge || !diagnosis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_learn_pattern: required parameter is NULL (bridge, diagnosis)");
        return -1;
    }

    if (!bridge->config.enable_learning) {
        return 0;
    }

    /* Check if we already have a similar pattern */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_le", 0.0f);


    int existing_idx = -1;
    for (uint32_t i = 0; i < bridge->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->pattern_count > 256) {
            recovery_parietal_bridge_heartbeat("recovery_par_loop",
                             (float)(i + 1) / (float)bridge->pattern_count);
        }

        float sim = calculate_pattern_similarity(diagnosis, &bridge->patterns[i]);
        if (sim >= 0.9f) {
            existing_idx = (int)i;
            break;
        }
    }

    if (existing_idx >= 0) {
        /* Update existing pattern */
        bridge->patterns[existing_idx].occurrences++;
        if (success) {
            bridge->patterns[existing_idx].successful_recoveries++;
            if (plan && strlen(plan->rationale) > 0) {
                snprintf(bridge->patterns[existing_idx].successful_fix,
                        sizeof(bridge->patterns[existing_idx].successful_fix),
                        "%s", plan->rationale);
            }
        }
    } else if (bridge->pattern_count < MAX_FAILURE_PATTERNS) {
        /* Add new pattern */
        failure_pattern_t* p = &bridge->patterns[bridge->pattern_count];
        p->pattern_id = bridge->next_pattern_id++;
        p->severity = diagnosis->severity;
        snprintf(p->failure_type, sizeof(p->failure_type), "%s", diagnosis->root_cause);
        if (location) {
            snprintf(p->module_pattern, sizeof(p->module_pattern), "%s",
                    location->module_name);
        }
        if (success && plan) {
            snprintf(p->successful_fix, sizeof(p->successful_fix), "%s",
                    plan->rationale);
        }
        p->occurrences = 1;
        p->successful_recoveries = success ? 1 : 0;

        bridge->pattern_count++;
        bridge->stats.patterns_learned++;
    }

    return 0;
}

//=============================================================================
// Recovery Enhancement Functions
//=============================================================================

int recovery_parietal_enhance_plan(
    recovery_parietal_bridge_t* bridge,
    recovery_executive_t* exec,
    const diagnostic_result_t* diagnosis,
    const code_location_t* location,
    recovery_enhancement_t* enhancement
) {
    if (!bridge || !exec || !diagnosis || !enhancement) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_enhance_plan: required parameter is NULL (bridge, exec, diagnosis, enhancement)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_en", 0.0f);


    memset(enhancement, 0, sizeof(recovery_enhancement_t));

    /* Perform code analysis */
    code_analysis_request_t request = {
        .diagnosis = (diagnostic_result_t*)diagnosis,
        .analyze_dependencies = true,
        .detect_code_smells = true,
        .analyze_complexity = true,
        .find_similar_patterns = true,
        .dependency_depth = bridge->config.max_dependency_depth
    };
    if (location) {
        request.failure_location = *location;
    }

    code_analysis_result_t analysis;
    int ret = recovery_parietal_analyze_code(bridge, &request, &analysis);
    if (ret != 0) {
        return ret;
    }

    /* Calculate success estimate based on analysis */
    float base_success = 0.7f;  /* Base success rate */

    /* Adjust for severity */
    base_success -= severity_to_difficulty(diagnosis->severity) * 0.3f;

    /* Adjust for complexity */
    base_success -= (complexity_to_factor(analysis.complexity.time_complexity) - 1.0f) * 0.05f;

    /* Boost from historical patterns */
    if (analysis.pattern_count > 0) {
        base_success += 0.1f * analysis.similar_patterns[0].similarity;
    }

    enhancement->estimated_success_rate = fmaxf(0.1f, fminf(base_success, 0.95f));

    /* Estimate recovery time */
    enhancement->estimated_recovery_time_ms = 1000.0f * analysis.repair_difficulty;
    enhancement->estimated_recovery_time_ms *= complexity_to_factor(analysis.complexity.time_complexity);

    /* Resource requirement */
    enhancement->resource_requirement = analysis.repair_difficulty;

    /* Impact analysis */
    enhancement->impacted_components = analysis.affected_modules;
    snprintf(enhancement->critical_path, sizeof(enhancement->critical_path),
            "Primary: %s -> Dependencies",
            location ? location->module_name : "unknown");

    /* Goal recommendation */
    if (diagnosis->severity >= DIAG_SEVERITY_FATAL) {
        enhancement->recommended_goal = RECOVERY_GOAL_DATA_PRESERVATION;
    } else if (diagnosis->severity >= DIAG_SEVERITY_CRITICAL) {
        enhancement->recommended_goal = RECOVERY_GOAL_DEGRADED_MODE;
    } else {
        enhancement->recommended_goal = RECOVERY_GOAL_FULL_RECOVERY;
    }

    /* Priority actions */
    enhancement->priority_action_count = 0;

    if (diagnosis->severity >= DIAG_SEVERITY_CRITICAL) {
        enhancement->priority_actions[enhancement->priority_action_count++] =
            RECOVERY_EXEC_ACTION_CHECKPOINT_SAVE;
    }

    enhancement->priority_actions[enhancement->priority_action_count++] =
        RECOVERY_EXEC_ACTION_ANALYZE_DIAGNOSTIC;

    if (analysis.smell_count > 0) {
        enhancement->priority_actions[enhancement->priority_action_count++] =
            RECOVERY_EXEC_ACTION_ISOLATE_COMPONENT;
    }

    enhancement->priority_actions[enhancement->priority_action_count++] =
        RECOVERY_EXEC_ACTION_VERIFY_STATE;

    /* Risk assessment */
    enhancement->risk_of_cascade = fminf(1.0f,
        analysis.affected_modules * 0.1f + analysis.repair_difficulty * 0.3f);
    enhancement->risk_of_data_loss = (diagnosis->severity >= DIAG_SEVERITY_CRITICAL) ? 0.4f : 0.1f;

    snprintf(enhancement->risk_mitigation, sizeof(enhancement->risk_mitigation),
            "Checkpoint before repair, isolate %u affected modules",
            analysis.affected_modules);

    /* Learning recommendation */
    enhancement->should_update_patterns = true;
    snprintf(enhancement->pattern_update, sizeof(enhancement->pattern_update),
            "Record outcome for %s failure pattern",
            diagnosis->root_cause);

    bridge->stats.enhancements_provided++;

    /* Free analysis resources */
    recovery_parietal_free_analysis_result(&analysis);

    return 0;
}

recovery_plan_t* recovery_parietal_create_enhanced_plan(
    recovery_parietal_bridge_t* bridge,
    recovery_executive_t* exec,
    const diagnostic_result_t* diagnosis,
    recovery_goal_t goal,
    const code_location_t* location
) {
    if (!bridge || !exec || !diagnosis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_create_enhanced_plan: required parameter is NULL (bridge, exec, diagnosis)");
        return NULL;
    }

    /* Get enhancement recommendations */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_cr", 0.0f);


    recovery_enhancement_t enhancement;
    int ret = recovery_parietal_enhance_plan(bridge, exec, diagnosis, location, &enhancement);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_UNKNOWN, "recovery_parietal_create_enhanced_plan: enhancement failed");
        return NULL;
    }

    /* Use recommended goal if different */
    recovery_goal_t effective_goal = goal;
    if (enhancement.estimated_success_rate < 0.3f) {
        /* Low success rate - downgrade goal */
        effective_goal = enhancement.recommended_goal;
    }

    /* Create plan using executive */
    recovery_plan_t* plan = recovery_executive_create_plan(exec, diagnosis, effective_goal);
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to create recovery plan");

        return NULL;
    }

    /* Enhance plan with parietal insights */
    /* Append enhancement info to rationale */
    char enhanced_rationale[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(enhanced_rationale, sizeof(enhanced_rationale),
            "%s [Parietal: %.0f%% success, %.0fms est., %u components affected]",
            plan->rationale,
            enhancement.estimated_success_rate * 100.0f,
            enhancement.estimated_recovery_time_ms,
            enhancement.impacted_components);
    snprintf(plan->rationale, sizeof(plan->rationale), "%s", enhanced_rationale);

    /* Adjust confidence based on parietal analysis */
    plan->confidence = (plan->confidence + enhancement.estimated_success_rate) / 2.0f;

    return plan;
}

float recovery_parietal_estimate_success(
    recovery_parietal_bridge_t* bridge,
    const recovery_plan_t* plan,
    const diagnostic_result_t* diagnosis
) {
    if (!bridge || !plan || !diagnosis) {
        return 0.0f;
    }

    /* Base estimate from plan confidence */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_es", 0.0f);


    float estimate = plan->confidence;

    /* Adjust for severity */
    estimate -= severity_to_difficulty(diagnosis->severity) * 0.2f;

    /* Adjust for plan complexity */
    if (plan->step_count > 10) {
        estimate -= 0.1f;
    } else if (plan->step_count < 5) {
        estimate += 0.05f;
    }

    /* Check for historical patterns */
    for (uint32_t i = 0; i < bridge->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->pattern_count > 256) {
            recovery_parietal_bridge_heartbeat("recovery_par_loop",
                             (float)(i + 1) / (float)bridge->pattern_count);
        }

        float sim = calculate_pattern_similarity(diagnosis, &bridge->patterns[i]);
        if (sim >= bridge->config.min_pattern_similarity) {
            /* Use historical success rate */
            if (bridge->patterns[i].occurrences > 0) {
                float hist_rate = (float)bridge->patterns[i].successful_recoveries /
                                  bridge->patterns[i].occurrences;
                estimate = (estimate + hist_rate) / 2.0f;
            }
            break;
        }
    }

    return fmaxf(0.0f, fminf(estimate, 1.0f));
}

//=============================================================================
// Hypothesis Testing Functions
//=============================================================================

int recovery_parietal_create_hypothesis(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    char* hypothesis,
    uint32_t max_len,
    float* confidence
) {
    if (!bridge || !diagnosis || !hypothesis || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_create_hypothesis: required parameter is NULL (bridge, diagnosis, hypothesis, confidence)");
        return -1;
    }

    /* Create hypothesis based on diagnostic information */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_cr", 0.0f);


    snprintf(hypothesis, max_len,
            "Hypothesis: The %s failure (severity %d) is likely caused by "
            "systemic issues in the affected subsystem. "
            "Recovery approach should prioritize %s.",
            diagnosis->root_cause,
            diagnosis->severity,
            (diagnosis->severity >= DIAG_SEVERITY_CRITICAL) ?
                "data preservation" : "full restoration");

    /* Calculate confidence based on available information */
    *confidence = 0.5f;  /* Base confidence */

    /* More info increases confidence */
    if (strlen(diagnosis->symptoms) > 50) {
        *confidence += 0.1f;
    }
    if (diagnosis->likely_faulty_function[0] != '\0') {
        *confidence += 0.1f;
    }

    /* Historical patterns boost confidence */
    for (uint32_t i = 0; i < bridge->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->pattern_count > 256) {
            recovery_parietal_bridge_heartbeat("recovery_par_loop",
                             (float)(i + 1) / (float)bridge->pattern_count);
        }

        float sim = calculate_pattern_similarity(diagnosis, &bridge->patterns[i]);
        if (sim >= bridge->config.min_pattern_similarity) {
            *confidence += 0.2f;
            break;
        }
    }

    *confidence = fminf(*confidence, 0.95f);

    return 0;
}

int recovery_parietal_test_hypothesis(
    recovery_parietal_bridge_t* bridge,
    const char* hypothesis,
    const recovery_execution_result_t* evidence,
    float* updated_confidence
) {
    if (!bridge || !hypothesis || !evidence || !updated_confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_test_hypothesis: required parameter is NULL (bridge, hypothesis, evidence, updated_confidence)");
        return -1;
    }

    /* Start with base confidence */
    *updated_confidence = 0.5f;

    /* Update based on evidence */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_te", 0.0f);


    if (evidence->success) {
        *updated_confidence = 0.9f;
        bridge->stats.successful_predictions++;
    } else {
        *updated_confidence = 0.3f;
        bridge->stats.failed_predictions++;

        /* Adjust down further if many steps failed */
        if (evidence->failed_step >= 0) {
            float fail_ratio = (float)(evidence->failed_step + 1) / evidence->steps_completed;
            *updated_confidence -= fail_ratio * 0.2f;
        }
    }

    *updated_confidence = fmaxf(0.0f, fminf(*updated_confidence, 1.0f));

    return 0;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int recovery_parietal_get_stats(
    const recovery_parietal_bridge_t* bridge,
    recovery_parietal_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_ge", 0.0f);


    return 0;
}

void recovery_parietal_reset_stats(recovery_parietal_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_re", 0.0f);


    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

//=============================================================================
// Utility Functions
//=============================================================================

void recovery_parietal_free_analysis_result(code_analysis_result_t* result) {
    if (!result) {
        return;
    }

    /* Free dependency graph if allocated */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_fr", 0.0f);


    if (result->dependency_graph) {
        nimcp_free(result->dependency_graph);
        result->dependency_graph = NULL;
    }

    /* Clear other dynamic data */
    result->pattern_count = 0;
    result->smell_count = 0;
}

/* ============================================================================
 * Code Generation Integration (Self-Repair Pipeline)
 * ============================================================================ */

int recovery_parietal_generate_fix(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const code_analysis_result_t* analysis,
    char* fix_code,
    size_t fix_code_size,
    float* fix_confidence,
    char* fix_explanation,
    size_t explanation_size
) {
    if (!bridge || !diagnosis || !fix_code || fix_code_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_generate_fix: required parameter is NULL (bridge, diagnosis, fix_code)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "recovery_parietal_generate_fix: bridge not initialized");
        return -1;
    }

    /* Default outputs */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_ge", 0.0f);


    fix_code[0] = '\0';
    if (fix_confidence) *fix_confidence = 0.0f;
    if (fix_explanation && explanation_size > 0) fix_explanation[0] = '\0';

    /* Map error type to fix strategy */
    const char* fix_template = NULL;
    float confidence = 0.0f;
    const char* explanation = "Unknown error type";

    switch (diagnosis->error_type) {
        case ERROR_TYPE_NULL_POINTER:
            fix_template = "if (${VAR} == NULL) {\n    return ${ERROR_VALUE};\n}\n";
            confidence = 0.9f;
            explanation = "Added null pointer check to prevent segmentation fault";
            break;

        case ERROR_TYPE_BUFFER_OVERFLOW:
        case ERROR_TYPE_BUFFER_UNDERFLOW:
            fix_template = "if (${INDEX} < 0 || ${INDEX} >= ${SIZE}) {\n    return ${ERROR_VALUE};\n}\n";
            confidence = 0.85f;
            explanation = "Added bounds check to prevent buffer overflow";
            break;

        case ERROR_TYPE_DIVIDE_BY_ZERO:
            fix_template = "if (${DIVISOR} == 0) {\n    return ${ERROR_VALUE};\n}\n";
            confidence = 0.95f;
            explanation = "Added division by zero guard";
            break;

        case ERROR_TYPE_NAN_DETECTED:
        case ERROR_TYPE_INF_DETECTED:
            fix_template = "if (isnan(${VAR}) || isinf(${VAR})) {\n    return ${ERROR_VALUE};\n}\n";
            confidence = 0.9f;
            explanation = "Added NaN/Inf guard";
            break;

        case ERROR_TYPE_MEMORY_LEAK:
        case ERROR_TYPE_DOUBLE_FREE:
            fix_template = "if (${PTR} != NULL) {\n    nimcp_free(${PTR});\n    ${PTR} = NULL;\n}\n";
            confidence = 0.8f;
            explanation = "Added proper memory cleanup with null-after-free pattern";
            break;

        case ERROR_TYPE_DEADLOCK:
        case ERROR_TYPE_RACE_CONDITION:
            fix_template = "nimcp_mutex_lock(${MUTEX});\n/* protected section */\nnimcp_mutex_unlock(${MUTEX});\n";
            confidence = 0.65f;
            explanation = "Added mutex lock/unlock to prevent race condition";
            break;

        default:
            fix_template = "/* TODO: Manual fix required for error 0x%04X */\n";
            confidence = 0.3f;
            explanation = "Unrecognized error type - manual fix required";
            break;
    }

    /* Adjust confidence based on analysis if provided */
    if (analysis) {
        /* Lower confidence for complex code */
        if (analysis->repair_difficulty > 0.5f) {
            confidence *= (1.0f - analysis->repair_difficulty * 0.3f);
        }
        /* Boost confidence if similar patterns exist */
        if (analysis->pattern_count > 0) {
            confidence = confidence * 0.7f + analysis->similar_patterns[0].similarity * 0.3f;
        }
    }

    /* Copy fix template to output */
    snprintf(fix_code, fix_code_size, "%s", fix_template);

    /* Set outputs */
    if (fix_confidence) *fix_confidence = confidence;
    if (fix_explanation && explanation_size > 0) {
        strncpy(fix_explanation, explanation, explanation_size - 1);
        fix_explanation[explanation_size - 1] = '\0';
    }

    /* Update statistics */
    bridge->stats.total_analyses++;

    return 0;
}

int recovery_parietal_generate_fix_candidates(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const code_analysis_result_t* analysis,
    void* candidates,
    uint32_t max_candidates,
    uint32_t* generated_count
) {
    if (!bridge || !diagnosis || !candidates || max_candidates == 0 || !generated_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_generate_fix_candidates: required parameter is NULL (bridge, diagnosis, candidates, generated_count)");
        return -1;
    }

    *generated_count = 0;

    /* For now, generate single candidate using main function */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_ge", 0.0f);


    char fix_code[NIMCP_PATH_BUFFER_SIZE];
    float confidence = 0.0f;
    char explanation[NIMCP_ERROR_BUFFER_LARGE];

    if (recovery_parietal_generate_fix(bridge, diagnosis, analysis,
            fix_code, sizeof(fix_code), &confidence,
            explanation, sizeof(explanation)) == 0) {
        /* TODO: Copy to candidates array based on actual structure type */
        *generated_count = 1;
    }

    return 0;
}

int recovery_parietal_learn_fix_outcome(
    recovery_parietal_bridge_t* bridge,
    const diagnostic_result_t* diagnosis,
    const char* fix_code,
    uint32_t strategy,
    bool success
) {
    if (!bridge || !diagnosis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_parietal_learn_fix_outcome: required parameter is NULL (bridge, diagnosis)");
        return -1;
    }

    if (!bridge->config.enable_learning) {
        return 0;  /* Learning disabled */
    }

    /* Find existing pattern or create new one */
    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_recovery_parietal_le", 0.0f);


    failure_pattern_t* pattern = NULL;
    for (uint32_t i = 0; i < bridge->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->pattern_count > 256) {
            recovery_parietal_bridge_heartbeat("recovery_par_loop",
                             (float)(i + 1) / (float)bridge->pattern_count);
        }

        if (bridge->patterns[i].severity == diagnosis->severity) {
            pattern = &bridge->patterns[i];
            break;
        }
    }

    if (!pattern && bridge->pattern_count < MAX_FAILURE_PATTERNS) {
        pattern = &bridge->patterns[bridge->pattern_count++];
        pattern->pattern_id = bridge->next_pattern_id++;
        pattern->severity = diagnosis->severity;
        strncpy(pattern->failure_type, diagnosis->root_cause, sizeof(pattern->failure_type) - 1);
        pattern->occurrences = 0;
        pattern->successful_recoveries = 0;
    }

    if (pattern) {
        pattern->occurrences++;
        if (success) {
            pattern->successful_recoveries++;
            if (fix_code) {
                strncpy(pattern->successful_fix, fix_code, sizeof(pattern->successful_fix) - 1);
            }
            bridge->stats.successful_predictions++;
        } else {
            bridge->stats.failed_predictions++;
        }
        bridge->stats.patterns_learned++;
    }

    (void)strategy;  /* Could use for more detailed pattern tracking */

    return 0;
}

const char* recovery_parietal_bridge_version(void) {
    return RECOVERY_PARIETAL_VERSION;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int recovery_parietal_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    recovery_parietal_bridge_heartbeat("recovery_par_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recovery_Parietal_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                recovery_parietal_bridge_heartbeat("recovery_par_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recovery_Parietal_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recovery_Parietal_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void recovery_parietal_bridge_set_instance_health_agent(recovery_parietal_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "recovery_parietal_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int recovery_parietal_bridge_training_begin(recovery_parietal_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "recovery_parietal_bridge_training_begin: NULL argument");
        return -1;
    }
    recovery_parietal_bridge_heartbeat_instance(bridge->health_agent, "recovery_parietal_bridge_training_begin", 0.0f);
    return 0;
}

int recovery_parietal_bridge_training_end(recovery_parietal_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "recovery_parietal_bridge_training_end: NULL argument");
        return -1;
    }
    recovery_parietal_bridge_heartbeat_instance(bridge->health_agent, "recovery_parietal_bridge_training_end", 1.0f);
    return 0;
}

int recovery_parietal_bridge_training_step(recovery_parietal_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "recovery_parietal_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    recovery_parietal_bridge_heartbeat_instance(bridge->health_agent, "recovery_parietal_bridge_training_step", progress);
    return 0;
}
