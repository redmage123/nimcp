/**
 * @file nimcp_health_agent_macros.h
 * @brief Macros for eliminating duplicated health agent boilerplate code
 *
 * STATUS: Active - Reduces boilerplate in 2000+ files
 *
 * PROBLEM: Many modules duplicate this identical pattern:
 *   - Forward declarations for health agent types
 *   - Static global health agent pointer
 *   - Set function to assign health agent
 *   - Heartbeat function to send progress updates
 *
 * SOLUTION: Provide macros that generate all this boilerplate with a single line.
 *
 * USAGE:
 *   // At file scope (after includes):
 *   NIMCP_DECLARE_HEALTH_AGENT(mymodule)
 *
 *   // This generates:
 *   // - static nimcp_health_agent_t* g_mymodule_health_agent = NULL;
 *   // - void mymodule_set_health_agent(nimcp_health_agent_t* agent);
 *   // - static inline void mymodule_heartbeat(const char* operation, float progress);
 *
 * BACKWARD COMPATIBILITY:
 *   Existing code continues to work. This macro simply generates the same code
 *   that was previously copy-pasted into each file.
 *
 * @author NIMCP Development Team
 * @date 2025-01-25
 */

#ifndef NIMCP_HEALTH_AGENT_MACROS_H
#define NIMCP_HEALTH_AGENT_MACROS_H

#include <stddef.h>    /* for NULL */
#include <stdatomic.h> /* for _Atomic, atomic_store, atomic_load */

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations (included once in header)
 *
 * These forward declarations allow modules to use health agent functionality
 * without including the full nimcp_health_agent.h header, which may have
 * additional dependencies.
 *============================================================================*/

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/**
 * @brief Extended heartbeat function with operation name and progress
 * @param agent Health agent instance (can be NULL)
 * @param operation Name of the current operation
 * @param progress Progress value (0.0 to 1.0)
 */
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/*=============================================================================
 * Helper Macros for Token Pasting
 *============================================================================*/

/* Two-level macro expansion for proper token pasting */
#define NIMCP_HA_CONCAT_(a, b) a##b
#define NIMCP_HA_CONCAT(a, b) NIMCP_HA_CONCAT_(a, b)

/*=============================================================================
 * NIMCP_HEALTH_AGENT_SET - Generate the set function
 *
 * Generates:
 *   void MODULE_set_health_agent(nimcp_health_agent_t* agent) {
 *       g_MODULE_health_agent = agent;
 *   }
 *============================================================================*/

#define NIMCP_HEALTH_AGENT_SET(MODULE)                                        \
    void NIMCP_HA_CONCAT(MODULE, _set_health_agent)(nimcp_health_agent_t* agent) { \
        NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent)) = agent;  \
    }

/*=============================================================================
 * NIMCP_HEALTH_AGENT_HEARTBEAT - Generate the heartbeat function
 *
 * Generates:
 *   static inline void MODULE_heartbeat(const char* operation, float progress) {
 *       if (g_MODULE_health_agent) {
 *           nimcp_health_agent_heartbeat_ex(g_MODULE_health_agent, operation, progress);
 *       }
 *   }
 *============================================================================*/

#define NIMCP_HEALTH_AGENT_HEARTBEAT(MODULE)                                   \
    static inline void NIMCP_HA_CONCAT(MODULE, _heartbeat)(const char* operation, float progress) { \
        if (NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent))) {    \
            nimcp_health_agent_heartbeat_ex(                                   \
                NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent)),   \
                operation,                                                     \
                progress                                                       \
            );                                                                 \
        }                                                                      \
    }

/*=============================================================================
 * NIMCP_DECLARE_HEALTH_AGENT - Complete health agent declaration
 *
 * This is the primary macro for most use cases. It generates:
 *   1. Static global health agent pointer
 *   2. Set function to assign the health agent
 *   3. Heartbeat function to send progress updates
 *
 * Usage:
 *   NIMCP_DECLARE_HEALTH_AGENT(mymodule)
 *
 * Generates:
 *   static nimcp_health_agent_t* g_mymodule_health_agent = NULL;
 *
 *   void mymodule_set_health_agent(nimcp_health_agent_t* agent) {
 *       g_mymodule_health_agent = agent;
 *   }
 *
 *   static inline void mymodule_heartbeat(const char* operation, float progress) {
 *       if (g_mymodule_health_agent) {
 *           nimcp_health_agent_heartbeat_ex(g_mymodule_health_agent, operation, progress);
 *       }
 *   }
 *============================================================================*/

#define NIMCP_DECLARE_HEALTH_AGENT(MODULE)                                     \
    /* Static global health agent pointer */                                   \
    static nimcp_health_agent_t* NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent)) = NULL; \
                                                                               \
    /* Set function */                                                         \
    NIMCP_HEALTH_AGENT_SET(MODULE)                                            \
                                                                               \
    /* Heartbeat function */                                                   \
    NIMCP_HEALTH_AGENT_HEARTBEAT(MODULE)

/*=============================================================================
 * NIMCP_DECLARE_HEALTH_AGENT_STATIC - Static-only version
 *
 * Same as NIMCP_DECLARE_HEALTH_AGENT but makes the set function static.
 * Use this when the set function should not be externally visible.
 *
 * Usage:
 *   NIMCP_DECLARE_HEALTH_AGENT_STATIC(mymodule)
 *============================================================================*/

#define NIMCP_HEALTH_AGENT_SET_STATIC(MODULE)                                  \
    static void NIMCP_HA_CONCAT(MODULE, _set_health_agent)(nimcp_health_agent_t* agent) { \
        NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent)) = agent;  \
    }

#define NIMCP_DECLARE_HEALTH_AGENT_STATIC(MODULE)                              \
    /* Static global health agent pointer */                                   \
    static nimcp_health_agent_t* NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent)) = NULL; \
                                                                               \
    /* Static set function */                                                  \
    NIMCP_HEALTH_AGENT_SET_STATIC(MODULE)                                     \
                                                                               \
    /* Heartbeat function */                                                   \
    NIMCP_HEALTH_AGENT_HEARTBEAT(MODULE)

/*=============================================================================
 * NIMCP_HEALTH_AGENT_EXTERN - External declaration for headers
 *
 * Use this in header files to declare the set function prototype.
 *
 * Usage (in .h file):
 *   NIMCP_HEALTH_AGENT_EXTERN(mymodule)
 *
 * Generates:
 *   void mymodule_set_health_agent(nimcp_health_agent_t* agent);
 *============================================================================*/

#define NIMCP_HEALTH_AGENT_EXTERN(MODULE)                                      \
    void NIMCP_HA_CONCAT(MODULE, _set_health_agent)(nimcp_health_agent_t* agent)

/*=============================================================================
 * Atomic Variants for Thread-Safe Access
 *
 * These macros use C11 atomics for thread-safe health agent access.
 * Use these for modules that may be accessed from multiple threads
 * (e.g., mesh modules, networking modules).
 *============================================================================*/

#define NIMCP_HEALTH_AGENT_SET_ATOMIC(MODULE)                                  \
    void NIMCP_HA_CONCAT(MODULE, _set_health_agent)(nimcp_health_agent_t* agent) { \
        atomic_store(&NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent)), agent); \
    }

#define NIMCP_HEALTH_AGENT_HEARTBEAT_ATOMIC(MODULE)                            \
    static inline void NIMCP_HA_CONCAT(MODULE, _heartbeat)(const char* operation, float progress) { \
        nimcp_health_agent_t* agent = atomic_load(&NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent))); \
        if (agent) {                                                           \
            nimcp_health_agent_heartbeat_ex(agent, operation, progress);       \
        }                                                                      \
    }

#define NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(MODULE)                              \
    /* Atomic global health agent pointer */                                   \
    static _Atomic(nimcp_health_agent_t*) NIMCP_HA_CONCAT(g_, NIMCP_HA_CONCAT(MODULE, _health_agent)) = NULL; \
                                                                               \
    /* Atomic set function */                                                  \
    NIMCP_HEALTH_AGENT_SET_ATOMIC(MODULE)                                     \
                                                                               \
    /* Atomic heartbeat function */                                            \
    NIMCP_HEALTH_AGENT_HEARTBEAT_ATOMIC(MODULE)

/*=============================================================================
 * Alias Macros for Simplified API
 *
 * These provide shorter names for common use cases:
 *   DEFINE_HEALTH_AGENT(prefix)        -> NIMCP_DECLARE_HEALTH_AGENT(prefix)
 *   DEFINE_HEALTH_AGENT_ATOMIC(prefix) -> NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(prefix)
 *   DEFINE_HEALTH_AGENT_STATIC(prefix) -> NIMCP_DECLARE_HEALTH_AGENT_STATIC(prefix)
 *============================================================================*/

#define DEFINE_HEALTH_AGENT(prefix)         NIMCP_DECLARE_HEALTH_AGENT(prefix)
#define DEFINE_HEALTH_AGENT_ATOMIC(prefix)  NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(prefix)
#define DEFINE_HEALTH_AGENT_STATIC(prefix)  NIMCP_DECLARE_HEALTH_AGENT_STATIC(prefix)
#define HEALTH_AGENT_HEARTBEAT_CALL(prefix, op, prog) NIMCP_HA_CONCAT(prefix, _heartbeat)((op), (prog))
#define DECLARE_HEALTH_AGENT_SETTER(prefix) NIMCP_HEALTH_AGENT_EXTERN(prefix)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEALTH_AGENT_MACROS_H */
