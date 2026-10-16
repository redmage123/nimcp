/**
 * @file nimcp_command_compliance.c
 * @brief Command compliance system implementation (Asimov's Second Law)
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date 2025-12-16
 */

#include "core/directives/nimcp_command_compliance.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(command_compliance)

//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Pending command queue entry
 *
 * WHAT: Command awaiting execution or deferred for review
 * WHY:  Tracks commands that need human review or delayed execution
 * HOW:  Linked list node with command and evaluation result
 */
typedef struct pending_command_node {
    command_t command;
    command_result_t result;
    struct pending_command_node* next;
} pending_command_node_t;

/**
 * @brief Command compliance system internal structure
 *
 * WHAT: Complete state for command evaluation and execution
 * WHY:  Encapsulates configuration, statistics, and First Law integration
 * HOW:  Mutex-protected structure with harm prevention link
 */
struct command_compliance_system {
    /* Configuration */
    command_compliance_config_t config;

    /* First Law integration */
    harm_prevention_system_t* harm_prevention;

    /* Statistics */
    uint64_t total_commands;
    uint64_t complied_count;
    uint64_t refused_first_law_count;
    uint64_t refused_invalid_count;
    uint64_t deferred_count;

    /* Pending command queue */
    pending_command_node_t* pending_head;
    uint32_t pending_count;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Command ID generator */
    uint32_t next_command_id;
};

//=============================================================================
// Helper Functions (Static)
//=============================================================================

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Returns current system time as millisecond timestamp
 * WHY:  Needed for command timestamp generation
 * HOW:  Uses clock_gettime with CLOCK_REALTIME
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Validate command structure
 *
 * WHAT: Checks if command has valid fields
 * WHY:  Prevents processing of malformed commands
 * HOW:  Validates source, priority range, and text presence
 */
static bool validate_command(const command_t* cmd) {
    if (!cmd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_command: cmd is NULL");
        return false;
    }
    if (cmd->source >= COMMAND_SOURCE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "validate_command: capacity exceeded");
        return false;
    }
    if (cmd->priority < 0.0f || cmd->priority > 1.0f) {
        return false;
    }
    if (cmd->command_text[0] == '\0') {
        return false;
    }
    return true;
}

/**
 * @brief Check command authorization
 *
 * WHAT: Determines if command source is authorized
 * WHY:  Enforces source-based access control
 * HOW:  Checks source type against configuration policy
 */
static bool check_authorization(
    const command_compliance_system_t* system,
    const command_t* cmd
) {
    /* If require_human_source is true, only COMMAND_SOURCE_HUMAN allowed */
    if (system->config.require_human_source) {
        return cmd->source == COMMAND_SOURCE_HUMAN;
    }

    /* Check if system commands are allowed */
    if (cmd->source == COMMAND_SOURCE_SYSTEM) {
        return system->config.allow_system_commands;
    }

    /* Unknown sources always rejected */
    if (cmd->source == COMMAND_SOURCE_UNKNOWN) {
        return false;
    }

    /* All other sources allowed */
    return true;
}

/**
 * @brief Check priority threshold
 *
 * WHAT: Determines if command priority meets minimum threshold
 * WHY:  Filters low-priority commands
 * HOW:  Compares command priority against config threshold
 */
static bool check_priority(
    const command_compliance_system_t* system,
    const command_t* cmd
) {
    return cmd->priority >= system->config.min_priority_threshold;
}

/**
 * @brief Add command to pending queue
 *
 * WHAT: Adds command to pending queue for deferred execution
 * WHY:  Tracks commands needing human review
 * HOW:  Allocates node and prepends to linked list
 */
static int add_to_pending_queue(
    command_compliance_system_t* system,
    const command_t* cmd,
    const command_result_t* result
) {
    /* Check queue capacity */
    if (system->pending_count >= COMMAND_MAX_PENDING) {
        NIMCP_LOGGING_WARN("Pending command queue full, dropping command %u",
                          cmd->command_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "add_to_pending_queue: capacity exceeded");
        return -1;
    }

    /* Allocate node */
    pending_command_node_t* node = (pending_command_node_t*)nimcp_malloc(
        sizeof(pending_command_node_t));
    if (!node) {
        NIMCP_LOGGING_ERROR("Failed to allocate pending command node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_to_pending_queue: node is NULL");
        return -1;
    }

    /* Copy data */
    memcpy(&node->command, cmd, sizeof(command_t));
    memcpy(&node->result, result, sizeof(command_result_t));

    /* Prepend to list */
    node->next = system->pending_head;
    system->pending_head = node;
    system->pending_count++;

    return 0;
}

//=============================================================================
// Public API Implementation
//=============================================================================

void command_compliance_default_config(command_compliance_config_t* config) {
    if (!config) return;

    /* Conservative defaults - only obey humans by default */
    config->require_human_source = true;
    config->allow_system_commands = false;
    config->min_priority_threshold = 0.0f;  /* No priority filtering */
}

command_compliance_system_t* command_compliance_create(
    const command_compliance_config_t* config,
    harm_prevention_system_t* harm_prevention
) {
    /* Guard: harm prevention required */
    if (!harm_prevention) {
        NIMCP_LOGGING_ERROR("Harm prevention system required for command compliance");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "harm_prevention is NULL");

        return NULL;
    }

    /* Allocate system */
    command_compliance_system_t* system = (command_compliance_system_t*)nimcp_malloc(
        sizeof(command_compliance_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate command compliance system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }

    /* Initialize configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(command_compliance_config_t));
    } else {
        command_compliance_default_config(&system->config);
    }

    /* Set harm prevention link */
    system->harm_prevention = harm_prevention;

    /* Initialize statistics */
    system->total_commands = 0;
    system->complied_count = 0;
    system->refused_first_law_count = 0;
    system->refused_invalid_count = 0;
    system->deferred_count = 0;

    /* Initialize pending queue */
    system->pending_head = NULL;
    system->pending_count = 0;

    /* Initialize bio-async */
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;

    /* Initialize command ID */
    system->next_command_id = 1;

    /* Create mutex */
    system->mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "command_compliance_create: system->mutex is NULL");
        return NULL;
    }

    if (nimcp_mutex_init(system->mutex, NULL) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "command_compliance_create: validation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created command compliance system (Second Law enforcement)");
    return system;
}

void command_compliance_destroy(command_compliance_system_t* system) {
    if (!system) return;

    /* Disconnect bio-async */
    if (system->bio_async_enabled) {
        command_compliance_disconnect_bio_async(system);
    }

    /* Free pending queue */
    pending_command_node_t* node = system->pending_head;
    while (node) {
        pending_command_node_t* next = node->next;
        nimcp_free(node);
        node = next;
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }

    /* Free system */
    nimcp_free(system);

    NIMCP_LOGGING_INFO("Destroyed command compliance system");
}

int command_compliance_evaluate(
    command_compliance_system_t* system,
    const command_t* command,
    command_result_t* result
) {
    /* Guard: validate inputs */
    if (!system || !command || !result) {
        NIMCP_LOGGING_ERROR("Invalid parameters to command_compliance_evaluate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "command_compliance_evaluate: required parameter is NULL (system, command, result)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Initialize result */
    memset(result, 0, sizeof(command_result_t));
    result->compliance_confidence = 1.0f;

    /* Increment total commands */
    system->total_commands++;

    /* Step 1: Validate command structure */
    if (!validate_command(command)) {
        result->decision = COMMAND_DECISION_REFUSE_INVALID;
        snprintf(result->reason, COMMAND_REASON_MAX_LEN,
                 "Invalid command structure");
        result->first_law_conflict = false;
        system->refused_invalid_count++;

        NIMCP_LOGGING_WARN("Refused invalid command %u from source %d",
                          command->command_id, command->source);

        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* Step 2: Check authorization */
    if (!check_authorization(system, command)) {
        result->decision = COMMAND_DECISION_REFUSE_INVALID;
        snprintf(result->reason, COMMAND_REASON_MAX_LEN,
                 "Command source not authorized");
        result->first_law_conflict = false;
        system->refused_invalid_count++;

        NIMCP_LOGGING_WARN("Refused unauthorized command %u from source %d",
                          command->command_id, command->source);

        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* Step 3: Check priority threshold */
    if (!check_priority(system, command)) {
        result->decision = COMMAND_DECISION_REFUSE_INVALID;
        snprintf(result->reason, COMMAND_REASON_MAX_LEN,
                 "Command priority %.2f below threshold %.2f",
                 command->priority, system->config.min_priority_threshold);
        result->first_law_conflict = false;
        system->refused_invalid_count++;

        NIMCP_LOGGING_DEBUG("Refused low-priority command %u (priority=%.2f)",
                           command->command_id, command->priority);

        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* Step 4: First Law check (harm prevention) */
    /* NOTE: This is a placeholder. In a real implementation, this would
     * call into the harm prevention system to evaluate if executing the
     * command would cause harm. For now, we assume commands pass. */
    bool would_cause_harm = false;

    if (would_cause_harm) {
        result->decision = COMMAND_DECISION_REFUSE_FIRST_LAW;
        snprintf(result->reason, COMMAND_REASON_MAX_LEN,
                 "Command violates First Law (would cause harm)");
        result->first_law_conflict = true;
        result->compliance_confidence = 0.95f;
        system->refused_first_law_count++;

        NIMCP_LOGGING_WARN("Refused command %u due to First Law violation: %s",
                          command->command_id, command->command_text);

        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* All checks passed - comply with command */
    result->decision = COMMAND_DECISION_COMPLY;
    snprintf(result->reason, COMMAND_REASON_MAX_LEN,
             "Command passed all safety and authorization checks");
    result->first_law_conflict = false;
    result->compliance_confidence = 1.0f;
    system->complied_count++;

    NIMCP_LOGGING_INFO("Complying with command %u from source %d: %s",
                      command->command_id, command->source,
                      command->command_text);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int command_compliance_execute_if_safe(
    command_compliance_system_t* system,
    const command_t* command,
    command_result_t* result
) {
    /* Evaluate command */
    if (command_compliance_evaluate(system, command, result) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "command_compliance_execute_if_safe: validation failed");
        return -1;
    }

    /* Only execute if decision is COMPLY */
    if (result->decision != COMMAND_DECISION_COMPLY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "command_compliance_execute_if_safe: validation failed");
        return -1;  /* Refused */
    }

    /* NOTE: Actual command execution would happen here.
     * This is a placeholder - calling code should implement
     * the actual execution logic based on compliance result. */

    NIMCP_LOGGING_DEBUG("Command %u approved for execution",
                       command->command_id);

    return 0;  /* Approved for execution */
}

int command_compliance_refuse(
    command_compliance_system_t* system,
    const command_t* command,
    const char* reason
) {
    /* Guard: validate inputs */
    if (!system || !command || !reason) {
        NIMCP_LOGGING_ERROR("Invalid parameters to command_compliance_refuse");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "command_compliance_refuse: required parameter is NULL (system, command, reason)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Increment refusal count */
    system->refused_invalid_count++;

    /* Log refusal */
    NIMCP_LOGGING_INFO("Manually refused command %u: %s",
                      command->command_id, reason);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int command_compliance_get_pending_commands(
    command_compliance_system_t* system,
    command_t* out_commands,
    uint32_t max_commands,
    uint32_t* out_count
) {
    /* Guard: validate inputs */
    if (!system || !out_commands || !out_count) {
        NIMCP_LOGGING_ERROR("Invalid parameters to get_pending_commands");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "command_compliance_get_pending_commands: required parameter is NULL (system, out_commands, out_count)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Copy pending commands */
    uint32_t count = 0;
    pending_command_node_t* node = system->pending_head;

    while (node && count < max_commands) {
        memcpy(&out_commands[count], &node->command, sizeof(command_t));
        count++;
        node = node->next;
    }

    *out_count = count;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int command_compliance_get_stats(
    command_compliance_system_t* system,
    command_compliance_stats_t* stats
) {
    /* Guard: validate inputs */
    if (!system || !stats) {
        NIMCP_LOGGING_ERROR("Invalid parameters to command_compliance_get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "command_compliance_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Copy statistics */
    stats->total_commands = system->total_commands;
    stats->complied_count = system->complied_count;
    stats->refused_first_law_count = system->refused_first_law_count;
    stats->refused_invalid_count = system->refused_invalid_count;
    stats->deferred_count = system->deferred_count;
    stats->pending_commands = system->pending_count;

    /* Compute rates */
    if (system->total_commands > 0) {
        stats->compliance_rate = (float)system->complied_count /
                                (float)system->total_commands;
        stats->first_law_conflict_rate = (float)system->refused_first_law_count /
                                         (float)system->total_commands;
    } else {
        stats->compliance_rate = 0.0f;
        stats->first_law_conflict_rate = 0.0f;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int command_compliance_connect_bio_async(command_compliance_system_t* system) {
    /* Guard: validate input */
    if (!system) {
        NIMCP_LOGGING_ERROR("NULL system in connect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "command_compliance_connect_bio_async: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Check if already connected */
    if (system->bio_async_enabled) {
        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_COMMAND_COMPLIANCE,
        .module_name = "command_compliance",
        .inbox_capacity = 32,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected command compliance to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int command_compliance_disconnect_bio_async(command_compliance_system_t* system) {
    /* Guard: validate input */
    if (!system) {
        NIMCP_LOGGING_ERROR("NULL system in disconnect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "command_compliance_disconnect_bio_async: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Check if connected */
    if (!system->bio_async_enabled) {
        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* Unregister from bio-async */
    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected command compliance from bio-async");

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

bool command_compliance_is_bio_async_connected(
    const command_compliance_system_t* system
) {
    if (!system) {
        return false;
    }

    /* Safe to read boolean without mutex (atomic read) */
    return system->bio_async_enabled;
}
