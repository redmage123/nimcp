/**
 * @file nimcp_bbb_helpers.c
 * @brief Simplified BBB Helper Functions Implementation
 *
 * WHAT: Implementation of simplified BBB wrapper functions
 * WHY:  Provide easy-to-use security validation API
 * HOW:  Manage global BBB instance and delegate to full BBB API
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include "security/nimcp_bbb_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#define LOG_MODULE "bbb_helpers"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bbb_helpers, MESH_ADAPTER_CATEGORY_SECURITY)


//=============================================================================
// Global State (standalone, no dependency on full BBB system)
//=============================================================================

static nimcp_mutex_t g_bbb_mutex;
static nimcp_platform_once_t g_bbb_init_once = NIMCP_PLATFORM_ONCE_INIT;
static nimcp_atomic_bool_t g_bbb_initialized = {0};
static uint64_t g_validations = 0;
static uint64_t g_threats_detected = 0;

//=============================================================================
// Initialization
//=============================================================================

/**
 * @brief Internal initialization function called via nimcp_platform_once
 */
static void bbb_helpers_init_internal(void)
{
    // Initialize mutex
    if (nimcp_mutex_init(&g_bbb_mutex, NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize BBB helpers mutex");
        return;
    }

    nimcp_atomic_store_bool(&g_bbb_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    LOG_INFO("BBB helpers initialized (standalone mode)");
}

bool bbb_helpers_init(void)
{
    // Thread-safe one-time initialization
    nimcp_platform_once(&g_bbb_init_once, bbb_helpers_init_internal);
    return nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE);
}

void bbb_helpers_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    nimcp_mutex_lock(&g_bbb_mutex);
    nimcp_atomic_store_bool(&g_bbb_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);
    nimcp_mutex_unlock(&g_bbb_mutex);
    nimcp_mutex_destroy(&g_bbb_mutex);

    // Reset once flag to allow reinitialization after shutdown
    g_bbb_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;

    LOG_INFO("BBB helpers shutdown");
}

bool bbb_helpers_is_initialized(void)
{
    return nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE);
}

//=============================================================================
// Module Registration
//=============================================================================

bool bbb_register_module(const char* module_name, bbb_module_type_t type)
{
    if (!module_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_register_module: module_name is NULL");
        return false;
    }

    // Auto-initialize if not already done
    if (!nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        if (!bbb_helpers_init()) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "bbb_register_module: bbb_helpers_init is NULL");
            return false;
        }
    }

    LOG_DEBUG("Registered BBB module: %s (type=%d)", module_name, type);
    return true;
}

bool bbb_unregister_module(const char* module_name)
{
    if (!module_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_unregister_module: module_name is NULL");
        return false;
    }

    LOG_DEBUG("Unregistered BBB module: %s", module_name);
    return true;
}

//=============================================================================
// Validation Functions
//=============================================================================

bool bbb_check_pointer(const void* ptr, const char* function_name)
{
    if (!nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        bbb_helpers_init();
    }

    __sync_fetch_and_add(&g_validations, 1);

    if (!ptr) {
        LOG_ERROR("[%s] NULL pointer validation failed",
                       function_name ? function_name : "unknown");
        __sync_fetch_and_add(&g_threats_detected, 1);
        return false;
    }

    return true;
}

bool bbb_check_string(const char* str, size_t max_len, const char* function_name)
{
    if (!nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        bbb_helpers_init();
    }

    __sync_fetch_and_add(&g_validations, 1);

    if (!str) {
        LOG_ERROR("[%s] NULL string validation failed",
                       function_name ? function_name : "unknown");
        __sync_fetch_and_add(&g_threats_detected, 1);
        return false;
    }

    size_t len = strnlen(str, max_len + 1);
    if (len > max_len) {
        LOG_ERROR("[%s] String length %zu exceeds max %zu",
                       function_name ? function_name : "unknown", len, max_len);
        __sync_fetch_and_add(&g_threats_detected, 1);
        return false;
    }

    // Check for null termination
    if (len == max_len + 1) {
        LOG_ERROR("[%s] String not null-terminated",
                       function_name ? function_name : "unknown");
        __sync_fetch_and_add(&g_threats_detected, 1);
        return false;
    }

    return true;
}

bool bbb_validate_range(int64_t value, int64_t min, int64_t max, const char* function_name)
{
    if (!nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        bbb_helpers_init();
    }

    __sync_fetch_and_add(&g_validations, 1);

    if (value < min || value > max) {
        LOG_ERROR("[%s] Value %ld out of range [%ld, %ld]",
                       function_name ? function_name : "unknown", value, min, max);
        __sync_fetch_and_add(&g_threats_detected, 1);
        return false;
    }

    return true;
}

bool bbb_validate_range_u(uint64_t value, uint64_t min, uint64_t max, const char* function_name)
{
    if (!nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        bbb_helpers_init();
    }

    __sync_fetch_and_add(&g_validations, 1);

    if (value < min || value > max) {
        LOG_ERROR("[%s] Value %lu out of range [%lu, %lu]",
                       function_name ? function_name : "unknown", value, min, max);
        __sync_fetch_and_add(&g_threats_detected, 1);
        return false;
    }

    return true;
}

bool bbb_validate_network_data(const void* data, size_t length, const char* function_name)
{
    if (!nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        bbb_helpers_init();
    }

    __sync_fetch_and_add(&g_validations, 1);

    if (!data) {
        LOG_ERROR("[%s] NULL network data",
                       function_name ? function_name : "unknown");
        __sync_fetch_and_add(&g_threats_detected, 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_validate_network_data: data is NULL");
        return false;
    }

    if (length == 0 || length > 65536) {  // Reasonable network packet size
        LOG_ERROR("[%s] Invalid network data length: %zu",
                       function_name ? function_name : "unknown", length);
        __sync_fetch_and_add(&g_threats_detected, 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_validate_network_data: length is zero");
        return false;
    }

    return true;
}

bool bbb_validate_buffer_access(const void* buffer, size_t offset, size_t access_size,
                                size_t buffer_size, const char* function_name)
{
    if (!nimcp_atomic_load_bool(&g_bbb_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        bbb_helpers_init();
    }

    __sync_fetch_and_add(&g_validations, 1);

    if (!buffer) {
        LOG_ERROR("[%s] NULL buffer",
                       function_name ? function_name : "unknown");
        __sync_fetch_and_add(&g_threats_detected, 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_validate_buffer_access: buffer is NULL");
        return false;
    }

    // Check for overflow
    if (offset > buffer_size || access_size > buffer_size) {
        LOG_ERROR("[%s] Buffer access overflow: offset=%zu size=%zu buffer_size=%zu",
                       function_name ? function_name : "unknown", offset, access_size, buffer_size);
        __sync_fetch_and_add(&g_threats_detected, 1);
        return false;
    }

    if (offset + access_size > buffer_size) {
        LOG_ERROR("[%s] Buffer access overflow: offset=%zu+size=%zu > buffer_size=%zu",
                       function_name ? function_name : "unknown", offset, access_size, buffer_size);
        __sync_fetch_and_add(&g_threats_detected, 1);
        return false;
    }

    return true;
}

//=============================================================================
// Threat Detection
//=============================================================================

bbb_threat_type_t bbb_detect_threat(const void* data, size_t length)
{
    if (!data || length == 0) {
        return BBB_THREAT_NONE;
    }

    const uint8_t* bytes = (const uint8_t*)data;

    // Check for NOP sled (shellcode indicator)
    int nop_count = 0;
    for (size_t i = 0; i < length && i < 256; i++) {
        if (bytes[i] == 0x90) {  // x86 NOP instruction
            nop_count++;
            if (nop_count >= 4) {
                return BBB_THREAT_SHELLCODE;
            }
        } else {
            nop_count = 0;
        }
    }

    // Check for SQL injection patterns
    if (length > 4) {
        char* str_data = (char*)data;
        if (strstr(str_data, "' OR ") || strstr(str_data, "'; DROP") ||
            strstr(str_data, "UNION SELECT")) {
            return BBB_THREAT_SQL_INJECTION;
        }
    }

    // Check for format string attacks
    int format_count = 0;
    for (size_t i = 0; i < length - 1; i++) {
        if (bytes[i] == '%' && (bytes[i+1] == 's' || bytes[i+1] == 'n' ||
                                bytes[i+1] == 'x' || bytes[i+1] == 'p')) {
            format_count++;
            if (format_count > 3) {
                return BBB_THREAT_FORMAT_STRING;
            }
        }
    }

    return BBB_THREAT_NONE;
}

bool bbb_verify_message_integrity(const void* message, size_t length)
{
    if (!bbb_check_pointer(message, "bbb_verify_message_integrity")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_verify_message_integrity: bbb_check_pointer is NULL");
        return false;
    }

    if (length == 0 || length > 1048576) {  // 1MB max
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_verify_message_integrity: length is zero");
        return false;
    }

    bbb_threat_type_t threat = bbb_detect_threat(message, length);
    return (threat == BBB_THREAT_NONE);
}

//=============================================================================
// Privilege Checking
//=============================================================================

bool bbb_validate_privileged_operation(const void* operation_data, bbb_privilege_t privilege)
{
    if (!bbb_check_pointer(operation_data, "bbb_validate_privileged_operation")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_validate_privileged_operation: bbb_check_pointer is NULL");
        return false;
    }

    // For now, allow all privileged operations
    // In production, this would check against ACLs
    return true;
}

//=============================================================================
// Audit Logging
//=============================================================================

void bbb_audit_log(bbb_audit_level_t level, const char* module, const char* event,
                   const char* format, ...)
{
    if (!module || !event) {
        return;
    }

    char message[NIMCP_ERROR_BUFFER_LARGE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    const char* level_str;
    switch (level) {
        case BBB_AUDIT_DEBUG:    level_str = "DEBUG"; break;
        case BBB_AUDIT_INFO:     level_str = "INFO"; break;
        case BBB_AUDIT_WARNING:  level_str = "WARNING"; break;
        case BBB_AUDIT_ERROR:    level_str = "ERROR"; break;
        case BBB_AUDIT_CRITICAL: level_str = "CRITICAL"; break;
        default:                 level_str = "UNKNOWN"; break;
    }

    // Log using NIMCP logging system
    switch (level) {
        case BBB_AUDIT_DEBUG:
            LOG_DEBUG("[BBB_AUDIT] %s.%s: %s", module, event, message);
            break;
        case BBB_AUDIT_INFO:
            LOG_INFO("[BBB_AUDIT] %s.%s: %s", module, event, message);
            break;
        case BBB_AUDIT_WARNING:
            LOG_WARNING("[BBB_AUDIT] %s.%s: %s", module, event, message);
            break;
        case BBB_AUDIT_ERROR:
        case BBB_AUDIT_CRITICAL:
            LOG_ERROR("[BBB_AUDIT] %s.%s: %s", module, event, message);
            break;
    }
}

//=============================================================================
// Statistics
//=============================================================================

uint64_t bbb_get_threats_detected(void)
{
    return __sync_fetch_and_add(&g_threats_detected, 0);
}

uint64_t bbb_get_validations_performed(void)
{
    return __sync_fetch_and_add(&g_validations, 0);
}

void bbb_reset_statistics(void)
{
    __sync_lock_test_and_set(&g_validations, 0);
    __sync_lock_test_and_set(&g_threats_detected, 0);
}
