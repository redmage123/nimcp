/**
 * @file nimcp_bbb_enhanced_detection.h
 * @brief BBB Enhanced Detection Integration Header
 *
 * WHAT: Integration layer for advanced pattern detection with BBB
 * WHY:  Extend BBB with path traversal and shell injection detection
 * HOW:  Provide BBB-compatible wrappers for enhanced detectors
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#ifndef NIMCP_BBB_ENHANCED_DETECTION_H
#define NIMCP_BBB_ENHANCED_DETECTION_H

#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_path_traversal.h"
#include "security/nimcp_shell_detector.h"

#ifdef __cplusplus
extern "C" {
#endif

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
bool bbb_enhanced_detection_init(void);

/**
 * @brief Cleanup enhanced detection modules
 *
 * WHAT: Destroy path and shell detectors
 * WHY:  Free resources when BBB shuts down
 * HOW:  Destroy detector instances
 */
void bbb_enhanced_detection_cleanup(void);

//=============================================================================
// Validation Functions
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
                            bbb_validation_result_t* result);

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
                          bbb_validation_result_t* result);

//=============================================================================
// Statistics
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
    nimcp_shell_detector_stats_t* shell_stats);

/**
 * @brief Reset statistics for enhanced detectors
 *
 * WHAT: Reset statistics counters for both detectors
 * WHY:  Enable fresh monitoring period
 * HOW:  Call reset on both detectors
 *
 * @return true on success
 */
bool bbb_enhanced_detection_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BBB_ENHANCED_DETECTION_H */
