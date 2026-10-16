#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_portia_logic_bridge.c - Portia-Logic Bridge Implementation
//=============================================================================

#include "portia/nimcp_portia_logic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_logic_bridge)

/*=============================================================================
 * TIME HELPER
 *============================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

#define PORTIA_LOGIC_DEFAULT_MAX_GATES 100
#define PORTIA_LOGIC_DEFAULT_MAX_CUSTOM_RULES 50
#define PORTIA_LOGIC_DEFAULT_DECISION_THRESHOLD 0.7f
#define PORTIA_LOGIC_DEFAULT_TIMEOUT_MS NIMCP_FAST_TIMEOUT_MS

/* Pre-built gate IDs */
#define GATE_ID_TIER_UPGRADE 1
#define GATE_ID_TIER_DOWNGRADE 2
#define GATE_ID_DEGRADATION 3
#define GATE_ID_ALLOCATION 4
#define GATE_ID_EMERGENCY_DETECT 5
#define GATE_ID_CUSTOM_START 100

/*=============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/**
 * @brief Main bridge structure
 */
struct portia_logic_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Neural logic network */
    neural_logic_network_t logic_network;

    /* Portia integration */
    portia_context_t* portia;

    /* Pre-built decision gates */
    uint32_t tier_upgrade_gate;      /**< AND gate for upgrade conditions */
    uint32_t tier_downgrade_gate;    /**< OR gate for downgrade triggers */
    uint32_t degradation_gate;       /**< IMPLIES gate for feature disable */
    uint32_t allocation_gate;        /**< AND gate for resource allocation */
    uint32_t emergency_gate;         /**< OR gate for emergency detection */

    /* Resource conditions cache */
    portia_resource_condition_t conditions;

    /* Configuration */
    portia_logic_config_t config;

    /* Integration */
    brain_t brain;
    void* immune_system;
    void* umm;

    /* Statistics */
    portia_logic_stats_t stats;

    /* Custom gates tracking */
    uint32_t next_custom_gate_id;
    uint32_t custom_gate_count;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Update resource conditions from Portia state
 *
 * WHAT: Synchronizes internal resource conditions with Portia
 * WHY:  Ensure logic gates operate on current data
 * HOW:  Queries Portia status, updates condition flags
 */
static int update_conditions_internal(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL && bridge->portia != NULL,
                      NIMCP_ERROR_NULL_POINTER, "bridge or portia is NULL");

    portia_status_t status;
    nimcp_error_t err = portia_get_status(&status);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    /* Update memory condition */
    bridge->conditions.memory_ok = (status.memory_usage < 0.85f);

    /* Update thermal condition */
    bridge->conditions.thermal_ok = (status.thermal_state <= PORTIA_THERMAL_WARM);

    /* Update battery condition */
    if (status.power_state == PORTIA_POWER_AC) {
        bridge->conditions.battery_ok = true;
    } else {
        bridge->conditions.battery_ok = (status.battery_level > 0.20f);
    }

    /* Update CPU condition */
    bridge->conditions.cpu_ok = (status.cpu_usage < 0.90f);

    /* Update accelerator availability */
    bridge->conditions.accelerator_available = (status.num_accelerators > 0);

    /* Update emergency mode */
    bridge->conditions.emergency_mode = (
        status.power_state == PORTIA_POWER_BATTERY_CRITICAL ||
        status.thermal_state == PORTIA_THERMAL_CRITICAL ||
        status.degradation_level >= PORTIA_DEGRADATION_EMERGENCY
    );

    /* Compute overall resource score */
    float score = 0.0f;
    score += bridge->conditions.memory_ok ? 0.25f : 0.0f;
    score += bridge->conditions.thermal_ok ? 0.25f : 0.0f;
    score += bridge->conditions.battery_ok ? 0.25f : 0.0f;
    score += bridge->conditions.cpu_ok ? 0.25f : 0.0f;
    bridge->conditions.resource_score = score;

    return NIMCP_SUCCESS;
}

/**
 * @brief Initialize pre-built decision gates
 *
 * WHAT: Creates standard decision gates for Portia logic
 * WHY:  Provide out-of-box decision logic
 * HOW:  Creates AND/OR/IMPLIES gates for common scenarios
 */
static int init_decision_gates(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL && bridge->logic_network != NULL,
                      NIMCP_ERROR_NULL_POINTER, "bridge or logic_network is NULL");

    /* Tier upgrade gate: memory_ok AND thermal_ok AND battery_ok */
    bridge->tier_upgrade_gate = neural_logic_create_gate(
        bridge->logic_network,
        LOGIC_GATE_AND,
        2.9f  /* Require all 3 inputs to be active (sum > 2.9) */
    );
    if (bridge->tier_upgrade_gate == UINT32_MAX) {
        NIMCP_LOGGING_ERROR("Failed to create tier upgrade gate");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Tier downgrade gate: memory_critical OR thermal_critical OR battery_critical */
    bridge->tier_downgrade_gate = neural_logic_create_gate(
        bridge->logic_network,
        LOGIC_GATE_OR,
        0.5f  /* Fire if any input active */
    );
    if (bridge->tier_downgrade_gate == UINT32_MAX) {
        NIMCP_LOGGING_ERROR("Failed to create tier downgrade gate");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Degradation gate: resource_critical IMPLIES disable_features */
    bridge->degradation_gate = neural_logic_create_gate(
        bridge->logic_network,
        LOGIC_GATE_IMPLIES,
        0.7f
    );
    if (bridge->degradation_gate == UINT32_MAX) {
        NIMCP_LOGGING_ERROR("Failed to create degradation gate");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Allocation gate: budget_available AND request_valid */
    bridge->allocation_gate = neural_logic_create_gate(
        bridge->logic_network,
        LOGIC_GATE_AND,
        1.5f
    );
    if (bridge->allocation_gate == UINT32_MAX) {
        NIMCP_LOGGING_ERROR("Failed to create allocation gate");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Emergency detection gate: critical_power OR critical_thermal */
    bridge->emergency_gate = neural_logic_create_gate(
        bridge->logic_network,
        LOGIC_GATE_OR,
        0.5f
    );
    if (bridge->emergency_gate == UINT32_MAX) {
        NIMCP_LOGGING_ERROR("Failed to create emergency gate");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    NIMCP_LOGGING_INFO("Initialized 5 pre-built decision gates");
    bridge->stats.active_gates = 5;

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - LIFECYCLE
 *============================================================================*/

void portia_logic_bridge_get_default_config(portia_logic_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(portia_logic_config_t));

    config->max_gates = PORTIA_LOGIC_DEFAULT_MAX_GATES;
    config->max_custom_rules = PORTIA_LOGIC_DEFAULT_MAX_CUSTOM_RULES;
    config->enable_bio_async = true;
    config->decision_threshold = PORTIA_LOGIC_DEFAULT_DECISION_THRESHOLD;
    config->evaluation_timeout_ms = PORTIA_LOGIC_DEFAULT_TIMEOUT_MS;
    config->enable_brain_integration = false;
    config->enable_immune_integration = false;
    config->enable_umm_integration = false;
    config->disable_auto_update = false;
}

portia_logic_bridge_t* portia_logic_bridge_create(
    const portia_logic_config_t* config,
    portia_context_t* portia)
{
    if (!portia) {
        NIMCP_LOGGING_ERROR("Portia context is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia is NULL");

        return NULL;
    }

    /* Use default config if not provided */
    portia_logic_config_t default_config;
    if (!config) {
        portia_logic_bridge_get_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge */
    portia_logic_bridge_t* bridge = (portia_logic_bridge_t*)nimcp_malloc(
        sizeof(portia_logic_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(portia_logic_bridge_t));

    /* Store config and portia reference */
    memcpy(&bridge->config, config, sizeof(portia_logic_config_t));
    bridge->portia = portia;

    /* Initialize bridge base (creates mutex, sets name) */
    if (bridge_base_init(&bridge->base, 0, "portia_logic") != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_logic_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Create neural logic network */
    neural_logic_config_t logic_config = neural_logic_default_config(config->max_gates);
    logic_config.enable_bio_async = config->enable_bio_async;
    logic_config.use_gpu = false;  /* CPU-only for portia decisions */

    bridge->logic_network = neural_logic_create(&logic_config);
    if (!bridge->logic_network) {
        NIMCP_LOGGING_ERROR("Failed to create neural logic network");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_logic_bridge_create: bridge->logic_network is NULL");
        return NULL;
    }

    /* Initialize decision gates */
    if (init_decision_gates(bridge) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to initialize decision gates");
        neural_logic_destroy(bridge->logic_network);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "portia_logic_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize custom gate tracking */
    bridge->next_custom_gate_id = GATE_ID_CUSTOM_START;
    bridge->custom_gate_count = 0;

    /* Update initial conditions (unless disabled for testing) */
    if (!bridge->config.disable_auto_update) {
        update_conditions_internal(bridge);
    }

    NIMCP_LOGGING_INFO("Created Portia-Logic bridge");

    return bridge;
}

void portia_logic_bridge_destroy(portia_logic_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        portia_logic_disconnect_bio_async(bridge);
    }

    /* Destroy neural logic network */
    if (bridge->logic_network) {
        neural_logic_destroy(bridge->logic_network);
    }

    /* Cleanup bridge base (destroys mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed Portia-Logic bridge");
}

int portia_logic_bridge_start(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async && !bridge->base.bio_async_enabled) {
        int result = portia_logic_connect_bio_async(bridge);
        if (result != NIMCP_SUCCESS) {
            NIMCP_LOGGING_WARN("Bio-async connection failed, continuing without it");
        }
    }

    /* Update conditions (unless disabled for testing) */
    if (!bridge->config.disable_auto_update) {
        update_conditions_internal(bridge);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Started Portia-Logic bridge");

    return NIMCP_SUCCESS;
}

int portia_logic_bridge_stop(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        portia_logic_disconnect_bio_async(bridge);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Stopped Portia-Logic bridge");

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - INTEGRATION
 *============================================================================*/

int portia_logic_connect_brain(portia_logic_bridge_t* bridge, brain_t brain) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->brain = brain;

    /* Connect brain to neural logic network for neuromodulation */
    if (bridge->logic_network && brain) {
        neural_logic_set_brain(bridge->logic_network, brain);
        NIMCP_LOGGING_INFO("Connected brain to Portia-Logic bridge");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int portia_logic_connect_immune(portia_logic_bridge_t* bridge, void* immune_system) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->immune_system = immune_system;
    NIMCP_LOGGING_INFO("Connected immune system to Portia-Logic bridge");

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int portia_logic_connect_umm(portia_logic_bridge_t* bridge, void* umm) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->umm = umm;
    NIMCP_LOGGING_INFO("Connected UMM to Portia-Logic bridge");

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - DECISION EVALUATION
 *============================================================================*/

bool portia_logic_can_upgrade_tier(
    portia_logic_bridge_t* bridge,
    uint8_t current_tier,
    uint8_t target_tier)
{
    if (!bridge || !bridge->logic_network) {
        return false;
    }

    if (target_tier <= current_tier) {
        return false;  /* Not an upgrade */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update conditions (unless disabled for testing) */
    if (!bridge->config.disable_auto_update) {
        update_conditions_internal(bridge);
    }

    /* Direct AND logic: all conditions must be true for upgrade */
    bool can_upgrade = bridge->conditions.memory_ok &&
                       bridge->conditions.thermal_ok &&
                       bridge->conditions.battery_ok;

    /* Update stats */
    bridge->stats.total_evaluations++;
    bridge->stats.tier_upgrade_decisions++;
    bridge->stats.successful_evaluations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return can_upgrade;
}

bool portia_logic_must_downgrade_tier(
    portia_logic_bridge_t* bridge,
    uint8_t current_tier)
{
    if (!bridge || !bridge->logic_network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_logic_must_downgrade_tier: required parameter is NULL (bridge, bridge->logic_network)");
        return false;
    }

    (void)current_tier;  /* Unused for now */

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update conditions (unless disabled for testing) */
    if (!bridge->config.disable_auto_update) {
        update_conditions_internal(bridge);
    }

    /* Direct OR logic: any critical condition triggers downgrade */
    bool must_downgrade = !bridge->conditions.memory_ok ||
                          !bridge->conditions.thermal_ok ||
                          !bridge->conditions.battery_ok;

    /* Update stats */
    bridge->stats.total_evaluations++;
    bridge->stats.tier_downgrade_decisions++;
    bridge->stats.successful_evaluations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return must_downgrade;
}

bool portia_logic_can_disable_feature(
    portia_logic_bridge_t* bridge,
    uint32_t feature_id)
{
    if (!bridge || !bridge->logic_network) {
        return false;
    }

    (void)feature_id;  /* For now, use generic logic */

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update conditions (unless disabled for testing) */
    if (!bridge->config.disable_auto_update) {
        update_conditions_internal(bridge);
    }

    /* IMPLIES logic: resource_critical → can_disable
     * Can only disable features when resources are critical (score < 0.5) */
    bool resource_critical = (bridge->conditions.resource_score < 0.5f);
    bool can_disable = resource_critical;

    /* Update stats */
    bridge->stats.total_evaluations++;
    bridge->stats.degradation_decisions++;
    bridge->stats.successful_evaluations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return can_disable;
}

bool portia_logic_can_allocate_resource(
    portia_logic_bridge_t* bridge,
    uint32_t target_id,
    float amount)
{
    if (!bridge || !bridge->logic_network) {
        return false;
    }

    (void)target_id;  /* For now, generic allocation logic */

    if (amount < 0.0f || amount > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_logic_can_allocate_resource: validation failed");
        return false;  /* Invalid amount */
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update conditions (unless disabled for testing) */
    if (!bridge->config.disable_auto_update) {
        update_conditions_internal(bridge);
    }

    /* AND logic: budget_available AND request_valid
     * Can allocate if resource score >= requested amount */
    bool budget_available = (bridge->conditions.resource_score >= amount);
    bool can_allocate = budget_available;  /* Request validity assumed true */

    /* Update stats */
    bridge->stats.total_evaluations++;
    bridge->stats.allocation_decisions++;
    bridge->stats.successful_evaluations++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return can_allocate;
}

/*=============================================================================
 * PUBLIC API - GATE MANAGEMENT
 *============================================================================*/

int portia_logic_add_custom_gate(
    portia_logic_bridge_t* bridge,
    const char* expression,
    uint32_t* gate_id_out)
{
    NIMCP_CHECK_THROW(bridge != NULL && expression != NULL && gate_id_out != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in add_custom_gate");

    if (bridge->custom_gate_count >= bridge->config.max_custom_rules) {
        NIMCP_LOGGING_ERROR("Maximum custom gates reached");
        return NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simple expression parsing - support basic patterns */
    logic_gate_type_t gate_type = LOGIC_GATE_AND;
    if (strstr(expression, "AND")) {
        gate_type = LOGIC_GATE_AND;
    } else if (strstr(expression, "OR")) {
        gate_type = LOGIC_GATE_OR;
    } else if (strstr(expression, "NOT")) {
        gate_type = LOGIC_GATE_NOT;
    } else if (strstr(expression, "XOR")) {
        gate_type = LOGIC_GATE_XOR;
    } else if (strstr(expression, "IMPLIES")) {
        gate_type = LOGIC_GATE_IMPLIES;
    } else {
        NIMCP_LOGGING_ERROR("Unsupported gate expression: %s", expression);
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Create gate */
    uint32_t gate_id = neural_logic_create_gate(
        bridge->logic_network,
        gate_type,
        0.7f  /* Default threshold */
    );

    if (gate_id == UINT32_MAX) {
        NIMCP_LOGGING_ERROR("Failed to create custom gate");
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    *gate_id_out = gate_id;
    bridge->custom_gate_count++;
    bridge->stats.active_gates++;

    NIMCP_LOGGING_INFO("Created custom gate %u: %s", gate_id, expression);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool portia_logic_evaluate_gate(portia_logic_bridge_t* bridge, uint32_t gate_id) {
    if (!bridge || !bridge->logic_network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_logic_evaluate_gate: required parameter is NULL (bridge, bridge->logic_network)");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update conditions first */
    update_conditions_internal(bridge);

    /* Prepare generic inputs based on current conditions */
    float inputs[4] = {
        bridge->conditions.memory_ok ? 1.0f : 0.0f,
        bridge->conditions.thermal_ok ? 1.0f : 0.0f,
        bridge->conditions.battery_ok ? 1.0f : 0.0f,
        bridge->conditions.cpu_ok ? 1.0f : 0.0f
    };

    float output = 0.0f;
    bool success = neural_logic_evaluate(
        bridge->logic_network,
        gate_id,
        inputs,
        4,
        &output
    );

    bridge->stats.total_evaluations++;
    if (success) {
        bridge->stats.successful_evaluations++;
    } else {
        bridge->stats.failed_evaluations++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return success && (output >= bridge->config.decision_threshold);
}

int portia_logic_get_gate_decision(
    portia_logic_bridge_t* bridge,
    uint32_t gate_id,
    portia_logic_decision_t* decision)
{
    NIMCP_CHECK_THROW(bridge != NULL && decision != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_gate_decision");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update conditions */
    update_conditions_internal(bridge);

    /* Evaluate gate */
    float inputs[4] = {
        bridge->conditions.memory_ok ? 1.0f : 0.0f,
        bridge->conditions.thermal_ok ? 1.0f : 0.0f,
        bridge->conditions.battery_ok ? 1.0f : 0.0f,
        bridge->conditions.cpu_ok ? 1.0f : 0.0f
    };

    float output = 0.0f;
    uint64_t start_time = get_time_us();
    bool success = neural_logic_evaluate(
        bridge->logic_network,
        gate_id,
        inputs,
        4,
        &output
    );
    uint64_t end_time = get_time_us();

    /* Populate decision */
    memset(decision, 0, sizeof(portia_logic_decision_t));
    decision->gate_id = gate_id;
    decision->result = success && (output >= bridge->config.decision_threshold);
    decision->confidence = output;
    decision->evaluation_time_us = end_time - start_time;
    snprintf(decision->explanation, sizeof(decision->explanation),
             "Gate %u evaluated to %.2f (threshold %.2f)",
             gate_id, output, bridge->config.decision_threshold);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - RESOURCE CONDITIONS
 *============================================================================*/

int portia_logic_update_conditions(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    int result = update_conditions_internal(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

int portia_logic_get_conditions(
    const portia_logic_bridge_t* bridge,
    portia_resource_condition_t* conditions)
{
    NIMCP_CHECK_THROW(bridge != NULL && conditions != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_conditions");

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(conditions, &bridge->conditions, sizeof(portia_resource_condition_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int portia_logic_set_condition(
    portia_logic_bridge_t* bridge,
    const char* condition_name,
    bool value)
{
    NIMCP_CHECK_THROW(bridge != NULL && condition_name != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in set_condition");

    nimcp_mutex_lock(bridge->base.mutex);

    if (strcmp(condition_name, "memory_ok") == 0) {
        bridge->conditions.memory_ok = value;
    } else if (strcmp(condition_name, "thermal_ok") == 0) {
        bridge->conditions.thermal_ok = value;
    } else if (strcmp(condition_name, "battery_ok") == 0) {
        bridge->conditions.battery_ok = value;
    } else if (strcmp(condition_name, "cpu_ok") == 0) {
        bridge->conditions.cpu_ok = value;
    } else if (strcmp(condition_name, "accelerator_available") == 0) {
        bridge->conditions.accelerator_available = value;
    } else if (strcmp(condition_name, "emergency_mode") == 0) {
        bridge->conditions.emergency_mode = value;
    } else {
        NIMCP_LOGGING_ERROR("Unknown condition: %s", condition_name);
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Recalculate resource score */
    float score = 0.0f;
    score += bridge->conditions.memory_ok ? 0.25f : 0.0f;
    score += bridge->conditions.thermal_ok ? 0.25f : 0.0f;
    score += bridge->conditions.battery_ok ? 0.25f : 0.0f;
    score += bridge->conditions.cpu_ok ? 0.25f : 0.0f;
    bridge->conditions.resource_score = score;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - BIO-ASYNC
 *============================================================================*/

int portia_logic_connect_bio_async(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_PORTIA_LOGIC,
        .module_name = "portia_logic_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return NIMCP_ERROR_OPERATION_FAILED;
}

int portia_logic_disconnect_bio_async(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already disconnected */
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool portia_logic_is_bio_async_connected(const portia_logic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}

int portia_logic_process_inbox(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;  /* No messages to process */
    }

    return bio_router_process_inbox(bridge->base.bio_ctx, 10);
}

int portia_logic_broadcast_decision(
    portia_logic_bridge_t* bridge,
    const portia_logic_decision_t* decision)
{
    NIMCP_CHECK_THROW(bridge != NULL && decision != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in broadcast_decision");

    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return NIMCP_ERROR_INVALID_STATE;  /* Bio-async not connected */
    }

    /* Create bio-async message with proper header */
    bio_msg_logic_gate_result_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_LOGIC_GATE_RESULT,
                        bio_module_context_get_id(bridge->base.bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;

    msg.gate_id = decision->gate_id;
    msg.gate_type = 0;  /* Generic decision gate */
    msg.output = decision->result ? 1.0f : 0.0f;
    msg.spiked = decision->result;
    msg.spike_time_us = decision->evaluation_time_us;
    msg.threshold_used = decision->confidence;

    /* Broadcast to subscribers */
    bio_router_broadcast(bridge->base.bio_ctx, &msg, sizeof(msg));

    NIMCP_LOGGING_DEBUG("Broadcast decision for gate %u", decision->gate_id);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API - STATISTICS
 *============================================================================*/

int portia_logic_get_stats(
    const portia_logic_bridge_t* bridge,
    portia_logic_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge != NULL && stats != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_stats");

    memcpy(stats, &bridge->stats, sizeof(portia_logic_stats_t));

    /* Compute average evaluation time */
    if (bridge->stats.total_evaluations > 0) {
        /* This is a placeholder - would need actual timing tracking */
        stats->avg_evaluation_time_us = 100.0f;
    }

    return NIMCP_SUCCESS;
}

int portia_logic_reset_stats(portia_logic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all counters except active_gates */
    uint32_t active_gates = bridge->stats.active_gates;
    memset(&bridge->stats, 0, sizeof(portia_logic_stats_t));
    bridge->stats.active_gates = active_gates;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Reset Portia-Logic statistics");

    return NIMCP_SUCCESS;
}

uint32_t portia_logic_get_gate_count(const portia_logic_bridge_t* bridge) {
    if (!bridge) {
        return 0;
    }

    return bridge->stats.active_gates;
}

void portia_logic_dump_gates(const portia_logic_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    NIMCP_LOGGING_INFO("=== Portia-Logic Gates ===");
    NIMCP_LOGGING_INFO("Tier Upgrade Gate: %u", bridge->tier_upgrade_gate);
    NIMCP_LOGGING_INFO("Tier Downgrade Gate: %u", bridge->tier_downgrade_gate);
    NIMCP_LOGGING_INFO("Degradation Gate: %u", bridge->degradation_gate);
    NIMCP_LOGGING_INFO("Allocation Gate: %u", bridge->allocation_gate);
    NIMCP_LOGGING_INFO("Emergency Gate: %u", bridge->emergency_gate);
    NIMCP_LOGGING_INFO("Custom Gates: %u", bridge->custom_gate_count);
    NIMCP_LOGGING_INFO("Total Active Gates: %u", bridge->stats.active_gates);
    NIMCP_LOGGING_INFO("=========================");
}
