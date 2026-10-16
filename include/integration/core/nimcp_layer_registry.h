/**
 * @file nimcp_layer_registry.h
 * @brief Layer Registry - Central registration for all layers and modules
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Manages registration of layers and their constituent modules
 * WHY:  Provide a central directory for layer lookup and module discovery
 * HOW:  Maintains registry of all layers, their modules, and connections
 *
 * USAGE:
 * ======
 *   // Create registry
 *   nimcp_layer_registry_t* registry = nimcp_layer_registry_create(NULL);
 *
 *   // Register a layer
 *   nimcp_layer_config_t config = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
 *   nimcp_layer_registry_register_layer(registry, &config);
 *
 *   // Register a module within a layer
 *   nimcp_layer_registry_register_module(registry, NIMCP_LAYER_PHYSICS,
 *                                         module_ptr, &interface, "ephaptic");
 *
 *   // Query modules
 *   nimcp_module_info_t* modules;
 *   size_t count;
 *   nimcp_layer_registry_get_modules(registry, NIMCP_LAYER_PHYSICS, &modules, &count);
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LAYER_REGISTRY_H
#define NIMCP_LAYER_REGISTRY_H

#include "nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Opaque layer registry handle
 */
typedef struct nimcp_layer_registry_struct* nimcp_layer_registry_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Registry configuration
 */
typedef struct {
    uint32_t max_layers;            /**< Max layers (default: NIMCP_MAX_LAYERS) */
    uint32_t max_modules_per_layer; /**< Max modules per layer */
    bool enable_logging;            /**< Enable registry logging */
    bool thread_safe;               /**< Enable thread safety */
} nimcp_layer_registry_config_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default registry configuration
 */
NIMCP_EXPORT nimcp_layer_registry_config_t nimcp_layer_registry_default_config(void);

/**
 * @brief Create layer registry
 *
 * WHAT: Creates the central layer registry
 * WHY:  Single source of truth for all layer/module information
 * HOW:  Allocates registry, initializes internal structures
 *
 * @param config Configuration (NULL for defaults)
 * @return Registry handle or NULL on failure
 */
NIMCP_EXPORT nimcp_layer_registry_t nimcp_layer_registry_create(
    const nimcp_layer_registry_config_t* config
);

/**
 * @brief Destroy layer registry
 *
 * @param registry Registry to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_layer_registry_destroy(nimcp_layer_registry_t registry);

/**
 * @brief Reset registry to empty state
 *
 * @param registry Registry handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_reset(
    nimcp_layer_registry_t registry
);

//=============================================================================
// Layer Registration API
//=============================================================================

/**
 * @brief Register a layer
 *
 * WHAT: Registers a new layer in the system
 * WHY:  Layer must be registered before modules can be added
 * HOW:  Stores layer configuration, creates module slots
 *
 * @param registry Registry handle
 * @param config Layer configuration
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_register_layer(
    nimcp_layer_registry_t registry,
    const nimcp_layer_config_t* config
);

/**
 * @brief Unregister a layer
 *
 * Note: All modules in the layer must be unregistered first
 *
 * @param registry Registry handle
 * @param layer_id Layer to unregister
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_unregister_layer(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id
);

/**
 * @brief Check if layer is registered
 *
 * @param registry Registry handle
 * @param layer_id Layer ID to check
 * @return true if registered
 */
NIMCP_EXPORT bool nimcp_layer_registry_is_layer_registered(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id
);

/**
 * @brief Get layer configuration
 *
 * @param registry Registry handle
 * @param layer_id Layer ID
 * @param config_out Output: layer configuration
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_get_layer_config(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id,
    nimcp_layer_config_t* config_out
);

/**
 * @brief Get all registered layers
 *
 * @param registry Registry handle
 * @param layer_ids_out Output: array of layer IDs (caller provides)
 * @param max_layers Size of layer_ids_out array
 * @param count_out Output: number of registered layers
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_get_layers(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t* layer_ids_out,
    size_t max_layers,
    size_t* count_out
);

//=============================================================================
// Module Registration API
//=============================================================================

/**
 * @brief Register a module within a layer
 *
 * WHAT: Registers a module in a specific layer
 * WHY:  Modules must be registered to participate in layer messaging
 * HOW:  Stores module pointer and interface, assigns module ID
 *
 * @param registry Registry handle
 * @param layer_id Layer to register module in
 * @param module_ptr Pointer to module instance
 * @param interface Module callback interface
 * @param name Module name
 * @param module_id_out Output: assigned module ID
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_register_module(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id,
    void* module_ptr,
    nimcp_module_interface_t* interface,
    const char* name,
    uint32_t* module_id_out
);

/**
 * @brief Unregister a module
 *
 * @param registry Registry handle
 * @param layer_id Layer containing module
 * @param module_id Module ID to unregister
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_unregister_module(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id,
    uint32_t module_id
);

/**
 * @brief Get module info
 *
 * @param registry Registry handle
 * @param layer_id Layer ID
 * @param module_id Module ID
 * @param info_out Output: module info
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_get_module(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id,
    uint32_t module_id,
    nimcp_module_info_t* info_out
);

/**
 * @brief Get all modules in a layer
 *
 * @param registry Registry handle
 * @param layer_id Layer ID
 * @param modules_out Output: array of module info (caller provides)
 * @param max_modules Size of modules_out array
 * @param count_out Output: number of modules
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_get_modules(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id,
    nimcp_module_info_t* modules_out,
    size_t max_modules,
    size_t* count_out
);

/**
 * @brief Find module by name
 *
 * @param registry Registry handle
 * @param layer_id Layer ID
 * @param name Module name
 * @param info_out Output: module info
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_find_module_by_name(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id,
    const char* name,
    nimcp_module_info_t* info_out
);

/**
 * @brief Get module count in layer
 *
 * @param registry Registry handle
 * @param layer_id Layer ID
 * @return Number of modules or -1 on error
 */
NIMCP_EXPORT int nimcp_layer_registry_get_module_count(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id
);

//=============================================================================
// Connection Management API
//=============================================================================

/**
 * @brief Register an inter-layer connection
 *
 * WHAT: Registers a connection between two layers
 * WHY:  Defines which layers can communicate
 * HOW:  Stores connection info for routing
 *
 * @param registry Registry handle
 * @param connection Connection info
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_register_connection(
    nimcp_layer_registry_t registry,
    const nimcp_layer_connection_t* connection
);

/**
 * @brief Unregister an inter-layer connection
 *
 * @param registry Registry handle
 * @param layer_a First layer
 * @param layer_b Second layer
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_unregister_connection(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_a,
    nimcp_layer_id_t layer_b
);

/**
 * @brief Check if connection exists between layers
 *
 * @param registry Registry handle
 * @param layer_a First layer
 * @param layer_b Second layer
 * @return true if connected
 */
NIMCP_EXPORT bool nimcp_layer_registry_are_connected(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_a,
    nimcp_layer_id_t layer_b
);

/**
 * @brief Get connection info
 *
 * @param registry Registry handle
 * @param layer_a First layer
 * @param layer_b Second layer
 * @param connection_out Output: connection info
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_get_connection(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_a,
    nimcp_layer_id_t layer_b,
    nimcp_layer_connection_t* connection_out
);

/**
 * @brief Get all connections for a layer
 *
 * @param registry Registry handle
 * @param layer_id Layer ID
 * @param connections_out Output: array of connections (caller provides)
 * @param max_connections Size of connections_out array
 * @param count_out Output: number of connections
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_get_layer_connections(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id,
    nimcp_layer_connection_t* connections_out,
    size_t max_connections,
    size_t* count_out
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get total number of registered layers
 *
 * @param registry Registry handle
 * @return Layer count or -1 on error
 */
NIMCP_EXPORT int nimcp_layer_registry_get_layer_count(
    nimcp_layer_registry_t registry
);

/**
 * @brief Get total number of registered modules
 *
 * @param registry Registry handle
 * @return Total module count or -1 on error
 */
NIMCP_EXPORT int nimcp_layer_registry_get_total_module_count(
    nimcp_layer_registry_t registry
);

/**
 * @brief Get total number of connections
 *
 * @param registry Registry handle
 * @return Connection count or -1 on error
 */
NIMCP_EXPORT int nimcp_layer_registry_get_connection_count(
    nimcp_layer_registry_t registry
);

//=============================================================================
// Iteration API
//=============================================================================

/**
 * @brief Callback for layer iteration
 */
typedef bool (*nimcp_layer_iterator_cb)(
    nimcp_layer_id_t layer_id,
    const nimcp_layer_config_t* config,
    void* user_data
);

/**
 * @brief Callback for module iteration
 */
typedef bool (*nimcp_module_iterator_cb)(
    nimcp_layer_id_t layer_id,
    const nimcp_module_info_t* module,
    void* user_data
);

/**
 * @brief Iterate over all layers
 *
 * @param registry Registry handle
 * @param callback Callback function (return false to stop)
 * @param user_data User data passed to callback
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_foreach_layer(
    nimcp_layer_registry_t registry,
    nimcp_layer_iterator_cb callback,
    void* user_data
);

/**
 * @brief Iterate over all modules in a layer
 *
 * @param registry Registry handle
 * @param layer_id Layer ID
 * @param callback Callback function (return false to stop)
 * @param user_data User data passed to callback
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_foreach_module(
    nimcp_layer_registry_t registry,
    nimcp_layer_id_t layer_id,
    nimcp_module_iterator_cb callback,
    void* user_data
);

/**
 * @brief Iterate over all modules in all layers
 *
 * @param registry Registry handle
 * @param callback Callback function (return false to stop)
 * @param user_data User data passed to callback
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_registry_foreach_all_modules(
    nimcp_layer_registry_t registry,
    nimcp_module_iterator_cb callback,
    void* user_data
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LAYER_REGISTRY_H */
