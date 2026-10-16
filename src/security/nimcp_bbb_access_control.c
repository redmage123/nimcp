/**
 * @file nimcp_bbb_access_control.c
 * @brief Blood-Brain Barrier - Access Control Enforcer Implementation
 *
 * WHAT: Implements capability-based access control with role-based policies
 *       for the NIMCP Blood-Brain Barrier security layer.
 *
 * WHY:  The access control enforcer acts as "pericytes" in the BBB,
 *       preventing unauthorized access to neural substrate resources:
 *       - Unauthorized function execution
 *       - Privilege escalation attacks
 *       - Cross-component interference
 *       - Resource hijacking
 *
 * HOW:  Uses a combination of:
 *       1. Subject registry (entities requesting access)
 *       2. Object registry (resources being accessed)
 *       3. Capability-based permissions (fine-grained control)
 *       4. Role-based access control (RBAC) for organizational policy
 *
 * BIOLOGICAL MODEL:
 * ```
 * Pericytes                  ->  Access Control Enforcer
 * - Control vessel diameter  ->  Control access permissions
 * - Filter blood contents    ->  Filter unauthorized requests
 * - Regulate capillary flow  ->  Regulate resource access
 * - Contract to block entry  ->  Deny unauthorized access
 * ```
 *
 * ACCESS CONTROL MODEL:
 * ```
 * Subject (who)     + Object (what)   + Capability (how) = Decision
 * ----------------   ---------------   -----------------   --------
 * neuron_reader     + weight_matrix   + CAP_READ          = ALLOW
 * neuron_writer     + activation      + CAP_WRITE         = ALLOW
 * external_entity   + core_directive  + CAP_MODIFY        = DENY
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe considerations
 *
 * @author NIMCP Team
 * @date 2025-11-24
 */

#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>  /* for NULL */
#include <stdatomic.h>

#define LOG_MODULE "security_bbb_access_control"

//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Global health agent for bbb_access_control module - P2 fix: Use atomic for thread safety */
static _Atomic(nimcp_health_agent_t*) g_bbb_access_control_health_agent = NULL;

/**
 * @brief Set health agent for bbb_access_control heartbeats (thread-safe)
 * @param agent Health agent (can be NULL to disable)
 */
void bbb_access_control_set_health_agent(nimcp_health_agent_t* agent) {
    atomic_store(&g_bbb_access_control_health_agent, agent);
}

/** @brief Send heartbeat from bbb_access_control module (thread-safe) */
static inline void bbb_access_control_heartbeat(const char* operation, float progress) {
    /* P2 fix: Atomic load to prevent data race */
    nimcp_health_agent_t* agent = atomic_load(&g_bbb_access_control_health_agent);
    if (agent) {
        nimcp_health_agent_heartbeat_ex(agent, operation, progress);
    }
}

//=============================================================================
// Constants
//=============================================================================

/**
 * WHAT: Maximum number of registered subjects
 * WHY:  Bounded array enables efficient O(1) lookup by ID
 */
#define BBB_MAX_SUBJECTS 256

/**
 * WHAT: Maximum number of registered objects
 * WHY:  Bounded array enables efficient O(1) lookup by ID
 */
#define BBB_MAX_OBJECTS 256

/**
 * WHAT: Invalid subject/object ID sentinel
 * WHY:  Distinguishes unregistered entries from valid IDs
 */
#define BBB_INVALID_ID 0

//=============================================================================
// Capability Bits
//=============================================================================

/**
 * WHAT: Fine-grained capability bit definitions
 * WHY:  Each bit represents a specific permission that can be granted/revoked
 *
 * DESIGN: 64-bit bitmask allows 64 distinct capabilities
 *         Bitwise operations enable efficient permission checking
 */
#define BBB_CAP_READ          (1ULL << 0)   /**< Read access */
#define BBB_CAP_WRITE         (1ULL << 1)   /**< Write access */
#define BBB_CAP_EXECUTE       (1ULL << 2)   /**< Execute/call access */
#define BBB_CAP_DELETE        (1ULL << 3)   /**< Delete access */
#define BBB_CAP_CREATE        (1ULL << 4)   /**< Create new resources */
#define BBB_CAP_ADMIN         (1ULL << 5)   /**< Administrative access */
#define BBB_CAP_PROPAGATE     (1ULL << 6)   /**< Can grant to others */
#define BBB_CAP_NEURAL_READ   (1ULL << 7)   /**< Read neural state */
#define BBB_CAP_NEURAL_WRITE  (1ULL << 8)   /**< Modify neural state */
#define BBB_CAP_WEIGHT_UPDATE (1ULL << 9)   /**< Update synaptic weights */
#define BBB_CAP_PLASTICITY    (1ULL << 10)  /**< Modify plasticity rules */
#define BBB_CAP_NEUROMOD      (1ULL << 11)  /**< Control neuromodulators */
#define BBB_CAP_TOPOLOGY      (1ULL << 12)  /**< Modify network structure */
#define BBB_CAP_DIRECTIVE     (1ULL << 13)  /**< Access core directives */
#define BBB_CAP_SECURITY      (1ULL << 14)  /**< Modify security settings */

//=============================================================================
// Role Definitions
//=============================================================================

/**
 * WHAT: Predefined role bitmasks for RBAC
 * WHY:  Roles group related permissions for organizational policy
 *
 * DESIGN: Each role is a bitmask that can be assigned to subjects
 *         Subject's effective permissions = role_caps | explicit_caps
 */
#define BBB_ROLE_NONE         (0)
#define BBB_ROLE_READER       (1U << 0)   /**< Read-only access role */
#define BBB_ROLE_WRITER       (1U << 1)   /**< Read-write access role */
#define BBB_ROLE_OPERATOR     (1U << 2)   /**< System operator role */
#define BBB_ROLE_ADMIN        (1U << 3)   /**< Administrator role */
#define BBB_ROLE_NEURAL_CORE  (1U << 4)   /**< Core neural component role */
#define BBB_ROLE_PLASTICITY   (1U << 5)   /**< Plasticity subsystem role */
#define BBB_ROLE_SECURITY     (1U << 6)   /**< Security subsystem role */

//=============================================================================
// Access Types
//=============================================================================

/**
 * WHAT: Access type flags for access checking
 * WHY:  Represents the type of access being requested
 */
#define BBB_ACCESS_READ    0x01
#define BBB_ACCESS_WRITE   0x02
#define BBB_ACCESS_EXECUTE 0x04

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal subject record with extended metadata
 *
 * WHAT: Tracks a registered access control subject with audit info
 * WHY:  Extends public bbb_subject_t with internal tracking data
 */
typedef struct {
    bbb_subject_t subject;        /**< Public subject data */
    bool active;                  /**< Whether entry is in use */
    uint64_t registration_ts;     /**< When registered */
    uint64_t last_access_ts;      /**< Last access attempt time */
    uint64_t access_count;        /**< Total access attempts */
    uint64_t denied_count;        /**< Total denied accesses */
} bbb_subject_record_t;

/**
 * @brief Internal object record with extended metadata
 *
 * WHAT: Tracks a registered access control object with audit info
 * WHY:  Extends public bbb_object_t with internal tracking data
 */
typedef struct {
    bbb_object_t object;          /**< Public object data */
    bool active;                  /**< Whether entry is in use */
    uint64_t registration_ts;     /**< When registered */
    uint64_t access_count;        /**< Total access attempts */
} bbb_object_record_t;

/**
 * @brief Access control system state
 *
 * WHAT: Internal state for the access control subsystem
 * WHY:  Encapsulates all subjects, objects, and configuration
 */
typedef struct {
    bbb_subject_record_t subjects[BBB_MAX_SUBJECTS];
    bbb_object_record_t objects[BBB_MAX_OBJECTS];
    uint32_t subject_count;           /**< Number of active subjects */
    uint32_t object_count;            /**< Number of active objects */
    uint64_t total_checks;            /**< Total access checks performed */
    uint64_t total_denials;           /**< Total access denials */
    bool initialized;                 /**< Whether subsystem is initialized */
    bool log_access_attempts;         /**< Whether to log all attempts */
} bbb_access_state_t;

//=============================================================================
// Global State
//=============================================================================

/**
 * WHAT: Global access control state
 * WHY:  Singleton pattern - one instance per process
 *
 * NOTE: In production, this would be stored in bbb_system_t structure.
 *
 * THREAD-SAFETY: Protected by g_access_state_lock mutex
 */
static bbb_access_state_t g_access_state = {0};

/**
 * WHAT: Mutex protecting g_access_state
 * WHY:  Thread-safe access to global state
 * HOW:  All read-modify-write operations must hold this lock
 */
static nimcp_platform_mutex_t g_access_state_lock;

/**
 * WHAT: Atomic statistics counters for lock-free updates
 * WHY:  Statistics are frequently updated, avoid lock contention
 * HOW:  Use atomic operations for increment-only counters
 */
static nimcp_atomic_uint64_t g_atomic_total_checks = {0};
static nimcp_atomic_uint64_t g_atomic_total_denials = {0};

/**
 * WHAT: Module initialization state using platform_once
 * WHY:  Thread-safe lazy initialization
 */
static nimcp_platform_once_t g_access_control_init_once = NIMCP_PLATFORM_ONCE_INIT;
static bool g_access_control_module_initialized = false;

/* Note: g_bbb_access_control_health_agent is declared near top of file with _Atomic type */

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Find subject index by ID
 *
 * WHAT: Locate a registered subject by its unique ID
 * WHY:  Required for permission checks and capability management
 * HOW:  Linear scan through active subjects
 *
 * COMPLEXITY: O(n) where n = number of registered subjects
 *
 * @param id Subject ID to find
 * @return Index in subjects array, or -1 if not found
 */
static int find_subject_by_id(uint32_t id)
{
    if (id == BBB_INVALID_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "find_subject_by_id: invalid ID 0");
        return -1;
    }

    for (uint32_t i = 0; i < BBB_MAX_SUBJECTS; i++) {
        if (g_access_state.subjects[i].active &&
            g_access_state.subjects[i].subject.id == id) {
            return (int)i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
        "find_subject_by_id: subject %u not found", id);
    return -1;
}

/**
 * @brief Find object index by ID
 *
 * WHAT: Locate a registered object by its unique ID
 * WHY:  Required for access checking
 * HOW:  Linear scan through active objects
 *
 * COMPLEXITY: O(n) where n = number of registered objects
 *
 * @param id Object ID to find
 * @return Index in objects array, or -1 if not found
 */
static int find_object_by_id(uint32_t id)
{
    if (id == BBB_INVALID_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "find_object_by_id: invalid ID 0");
        return -1;
    }

    for (uint32_t i = 0; i < BBB_MAX_OBJECTS; i++) {
        if (g_access_state.objects[i].active &&
            g_access_state.objects[i].object.id == id) {
            return (int)i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
        "find_object_by_id: object %u not found", id);
    return -1;
}

/**
 * @brief Find first available subject slot
 *
 * WHAT: Locate an unused slot for new subject registration
 * WHY:  Enables O(1) registration when slots are available
 *
 * @return Index of available slot, or -1 if full
 */
static int find_available_subject_slot(void)
{
    for (uint32_t i = 0; i < BBB_MAX_SUBJECTS; i++) {
        if (!g_access_state.subjects[i].active) {
            return (int)i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED,
        "find_available_subject_slot: subject registry full (%u slots)",
        (unsigned)BBB_MAX_SUBJECTS);
    return -1;
}

/**
 * @brief Find first available object slot
 *
 * WHAT: Locate an unused slot for new object registration
 * WHY:  Enables O(1) registration when slots are available
 *
 * @return Index of available slot, or -1 if full
 */
static int find_available_object_slot(void)
{
    for (uint32_t i = 0; i < BBB_MAX_OBJECTS; i++) {
        if (!g_access_state.objects[i].active) {
            return (int)i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED,
        "find_available_object_slot: object registry full (%u slots)",
        (unsigned)BBB_MAX_OBJECTS);
    return -1;
}

/**
 * @brief Get capabilities granted by a role bitmask
 *
 * WHAT: Convert role bitmask to capability bitmask
 * WHY:  RBAC - roles grant sets of capabilities
 * HOW:  Map each role bit to its corresponding capabilities
 *
 * @param roles Role bitmask
 * @return Capability bitmask granted by roles
 */
static uint64_t get_role_capabilities(uint32_t roles)
{
    uint64_t caps = 0;

    /* WHAT: Reader role grants read capabilities */
    if (roles & BBB_ROLE_READER) {
        caps |= BBB_CAP_READ | BBB_CAP_NEURAL_READ;
    }

    /* WHAT: Writer role grants read + write capabilities */
    if (roles & BBB_ROLE_WRITER) {
        caps |= BBB_CAP_READ | BBB_CAP_WRITE | BBB_CAP_NEURAL_READ | BBB_CAP_NEURAL_WRITE;
    }

    /* WHAT: Operator role grants operational capabilities */
    if (roles & BBB_ROLE_OPERATOR) {
        caps |= BBB_CAP_READ | BBB_CAP_WRITE | BBB_CAP_EXECUTE | BBB_CAP_CREATE;
        caps |= BBB_CAP_NEURAL_READ | BBB_CAP_NEURAL_WRITE;
    }

    /* WHAT: Admin role grants all capabilities */
    if (roles & BBB_ROLE_ADMIN) {
        caps |= BBB_CAP_READ | BBB_CAP_WRITE | BBB_CAP_EXECUTE;
        caps |= BBB_CAP_DELETE | BBB_CAP_CREATE | BBB_CAP_ADMIN | BBB_CAP_PROPAGATE;
        caps |= BBB_CAP_NEURAL_READ | BBB_CAP_NEURAL_WRITE | BBB_CAP_WEIGHT_UPDATE;
        caps |= BBB_CAP_PLASTICITY | BBB_CAP_NEUROMOD | BBB_CAP_TOPOLOGY;
    }

    /* WHAT: Neural core role grants neural-specific capabilities */
    if (roles & BBB_ROLE_NEURAL_CORE) {
        caps |= BBB_CAP_NEURAL_READ | BBB_CAP_NEURAL_WRITE | BBB_CAP_WEIGHT_UPDATE;
    }

    /* WHAT: Plasticity role grants learning-related capabilities */
    if (roles & BBB_ROLE_PLASTICITY) {
        caps |= BBB_CAP_NEURAL_READ | BBB_CAP_WEIGHT_UPDATE | BBB_CAP_PLASTICITY;
    }

    /* WHAT: Security role grants security management capabilities */
    if (roles & BBB_ROLE_SECURITY) {
        caps |= BBB_CAP_READ | BBB_CAP_SECURITY | BBB_CAP_DIRECTIVE;
    }

    return caps;
}

/**
 * @brief Convert access type to required capability
 *
 * WHAT: Map access type flags to capability requirements
 * WHY:  Translates access requests into capability checks
 *
 * @param access_type Access type flags
 * @return Required capability bitmask
 */
static uint64_t access_type_to_capability(uint32_t access_type)
{
    uint64_t caps = 0;

    if (access_type & BBB_ACCESS_READ)
        caps |= BBB_CAP_READ;
    if (access_type & BBB_ACCESS_WRITE)
        caps |= BBB_CAP_WRITE;
    if (access_type & BBB_ACCESS_EXECUTE)
        caps |= BBB_CAP_EXECUTE;

    return caps;
}

/**
 * @brief Internal initialization routine called by nimcp_platform_once
 *
 * WHAT: Thread-safe one-time initialization of module state
 * WHY:  Ensures mutex and state are initialized exactly once
 */
static void access_control_module_init_internal(void)
{
    /* Initialize the global state mutex */
    nimcp_platform_mutex_init(&g_access_state_lock, false);

    /* Initialize atomic counters */
    nimcp_atomic_init_u64(&g_atomic_total_checks, 0);
    nimcp_atomic_init_u64(&g_atomic_total_denials, 0);
    /* Health agent uses _Atomic type, no init needed (statically initialized to NULL) */

    /* Initialize the state itself */
    memset(&g_access_state, 0, sizeof(g_access_state));
    g_access_state.initialized = true;
    g_access_state.log_access_attempts = true;

    g_access_control_module_initialized = true;

    LOG_MODULE_INFO("bbb_access_control", "Access control module initialized");
}

/**
 * @brief Initialize access control subsystem (thread-safe)
 *
 * WHAT: Initialize global access state on first use
 * WHY:  Lazy initialization avoids startup overhead
 * HOW:  Uses nimcp_platform_once for thread-safe init
 *
 * THREAD-SAFETY: nimcp_platform_once guarantees exactly-once execution
 */
static void ensure_initialized(void)
{
    /* Thread-safe one-time initialization */
    nimcp_platform_once(&g_access_control_init_once, access_control_module_init_internal);
}

/**
 * @brief Reset access control state (internal)
 *
 * WHAT: Clear all registered subjects and objects
 * WHY:  Enable test isolation by resetting between test cases
 * HOW:  Zero out global state and re-initialize under lock
 *
 * THREAD-SAFETY: Acquires g_access_state_lock before modifying state
 */
void bbb_access_control_reset_internal(void)
{
    /* Ensure module is initialized first */
    ensure_initialized();

    /* Lock before modifying state */
    nimcp_platform_mutex_lock(&g_access_state_lock);

    memset(&g_access_state, 0, sizeof(g_access_state));
    g_access_state.initialized = true;
    g_access_state.log_access_attempts = true;

    /* Reset atomic counters */
    nimcp_atomic_store_u64(&g_atomic_total_checks, 0, NIMCP_MEMORY_ORDER_RELEASE);
    nimcp_atomic_store_u64(&g_atomic_total_denials, 0, NIMCP_MEMORY_ORDER_RELEASE);

    nimcp_platform_mutex_unlock(&g_access_state_lock);
}

/**
 * @brief Log access denial and update statistics (thread-safe)
 *
 * WHAT: Record denial in statistics and optionally log message
 * WHY:  Centralizes denial handling for cleaner access check code
 *
 * THREAD-SAFETY: Uses atomic increment for statistics counter
 */
static void log_denial(const char* reason, uint32_t subject_id)
{
    /* Atomic increment for statistics (lock-free) */
    nimcp_atomic_fetch_add_u64(&g_atomic_total_denials, 1, NIMCP_MEMORY_ORDER_RELAXED);

    /* Read log flag - may race but acceptable for logging decision */
    if (g_access_state.log_access_attempts) {
        fprintf(stderr, "[BBB-AC] DENIED: %s (subject %u)\n", reason, subject_id);
    }
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * @brief Check if access is permitted
 *
 * WHAT: Evaluate subject's permission to access object
 * WHY:  Core access control decision point
 * HOW:  Check privilege level, roles, and capabilities
 *
 * ALGORITHM:
 * 1. Check privilege level (subject >= object required)
 * 2. Check role requirements (subject has required roles)
 * 3. Check capability requirements (subject has required capabilities)
 *
 * COMPLEXITY: O(1) - constant time permission check
 *
 * @param system BBB system handle
 * @param subject Subject requesting access
 * @param object Object being accessed
 * @param access_type Type of access (read/write/execute)
 * @return true if access permitted, false if denied
 */
NIMCP_EXPORT bool bbb_check_access(bbb_system_t system,
                                    const bbb_subject_t* subject,
                                    const bbb_object_t* object,
                                    uint32_t access_type)
{
    (void)system;

    /* Guard: Null parameters */
    if (!subject || !object) {
        return false;
    }

    ensure_initialized();

    /* Atomic increment for statistics (lock-free) */
    nimcp_atomic_fetch_add_u64(&g_atomic_total_checks, 1, NIMCP_MEMORY_ORDER_RELAXED);

    /* Check privilege level requirement */
    if (subject->privilege_level < object->required_privilege) {
        log_denial("insufficient privilege level", subject->id);
        return false;
    }

    /* Check role requirement */
    if (object->required_roles != 0) {
        if ((subject->roles & object->required_roles) != object->required_roles) {
            log_denial("missing required roles", subject->id);
            return false;
        }
    }

    /* Compute subject's effective capabilities (roles + explicit grants) */
    uint64_t role_caps = get_role_capabilities(subject->roles);
    uint64_t effective_caps = subject->capabilities | role_caps;

    /* Check object-specific capability requirements */
    if (object->required_capabilities != 0) {
        if ((effective_caps & object->required_capabilities) != object->required_capabilities) {
            log_denial("missing required capabilities", subject->id);
            return false;
        }
    }

    /* Check access type capability */
    uint64_t required_caps = access_type_to_capability(access_type);
    if ((effective_caps & required_caps) != required_caps) {
        log_denial("missing access type capability", subject->id);
        return false;
    }

    return true;
}

/**
 * @brief Register access control subject
 *
 * WHAT: Add a subject to the access control registry
 * WHY:  Registered subjects can be granted capabilities and tracked
 * HOW:  Store subject in internal registry with metadata
 *
 * COMPLEXITY: O(n) for slot search, O(1) for registration
 *
 * @param system BBB system handle
 * @param subject Subject to register
 * @return true on success, false if registry full or invalid subject
 */
NIMCP_EXPORT bool bbb_register_subject(bbb_system_t system,
                                        const bbb_subject_t* subject)
{
    (void)system;

    /* Guard: Null subject */
    if (!subject)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_register_subject: subject is NULL");

            return false;

        }

    /* Guard: Invalid subject ID */
    if (subject->id == BBB_INVALID_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_register_subject: validation failed");
        return false;
    }

    ensure_initialized();

    /* Lock for read-modify-write operations on global state */
    nimcp_platform_mutex_lock(&g_access_state_lock);

    /* Guard: Subject already registered */
    if (find_subject_by_id(subject->id) >= 0) {
        nimcp_platform_mutex_unlock(&g_access_state_lock);
        fprintf(stderr, "[BBB-AC] Subject %u already registered\n", subject->id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_register_subject: capacity exceeded");
        return false;
    }

    /* Guard: No available slots */
    int slot = find_available_subject_slot();
    if (slot < 0) {
        nimcp_platform_mutex_unlock(&g_access_state_lock);
        fprintf(stderr, "[BBB-AC] Subject registry full (max %d)\n", BBB_MAX_SUBJECTS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_register_subject: validation failed");
        return false;
    }

    /* WHAT: Store subject with metadata
     * WHY:  Track registration time and access statistics
     */
    bbb_subject_record_t* record = &g_access_state.subjects[slot];
    record->subject = *subject;
    record->active = true;
    record->registration_ts = (uint64_t)time(NULL);
    record->last_access_ts = 0;
    record->access_count = 0;
    record->denied_count = 0;

    g_access_state.subject_count++;

    nimcp_platform_mutex_unlock(&g_access_state_lock);

    return true;
}

/**
 * @brief Register access control object
 *
 * WHAT: Add an object to the access control registry
 * WHY:  Registered objects have defined access requirements
 * HOW:  Store object in internal registry with metadata
 *
 * COMPLEXITY: O(n) for slot search, O(1) for registration
 *
 * @param system BBB system handle
 * @param object Object to register
 * @return true on success, false if registry full or invalid object
 */
NIMCP_EXPORT bool bbb_register_object(bbb_system_t system,
                                       const bbb_object_t* object)
{
    (void)system;

    /* Guard: Null object */
    if (!object)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_register_object: object is NULL");

            return false;

        }

    /* Guard: Invalid object ID */
    if (object->id == BBB_INVALID_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_register_object: validation failed");
        return false;
    }

    ensure_initialized();

    /* Lock for read-modify-write operations on global state */
    nimcp_platform_mutex_lock(&g_access_state_lock);

    /* Guard: Object already registered */
    if (find_object_by_id(object->id) >= 0) {
        nimcp_platform_mutex_unlock(&g_access_state_lock);
        fprintf(stderr, "[BBB-AC] Object %u already registered\n", object->id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_register_object: capacity exceeded");
        return false;
    }

    /* Guard: No available slots */
    int slot = find_available_object_slot();
    if (slot < 0) {
        nimcp_platform_mutex_unlock(&g_access_state_lock);
        fprintf(stderr, "[BBB-AC] Object registry full (max %d)\n", BBB_MAX_OBJECTS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_register_object: validation failed");
        return false;
    }

    /* WHAT: Store object with metadata
     * WHY:  Track registration time and access statistics
     */
    bbb_object_record_t* record = &g_access_state.objects[slot];
    record->object = *object;
    record->active = true;
    record->registration_ts = (uint64_t)time(NULL);
    record->access_count = 0;

    g_access_state.object_count++;

    nimcp_platform_mutex_unlock(&g_access_state_lock);

    return true;
}

/**
 * @brief Grant capability to subject
 *
 * WHAT: Add capability bits to a registered subject
 * WHY:  Enables fine-grained permission grants
 * HOW:  Bitwise OR to add capability to existing set
 *
 * SECURITY: Only subjects with CAP_PROPAGATE should be able to grant
 *           (This would be enforced at a higher level)
 *
 * @param system BBB system handle
 * @param subject_id Subject to grant capability to
 * @param capability Capability bitmask to grant
 * @return true on success, false if subject not found
 */
NIMCP_EXPORT bool bbb_grant_capability(bbb_system_t system,
                                        uint32_t subject_id,
                                        uint64_t capability)
{
    (void)system;

    /* Guard: Invalid subject ID */
    if (subject_id == BBB_INVALID_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_grant_capability: validation failed");
        return false;
    }

    /* Guard: No capability to grant */
    if (capability == 0)
        return true;  /* No-op is success */

    ensure_initialized();

    /* Lock for read-modify-write on subject capabilities */
    nimcp_platform_mutex_lock(&g_access_state_lock);

    /* Find subject */
    int slot = find_subject_by_id(subject_id);
    if (slot < 0) {
        nimcp_platform_mutex_unlock(&g_access_state_lock);
        fprintf(stderr, "[BBB-AC] Cannot grant: subject %u not found\n", subject_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_grant_capability: validation failed");
        return false;
    }

    /* WHAT: Add capability using bitwise OR
     * WHY:  Preserves existing capabilities while adding new ones
     */
    g_access_state.subjects[slot].subject.capabilities |= capability;

    nimcp_platform_mutex_unlock(&g_access_state_lock);

    return true;
}

/**
 * @brief Revoke capability from subject
 *
 * WHAT: Remove capability bits from a registered subject
 * WHY:  Enables capability downgrade and revocation
 * HOW:  Bitwise AND with inverted mask to remove capability
 *
 * @param system BBB system handle
 * @param subject_id Subject to revoke capability from
 * @param capability Capability bitmask to revoke
 * @return true on success, false if subject not found
 */
NIMCP_EXPORT bool bbb_revoke_capability(bbb_system_t system,
                                         uint32_t subject_id,
                                         uint64_t capability)
{
    (void)system;

    /* Guard: Invalid subject ID */
    if (subject_id == BBB_INVALID_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_revoke_capability: validation failed");
        return false;
    }

    /* Guard: No capability to revoke */
    if (capability == 0)
        return true;  /* No-op is success */

    ensure_initialized();

    /* Lock for read-modify-write on subject capabilities */
    nimcp_platform_mutex_lock(&g_access_state_lock);

    /* Find subject */
    int slot = find_subject_by_id(subject_id);
    if (slot < 0) {
        nimcp_platform_mutex_unlock(&g_access_state_lock);
        fprintf(stderr, "[BBB-AC] Cannot revoke: subject %u not found\n", subject_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_revoke_capability: validation failed");
        return false;
    }

    /* WHAT: Remove capability using bitwise AND with inverted mask
     * WHY:  Clears only the specified bits, preserves others
     */
    g_access_state.subjects[slot].subject.capabilities &= ~capability;

    nimcp_platform_mutex_unlock(&g_access_state_lock);

    return true;
}
