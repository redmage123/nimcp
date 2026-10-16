/**
 * @file nimcp_mesh_module_registry.h
 * @brief Type-Safe Module Registration API for Mesh Network
 *
 * WHAT: Provides type-safe registration of real NIMCP modules into the mesh
 * WHY:  Replace dummy pointers with validated, type-checked module instances
 * HOW:  Magic validation, size checking, and category-based organization
 *
 * USAGE:
 * ```c
 * // Register a real module with type safety
 * MESH_REGISTER_MODULE(bootstrap, hippocampus_ptr, hippocampus_t,
 *                      MESH_ADAPTER_CATEGORY_MEMORY);
 *
 * // Or use the functional API
 * mesh_module_descriptor_t desc = {
 *     .module_name = "hippocampus",
 *     .category = MESH_ADAPTER_CATEGORY_MEMORY,
 *     .module_instance = hippocampus_ptr,
 *     .module_size = sizeof(hippocampus_t),
 *     .module_magic = HIPPOCAMPUS_MAGIC,
 * };
 * mesh_module_registry_register(registry, &desc);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_MODULE_REGISTRY_H
#define NIMCP_MESH_MODULE_REGISTRY_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_module_registry mesh_module_registry_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* ============================================================================
 * Module Descriptor
 * ============================================================================ */

/**
 * @brief Module registration descriptor
 *
 * Contains all information needed to register a module with the mesh network
 * in a type-safe manner.
 */
typedef struct mesh_module_descriptor {
    const char* module_name;              /**< Unique module name */
    mesh_adapter_category_t category;     /**< Module category for channel assignment */

    void* module_instance;                /**< Pointer to real module instance */
    size_t module_size;                   /**< sizeof(module_type) for validation */
    uint32_t module_magic;                /**< Magic number for type validation */

    /* Optional pattern-based routing */
    const mesh_receptive_field_t* receptive_field;  /**< Module's receptive field */

    /* Optional health agent */
    nimcp_health_agent_t* health_agent;         /**< Health agent for monitoring */

    /* Optional endorsement configuration */
    endorser_role_t endorser_role;        /**< Role in endorsement policies */
    const char** policies;                /**< Policies to join as endorser */
    size_t policy_count;                  /**< Number of policies */

    /* Optional secondary channels */
    mesh_channel_id_t* secondary_channels; /**< Additional channels to join */
    size_t secondary_channel_count;       /**< Number of secondary channels */

} mesh_module_descriptor_t;

/**
 * @brief Registered module entry
 */
typedef struct mesh_registered_module {
    mesh_module_descriptor_t descriptor;  /**< Original descriptor */
    mesh_participant_id_t participant_id; /**< Assigned participant ID */
    mesh_adapter_base_t* adapter;         /**< Mesh adapter instance */
    bool registered;                      /**< Successfully registered flag */
    uint64_t registration_time_ns;        /**< Registration timestamp */
} mesh_registered_module_t;

/* ============================================================================
 * Registry Configuration
 * ============================================================================ */

/**
 * @brief Module registry configuration
 */
typedef struct mesh_module_registry_config {
    size_t max_modules;                   /**< Maximum modules (default 512) */
    bool require_magic_validation;        /**< Require magic numbers (default true) */
    bool require_size_validation;         /**< Require size validation (default true) */
    bool enable_duplicate_detection;      /**< Detect duplicate names (default true) */
    bool verbose_logging;                 /**< Enable verbose logging */
} mesh_module_registry_config_t;

/**
 * @brief Registry statistics
 */
typedef struct mesh_module_registry_stats {
    size_t total_registered;              /**< Total modules registered */
    size_t registration_failures;         /**< Failed registrations */
    size_t magic_validation_failures;     /**< Magic validation failures */
    size_t duplicate_detections;          /**< Duplicate name detections */

    /* Per-category counts */
    size_t cognitive_count;
    size_t perception_count;
    size_t subcortical_count;
    size_t motor_count;
    size_t memory_count;
    size_t security_count;
    size_t swarm_count;
    size_t gpu_count;
    size_t plasticity_count;
    size_t glial_count;
    size_t system_count;
} mesh_module_registry_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default registry configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_module_registry_default_config(
    mesh_module_registry_config_t* config
);

/**
 * @brief Create module registry
 *
 * @param config Configuration (NULL for defaults)
 * @return Registry handle or NULL on failure
 */
mesh_module_registry_t* mesh_module_registry_create(
    const mesh_module_registry_config_t* config
);

/**
 * @brief Destroy module registry
 *
 * @param registry Registry to destroy (NULL-safe)
 */
void mesh_module_registry_destroy(mesh_module_registry_t* registry);

/* ============================================================================
 * Registration API
 * ============================================================================ */

/**
 * @brief Register a module with the registry
 *
 * @param registry Module registry
 * @param descriptor Module descriptor with all registration info
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_module_registry_register(
    mesh_module_registry_t* registry,
    const mesh_module_descriptor_t* descriptor
);

/**
 * @brief Unregister a module by name
 *
 * @param registry Module registry
 * @param module_name Name of module to unregister
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_module_registry_unregister(
    mesh_module_registry_t* registry,
    const char* module_name
);

/**
 * @brief Check if a module is registered
 *
 * @param registry Module registry
 * @param module_name Name to check
 * @return true if registered
 */
bool mesh_module_registry_contains(
    const mesh_module_registry_t* registry,
    const char* module_name
);

/* ============================================================================
 * Lookup API
 * ============================================================================ */

/**
 * @brief Get registered module by name
 *
 * @param registry Module registry
 * @param module_name Module name
 * @return Registered module entry or NULL if not found
 */
const mesh_registered_module_t* mesh_module_registry_get(
    const mesh_module_registry_t* registry,
    const char* module_name
);

/**
 * @brief Get registered module by participant ID
 *
 * @param registry Module registry
 * @param participant_id Participant ID
 * @return Registered module entry or NULL if not found
 */
const mesh_registered_module_t* mesh_module_registry_get_by_id(
    const mesh_module_registry_t* registry,
    mesh_participant_id_t participant_id
);

/**
 * @brief Get module instance by name (returns raw pointer)
 *
 * @param registry Module registry
 * @param module_name Module name
 * @return Module instance pointer or NULL if not found
 */
void* mesh_module_registry_get_instance(
    const mesh_module_registry_t* registry,
    const char* module_name
);

/**
 * @brief Get all modules in a category
 *
 * @param registry Module registry
 * @param category Category to filter by
 * @param modules Output array of registered modules
 * @param max_modules Maximum modules to return
 * @param count_out Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_module_registry_get_by_category(
    const mesh_module_registry_t* registry,
    mesh_adapter_category_t category,
    const mesh_registered_module_t** modules,
    size_t max_modules,
    size_t* count_out
);

/* ============================================================================
 * Validation API
 * ============================================================================ */

/**
 * @brief Validate a module's magic number
 *
 * @param instance Module instance pointer
 * @param expected_magic Expected magic number
 * @return true if magic matches
 */
bool mesh_module_validate_magic(
    const void* instance,
    uint32_t expected_magic
);

/**
 * @brief Validate all registered modules
 *
 * Checks that all registered modules still have valid magic numbers.
 * Useful for detecting memory corruption.
 *
 * @param registry Module registry
 * @param invalid_count Output: number of invalid modules found
 * @return NIMCP_SUCCESS if all valid, NIMCP_ERROR_VALIDATION if any invalid
 */
nimcp_error_t mesh_module_registry_validate_all(
    const mesh_module_registry_t* registry,
    size_t* invalid_count
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get registry statistics
 *
 * @param registry Module registry
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_module_registry_get_stats(
    const mesh_module_registry_t* registry,
    mesh_module_registry_stats_t* stats
);

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

/**
 * @brief Get module registry from bootstrap
 *
 * @param bootstrap Mesh bootstrap handle
 * @return Module registry or NULL
 */
mesh_module_registry_t* mesh_bootstrap_get_module_registry(
    mesh_bootstrap_t* bootstrap
);

/* ============================================================================
 * Type-Safe Registration Macro
 * ============================================================================ */

/**
 * @brief Type-safe module registration macro
 *
 * This macro provides compile-time type checking and automatic size/magic
 * extraction for module registration.
 *
 * @param bootstrap Mesh bootstrap handle
 * @param module_ptr Pointer to module instance
 * @param module_type Module struct type (must have _MAGIC defined)
 * @param category Mesh adapter category
 *
 * Example:
 * ```c
 * // Requires that hippocampus_t has hippocampus_t_MAGIC defined
 * MESH_REGISTER_MODULE(bootstrap, hippocampus_ptr, hippocampus_t,
 *                      MESH_ADAPTER_CATEGORY_MEMORY);
 * ```
 */
#define MESH_REGISTER_MODULE(bootstrap, module_ptr, module_type, category) \
    mesh_module_registry_register( \
        mesh_bootstrap_get_module_registry(bootstrap), \
        &(mesh_module_descriptor_t){ \
            .module_name = #module_type, \
            .category = (category), \
            .module_instance = (module_ptr), \
            .module_size = sizeof(module_type), \
            .module_magic = module_type##_MAGIC, \
            .receptive_field = NULL, \
            .health_agent = NULL, \
            .endorser_role = ENDORSER_ROLE_OPTIONAL, \
            .policies = NULL, \
            .policy_count = 0, \
            .secondary_channels = NULL, \
            .secondary_channel_count = 0 \
        } \
    )

/**
 * @brief Extended registration macro with receptive field
 */
#define MESH_REGISTER_MODULE_WITH_RF(bootstrap, module_ptr, module_type, category, rf) \
    mesh_module_registry_register( \
        mesh_bootstrap_get_module_registry(bootstrap), \
        &(mesh_module_descriptor_t){ \
            .module_name = #module_type, \
            .category = (category), \
            .module_instance = (module_ptr), \
            .module_size = sizeof(module_type), \
            .module_magic = module_type##_MAGIC, \
            .receptive_field = (rf), \
            .health_agent = NULL, \
            .endorser_role = ENDORSER_ROLE_OPTIONAL, \
            .policies = NULL, \
            .policy_count = 0, \
            .secondary_channels = NULL, \
            .secondary_channel_count = 0 \
        } \
    )

/**
 * @brief Registration macro with custom name
 */
#define MESH_REGISTER_MODULE_NAMED(bootstrap, module_ptr, module_type, name, category) \
    mesh_module_registry_register( \
        mesh_bootstrap_get_module_registry(bootstrap), \
        &(mesh_module_descriptor_t){ \
            .module_name = (name), \
            .category = (category), \
            .module_instance = (module_ptr), \
            .module_size = sizeof(module_type), \
            .module_magic = module_type##_MAGIC, \
            .receptive_field = NULL, \
            .health_agent = NULL, \
            .endorser_role = ENDORSER_ROLE_OPTIONAL, \
            .policies = NULL, \
            .policy_count = 0, \
            .secondary_channels = NULL, \
            .secondary_channel_count = 0 \
        } \
    )

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_MODULE_REGISTRY_H */
