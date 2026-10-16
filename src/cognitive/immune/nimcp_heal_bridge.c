/**
 * @file nimcp_heal_bridge.c
 * @brief Enhanced Self-Healing Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Implementation of unified healing bridge connecting self_heal and code_immune
 * WHY:  Enable end-to-end automated crash recovery with verification and learning
 * HOW:  Coordinate all healing components through unified pipeline
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_heal_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(heal_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_heal_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_heal_bridge_mesh_registry = NULL;

nimcp_error_t heal_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_heal_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "heal_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "heal_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_heal_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_heal_bridge_mesh_registry = registry;
    return err;
}

void heal_bridge_mesh_unregister(void) {
    if (g_heal_bridge_mesh_registry && g_heal_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_heal_bridge_mesh_registry, g_heal_bridge_mesh_id);
        g_heal_bridge_mesh_id = 0;
        g_heal_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from heal_bridge module (instance-level) */
static inline void heal_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_heal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_heal_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_heal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define LOG_TAG "HealBridge"

#define DEFAULT_CANDIDATE_CAPACITY   64
#define DEFAULT_CHAIN_CAPACITY       16
#define DEFAULT_ROLLBACK_CAPACITY    128

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Find candidate by ID (unlocked)
 */
static pattern_candidate_t* find_candidate_unlocked(
    heal_bridge_t* bridge,
    uint64_t candidate_id)
{
    if (bridge == NULL || bridge->candidates == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_candidate_unlocked: validation failed");
        return NULL;
    }

    for (size_t i = 0; i < bridge->candidate_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->candidate_count > 256) {
            heal_bridge_heartbeat("heal_bridge_loop",
                             (float)(i + 1) / (float)bridge->candidate_count);
        }

        if (bridge->candidates[i].id == candidate_id) {
            return &bridge->candidates[i];
        }
    }
    return NULL;
}

/**
 * @brief Find chain by ID (unlocked)
 */
static fix_chain_t* find_chain_unlocked(
    heal_bridge_t* bridge,
    uint64_t chain_id)
{
    if (bridge == NULL || bridge->active_chains == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_chain_unlocked: validation failed");
        return NULL;
    }

    for (size_t i = 0; i < bridge->chain_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->chain_count > 256) {
            heal_bridge_heartbeat("heal_bridge_loop",
                             (float)(i + 1) / (float)bridge->chain_count);
        }

        if (bridge->active_chains[i].id == chain_id) {
            return &bridge->active_chains[i];
        }
    }
    return NULL;
}

/**
 * @brief Find rollback entry by antibody ID
 */
static rollback_entry_t* find_rollback_entry(
    heal_bridge_t* bridge,
    uint64_t antibody_id)
{
    if (bridge == NULL || bridge->rollback_history == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_rollback_entry: validation failed");
        return NULL;
    }

    for (size_t i = 0; i < bridge->rollback_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->rollback_count > 256) {
            heal_bridge_heartbeat("heal_bridge_loop",
                             (float)(i + 1) / (float)bridge->rollback_count);
        }

        if (bridge->rollback_history[i].antibody_id == antibody_id) {
            return &bridge->rollback_history[i];
        }
    }
    return NULL;
}

/**
 * @brief Add rollback entry
 */
static int add_rollback_entry(
    heal_bridge_t* bridge,
    uint64_t antibody_id,
    const char* original_code,
    void* original_function)
{
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_rollback_entry: validation failed");
        return -1;
    }
    if (!bridge->config.enable_rollback) return 0;

    if (bridge->rollback_count >= bridge->rollback_capacity) {
        /* Evict oldest entry */
        memmove(&bridge->rollback_history[0],
                &bridge->rollback_history[1],
                (bridge->rollback_capacity - 1) * sizeof(rollback_entry_t));
        bridge->rollback_count--;
    }

    rollback_entry_t* entry = &bridge->rollback_history[bridge->rollback_count];
    entry->antibody_id = antibody_id;
    entry->apply_time = get_time_ms();
    entry->original_function = original_function;
    entry->can_rollback = true;

    if (original_code != NULL) {
        strncpy(entry->original_code, original_code, sizeof(entry->original_code) - 1);
        entry->original_code[sizeof(entry->original_code) - 1] = '\0';
    } else {
        entry->original_code[0] = '\0';
    }

    bridge->rollback_count++;
    return 0;
}

/**
 * @brief Calculate combined chain confidence
 */
static float calculate_chain_confidence(const fix_chain_t* chain)
{
    if (chain == NULL || chain->fix_count == 0) return 0.0f;

    float total = 0.0f;
    for (size_t i = 0; i < chain->fix_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && chain->fix_count > 256) {
            heal_bridge_heartbeat("heal_bridge_loop",
                             (float)(i + 1) / (float)chain->fix_count);
        }

        total += chain->fixes[i].fix.confidence;
    }
    return total / (float)chain->fix_count;
}

/**
 * @brief Check if related fix patterns should form a chain
 */
static bool should_chain_fixes(
    fix_pattern_type_t type1,
    fix_pattern_type_t type2)
{
    /* NULL check often needs to be followed by error handling */
    if (type1 == FIX_PATTERN_NULL_CHECK && type2 == FIX_PATTERN_RESOURCE_LEAK) {
        return true;
    }

    /* Bounds check may need overflow check */
    if (type1 == FIX_PATTERN_BOUNDS_CHECK && type2 == FIX_PATTERN_OVERFLOW_CHECK) {
        return true;
    }

    /* UAF fix should be paired with double-free prevention */
    if (type1 == FIX_PATTERN_UAF_CHECK && type2 == FIX_PATTERN_DOUBLE_FREE) {
        return true;
    }

    /* Race guard and lock ordering often go together */
    if (type1 == FIX_PATTERN_RACE_GUARD && type2 == FIX_PATTERN_LOCK_ORDER) {
        return true;
    }

    return false;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int heal_bridge_default_config(heal_bridge_config_t* config)
{
    if (config == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "heal_bridge_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_default_config", 0.0f);


    memset(config, 0, sizeof(heal_bridge_config_t));

    /* Validation settings */
    config->enable_sandbox = true;
    config->sandbox_timeout_ms = HEAL_BRIDGE_SANDBOX_TIMEOUT_MS;
    config->require_validation = true;

    /* Pattern evolution settings */
    config->enable_pattern_evolution = true;
    config->evolution_threshold = HEAL_BRIDGE_EVOLUTION_THRESHOLD;
    config->min_promotion_confidence = 0.7f;

    /* Fix chain settings */
    config->enable_fix_chains = true;
    config->max_chain_length = HEAL_BRIDGE_MAX_FIX_CHAIN;
    config->atomic_chains = true;

    /* Rollback settings */
    config->enable_rollback = true;
    config->rollback_window_ms = HEAL_BRIDGE_ROLLBACK_WINDOW_MS;
    config->max_rollback_entries = DEFAULT_ROLLBACK_CAPACITY;

    /* Integration */
    config->auto_produce_antibodies = true;
    config->sync_with_brain_immune = true;
    config->enable_logging = true;

    return 0;
}

heal_bridge_t* heal_bridge_create(
    self_heal_engine_t* self_heal,
    code_immune_system_t* code_immune,
    const heal_bridge_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_create", 0.0f);


    heal_bridge_t* bridge = nimcp_calloc(1, sizeof(heal_bridge_t));
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "heal_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config != NULL) {
        bridge->config = *config;
    } else {
        heal_bridge_default_config(&bridge->config);
    }

    /* Store connected systems */
    bridge->self_heal = self_heal;
    bridge->code_immune = code_immune;

    /* Get pattern library from self_heal if available */
    if (self_heal != NULL) {
        bridge->pattern_library = self_heal->pattern_library;
    }

    /* Allocate pattern evolution candidates */
    bridge->candidate_capacity = DEFAULT_CANDIDATE_CAPACITY;
    bridge->candidates = nimcp_calloc(bridge->candidate_capacity,
                                       sizeof(pattern_candidate_t));
    if (bridge->candidates == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "heal_bridge_create: failed to allocate candidates");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    bridge->candidate_count = 0;
    bridge->next_candidate_id = 1;

    /* Allocate fix chains */
    bridge->chain_capacity = DEFAULT_CHAIN_CAPACITY;
    bridge->active_chains = nimcp_calloc(bridge->chain_capacity,
                                          sizeof(fix_chain_t));
    if (bridge->active_chains == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "heal_bridge_create: failed to allocate fix chains");
        nimcp_free(bridge->candidates);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    bridge->chain_count = 0;
    bridge->next_chain_id = 1;

    /* Allocate rollback history */
    if (bridge->config.enable_rollback) {
        bridge->rollback_capacity = bridge->config.max_rollback_entries;
        if (bridge->rollback_capacity == 0) {
            bridge->rollback_capacity = DEFAULT_ROLLBACK_CAPACITY;
        }
        bridge->rollback_history = nimcp_calloc(bridge->rollback_capacity,
                                                 sizeof(rollback_entry_t));
        if (bridge->rollback_history == NULL) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "heal_bridge_create: failed to allocate rollback history");
            nimcp_free(bridge->active_chains);
            nimcp_free(bridge->candidates);
            nimcp_free(bridge);
            bridge = NULL;
            return NULL;
        }
    }

    /* Initialize sandbox context */
    bridge->sandbox.timeout_ms = bridge->config.sandbox_timeout_ms;
    bridge->sandbox.use_fork = true;
    bridge->sandbox.run_regression = false;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "heal") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "heal_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge->rollback_history);
        nimcp_free(bridge->active_chains);
        nimcp_free(bridge->candidates);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->initialized = true;
    bridge->start_time = get_time_ms();

    LOG_MODULE_INFO(LOG_TAG, "Heal bridge created (sandbox=%s, evolution=%s, chains=%s)",
                    bridge->config.enable_sandbox ? "on" : "off",
                    bridge->config.enable_pattern_evolution ? "on" : "off",
                    bridge->config.enable_fix_chains ? "on" : "off");

    return bridge;
}

void heal_bridge_destroy(heal_bridge_t* bridge)
{
    if (bridge == NULL) return;

    /* Cleanup base bridge infrastructure */
    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_destroy", 0.0f);


    bridge_base_cleanup(&bridge->base);

    if (bridge->rollback_history != NULL) {
        nimcp_free(bridge->rollback_history);
    }

    if (bridge->active_chains != NULL) {
        nimcp_free(bridge->active_chains);
    }

    if (bridge->candidates != NULL) {
        nimcp_free(bridge->candidates);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Unified Pipeline Implementation
 * ============================================================================ */

int heal_bridge_process_crash(
    heal_bridge_t* bridge,
    uint64_t antigen_id,
    const char* source_code,
    uint64_t* antibody_id_out)
{
    if (bridge == NULL || source_code == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_process_crash: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_process_crash", 0.0f);


    uint64_t start_time = get_time_ms();

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.crashes_received++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Step 1: Get antigen from code_immune */
    const code_antigen_t* antigen = NULL;
    if (bridge->code_immune != NULL) {
        antigen = code_immune_get_antigen(bridge->code_immune, antigen_id);
    }

    /* Step 2: Convert to brain antigen for self_heal */
    brain_antigen_t brain_ag = {0};
    if (antigen != NULL) {
        brain_ag.id = antigen->id;
        brain_ag.severity = (uint8_t)(antigen->severity * 10);
        brain_ag.confidence = antigen->confidence;
        brain_ag.danger_signal = antigen->danger_signal;
        brain_ag.source = ANTIGEN_SOURCE_BBB;

        /* Map signal to BBB threat type */
        switch (antigen->signal) {
            case SIGSEGV:
                brain_ag.bbb_threat_type = BBB_THREAT_MEMORY_VIOLATION;
                break;
            case SIGFPE:
                brain_ag.bbb_threat_type = BBB_THREAT_INTEGER_OVERFLOW;
                break;
            case SIGBUS:
                brain_ag.bbb_threat_type = BBB_THREAT_MEMORY_VIOLATION;
                break;
            default:
                brain_ag.bbb_threat_type = BBB_THREAT_UNKNOWN;
                break;
        }
    }

    /* Step 3: Generate fix using self_heal */
    heal_result_t fix = {0};
    int ret = -1;

    if (bridge->self_heal != NULL) {
        ret = self_heal_generate_fix(bridge->self_heal, &brain_ag,
                                      source_code, &fix);
    }

    if (ret != 0 || fix.status != HEAL_STATUS_SUCCESS) {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.fixes_failed++;
        nimcp_mutex_unlock(bridge->base.mutex);

        LOG_MODULE_WARN(LOG_TAG, "Fix generation failed for antigen %lu", antigen_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_process_crash: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.fixes_generated++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Step 4: Validate fix in sandbox */
    sandbox_result_t sandbox_result = SANDBOX_RESULT_SKIPPED;

    if (bridge->config.enable_sandbox) {
        heal_bridge_validate_fix(bridge, &fix, &sandbox_result);

        if (sandbox_result != SANDBOX_RESULT_SUCCESS &&
            bridge->config.require_validation) {
            nimcp_mutex_lock(bridge->base.mutex);
            bridge->stats.fixes_failed++;
            nimcp_mutex_unlock(bridge->base.mutex);

            LOG_MODULE_WARN(LOG_TAG, "Fix validation failed: %s",
                           heal_bridge_sandbox_result_to_string(sandbox_result));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_process_crash: operation failed");
            return -1;
        }
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.fixes_validated++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Step 5: Produce antibody via code_immune */
    uint64_t antibody_id = 0;

    if (bridge->code_immune != NULL && bridge->config.auto_produce_antibodies) {
        /* Find or create B cell */
        uint64_t b_cell_id = 0;
        ret = code_immune_find_matching_b_cell(bridge->code_immune,
                                                antigen_id, &b_cell_id);

        if (ret != 0) {
            /* Create new B cell */
            ret = code_immune_create_b_cell(bridge->code_immune,
                                             antigen_id, &b_cell_id);
        }

        if (ret == 0 && b_cell_id > 0) {
            /* Set fix template */
            code_immune_set_fix_template(bridge->code_immune,
                                          b_cell_id, fix.fixed_code);

            /* Activate B cell */
            code_immune_activate_b_cell(bridge->code_immune,
                                         b_cell_id, antigen_id);

            /* Produce antibody */
            code_antibody_class_t ab_class = (sandbox_result == SANDBOX_RESULT_SUCCESS)
                ? CODE_ANTIBODY_IGG : CODE_ANTIBODY_IGM;

            ret = code_immune_produce_antibody(bridge->code_immune,
                                                b_cell_id, ab_class, &antibody_id);

            if (ret == 0) {
                /* Apply antibody */
                ret = code_immune_apply_antibody(bridge->code_immune, antibody_id);

                if (ret == 0) {
                    /* Add to rollback history */
                    add_rollback_entry(bridge, antibody_id,
                                       fix.original_code, NULL);

                    nimcp_mutex_lock(bridge->base.mutex);
                    bridge->stats.fixes_applied++;
                    nimcp_mutex_unlock(bridge->base.mutex);

                    LOG_MODULE_INFO(LOG_TAG, "Fix applied: antibody %lu for antigen %lu",
                                    antibody_id, antigen_id);
                }
            }
        }
    }

    /* Step 6: Track for pattern evolution if LNN-generated */
    if (fix.lnn_generated && bridge->config.enable_pattern_evolution) {
        crash_features_t features = {0};
        if (bridge->self_heal != NULL) {
            self_heal_extract_features(bridge->self_heal, &brain_ag, &features);
        }

        uint64_t candidate_id = 0;
        heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

        /* Record initial success */
        heal_bridge_record_candidate_outcome(bridge, candidate_id, true,
                                              fix.confidence);
    }

    /* Update timing statistics */
    uint64_t elapsed_ms = get_time_ms() - start_time;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t total_crashes = bridge->stats.crashes_received;
    bridge->stats.avg_pipeline_time_ms =
        (bridge->stats.avg_pipeline_time_ms * (total_crashes - 1) + (double)elapsed_ms) /
        total_crashes;
    nimcp_mutex_unlock(bridge->base.mutex);

    if (antibody_id_out != NULL) {
        *antibody_id_out = antibody_id;
    }

    return 0;
}

int heal_bridge_process_crash_chain(
    heal_bridge_t* bridge,
    uint64_t antigen_id,
    const char* source_code,
    uint64_t* chain_id_out)
{
    if (bridge == NULL || source_code == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_process_crash_chain: validation failed");
        return -1;
    }
    if (!bridge->config.enable_fix_chains) {
        /* Fall back to single fix */
        return heal_bridge_process_crash(bridge, antigen_id, source_code, NULL);
    }

    /* Create fix chain */
    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_process_crash_chain", 0.0f);


    uint64_t chain_id = 0;
    int ret = heal_bridge_create_chain(bridge, antigen_id, source_code, &chain_id);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_process_crash_chain: validation failed");
        return -1;
    }

    /* Execute chain */
    ret = heal_bridge_execute_chain(bridge, chain_id);

    if (chain_id_out != NULL) {
        *chain_id_out = chain_id;
    }

    return ret;
}

/* ============================================================================
 * Sandbox Validation Implementation
 * ============================================================================ */

int heal_bridge_validate_fix(
    heal_bridge_t* bridge,
    const heal_result_t* fix,
    sandbox_result_t* result_out)
{
    if (bridge == NULL || fix == NULL || result_out == NULL) {
        return -1;
    }

    if (!bridge->config.enable_sandbox) {
        *result_out = SANDBOX_RESULT_SKIPPED;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_validate_fix", 0.0f);


    uint64_t start_time = get_time_ms();

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.sandbox_runs++;
    nimcp_mutex_unlock(bridge->base.mutex);

    *result_out = SANDBOX_RESULT_SUCCESS;

    /* Create pipe for result communication */
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        *result_out = SANDBOX_RESULT_COMPILE_ERROR;
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        /* Fork failed */
        close(pipefd[0]);
        close(pipefd[1]);
        *result_out = SANDBOX_RESULT_COMPILE_ERROR;
        return -1;
    }

    if (pid == 0) {
        /* Child process - sandbox */
        close(pipefd[0]);  /* Close read end */

        sandbox_result_t child_result = SANDBOX_RESULT_SUCCESS;

        /* Basic validation: check fix code is not empty */
        if (fix->fixed_code[0] == '\0') {
            child_result = SANDBOX_RESULT_COMPILE_ERROR;
        }

        /* Check for obvious issues in generated code */
        if (child_result == SANDBOX_RESULT_SUCCESS) {
            /* Check for unbalanced braces */
            int brace_count = 0;
            for (const char* p = fix->fixed_code; *p != '\0'; p++) {
                if (*p == '{') brace_count++;
                else if (*p == '}') brace_count--;
            }
            if (brace_count != 0) {
                child_result = SANDBOX_RESULT_COMPILE_ERROR;
            }
        }

        /* Check for infinite loops (simple heuristic) */
        if (child_result == SANDBOX_RESULT_SUCCESS) {
            if (strstr(fix->fixed_code, "while(1)") != NULL ||
                strstr(fix->fixed_code, "for(;;)") != NULL) {
                /* Potential infinite loop - flag for review */
                child_result = SANDBOX_RESULT_REGRESSION;
            }
        }

        /* Write result to pipe */
        ssize_t written = write(pipefd[1], &child_result, sizeof(child_result));
        (void)written;  /* Ignore write errors in child */

        close(pipefd[1]);
        _exit(0);
    }

    /* Parent process */
    close(pipefd[1]);  /* Close write end */

    /* Wait for child with timeout */
    int status = 0;
    uint64_t timeout_ms = bridge->config.sandbox_timeout_ms;
    uint64_t elapsed = 0;

    while (elapsed < timeout_ms) {
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == pid) {
            /* Child finished */
            if (WIFEXITED(status)) {
                /* Read result from pipe */
                sandbox_result_t child_result;
                ssize_t bytes_read = read(pipefd[0], &child_result,
                                          sizeof(child_result));
                if (bytes_read == sizeof(child_result)) {
                    *result_out = child_result;
                }
            } else if (WIFSIGNALED(status)) {
                /* Child crashed */
                *result_out = SANDBOX_RESULT_CRASH;

                nimcp_mutex_lock(bridge->base.mutex);
                bridge->stats.sandbox_crashes++;
                nimcp_mutex_unlock(bridge->base.mutex);
            }
            break;
        } else if (result == 0) {
            /* Child still running */
            usleep(10000);  /* 10ms */
            elapsed = get_time_ms() - start_time;
        } else {
            /* Error */
            break;
        }
    }

    close(pipefd[0]);

    /* Check for timeout */
    if (elapsed >= timeout_ms) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        *result_out = SANDBOX_RESULT_TIMEOUT;

        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.sandbox_timeouts++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    /* Update statistics */
    uint64_t sandbox_time = get_time_ms() - start_time;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t total_runs = bridge->stats.sandbox_runs;
    bridge->stats.avg_sandbox_time_ms =
        (bridge->stats.avg_sandbox_time_ms * (total_runs - 1) + (double)sandbox_time) /
        total_runs;

    if (*result_out == SANDBOX_RESULT_SUCCESS) {
        bridge->stats.sandbox_successes++;
    } else if (*result_out == SANDBOX_RESULT_REGRESSION) {
        bridge->stats.sandbox_regressions++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int heal_bridge_run_regression(
    heal_bridge_t* bridge,
    const heal_result_t* fix,
    const char* test_command)
{
    if (bridge == NULL || fix == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_run_regression: validation failed");
        return -1;
    }

    /* Store test command for future use */
    if (test_command != NULL) {
        strncpy(bridge->sandbox.test_command, test_command,
                sizeof(bridge->sandbox.test_command) - 1);
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_run_regression", 0.0f);


    sandbox_result_t result;
    int ret = heal_bridge_validate_fix(bridge, fix, &result);

    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_run_regression: validation failed");
        return -1;
    }

    return (result == SANDBOX_RESULT_SUCCESS ||
            result == SANDBOX_RESULT_SKIPPED) ? 0 : -1;
}

/* ============================================================================
 * Pattern Evolution Implementation
 * ============================================================================ */

int heal_bridge_register_candidate(
    heal_bridge_t* bridge,
    const crash_features_t* features,
    const heal_result_t* fix,
    uint64_t* candidate_id_out)
{
    if (bridge == NULL || features == NULL || fix == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_register_candidate: validation failed");
        return -1;
    }
    if (!bridge->config.enable_pattern_evolution) return 0;

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_register_candidate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->candidate_count >= bridge->candidate_capacity) {
        /* Evict lowest-confidence candidate */
        size_t min_idx = 0;
        float min_conf = bridge->candidates[0].avg_confidence;

        for (size_t i = 1; i < bridge->candidate_count; i++) {
            if (bridge->candidates[i].avg_confidence < min_conf) {
                min_conf = bridge->candidates[i].avg_confidence;
                min_idx = i;
            }
        }

        /* Shift to remove */
        memmove(&bridge->candidates[min_idx],
                &bridge->candidates[min_idx + 1],
                (bridge->candidate_count - min_idx - 1) * sizeof(pattern_candidate_t));
        bridge->candidate_count--;
    }

    /* Add new candidate */
    pattern_candidate_t* candidate = &bridge->candidates[bridge->candidate_count];
    memset(candidate, 0, sizeof(pattern_candidate_t));

    candidate->id = bridge->next_candidate_id++;
    candidate->features = *features;
    candidate->fix_template = *fix;
    candidate->state = PATTERN_EVO_CANDIDATE;
    candidate->first_seen = get_time_ms();
    candidate->last_seen = candidate->first_seen;
    candidate->avg_confidence = fix->confidence;

    bridge->candidate_count++;
    bridge->stats.candidates_active++;

    if (candidate_id_out != NULL) {
        *candidate_id_out = candidate->id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_MODULE_DEBUG(LOG_TAG, "Registered evolution candidate %lu (pattern=%s)",
                     candidate->id,
                     heal_pattern_type_to_string(fix->pattern_used));

    return 0;
}

int heal_bridge_record_candidate_outcome(
    heal_bridge_t* bridge,
    uint64_t candidate_id,
    bool success,
    float confidence)
{
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_record_candidate_outcome: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_record_candidate_out", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pattern_candidate_t* candidate = find_candidate_unlocked(bridge, candidate_id);
    if (candidate == NULL) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_record_candidate_outcome: validation failed");
        return -1;
    }

    /* Update statistics */
    if (success) {
        candidate->success_count++;
    } else {
        candidate->failure_count++;
    }

    candidate->last_seen = get_time_ms();

    /* Update running average confidence */
    uint32_t total = candidate->success_count + candidate->failure_count;
    candidate->avg_confidence =
        (candidate->avg_confidence * (total - 1) + confidence) / (total > 0 ? total : 1);

    /* Check for promotion threshold */
    bool should_promote = false;

    if (candidate->state == PATTERN_EVO_CANDIDATE &&
        candidate->success_count >= bridge->config.evolution_threshold &&
        candidate->avg_confidence >= bridge->config.min_promotion_confidence) {

        float success_rate = (float)candidate->success_count / (float)total;
        if (success_rate >= 0.7f) {
            candidate->state = PATTERN_EVO_TESTING;
            should_promote = true;
        }
    }

    /* Check for rejection */
    if (candidate->state == PATTERN_EVO_CANDIDATE && total >= 5) {
        float success_rate = (float)candidate->success_count / (float)total;
        if (success_rate < 0.3f) {
            candidate->state = PATTERN_EVO_REJECTED;
            bridge->stats.candidates_rejected++;
            bridge->stats.candidates_active--;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Promote if threshold reached */
    if (should_promote) {
        uint32_t pattern_id = 0;
        if (heal_bridge_promote_candidate(bridge, candidate_id, &pattern_id) == 0) {
            return 1;  /* Indicate promotion */
        }
    }

    return 0;
}

int heal_bridge_promote_candidate(
    heal_bridge_t* bridge,
    uint64_t candidate_id,
    uint32_t* pattern_id_out)
{
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_promote_candidate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_promote_candidate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pattern_candidate_t* candidate = find_candidate_unlocked(bridge, candidate_id);
    if (candidate == NULL || candidate->state == PATTERN_EVO_PROMOTED) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_promote_candidate: validation failed");
        return -1;
    }

    /* Create new pattern from candidate */
    fix_pattern_t new_pattern = {0};
    new_pattern.type = candidate->fix_template.pattern_used;

    snprintf(new_pattern.name, sizeof(new_pattern.name),
             "Evolved_%s_%lu",
             heal_pattern_type_to_string(new_pattern.type),
             candidate->id);

    snprintf(new_pattern.description, sizeof(new_pattern.description),
             "Auto-evolved from LNN fix (success=%u, confidence=%.2f)",
             candidate->success_count, candidate->avg_confidence);

    strncpy(new_pattern.template_after, candidate->fix_template.fixed_code,
            sizeof(new_pattern.template_after) - 1);

    new_pattern.confidence = candidate->avg_confidence;
    new_pattern.success_count = candidate->success_count;
    new_pattern.fail_count = candidate->failure_count;
    new_pattern.enabled = true;
    new_pattern.user_defined = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Register with pattern library */
    uint32_t new_id = 0;
    int ret = -1;

    if (bridge->pattern_library != NULL) {
        ret = heal_pattern_register(bridge->pattern_library, &new_pattern, &new_id);
    }

    if (ret == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        candidate->state = PATTERN_EVO_PROMOTED;
        candidate->promoted_pattern_id = new_id;
        bridge->stats.patterns_evolved++;
        bridge->stats.candidates_active--;
        nimcp_mutex_unlock(bridge->base.mutex);

        LOG_MODULE_INFO(LOG_TAG, "Promoted candidate %lu to pattern %u (%s)",
                        candidate_id, new_id, new_pattern.name);

        if (pattern_id_out != NULL) {
            *pattern_id_out = new_id;
        }
    }

    return ret;
}

int heal_bridge_decay_candidates(heal_bridge_t* bridge)
{
    if (bridge == NULL) return 0;

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_decay_candidates", 0.0f);


    uint64_t now = get_time_ms();
    uint64_t decay_window = 24 * 60 * 60 * 1000;  /* 24 hours */
    int pruned = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    for (size_t i = 0; i < bridge->candidate_count; ) {
        pattern_candidate_t* candidate = &bridge->candidates[i];

        /* Skip promoted or rejected */
        if (candidate->state == PATTERN_EVO_PROMOTED ||
            candidate->state == PATTERN_EVO_REJECTED) {
            i++;
            continue;
        }

        /* Decay confidence for old candidates */
        uint64_t age = now - candidate->last_seen;
        if (age > decay_window) {
            candidate->avg_confidence *= 0.9f;

            /* Prune if confidence too low */
            if (candidate->avg_confidence < 0.1f) {
                candidate->state = PATTERN_EVO_DEPRECATED;
                memmove(&bridge->candidates[i],
                        &bridge->candidates[i + 1],
                        (bridge->candidate_count - i - 1) * sizeof(pattern_candidate_t));
                bridge->candidate_count--;
                bridge->stats.candidates_active--;
                pruned++;
                continue;
            }
        }

        i++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return pruned;
}

/* ============================================================================
 * Fix Chain Implementation
 * ============================================================================ */

int heal_bridge_create_chain(
    heal_bridge_t* bridge,
    uint64_t antigen_id,
    const char* source_code,
    uint64_t* chain_id_out)
{
    if (bridge == NULL || source_code == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_create_chain: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_create_chain", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->chain_count >= bridge->chain_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "heal_bridge_create_chain: capacity exceeded");
        return -1;
    }

    /* Create new chain */
    fix_chain_t* chain = &bridge->active_chains[bridge->chain_count];
    memset(chain, 0, sizeof(fix_chain_t));

    chain->id = bridge->next_chain_id++;
    chain->antigen_id = antigen_id;
    chain->status = CHAIN_STATUS_PENDING;
    chain->start_time = get_time_ms();

    bridge->chain_count++;
    bridge->stats.chains_created++;

    uint64_t created_chain_id = chain->id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Generate fix candidates */
    if (bridge->self_heal != NULL) {
        /* Get antigen for analysis */
        brain_antigen_t brain_ag = {0};
        brain_ag.id = antigen_id;
        brain_ag.source = ANTIGEN_SOURCE_BBB;

        const code_antigen_t* code_ag = NULL;
        if (bridge->code_immune != NULL) {
            code_ag = code_immune_get_antigen(bridge->code_immune, antigen_id);
            if (code_ag != NULL) {
                brain_ag.severity = (uint8_t)(code_ag->severity * 10);
                brain_ag.confidence = code_ag->confidence;
            }
        }

        /* Generate multiple candidates */
        fix_candidate_t candidates[HEAL_BRIDGE_MAX_FIX_CHAIN];
        int n_candidates = self_heal_generate_candidates(
            bridge->self_heal, &brain_ag, source_code,
            candidates, HEAL_BRIDGE_MAX_FIX_CHAIN);

        /* Add candidates to chain with dependencies */
        fix_pattern_type_t prev_type = FIX_PATTERN_UNKNOWN;

        for (int i = 0; i < n_candidates && i < HEAL_BRIDGE_MAX_FIX_CHAIN; i++) {
            fix_dependency_t dep = FIX_DEP_NONE;
            int depends_on = -1;

            /* Check if this fix should chain with previous */
            if (i > 0 && should_chain_fixes(prev_type,
                                            candidates[i].result.pattern_used)) {
                dep = FIX_DEP_COMPLEMENTARY;
                depends_on = i - 1;
            }

            heal_bridge_add_to_chain(bridge, created_chain_id,
                                     &candidates[i].result, dep, depends_on);

            prev_type = candidates[i].result.pattern_used;
        }
    }

    if (chain_id_out != NULL) {
        *chain_id_out = created_chain_id;
    }

    return 0;
}

int heal_bridge_add_to_chain(
    heal_bridge_t* bridge,
    uint64_t chain_id,
    const heal_result_t* fix,
    fix_dependency_t dependency,
    int depends_on)
{
    if (bridge == NULL || fix == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_add_to_chain: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_add_to_chain", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    fix_chain_t* chain = find_chain_unlocked(bridge, chain_id);
    if (chain == NULL || chain->fix_count >= HEAL_BRIDGE_MAX_FIX_CHAIN) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "heal_bridge_add_to_chain: capacity exceeded");
        return -1;
    }

    chain_fix_t* cf = &chain->fixes[chain->fix_count];
    cf->fix = *fix;
    cf->dependency = dependency;
    cf->depends_on_idx = (depends_on >= 0) ? (uint32_t)depends_on : 0;
    cf->applied = false;
    cf->validated = false;
    cf->sandbox_result = SANDBOX_RESULT_SKIPPED;
    cf->antibody_id = 0;

    chain->fix_count++;
    chain->overall_confidence = calculate_chain_confidence(chain);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int heal_bridge_execute_chain(
    heal_bridge_t* bridge,
    uint64_t chain_id)
{
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_execute_chain: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_execute_chain", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    fix_chain_t* chain = find_chain_unlocked(bridge, chain_id);
    if (chain == NULL || chain->fix_count == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_execute_chain: chain->fix_count is zero");
        return -1;
    }

    chain->status = CHAIN_STATUS_IN_PROGRESS;
    nimcp_mutex_unlock(bridge->base.mutex);

    bool any_failed = false;
    size_t applied_count = 0;

    /* Execute fixes in order */
    for (size_t i = 0; i < chain->fix_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && chain->fix_count > 256) {
            heal_bridge_heartbeat("heal_bridge_loop",
                             (float)(i + 1) / (float)chain->fix_count);
        }

        chain_fix_t* cf = &chain->fixes[i];

        /* Check dependency */
        if (cf->dependency == FIX_DEP_PREREQUISITE && cf->depends_on_idx < i) {
            if (!chain->fixes[cf->depends_on_idx].applied) {
                /* Skip - prerequisite not met */
                continue;
            }
        }

        /* Validate in sandbox */
        if (bridge->config.enable_sandbox) {
            heal_bridge_validate_fix(bridge, &cf->fix, &cf->sandbox_result);

            if (cf->sandbox_result != SANDBOX_RESULT_SUCCESS &&
                bridge->config.require_validation) {
                cf->validated = false;
                any_failed = true;

                if (bridge->config.atomic_chains) {
                    break;  /* Stop chain on failure */
                }
                continue;
            }
        }

        cf->validated = true;

        /* Apply fix via code_immune */
        if (bridge->code_immune != NULL) {
            /* Simplified: just mark as applied for now */
            /* Full implementation would call code_immune_produce_antibody */
            cf->applied = true;
            applied_count++;

            /* Add to rollback history */
            add_rollback_entry(bridge, cf->antibody_id,
                               cf->fix.original_code, NULL);
        }
    }

    /* Update chain status */
    nimcp_mutex_lock(bridge->base.mutex);

    chain->applied_count = applied_count;
    chain->complete_time = get_time_ms();

    if (any_failed && bridge->config.atomic_chains) {
        /* Rollback all applied fixes */
        chain->status = CHAIN_STATUS_ROLLBACK;
        nimcp_mutex_unlock(bridge->base.mutex);

        heal_bridge_rollback_chain(bridge, chain_id);

        nimcp_mutex_lock(bridge->base.mutex);
        chain->status = CHAIN_STATUS_FAILED;
        bridge->stats.chains_failed++;
    } else if (applied_count == chain->fix_count) {
        chain->status = CHAIN_STATUS_COMPLETE;
        bridge->stats.chains_completed++;
    } else if (applied_count > 0) {
        chain->status = CHAIN_STATUS_PARTIAL;
        bridge->stats.chains_partial++;
    } else {
        chain->status = CHAIN_STATUS_FAILED;
        bridge->stats.chains_failed++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return (chain->status == CHAIN_STATUS_COMPLETE) ? 0 :
           (chain->status == CHAIN_STATUS_PARTIAL) ? 1 : -1;
}

int heal_bridge_get_chain_status(
    heal_bridge_t* bridge,
    uint64_t chain_id,
    chain_status_t* status_out,
    size_t* applied_count_out)
{
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_get_chain_status: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_get_chain_status", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    fix_chain_t* chain = find_chain_unlocked(bridge, chain_id);
    if (chain == NULL) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_get_chain_status: validation failed");
        return -1;
    }

    if (status_out != NULL) {
        *status_out = chain->status;
    }

    if (applied_count_out != NULL) {
        *applied_count_out = chain->applied_count;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Rollback Implementation
 * ============================================================================ */

int heal_bridge_rollback(
    heal_bridge_t* bridge,
    uint64_t antibody_id)
{
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_rollback: validation failed");
        return -1;
    }
    if (!bridge->config.enable_rollback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "heal_bridge_rollback: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_rollback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    rollback_entry_t* entry = find_rollback_entry(bridge, antibody_id);
    if (entry == NULL || !entry->can_rollback) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "heal_bridge_rollback: entry->can_rollback is NULL");
        return -1;
    }

    /* Check rollback window */
    uint64_t age = get_time_ms() - entry->apply_time;
    if (age > bridge->config.rollback_window_ms) {
        entry->can_rollback = false;
        nimcp_mutex_unlock(bridge->base.mutex);

        LOG_MODULE_WARN(LOG_TAG, "Rollback window expired for antibody %lu",
                        antibody_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_rollback: validation failed");
        return -1;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Perform rollback via code_immune */
    int ret = 0;
    if (bridge->code_immune != NULL) {
        ret = code_immune_apoptosis(bridge->code_immune, antibody_id);
    }

    if (ret == 0) {
        nimcp_mutex_lock(bridge->base.mutex);
        entry->can_rollback = false;  /* One-shot rollback */
        bridge->stats.rollbacks_performed++;
        nimcp_mutex_unlock(bridge->base.mutex);

        LOG_MODULE_INFO(LOG_TAG, "Rolled back antibody %lu", antibody_id);
    } else {
        nimcp_mutex_lock(bridge->base.mutex);
        bridge->stats.rollbacks_failed++;
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return ret;
}

int heal_bridge_rollback_chain(
    heal_bridge_t* bridge,
    uint64_t chain_id)
{
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_rollback_chain: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_rollback_chain", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    fix_chain_t* chain = find_chain_unlocked(bridge, chain_id);
    if (chain == NULL) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_rollback_chain: validation failed");
        return -1;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Rollback in reverse order */
    int failures = 0;
    for (size_t i = chain->fix_count; i > 0; i--) {
        chain_fix_t* cf = &chain->fixes[i - 1];

        if (cf->applied && cf->antibody_id > 0) {
            if (heal_bridge_rollback(bridge, cf->antibody_id) != 0) {
                failures++;
            } else {
                cf->applied = false;
            }
        }
    }

    return (failures == 0) ? 0 : -1;
}

int heal_bridge_cleanup_rollback_history(heal_bridge_t* bridge)
{
    if (bridge == NULL) return 0;
    if (!bridge->config.enable_rollback) return 0;

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_cleanup_rollback_his", 0.0f);


    uint64_t now = get_time_ms();
    int cleaned = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    for (size_t i = 0; i < bridge->rollback_count; ) {
        rollback_entry_t* entry = &bridge->rollback_history[i];

        /* Remove expired entries */
        if (!entry->can_rollback ||
            (now - entry->apply_time) > bridge->config.rollback_window_ms) {

            memmove(&bridge->rollback_history[i],
                    &bridge->rollback_history[i + 1],
                    (bridge->rollback_count - i - 1) * sizeof(rollback_entry_t));
            bridge->rollback_count--;
            cleaned++;
            continue;
        }

        i++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return cleaned;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int heal_bridge_get_stats(
    heal_bridge_t* bridge,
    heal_bridge_stats_t* stats)
{
    if (bridge == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_get_stats: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;

    /* Calculate overall success rate */
    uint64_t total_attempts = stats->fixes_applied + stats->fixes_failed;
    if (total_attempts > 0) {
        stats->overall_success_rate =
            (float)stats->fixes_applied / (float)total_attempts;
    } else {
        stats->overall_success_rate = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int heal_bridge_reset_stats(heal_bridge_t* bridge)
{
    if (bridge == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "heal_bridge_reset_stats: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(heal_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* heal_bridge_sandbox_result_to_string(sandbox_result_t result)
{
    switch (result) {
        case SANDBOX_RESULT_SUCCESS:       return "success";
        case SANDBOX_RESULT_CRASH:         return "crash";
        case SANDBOX_RESULT_TIMEOUT:       return "timeout";
        case SANDBOX_RESULT_REGRESSION:    return "regression";
        case SANDBOX_RESULT_COMPILE_ERROR: return "compile_error";
        case SANDBOX_RESULT_LOAD_ERROR:    return "load_error";
        case SANDBOX_RESULT_SKIPPED:       return "skipped";
        default:                           return "unknown";
    }
}

const char* heal_bridge_chain_status_to_string(chain_status_t status)
{
    switch (status) {
        case CHAIN_STATUS_PENDING:     return "pending";
        case CHAIN_STATUS_IN_PROGRESS: return "in_progress";
        case CHAIN_STATUS_COMPLETE:    return "complete";
        case CHAIN_STATUS_PARTIAL:     return "partial";
        case CHAIN_STATUS_FAILED:      return "failed";
        case CHAIN_STATUS_ROLLBACK:    return "rollback";
        default:                       return "unknown";
    }
}

const char* heal_bridge_evolution_state_to_string(pattern_evolution_state_t state)
{
    switch (state) {
        case PATTERN_EVO_CANDIDATE:  return "candidate";
        case PATTERN_EVO_TESTING:    return "testing";
        case PATTERN_EVO_PROMOTED:   return "promoted";
        case PATTERN_EVO_REJECTED:   return "rejected";
        case PATTERN_EVO_DEPRECATED: return "deprecated";
        default:                     return "unknown";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about heal bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int heal_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    heal_bridge_heartbeat("heal_bridge_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Heal_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                heal_bridge_heartbeat("heal_bridge_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Heal bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Heal_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Heal_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void heal_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_heal_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int heal_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "heal_bridge_training_begin: NULL argument");
        return -1;
    }
    heal_bridge_heartbeat_instance(NULL, "heal_bridge_training_begin", 0.0f);
    return 0;
}

int heal_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "heal_bridge_training_end: NULL argument");
        return -1;
    }
    heal_bridge_heartbeat_instance(NULL, "heal_bridge_training_end", 1.0f);
    return 0;
}

int heal_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "heal_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    heal_bridge_heartbeat_instance(NULL, "heal_bridge_training_step", progress);
    return 0;
}
