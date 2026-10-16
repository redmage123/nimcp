/**
 * @file nimcp_bridge_boilerplate.h
 * @brief Macros to eliminate duplicated mesh registration and heartbeat boilerplate
 * @version 1.0.0
 * @date 2026-02-15
 *
 * WHAT: Provides macros to replace the ~30-line mesh registration + heartbeat
 *       boilerplate that is identically duplicated across ~582 bridge files.
 * WHY:  Reduces codebase by ~25,000 lines and ensures consistent implementation
 * HOW:  Three macros that generate the boilerplate via token pasting
 *
 * USAGE:
 *   // Full boilerplate (mesh registration + heartbeat instance):
 *   BRIDGE_BOILERPLATE(attention_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)
 *
 *   // Mesh registration only (no heartbeat instance):
 *   BRIDGE_DEFINE_MESH_REGISTRATION(basal_ganglia_executive_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)
 *
 *   // Heartbeat instance only:
 *   BRIDGE_DEFINE_HEARTBEAT_INSTANCE(perception_immune)
 *
 * All required headers are included automatically by this header.
 */

#ifndef NIMCP_BRIDGE_BOILERPLATE_H
#define NIMCP_BRIDGE_BOILERPLATE_H

#include <stddef.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

/*=============================================================================
 * Helper Macros for Token Pasting
 *===========================================================================*/

#ifndef BRIDGE_BP_CONCAT_
#define BRIDGE_BP_CONCAT_(a, b) a##b
#endif
#ifndef BRIDGE_BP_CONCAT
#define BRIDGE_BP_CONCAT(a, b) BRIDGE_BP_CONCAT_(a, b)
#endif

/* Stringify helper for module name strings */
#ifndef BRIDGE_BP_STR_
#define BRIDGE_BP_STR_(x) #x
#endif
#ifndef BRIDGE_BP_STR
#define BRIDGE_BP_STR(x) BRIDGE_BP_STR_(x)
#endif

/*=============================================================================
 * BRIDGE_DEFINE_MESH_REGISTRATION - Mesh participant registration boilerplate
 *
 * Generates:
 *   - static mesh_participant_id_t g_MODULE_mesh_id = 0;
 *   - static mesh_participant_registry_t* g_MODULE_mesh_registry = NULL;
 *   - nimcp_error_t MODULE_mesh_register(mesh_participant_registry_t* registry);
 *   - void MODULE_mesh_unregister(void);
 *
 * Parameters:
 *   MODULE   - Module name prefix (e.g., attention_plasticity_bridge)
 *   CATEGORY - Mesh adapter category (e.g., MESH_ADAPTER_CATEGORY_COGNITIVE)
 *===========================================================================*/

#define BRIDGE_DEFINE_MESH_REGISTRATION(MODULE, CATEGORY)                      \
                                                                               \
static mesh_participant_id_t BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_id)) = 0; \
static mesh_participant_registry_t* BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_registry)) = NULL; \
                                                                               \
nimcp_error_t BRIDGE_BP_CONCAT(MODULE, _mesh_register)(                        \
    mesh_participant_registry_t* registry)                                      \
{                                                                              \
    if (!registry) return NIMCP_ERROR_NULL_POINTER;                            \
    if (BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_id)) != 0)        \
        return NIMCP_SUCCESS;                                                  \
    mesh_participant_interface_t iface;                                         \
    mesh_participant_interface_init(&iface);                                    \
    strncpy(iface.module_name, BRIDGE_BP_STR(MODULE), MESH_MAX_NAME_LEN - 1); \
    iface.type = MESH_PARTICIPANT_MODULE;                                      \
    iface.home_channel = mesh_adapter_get_default_channel(CATEGORY);           \
    mesh_participant_config_t config;                                           \
    mesh_participant_config_init(&config);                                      \
    config.module_name = BRIDGE_BP_STR(MODULE);                                \
    config.type = MESH_PARTICIPANT_MODULE;                                      \
    config.home_channel = iface.home_channel;                                  \
    nimcp_error_t err = mesh_participant_register(                              \
        registry, &iface, &config,                                             \
        &BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_id)));            \
    if (err == NIMCP_SUCCESS)                                                   \
        BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_registry)) = registry; \
    return err;                                                                \
}                                                                              \
                                                                               \
void BRIDGE_BP_CONCAT(MODULE, _mesh_unregister)(void)                          \
{                                                                              \
    if (BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_registry)) &&      \
        BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_id)) != 0)        \
    {                                                                          \
        mesh_participant_unregister(                                            \
            BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_registry)),     \
            BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_id)));          \
        BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_id)) = 0;          \
        BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _mesh_registry)) = NULL;  \
    }                                                                          \
}

/*=============================================================================
 * BRIDGE_DEFINE_HEARTBEAT_INSTANCE - Instance-level heartbeat helper
 *
 * Generates a static inline heartbeat function that sends to both the global
 * module health agent and an optional instance-level agent.
 *
 * Parameters:
 *   MODULE - Module name prefix (must match NIMCP_DECLARE_HEALTH_AGENT_ATOMIC)
 *===========================================================================*/

#define BRIDGE_DEFINE_HEARTBEAT_INSTANCE(MODULE)                               \
static nimcp_health_agent_t* BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _instance_health_agent)) = NULL; \
static inline void BRIDGE_BP_CONCAT(MODULE, _heartbeat_instance)(              \
    void* instance_agent_ptr,                                                   \
    const char* operation, float progress)                                      \
{                                                                              \
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)instance_agent_ptr; \
    if (BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _health_agent))) {       \
        nimcp_health_agent_heartbeat_ex(                                        \
            BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _health_agent)),      \
            operation, progress);                                               \
    }                                                                          \
    if (instance_agent &&                                                       \
        instance_agent != BRIDGE_BP_CONCAT(g_, BRIDGE_BP_CONCAT(MODULE, _health_agent))) \
    {                                                                          \
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);   \
    }                                                                          \
}

/*=============================================================================
 * BRIDGE_BOILERPLATE - Complete bridge boilerplate (all three)
 *
 * Combines:
 *   1. NIMCP_DECLARE_HEALTH_AGENT_ATOMIC (from health_agent_macros.h)
 *   2. BRIDGE_DEFINE_MESH_REGISTRATION
 *   3. BRIDGE_DEFINE_HEARTBEAT_INSTANCE
 *
 * Usage:
 *   BRIDGE_BOILERPLATE(attention_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)
 *
 * Prerequisites: All included automatically by this header.
 *===========================================================================*/

#define BRIDGE_BOILERPLATE(MODULE, CATEGORY)                                    \
    NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(MODULE)                                  \
    BRIDGE_DEFINE_MESH_REGISTRATION(MODULE, CATEGORY)                          \
    BRIDGE_DEFINE_HEARTBEAT_INSTANCE(MODULE)

/*=============================================================================
 * BRIDGE_BOILERPLATE_MESH_ONLY - Health agent + mesh registration (no heartbeat)
 *
 * For bridges that need mesh registration but not the instance heartbeat helper.
 *===========================================================================*/

#define BRIDGE_BOILERPLATE_MESH_ONLY(MODULE, CATEGORY)                         \
    NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(MODULE)                                  \
    BRIDGE_DEFINE_MESH_REGISTRATION(MODULE, CATEGORY)

/*=============================================================================
 * BRIDGE_BOILERPLATE_MINIMAL - Health agent only (no mesh, no heartbeat)
 *
 * For bridges that only need health agent functionality.
 *===========================================================================*/

#define BRIDGE_BOILERPLATE_MINIMAL(MODULE)                                     \
    NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(MODULE)

#endif /* NIMCP_BRIDGE_BOILERPLATE_H */
