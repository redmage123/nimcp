/**
 * @file nimcp_bbb_helpers.h
 * @brief Simplified BBB Helper Functions for Easy Integration
 *
 * WHAT: Simplified wrapper functions for Blood-Brain Barrier security
 * WHY:  Provide easy-to-use security validation without managing bbb_system_t handles
 * HOW:  Global BBB instance with simplified validation and logging functions
 *
 * USAGE EXAMPLE:
 * ```c
 * #include "security/nimcp_bbb_helpers.h"
 *
 * // In init function
 * bbb_register_module("swarm_protocol", BBB_MODULE_TYPE_SWARM);
 *
 * // In public functions
 * if (!bbb_check_pointer(adapter, "swarm_signal_send")) {
 *     return NIMCP_ERROR_INVALID_PARAM;
 * }
 *
 * if (!bbb_check_string(str, max_len, "swarm_signal_send")) {
 *     return NIMCP_ERROR_INVALID_PARAM;
 * }
 *
 * // Log security events
 * bbb_audit_log(BBB_AUDIT_INFO, "swarm_protocol", "message_sent",
 *               "dest=%u size=%u", dest_id, len);
 * ```
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#ifndef NIMCP_BBB_HELPERS_H
#define NIMCP_BBB_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module Types
//=============================================================================

typedef enum {
    BBB_MODULE_TYPE_COGNITIVE = 0,
    BBB_MODULE_TYPE_CORE,
    BBB_MODULE_TYPE_SWARM,
    BBB_MODULE_TYPE_PLATFORM,
    BBB_MODULE_TYPE_NETWORK,
    BBB_MODULE_TYPE_PLASTICITY,
    BBB_MODULE_TYPE_MIDDLEWARE
} bbb_module_type_t;

//=============================================================================
// Audit Log Levels
//=============================================================================

typedef enum {
    BBB_AUDIT_DEBUG = 0,
    BBB_AUDIT_INFO,
    BBB_AUDIT_WARNING,
    BBB_AUDIT_ERROR,
    BBB_AUDIT_CRITICAL
} bbb_audit_level_t;

//=============================================================================
// Privilege Levels
//=============================================================================

typedef enum {
    BBB_PRIV_PUBLIC = 0,
    BBB_PRIV_INTERNAL,
    BBB_PRIV_PRIVILEGED,
    BBB_PRIV_SWARM_COMMAND,
    BBB_PRIV_ADMIN
} bbb_privilege_t;

//=============================================================================
// Threat Detection Types
//=============================================================================

// Only define if not already defined in nimcp_blood_brain_barrier.h
#ifndef BBB_THREAT_TYPE_DEFINED
#define BBB_THREAT_TYPE_DEFINED
typedef enum {
    BBB_THREAT_NONE = 0,
    BBB_THREAT_BUFFER_OVERFLOW,
    BBB_THREAT_FORMAT_STRING,
    BBB_THREAT_SQL_INJECTION,
    BBB_THREAT_CODE_INJECTION,
    BBB_THREAT_SHELLCODE,
    BBB_THREAT_INVALID_SIGNATURE,
    BBB_THREAT_MEMORY_VIOLATION,
    BBB_THREAT_UNAUTHORIZED_ACCESS,
    BBB_THREAT_NETWORK_INJECTION,
    BBB_THREAT_BYZANTINE_ATTACK,
    BBB_THREAT_UNKNOWN
} bbb_threat_type_t;
#endif /* BBB_THREAT_TYPE_DEFINED */

//=============================================================================
// Initialization API
//=============================================================================

/**
 * @brief Initialize global BBB helper system
 * @return true on success
 */
bool bbb_helpers_init(void);

/**
 * @brief Shutdown global BBB helper system
 */
void bbb_helpers_shutdown(void);

/**
 * @brief Check if BBB helpers are initialized
 * @return true if initialized
 */
bool bbb_helpers_is_initialized(void);

//=============================================================================
// Module Registration
//=============================================================================

/**
 * @brief Register module with BBB for monitoring
 * @param module_name Name of the module (e.g., "swarm_protocol")
 * @param type Type of module
 * @return true on success
 */
bool bbb_register_module(const char* module_name, bbb_module_type_t type);

/**
 * @brief Unregister module from BBB
 * @param module_name Name of the module
 * @return true on success
 */
bool bbb_unregister_module(const char* module_name);

//=============================================================================
// Validation Functions
//=============================================================================

/**
 * @brief Validate pointer is not NULL (simplified API)
 * @param ptr Pointer to validate
 * @param function_name Name of calling function (for logging)
 * @return true if valid
 */
bool bbb_check_pointer(const void* ptr, const char* function_name);

/**
 * @brief Validate string (simplified API)
 * @param str String to validate
 * @param max_len Maximum allowed length
 * @param function_name Name of calling function
 * @return true if valid
 */
bool bbb_check_string(const char* str, size_t max_len, const char* function_name);

/**
 * @brief Validate integer is in range
 * @param value Value to validate
 * @param min Minimum value
 * @param max Maximum value
 * @param function_name Name of calling function
 * @return true if valid
 */
bool bbb_validate_range(int64_t value, int64_t min, int64_t max, const char* function_name);

/**
 * @brief Validate unsigned integer is in range
 * @param value Value to validate
 * @param min Minimum value
 * @param max Maximum value
 * @param function_name Name of calling function
 * @return true if valid
 */
bool bbb_validate_range_u(uint64_t value, uint64_t min, uint64_t max, const char* function_name);

/**
 * @brief Validate network data for injection attacks
 * @param data Network data buffer
 * @param length Length of data
 * @param function_name Name of calling function
 * @return true if valid
 */
bool bbb_validate_network_data(const void* data, size_t length, const char* function_name);

/**
 * @brief Validate buffer bounds
 * @param buffer Buffer pointer
 * @param offset Offset into buffer
 * @param access_size Size of access
 * @param buffer_size Total buffer size
 * @param function_name Name of calling function
 * @return true if access is within bounds
 */
bool bbb_validate_buffer_access(const void* buffer, size_t offset, size_t access_size,
                                size_t buffer_size, const char* function_name);

//=============================================================================
// Threat Detection
//=============================================================================

/**
 * @brief Detect threat in data
 * @param data Data to scan
 * @param length Length of data
 * @return Threat type detected
 */
bbb_threat_type_t bbb_detect_threat(const void* data, size_t length);

/**
 * @brief Verify message integrity
 * @param message Message buffer
 * @param length Message length
 * @return true if message is valid
 */
bool bbb_verify_message_integrity(const void* message, size_t length);

//=============================================================================
// Privilege Checking
//=============================================================================

/**
 * @brief Validate privileged operation
 * @param operation_data Operation data/command
 * @param privilege Required privilege level
 * @return true if authorized
 */
bool bbb_validate_privileged_operation(const void* operation_data, bbb_privilege_t privilege);

//=============================================================================
// Audit Logging
//=============================================================================

/**
 * @brief Log security audit event
 * @param level Log level
 * @param module Module name
 * @param event Event name
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void bbb_audit_log(bbb_audit_level_t level, const char* module, const char* event,
                   const char* format, ...);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get count of threats detected
 * @return Number of threats detected
 */
uint64_t bbb_get_threats_detected(void);

/**
 * @brief Get count of validations performed
 * @return Number of validations
 */
uint64_t bbb_get_validations_performed(void);

/**
 * @brief Reset BBB statistics
 */
void bbb_reset_statistics(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BBB_HELPERS_H */
