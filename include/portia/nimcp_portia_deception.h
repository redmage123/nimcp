/**
 * @file nimcp_portia_deception.h
 * @brief Stealth and Deception System for Portia Spider
 *
 * WHAT: Lightweight stealth, mimicry, and countermeasure system
 * WHY:  Portia spiders use deception signals to lure prey
 * HOW:  Emission control, mimicry profiles, and jamming
 *
 * BIOLOGICAL MODEL:
 * ```
 * BIOLOGICAL                          NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Signal mimicry                     → Mimicry profiles
 * Vibration patterns                 → Emission control
 * Deceptive courtship signals        → Active countermeasures
 * Predator avoidance                 → Stealth modes
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║              STEALTH & DECEPTION SYSTEM                       ║
 * ║  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐            ║
 * ║  │   Stealth   │ │   Mimicry   │ │   Jamming   │            ║
 * ║  │   Control   │ │   Profiles  │ │  Generator  │            ║
 * ║  └─────────────┘ └─────────────┘ └─────────────┘            ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 * - BBB security validation
 *
 * @author NIMCP Portia Team
 * @date 2025-12-08
 */

#ifndef NIMCP_PORTIA_DECEPTION_H
#define NIMCP_PORTIA_DECEPTION_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Stealth and Deception Types
//=============================================================================

/**
 * @brief Stealth operation modes
 */
typedef enum {
    STEALTH_MODE_NONE = 0,       /**< Normal operation */
    STEALTH_MODE_PASSIVE,        /**< Minimize emissions */
    STEALTH_MODE_ACTIVE,         /**< Active countermeasures */
    STEALTH_MODE_MIMICRY         /**< Imitate another entity */
} stealth_mode_t;

/**
 * @brief Current stealth state
 */
typedef struct {
    stealth_mode_t mode;         /**< Current stealth mode */
    float emission_level;        /**< Emission level (0.0 = silent, 1.0 = normal) */
    uint32_t mimicry_profile;    /**< Active mimicry profile ID */
    bool jamming_active;         /**< Jamming enabled */
    float effectiveness;         /**< Current stealth effectiveness (0-1) */
    uint64_t mode_started_ms;    /**< When current mode started */
} stealth_state_t;

/**
 * @brief Mimicry profile definition
 */
typedef struct {
    uint32_t profile_id;         /**< Profile identifier */
    char name[64];               /**< Profile name (e.g., "prey_spider") */
    float signal_pattern[16];    /**< Signal characteristics */
    uint32_t pattern_length;     /**< Number of pattern elements */
    float effectiveness;         /**< Profile effectiveness (0-1) */
    bool active;                 /**< Profile available */
} mimicry_profile_t;

/**
 * @brief Configuration for deception system
 */
typedef struct {
    bool enable_stealth;         /**< Enable stealth capabilities */
    bool enable_mimicry;         /**< Enable mimicry system */
    bool enable_jamming;         /**< Enable jamming */
    float default_emission_level; /**< Default emission level */
    uint32_t profile_count;      /**< Number of mimicry profiles */
    bool enable_bio_async;       /**< Enable bio-async messaging */
} portia_deception_config_t;

/**
 * @brief Opaque deception handle
 */
typedef struct portia_deception_struct* portia_deception_t;

//=============================================================================
// Deception Lifecycle
//=============================================================================

/**
 * @brief Create deception system
 *
 * WHAT: Initialize stealth and deception capabilities
 * WHY:  Enable covert operations
 * HOW:  Allocate state, configure modes
 *
 * @param config Configuration parameters
 * @return Deception handle or NULL on error
 */
NIMCP_EXPORT portia_deception_t portia_deception_init(
    const portia_deception_config_t* config);

/**
 * @brief Destroy deception system
 *
 * WHAT: Free all resources
 * WHY:  Clean shutdown
 * HOW:  Free profiles and internal structures
 *
 * @param deception Deception system to destroy
 */
NIMCP_EXPORT void portia_deception_destroy(portia_deception_t deception);

//=============================================================================
// Stealth Operations
//=============================================================================

/**
 * @brief Set stealth mode
 *
 * WHAT: Change current stealth mode
 * WHY:  Adapt to operational requirements
 * HOW:  Update mode, adjust emission levels
 *
 * @param deception Deception handle
 * @param mode Target stealth mode
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_deception_set_mode(
    portia_deception_t deception,
    stealth_mode_t mode);

/**
 * @brief Control emission level
 *
 * WHAT: Set signal emission level
 * WHY:  Fine-grained stealth control
 * HOW:  Update emission level (0.0-1.0)
 *
 * @param deception Deception handle
 * @param level Emission level (0.0 = silent, 1.0 = normal)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_deception_emit(
    portia_deception_t deception,
    float level);

/**
 * @brief Get current stealth effectiveness
 *
 * WHAT: Query stealth performance
 * WHY:  Monitor detection risk
 * HOW:  Calculate effectiveness from mode and emissions
 *
 * @param deception Deception handle
 * @return Effectiveness value (0-1) or negative on error
 */
NIMCP_EXPORT float portia_deception_get_effectiveness(
    portia_deception_t deception);

//=============================================================================
// Mimicry Operations
//=============================================================================

/**
 * @brief Activate mimicry profile
 *
 * WHAT: Begin mimicking another entity
 * WHY:  Deceive observers
 * HOW:  Load profile, adjust emissions
 *
 * @param deception Deception handle
 * @param profile_id Profile to activate
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_deception_mimic(
    portia_deception_t deception,
    uint32_t profile_id);

/**
 * @brief Register custom mimicry profile
 *
 * WHAT: Add new mimicry profile
 * WHY:  Expand deception capabilities
 * HOW:  Store profile in registry
 *
 * @param deception Deception handle
 * @param profile Profile definition
 * @return Profile ID or 0 on error
 */
NIMCP_EXPORT uint32_t portia_deception_register_profile(
    portia_deception_t deception,
    const mimicry_profile_t* profile);

/**
 * @brief Get available mimicry profiles
 *
 * WHAT: List registered profiles
 * WHY:  Show available deception options
 * HOW:  Copy profiles to output array
 *
 * @param deception Deception handle
 * @param profiles Output array for profiles
 * @param max_profiles Size of output array
 * @return Number of profiles returned
 */
NIMCP_EXPORT uint32_t portia_deception_get_profiles(
    portia_deception_t deception,
    mimicry_profile_t* profiles,
    uint32_t max_profiles);

//=============================================================================
// Countermeasure Operations
//=============================================================================

/**
 * @brief Activate jamming
 *
 * WHAT: Enable active countermeasures
 * WHY:  Disrupt enemy detection
 * HOW:  Generate interference signals
 *
 * @param deception Deception handle
 * @param enable Enable or disable jamming
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_deception_jam(
    portia_deception_t deception,
    bool enable);

/**
 * @brief Get current state
 *
 * WHAT: Retrieve full deception state
 * WHY:  Monitor system status
 * HOW:  Copy state to output structure
 *
 * @param deception Deception handle
 * @param state Output structure for state
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_deception_get_state(
    portia_deception_t deception,
    stealth_state_t* state);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL
 */
NIMCP_EXPORT const char* portia_deception_get_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_DECEPTION_H
