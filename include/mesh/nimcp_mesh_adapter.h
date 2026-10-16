/**
 * @file nimcp_mesh_adapter.h
 * @brief Mesh Network Adapter Framework for NIMCP Component Integration
 *
 * WHAT: Generic adapter interface for integrating any NIMCP module into mesh network
 * WHY:  Standardized pattern for mesh participation across all brain components
 * HOW:  Macro-based adapter generation with category-specific callbacks
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                         MESH ADAPTER FRAMEWORK                               │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────────┐                                                       │
 * │  │  NIMCP Module    │  (any brain component)                                │
 * │  │  - hippocampus_t │                                                       │
 * │  │  - amygdala_t    │                                                       │
 * │  │  - cortex_t      │                                                       │
 * │  └────────┬─────────┘                                                       │
 * │           │                                                                  │
 * │           ▼                                                                  │
 * │  ┌──────────────────┐                                                       │
 * │  │  Mesh Adapter    │  MESH_ADAPTER_DEFINE(module_name, module_type)        │
 * │  │  - interface     │  - Implements mesh_participant_interface_t            │
 * │  │  - callbacks     │  - Wraps module-specific callbacks                    │
 * │  │  - health        │  - Maps module health to mesh health_metrics_t        │
 * │  └────────┬─────────┘                                                       │
 * │           │                                                                  │
 * │           ▼                                                                  │
 * │  ┌──────────────────┐                                                       │
 * │  │  Mesh Network    │                                                       │
 * │  │  - Registry      │  mesh_participant_register()                          │
 * │  │  - Channel       │  mesh_channel_add_participant()                       │
 * │  │  - Endorsement   │  mesh_endorsement_policy_add_endorser()               │
 * │  └──────────────────┘                                                       │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * USAGE:
 * ```c
 * // 1. Define adapter for module
 * MESH_ADAPTER_DEFINE(hippocampus, hippocampus_t)
 *
 * // 2. Create adapter instance
 * mesh_adapter_hippocampus_t* adapter = mesh_adapter_hippocampus_create(
 *     hippocampus_instance,
 *     MESH_CHANNEL_SUBCORTICAL
 * );
 *
 * // 3. Register with mesh
 * mesh_adapter_hippocampus_register(adapter, registry);
 *
 * // 4. Join channels
 * mesh_adapter_hippocampus_join_channel(adapter, channel);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_ADAPTER_H
#define NIMCP_MESH_ADAPTER_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_endorsement.h"
#include "utils/error/nimcp_error_codes.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Adapter Categories
 * ============================================================================ */

/**
 * @brief Mesh adapter category for channel assignment
 */
typedef enum mesh_adapter_category {
    MESH_ADAPTER_CATEGORY_COGNITIVE,      /**< Cognitive processing modules */
    MESH_ADAPTER_CATEGORY_PERCEPTION,     /**< Sensory perception modules */
    MESH_ADAPTER_CATEGORY_SUBCORTICAL,    /**< Limbic/subcortical modules */
    MESH_ADAPTER_CATEGORY_MOTOR,          /**< Motor control modules */
    MESH_ADAPTER_CATEGORY_MEMORY,         /**< Memory systems */
    MESH_ADAPTER_CATEGORY_SECURITY,       /**< Security/immune modules */
    MESH_ADAPTER_CATEGORY_SWARM,          /**< Swarm coordination modules */
    MESH_ADAPTER_CATEGORY_GPU,            /**< GPU-accelerated modules */
    MESH_ADAPTER_CATEGORY_PLASTICITY,     /**< Learning/plasticity modules */
    MESH_ADAPTER_CATEGORY_GLIAL,          /**< Glial support modules */
    MESH_ADAPTER_CATEGORY_SYSTEM,         /**< System infrastructure */
} mesh_adapter_category_t;

/**
 * @brief Get default channel for adapter category
 */
static inline mesh_channel_id_t mesh_adapter_get_default_channel(
    mesh_adapter_category_t category
) {
    switch (category) {
        case MESH_ADAPTER_CATEGORY_COGNITIVE:
            return MESH_CHANNEL_LEFT_HEMISPHERE;
        case MESH_ADAPTER_CATEGORY_PERCEPTION:
            return MESH_CHANNEL_RIGHT_HEMISPHERE;
        case MESH_ADAPTER_CATEGORY_SUBCORTICAL:
        case MESH_ADAPTER_CATEGORY_MOTOR:
        case MESH_ADAPTER_CATEGORY_MEMORY:
            return MESH_CHANNEL_SUBCORTICAL;
        case MESH_ADAPTER_CATEGORY_GPU:
            return MESH_CHANNEL_GPU_COMPUTE;
        case MESH_ADAPTER_CATEGORY_SECURITY:
        case MESH_ADAPTER_CATEGORY_SWARM:
        case MESH_ADAPTER_CATEGORY_PLASTICITY:
        case MESH_ADAPTER_CATEGORY_GLIAL:
        case MESH_ADAPTER_CATEGORY_SYSTEM:
        default:
            return MESH_CHANNEL_SYSTEM;
    }
}

/* ============================================================================
 * Adapter Configuration
 * ============================================================================ */

/**
 * @brief Generic mesh adapter configuration
 */
typedef struct mesh_adapter_config {
    const char* module_name;              /**< Module name for registration */
    mesh_adapter_category_t category;     /**< Adapter category */
    mesh_channel_id_t home_channel;       /**< Home channel (0 = use category default) */
    mesh_participant_type_t type;         /**< Participant type */
    
    /* Endorsement configuration */
    endorser_role_t endorser_role;        /**< Role in endorsement policies */
    const char** policies;                /**< Policies to join as endorser */
    size_t policy_count;                  /**< Number of policies */
    
    /* Optional secondary channels */
    mesh_channel_id_t secondary_channels[MESH_MAX_CHANNEL_MEMBERSHIPS];
    size_t secondary_channel_count;
    
    /* Health reporting configuration */
    bool report_health;                   /**< Report health metrics */
    float health_report_interval_ms;      /**< Health report interval */
    
} mesh_adapter_config_t;

/**
 * @brief Initialize adapter config with defaults
 */
static inline void mesh_adapter_config_init(
    mesh_adapter_config_t* config,
    const char* module_name,
    mesh_adapter_category_t category
) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->module_name = module_name;
    config->category = category;
    config->home_channel = mesh_adapter_get_default_channel(category);
    config->type = MESH_PARTICIPANT_MODULE;
    config->endorser_role = ENDORSER_ROLE_OPTIONAL;
    config->report_health = true;
    config->health_report_interval_ms = 1000.0f;
}

/* ============================================================================
 * Generic Adapter Base Structure
 * ============================================================================ */

/**
 * @brief Base adapter structure (embedded in specific adapters)
 */
typedef struct mesh_adapter_base {
    uint32_t magic;                       /**< Magic for validation */
    mesh_participant_interface_t interface; /**< Mesh interface */
    mesh_participant_id_t participant_id; /**< Assigned participant ID */
    mesh_adapter_config_t config;         /**< Adapter configuration */
    
    void* module;                         /**< Pointer to wrapped module */
    mesh_participant_registry_t* registry; /**< Registry reference */
    
    /* Health tracking */
    health_metrics_t last_health;         /**< Last reported health */
    uint64_t last_health_report_ns;       /**< Last health report time */
    
    /* Statistics */
    uint64_t proposals_received;
    uint64_t endorsements_made;
    uint64_t beliefs_received;
    uint64_t commits_processed;
    
} mesh_adapter_base_t;

#define MESH_ADAPTER_MAGIC 0x4D455348  /* "MESH" */

/* ============================================================================
 * Adapter Callback Types
 * ============================================================================ */

/**
 * @brief Module-specific health getter
 */
typedef void (*mesh_adapter_get_health_fn)(
    void* module,
    health_metrics_t* health_out
);

/**
 * @brief Module-specific free energy getter
 */
typedef float (*mesh_adapter_get_free_energy_fn)(void* module);

/**
 * @brief Module-specific belief handler
 */
typedef nimcp_error_t (*mesh_adapter_on_belief_fn)(
    void* module,
    const mesh_belief_t* belief
);

/**
 * @brief Module-specific endorsement handler
 */
typedef endorsement_result_t (*mesh_adapter_on_endorse_fn)(
    void* module,
    const mesh_transaction_t* tx
);

/**
 * @brief Module-specific commit handler
 */
typedef nimcp_error_t (*mesh_adapter_on_commit_fn)(
    void* module,
    const mesh_transaction_t* tx
);

/**
 * @brief Module callbacks structure
 */
typedef struct mesh_adapter_callbacks {
    mesh_adapter_get_health_fn get_health;
    mesh_adapter_get_free_energy_fn get_free_energy;
    mesh_adapter_on_belief_fn on_belief;
    mesh_adapter_on_endorse_fn on_endorse;
    mesh_adapter_on_commit_fn on_commit;
} mesh_adapter_callbacks_t;

/* ============================================================================
 * Generic Adapter Functions
 * ============================================================================ */

/**
 * @brief Initialize adapter base with module
 *
 * @param base Adapter base to initialize
 * @param module Module instance to wrap
 * @param config Adapter configuration
 * @param callbacks Module-specific callbacks
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_adapter_base_init(
    mesh_adapter_base_t* base,
    void* module,
    const mesh_adapter_config_t* config,
    const mesh_adapter_callbacks_t* callbacks
);

/**
 * @brief Register adapter with mesh registry
 *
 * @param base Adapter base
 * @param registry Mesh participant registry
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_adapter_base_register(
    mesh_adapter_base_t* base,
    mesh_participant_registry_t* registry
);

/**
 * @brief Join adapter to channel
 *
 * @param base Adapter base
 * @param channel Channel to join
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_adapter_base_join_channel(
    mesh_adapter_base_t* base,
    mesh_channel_t* channel
);

/**
 * @brief Add adapter as endorser to policy
 *
 * @param base Adapter base
 * @param collector Endorsement collector
 * @param policy_name Policy to join
 * @param role Endorser role
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_adapter_base_add_to_policy(
    mesh_adapter_base_t* base,
    mesh_endorsement_collector_t* collector,
    const char* policy_name,
    endorser_role_t role
);

/**
 * @brief Cleanup adapter base
 *
 * @param base Adapter base to cleanup
 */
void mesh_adapter_base_cleanup(mesh_adapter_base_t* base);

/* ============================================================================
 * Adapter Definition Macro
 * ============================================================================ */

/**
 * @brief Define a mesh adapter for a specific module type
 *
 * Creates:
 * - mesh_adapter_<name>_t structure
 * - mesh_adapter_<name>_create() function
 * - mesh_adapter_<name>_destroy() function
 * - mesh_adapter_<name>_register() function
 * - mesh_adapter_<name>_join_channel() function
 *
 * @param name Module name (e.g., hippocampus)
 * @param module_type Module struct type (e.g., hippocampus_t)
 */
#define MESH_ADAPTER_DEFINE(name, module_type) \
    \
    typedef struct mesh_adapter_##name { \
        mesh_adapter_base_t base; \
        module_type* module; \
        mesh_adapter_callbacks_t callbacks; \
    } mesh_adapter_##name##_t; \
    \
    static inline mesh_adapter_##name##_t* mesh_adapter_##name##_create( \
        module_type* module, \
        const mesh_adapter_config_t* config, \
        const mesh_adapter_callbacks_t* callbacks \
    ) { \
        if (!module) return NULL; \
        \
        mesh_adapter_##name##_t* adapter = \
            (mesh_adapter_##name##_t*)nimcp_calloc(1, sizeof(*adapter)); \
        if (!adapter) return NULL; \
        \
        adapter->module = module; \
        if (callbacks) { \
            adapter->callbacks = *callbacks; \
        } \
        \
        mesh_adapter_config_t default_config; \
        if (!config) { \
            mesh_adapter_config_init(&default_config, #name, \
                MESH_ADAPTER_CATEGORY_COGNITIVE); \
            config = &default_config; \
        } \
        \
        if (mesh_adapter_base_init(&adapter->base, module, config, \
                                   &adapter->callbacks) != NIMCP_SUCCESS) { \
            nimcp_free(adapter); \
            return NULL; \
        } \
        \
        return adapter; \
    } \
    \
    static inline void mesh_adapter_##name##_destroy( \
        mesh_adapter_##name##_t* adapter \
    ) { \
        if (!adapter) return; \
        mesh_adapter_base_cleanup(&adapter->base); \
        nimcp_free(adapter); \
    } \
    \
    static inline nimcp_error_t mesh_adapter_##name##_register( \
        mesh_adapter_##name##_t* adapter, \
        mesh_participant_registry_t* registry \
    ) { \
        if (!adapter) return NIMCP_ERROR_NULL_POINTER; \
        return mesh_adapter_base_register(&adapter->base, registry); \
    } \
    \
    static inline nimcp_error_t mesh_adapter_##name##_join_channel( \
        mesh_adapter_##name##_t* adapter, \
        mesh_channel_t* channel \
    ) { \
        if (!adapter) return NIMCP_ERROR_NULL_POINTER; \
        return mesh_adapter_base_join_channel(&adapter->base, channel); \
    } \
    \
    static inline mesh_participant_id_t mesh_adapter_##name##_get_id( \
        const mesh_adapter_##name##_t* adapter \
    ) { \
        return adapter ? adapter->base.participant_id : 0; \
    }

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_ADAPTER_H */
