/**
 * @file nimcp_bbb_enhanced_detection.c
 * @brief BBB Enhanced Detection Integration
 *
 * WHAT: Integration of path traversal and shell injection detectors with BBB
 * WHY:  Extend BBB input gate with advanced pattern detection capabilities
 * HOW:  Create wrapper functions that integrate detectors into BBB validation pipeline
 *
 * INTEGRATION POINTS:
 * - Path traversal detection for file path inputs
 * - Shell injection detection for command/parameter inputs
 * - Unified threat reporting through BBB
 * - Statistics aggregation
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "security/nimcp_path_traversal.h"
#include "security/nimcp_shell_detector.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_bbb_enhanced"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bbb_enhanced_detection, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdio.h>
#include <string.h>
#include <stdbool.h>

//=============================================================================
// Global Detector Instances
//=============================================================================

static nimcp_path_validator_t g_path_validator = NULL;
static nimcp_shell_detector_t g_shell_detector = NULL;
static bool g_detectors_initialized = false;

//=============================================================================
// Initialization and Cleanup
//=============================================================================

/**
 * @brief Initialize enhanced detection modules
 *
 * WHAT: Create and initialize path and shell detectors
 * WHY:  Enable enhanced detection capabilities for BBB
 * HOW:  Create detector instances with default configurations
 *
 * @return true on success
 */
bool bbb_enhanced_detection_init(void)
{
    /* Guard: Already initialized */
    if (g_detectors_initialized) {
        LOG_WARN(LOG_MODULE, "Enhanced detection already initialized");
        return true;
    }

    /* Create path validator */
    g_path_validator = nimcp_path_validator_create(NULL);
    if (!g_path_validator) {
        LOG_ERROR(LOG_MODULE, "Failed to create path validator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_enhanced_detection_init: g_path_validator is NULL");
        return false;
    }

    /* Create shell detector */
    g_shell_detector = nimcp_shell_detector_create(NULL);
    if (!g_shell_detector) {
        LOG_ERROR(LOG_MODULE, "Failed to create shell detector");
        nimcp_path_validator_destroy(g_path_validator);
        g_path_validator = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_enhanced_detection_init: g_shell_detector is NULL");
        return false;
    }

    g_detectors_initialized = true;
    LOG_INFO(LOG_MODULE, "Enhanced detection initialized successfully");
    return true;
}

/**
 * @brief Cleanup enhanced detection modules
 *
 * WHAT: Destroy path and shell detectors
 * WHY:  Free resources when BBB shuts down
 * HOW:  Destroy detector instances
 */
void bbb_enhanced_detection_cleanup(void)
{
    if (!g_detectors_initialized) {
        return;
    }

    if (g_path_validator) {
        nimcp_path_validator_destroy(g_path_validator);
        g_path_validator = NULL;
    }

    if (g_shell_detector) {
        nimcp_shell_detector_destroy(g_shell_detector);
        g_shell_detector = NULL;
    }

    g_detectors_initialized = false;
    LOG_INFO(LOG_MODULE, "Enhanced detection cleanup complete");
}

//=============================================================================
// Path Traversal Integration
//=============================================================================

/**
 * @brief Validate file path for traversal attacks
 *
 * WHAT: Wrapper for path traversal detection integrated with BBB
 * WHY:  Provide BBB-compatible interface for path validation
 * HOW:  Call path validator and convert result to BBB format
 *
 * @param system BBB system handle
 * @param path File path to validate
 * @param result Output validation result
 * @return true if valid
 */
bool bbb_validate_file_path(bbb_system_t system, const char* path,
                            bbb_validation_result_t* result)
{
    /* Guard: NULL result */
    if (!result) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_validate_file_path: result is NULL");

            return false;

        }

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->valid = true;

    /* Guard: Detectors not initialized */
    if (!g_detectors_initialized) {
        /* Try to initialize */
        if (!bbb_enhanced_detection_init()) {
            result->valid = false;
            result->threat = BBB_THREAT_UNKNOWN;
            result->severity = BBB_SEVERITY_HIGH;
            snprintf(result->reason, sizeof(result->reason),
                     "Enhanced detection not initialized");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "bbb_validate_file_path: bbb_enhanced_detection_init is NULL");
            return false;
        }
    }

    /* Guard: NULL path */
    if (!path) {
        result->valid = false;
        result->threat = BBB_THREAT_PATH_TRAVERSAL;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason), "NULL path");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_validate_file_path: path is NULL");
        return false;
    }

    /* Validate path */
    nimcp_path_validation_result_t path_result;
    nimcp_path_error_t err = nimcp_path_validate(
        g_path_validator, path, NIMCP_PATH_CONTEXT_FILE, &path_result);

    /* Convert to BBB result */
    if (err != NIMCP_PATH_SUCCESS || !path_result.valid) {
        result->valid = false;
        result->threat = BBB_THREAT_PATH_TRAVERSAL;

        /* Map severity */
        switch (path_result.severity) {
            case NIMCP_PATH_SEVERITY_LOW:
                result->severity = BBB_SEVERITY_LOW;
                break;
            case NIMCP_PATH_SEVERITY_MEDIUM:
                result->severity = BBB_SEVERITY_MEDIUM;
                break;
            case NIMCP_PATH_SEVERITY_HIGH:
                result->severity = BBB_SEVERITY_HIGH;
                break;
            case NIMCP_PATH_SEVERITY_CRITICAL:
                result->severity = BBB_SEVERITY_CRITICAL;
                break;
            default:
                result->severity = BBB_SEVERITY_MEDIUM;
                break;
        }

        /* Copy reason */
        strncpy(result->reason, path_result.reason, sizeof(result->reason) - 1);
        result->reason[sizeof(result->reason) - 1] = '\0';

        /* Report threat to BBB */
        if (system) {
            bbb_report_threat(system, BBB_THREAT_PATH_TRAVERSAL,
                            result->severity, result->reason,
                            path, path, strlen(path));
        }

        LOG_WARN(LOG_MODULE, "Path traversal detected: %s (pattern=%s)",
                 path, nimcp_path_pattern_name(path_result.pattern));

        return false;
    }

    /* Path is valid */
    LOG_DEBUG(LOG_MODULE, "Path validated successfully: %s", path);
    return true;
}

//=============================================================================
// Shell Injection Integration
//=============================================================================

/**
 * @brief Validate command/parameter for shell injection
 *
 * WHAT: Wrapper for shell injection detection integrated with BBB
 * WHY:  Provide BBB-compatible interface for command validation
 * HOW:  Call shell detector and convert result to BBB format
 *
 * @param system BBB system handle
 * @param input Command or parameter to validate
 * @param result Output validation result
 * @return true if valid
 */
bool bbb_validate_command(bbb_system_t system, const char* input,
                          bbb_validation_result_t* result)
{
    /* Guard: NULL result */
    if (!result) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_validate_command: result is NULL");

            return false;

        }

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->valid = true;

    /* Guard: Detectors not initialized */
    if (!g_detectors_initialized) {
        if (!bbb_enhanced_detection_init()) {
            result->valid = false;
            result->threat = BBB_THREAT_UNKNOWN;
            result->severity = BBB_SEVERITY_HIGH;
            snprintf(result->reason, sizeof(result->reason),
                     "Enhanced detection not initialized");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "bbb_validate_command: bbb_enhanced_detection_init is NULL");
            return false;
        }
    }

    /* Guard: NULL input */
    if (!input) {
        result->valid = false;
        result->threat = BBB_THREAT_SHELL_INJECTION;
        result->severity = BBB_SEVERITY_HIGH;
        snprintf(result->reason, sizeof(result->reason), "NULL input");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_validate_command: input is NULL");
        return false;
    }

    /* Detect shell injection */
    nimcp_shell_detection_result_t shell_result;
    nimcp_shell_error_t err = nimcp_shell_detect(
        g_shell_detector, input, NIMCP_SHELL_CONTEXT_AUTO, &shell_result);

    /* Convert to BBB result */
    if (err != NIMCP_SHELL_SUCCESS || !shell_result.valid) {
        result->valid = false;
        result->threat = BBB_THREAT_SHELL_INJECTION;

        /* Map severity */
        switch (shell_result.severity) {
            case NIMCP_SHELL_SEVERITY_LOW:
                result->severity = BBB_SEVERITY_LOW;
                break;
            case NIMCP_SHELL_SEVERITY_MEDIUM:
                result->severity = BBB_SEVERITY_MEDIUM;
                break;
            case NIMCP_SHELL_SEVERITY_HIGH:
                result->severity = BBB_SEVERITY_HIGH;
                break;
            case NIMCP_SHELL_SEVERITY_CRITICAL:
                result->severity = BBB_SEVERITY_CRITICAL;
                break;
            default:
                result->severity = BBB_SEVERITY_MEDIUM;
                break;
        }

        /* Copy reason */
        strncpy(result->reason, shell_result.reason, sizeof(result->reason) - 1);
        result->reason[sizeof(result->reason) - 1] = '\0';

        /* Report threat to BBB */
        if (system) {
            bbb_report_threat(system, BBB_THREAT_SHELL_INJECTION,
                            result->severity, result->reason,
                            input, input, strlen(input));
        }

        LOG_WARN(LOG_MODULE, "Shell injection detected: %s (pattern=%s)",
                 input, nimcp_shell_pattern_name(shell_result.pattern));

        return false;
    }

    /* Input is valid */
    LOG_DEBUG(LOG_MODULE, "Command validated successfully: %s", input);
    return true;
}

//=============================================================================
// Statistics Aggregation
//=============================================================================

/**
 * @brief Get combined statistics from enhanced detectors
 *
 * WHAT: Aggregate statistics from path and shell detectors
 * WHY:  Provide unified view of enhanced detection activity
 * HOW:  Combine statistics from both detectors
 *
 * @param path_stats Output path validator statistics (can be NULL)
 * @param shell_stats Output shell detector statistics (can be NULL)
 * @return true on success
 */
bool bbb_enhanced_detection_get_stats(
    nimcp_path_validator_stats_t* path_stats,
    nimcp_shell_detector_stats_t* shell_stats)
{
    if (!g_detectors_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_enhanced_detection_get_stats: g_detectors_initialized is NULL");

            return false;
    }

    if (path_stats && g_path_validator) {
        nimcp_path_validator_get_stats(g_path_validator, path_stats);
    }

    if (shell_stats && g_shell_detector) {
        nimcp_shell_detector_get_stats(g_shell_detector, shell_stats);
    }

    return true;
}

/**
 * @brief Reset statistics for enhanced detectors
 *
 * WHAT: Reset statistics counters for both detectors
 * WHY:  Enable fresh monitoring period
 * HOW:  Call reset on both detectors
 *
 * @return true on success
 */
bool bbb_enhanced_detection_reset_stats(void)
{
    if (!g_detectors_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "bbb_enhanced_detection_reset_stats: g_detectors_initialized is NULL");

            return false;
    }

    if (g_path_validator) {
        nimcp_path_validator_reset_stats(g_path_validator);
    }

    if (g_shell_detector) {
        nimcp_shell_detector_reset_stats(g_shell_detector);
    }

    LOG_INFO(LOG_MODULE, "Enhanced detection statistics reset");
    return true;
}
