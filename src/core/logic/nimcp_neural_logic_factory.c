/**
 * @file nimcp_neural_logic_factory.c
 * @brief MODULE 5: Neural Logic Factory Implementation
 * @version 3.0.0
 * @date 2025-11-20
 *
 * WHAT: Factory for creating and configuring neural logic networks for brains
 * WHY:  Single Responsibility: Encapsulate network creation and configuration
 * HOW:  Provide pre-configured network constructors with sensible defaults
 *
 * @author NIMCP Development Team
 */

#include "core/logic/nimcp_neural_logic_factory.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "core/logic/nimcp_neural_logic_attachment.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"

#include <stddef.h>  /* for NULL */
// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "neural_logic_factory"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_logic_factory)

#define BIO_MODULE_ID 0x0139


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Map brain size to logic network size
 *
 * WHAT: Convert brain size enum to gate count
 * WHY:  Scale logic network appropriately with brain
 * HOW:  Switch on brain_size enum
 */
static uint32_t get_logic_size_for_brain(uint32_t brain_size) {
    // Heuristic mapping (assumes brain_size is a hint, not exact enum)
    if (brain_size <= 1000) {
        return NEURAL_LOGIC_SIZE_SMALL;   // 100 gates
    } else if (brain_size <= 10000) {
        return NEURAL_LOGIC_SIZE_MEDIUM;  // 1000 gates
    } else {
        return NEURAL_LOGIC_SIZE_LARGE;   // 10000 gates
    }
}

/**
 * @brief Validate neural logic configuration
 *
 * WHAT: Check configuration parameters for validity
 * WHY:  Prevent invalid network creation
 * HOW:  Range checks on each field
 */
static bool validate_config(const neural_logic_config_t* config) {
    if (!config) {
        LOG_ERROR("validate_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_config: config is NULL");
        return false;
    }

    // Validate max_logic_neurons
    if (config->max_logic_neurons == 0) {
        LOG_ERROR("validate_config: max_logic_neurons is zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: config->max_logic_neurons is zero");
        return false;
    }

    // Validate max_variables
    if (config->max_variables == 0) {
        LOG_ERROR("validate_config: max_variables is zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: config->max_variables is zero");
        return false;
    }
    if (config->max_variables > 26) {
        LOG_WARNING("validate_config: max_variables %u exceeds 26 (A-Z)",
                    config->max_variables);
    }

    // Validate variable_pattern_dim
    if (config->variable_pattern_dim == 0) {
        LOG_ERROR("validate_config: variable_pattern_dim is zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: config->variable_pattern_dim is zero");
        return false;
    }
    if (config->variable_pattern_dim > 1024) {
        LOG_WARNING("validate_config: variable_pattern_dim %u is very large (>1024)",
                    config->variable_pattern_dim);
    }

    // Validate threads_per_block (CUDA constraint)
    if (config->use_gpu) {
        if (config->threads_per_block < 32 || config->threads_per_block > 1024) {
            LOG_WARNING("validate_config: threads_per_block %u outside recommended range [32,1024]",
                        config->threads_per_block);
        }
    }

    // Validate timestep_us
    if (config->timestep_us < 10 || config->timestep_us > 10000) {
        LOG_WARNING("validate_config: timestep_us %.1f outside practical range [10,10000]",
                    config->timestep_us);
    }

    // Validate integration_window_ms
    if (config->integration_window_ms < 1.0F || config->integration_window_ms > 1000.0F) {
        LOG_WARNING("validate_config: integration_window_ms %.1f outside practical range [1,1000]",
                    config->integration_window_ms);
    }

    return true;
}

//=============================================================================
// MODULE 5: Factory API Implementation
//=============================================================================

neural_logic_config_t get_default_neural_logic_config(uint32_t max_neurons) {
    // WHAT: Fill configuration structure with defaults
    // WHY:  Provide sensible starting point
    // HOW:  Set each field to recommended value

    // Guard: zero max_neurons
    if (max_neurons == 0) {
        max_neurons = 1000;  // Default to medium size
        LOG_WARNING("get_default_neural_logic_config: max_neurons is zero, using 1000");
    }

    neural_logic_config_t config = {0};

    // Network size
    config.max_logic_neurons = max_neurons;
    config.max_variables = 26;  // A-Z
    config.variable_pattern_dim = 64;

    // GPU configuration
    config.threads_per_block = 256;
    config.use_gpu = neural_logic_gpu_available();
    config.pin_host_memory = true;

    // Temporal parameters
    config.timestep_us = 100.0F;         // 10 kHz update rate
    config.integration_window_ms = 10.0F; // 10 ms integration

    // Learning configuration
    config.enable_learning = false;      // Static gates by default
    config.learning_rate = 0.0F;

    LOG_DEBUG("get_default_neural_logic_config: created config for %u neurons (GPU=%s)",
              max_neurons, config.use_gpu ? "enabled" : "disabled");

    return config;
}

neural_logic_network_t create_default_neural_logic(uint32_t brain_size) {
    // WHAT: Create network with defaults based on brain size
    // WHY:  Simplify common case of network creation
    // HOW:  Map brain_size → gate count → create config → create network

    uint32_t gate_count = get_logic_size_for_brain(brain_size);

    neural_logic_config_t config = get_default_neural_logic_config(gate_count);

    LOG_INFO("create_default_neural_logic: creating network with %u gates for brain_size %u",
             gate_count, brain_size);

    neural_logic_network_t network = neural_logic_create(&config);

    if (!network) {
        LOG_ERROR("create_default_neural_logic: failed to create network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");


        return NULL;
    }

    LOG_INFO("create_default_neural_logic: created network with %u gates, %u variables",
             config.max_logic_neurons, config.max_variables);

    return network;
}

neural_logic_network_t create_neural_logic_with_config(
    const neural_logic_config_t* config
) {
    // WHAT: Validate and create network with custom config
    // WHY:  Enable advanced users to customize network
    // HOW:  Validate → create → log

    // Guard: NULL config
    if (!nimcp_validate_pointer(config, "config")) {
        LOG_ERROR("create_neural_logic_with_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_neural_logic_with_config: nimcp_validate_pointer is NULL");
        return NULL;
    }

    // Validate configuration
    if (!validate_config(config)) {
        LOG_ERROR("create_neural_logic_with_config: invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "create_neural_logic_with_config: validate_config is NULL");
        return NULL;
    }

    // Create network
    neural_logic_network_t network = neural_logic_create(config);

    if (!network) {
        LOG_ERROR("create_neural_logic_with_config: failed to create network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");


        return NULL;
    }

    LOG_INFO("create_neural_logic_with_config: created network with %u gates, %u variables "
             "(GPU=%s, timestep=%.1fμs)",
             config->max_logic_neurons, config->max_variables,
             config->use_gpu ? "enabled" : "disabled",
             config->timestep_us);

    return network;
}

bool create_and_attach_neural_logic(
    brain_t brain,
    uint32_t brain_size
) {
    // WHAT: Validate brain before creating network
    // WHY:  Prevent wasted work if attachment will fail
    // HOW:  Guard clauses on brain

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("create_and_attach_neural_logic: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_and_attach_neural_logic: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: logic network already attached
    if (brain_has_neural_logic(brain)) {
        LOG_WARNING("create_and_attach_neural_logic: brain '%s' already has logic network",
                    "brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "create_and_attach_neural_logic: validation failed");
        return false;
    }

    // WHAT: Create default network
    // WHY:  Use factory function for consistency
    // HOW:  Call create_default_neural_logic()

    neural_logic_network_t network = create_default_neural_logic(brain_size);

    if (!network) {
        LOG_ERROR("create_and_attach_neural_logic: failed to create network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_and_attach_neural_logic: network is NULL");
        return false;
    }

    // WHAT: Attach network to brain
    // WHY:  Transfer ownership to brain
    // HOW:  Call MODULE 1 attachment function

    bool success = brain_attach_neural_logic(brain, network);

    if (!success) {
        LOG_ERROR("create_and_attach_neural_logic: failed to attach network to brain '%s'",
                  "brain");

        // Clean up network on failure
        neural_logic_destroy(network);

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_and_attach_neural_logic: success is NULL");
        return false;
    }

    LOG_INFO("create_and_attach_neural_logic: created and attached logic network to brain '%s'",
             "brain");

    return true;
}
