/**
 * @file nimcp_medulla_bbb.h
 * @brief Blood-Brain Barrier Integration for Medulla Module
 *
 * WHAT: BBB integration for validating external inputs to medulla
 * WHY:  Protect medulla vital functions from malicious/corrupted inputs
 * HOW:  Validate arousal changes, health alerts, and neuromodulator levels
 *
 * BIOLOGICAL BASIS:
 * The medulla oblongata controls vital functions (arousal, protection, circadian).
 * The Blood-Brain Barrier protects these critical systems from harmful substances.
 * This integration validates all external inputs before they affect vital systems.
 *
 * PROTECTED INPUTS:
 * - Arousal changes (boost/reduce) - prevent rapid destabilization
 * - Health monitor alerts - validate health scores before protection response
 * - Neuromodulator levels - validate levels before arousal modulation
 *
 * @author NIMCP Development Team
 * @date 2025-01-10
 * @version 1.0.0
 */

#ifndef NIMCP_MEDULLA_BBB_H
#define NIMCP_MEDULLA_BBB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Medulla BBB configuration
 *
 * Configuration for medulla-specific BBB validation rules
 */
typedef struct {
    /** Maximum allowed arousal change per call (prevents rapid destabilization) */
    float max_arousal_delta;

    /** Minimum valid health score (protect against underflow) */
    float min_health_score;

    /** Maximum valid health score (protect against overflow) */
    float max_health_score;

    /** Maximum allowed neuromodulator level */
    float max_neuromodulator_level;

    /** Whether to block or just warn on validation failure */
    bool strict_mode;

    /** Enable logging of validation attempts */
    bool enable_logging;
} medulla_bbb_config_t;

/**
 * @brief Result of medulla BBB validation
 */
typedef struct {
    /** Whether the input passed validation */
    bool valid;

    /** Reason for rejection (if invalid) */
    char reason[128];

    /** Suggested safe value (if applicable) */
    float safe_value;

    /** Whether safe_value is available */
    bool has_safe_value;
} medulla_bbb_validation_result_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default medulla BBB configuration
 *
 * DEFAULTS:
 * - max_arousal_delta: 0.5 (prevent >50% arousal change per call)
 * - min_health_score: 0.0
 * - max_health_score: 100.0
 * - max_neuromodulator_level: 2.0 (allow 2x baseline)
 * - strict_mode: true
 * - enable_logging: true
 *
 * @return Default configuration
 */
medulla_bbb_config_t medulla_bbb_default_config(void);

//=============================================================================
// System Management
//=============================================================================

/**
 * @brief Set BBB system for medulla validation
 *
 * WHAT: Register BBB system for medulla input validation
 * WHY:  Enable security validation of all external inputs
 * HOW:  Store BBB reference, use in validation functions
 *
 * @param bbb BBB system handle (NULL to disable validation)
 *
 * NOTE: When bbb is NULL, all validation passes (permissive mode)
 */
void medulla_bbb_set_system(bbb_system_t bbb);

/**
 * @brief Configure medulla BBB validation rules
 *
 * WHAT: Set medulla-specific validation parameters
 * WHY:  Customize validation for medulla's requirements
 * HOW:  Store config for use in validation functions
 *
 * @param config Configuration (NULL for defaults)
 */
void medulla_bbb_set_config(const medulla_bbb_config_t* config);

//=============================================================================
// Validation Functions
//=============================================================================

/**
 * @brief Validate arousal input change
 *
 * WHAT: Validate arousal delta before applying
 * WHY:  Prevent destabilizing rapid arousal changes
 * HOW:  Check delta against configured maximum, validate sign consistency
 *
 * VALIDATION CHECKS:
 * - Delta magnitude within configured maximum
 * - BBB general input validation
 * - Consistency check (boost delta must be positive)
 *
 * @param delta Arousal change amount (positive for boost, negative for reduce)
 * @param is_boost True if this is a boost operation (delta should be positive)
 * @param result Output validation result (optional, can be NULL)
 * @return true if valid, false if rejected
 */
bool medulla_bbb_validate_arousal_input(float delta, bool is_boost,
                                         medulla_bbb_validation_result_t* result);

/**
 * @brief Validate health alert input
 *
 * WHAT: Validate health score before protection response
 * WHY:  Prevent false protection activations from corrupted health data
 * HOW:  Range check, BBB validation, sanity checks
 *
 * VALIDATION CHECKS:
 * - Health score within valid range [min, max]
 * - BBB general input validation
 * - NaN/Inf check
 *
 * @param health_score Health score from health monitor
 * @param result Output validation result (optional, can be NULL)
 * @return true if valid, false if rejected
 */
bool medulla_bbb_validate_health_alert(float health_score,
                                        medulla_bbb_validation_result_t* result);

/**
 * @brief Validate neuromodulator input
 *
 * WHAT: Validate neuromodulator level before arousal modulation
 * WHY:  Prevent extreme neuromodulator values from destabilizing arousal
 * HOW:  Range check, BBB validation
 *
 * VALIDATION CHECKS:
 * - Neuromodulator level within valid range [0, max]
 * - BBB general input validation
 * - NaN/Inf check
 *
 * @param level Neuromodulator level
 * @param neuromod_type Type of neuromodulator (for logging)
 * @param result Output validation result (optional, can be NULL)
 * @return true if valid, false if rejected
 */
bool medulla_bbb_validate_neuromod_input(float level, uint32_t neuromod_type,
                                          medulla_bbb_validation_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEDULLA_BBB_H */
