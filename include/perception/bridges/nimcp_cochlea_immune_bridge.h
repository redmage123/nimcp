/**
 * @file nimcp_cochlea_immune_bridge.h
 * @brief Cochlea-Immune system integration bridge
 *
 * WHAT: Connect cochlear health to brain immune system
 * WHY:  Model noise-induced damage, inflammation, and recovery
 * HOW:  Hair cell damage triggers immune response, affects processing
 *
 * BIOLOGICAL BASIS:
 * - Noise exposure causes hair cell damage and apoptosis
 * - Cochlear inflammation affects hearing sensitivity
 * - Immune cells (macrophages) clear debris
 * - Cytokines modulate auditory sensitivity
 * - Chronic inflammation leads to permanent damage
 *
 * DAMAGE CASCADE:
 * 1. Acoustic trauma → Hair cell stress
 * 2. Oxidative stress → Cell damage
 * 3. Inflammatory response → Cytokine release
 * 4. Immune cell recruitment → Debris clearance
 * 5. Recovery or permanent loss
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_IMMUNE_BRIDGE_H
#define NIMCP_COCHLEA_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_immune_system brain_immune_system_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cochlear damage type
 */
typedef enum {
    COCHLEA_DAMAGE_NONE,            /**< No damage */
    COCHLEA_DAMAGE_TEMPORARY,       /**< Temporary threshold shift */
    COCHLEA_DAMAGE_PERMANENT,       /**< Permanent threshold shift */
    COCHLEA_DAMAGE_ACOUSTIC_TRAUMA  /**< Acute acoustic trauma */
} cochlea_damage_type_t;

/**
 * @brief Inflammation state
 */
typedef enum {
    INFLAMMATION_NONE,              /**< No inflammation */
    INFLAMMATION_ACUTE,             /**< Recent, reversible */
    INFLAMMATION_CHRONIC,           /**< Long-term, damaging */
    INFLAMMATION_RESOLVING          /**< Healing phase */
} inflammation_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Cochlear health state
 */
typedef struct {
    float ohc_survival[64];         /**< OHC survival per channel [0-1] */
    float ihc_survival[64];         /**< IHC survival per channel [0-1] */
    uint32_t num_channels;

    float overall_health;           /**< Aggregate health [0-1] */
    cochlea_damage_type_t damage_type;
    inflammation_state_t inflammation;

    float oxidative_stress;         /**< Current oxidative stress [0-1] */
    float cytokine_level;           /**< Inflammatory cytokines [0-1] */
} cochlea_health_state_t;

/**
 * @brief Immune response to cochlear damage
 */
typedef struct {
    bool response_active;           /**< Immune response ongoing */
    float macrophage_activity;      /**< Debris clearance [0-1] */
    float neuroprotection;          /**< Protective factor release */
    float recovery_rate;            /**< Healing rate */

    uint64_t damage_timestamp;      /**< When damage occurred */
    uint64_t recovery_eta_ms;       /**< Estimated recovery time */
} immune_response_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Damage thresholds */
    float tts_threshold_db;         /**< Temporary threshold shift */
    float pts_threshold_db;         /**< Permanent threshold shift */
    float trauma_threshold_db;      /**< Acoustic trauma threshold */

    /* Exposure duration effects */
    float safe_exposure_time_min;   /**< Safe exposure at 85 dB */
    float time_intensity_tradeoff;  /**< 3 dB per halving of time */

    /* Recovery parameters */
    float recovery_rate_per_hour;   /**< TTS recovery rate */
    bool enable_permanent_damage;   /**< Allow PTS modeling */

    /* Immune coupling */
    bool enable_immune_response;    /**< Trigger immune on damage */
    float inflammation_threshold;   /**< Damage level for inflammation */
} cochlea_immune_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_immune_bridge cochlea_immune_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_immune_config_t cochlea_immune_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_immune_bridge_t* cochlea_immune_bridge_create(
    cochlea_t* cochlea,
    brain_immune_system_t* immune,
    const cochlea_immune_config_t* config
);

void cochlea_immune_bridge_destroy(cochlea_immune_bridge_t* bridge);

nimcp_error_t cochlea_immune_bridge_update(
    cochlea_immune_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_immune_bridge_reset(cochlea_immune_bridge_t* bridge);

//=============================================================================
// Damage and Health
//=============================================================================

nimcp_error_t cochlea_immune_get_health(
    const cochlea_immune_bridge_t* bridge,
    cochlea_health_state_t* health
);

nimcp_error_t cochlea_immune_apply_exposure(
    cochlea_immune_bridge_t* bridge,
    float level_db,
    float duration_min
);

nimcp_error_t cochlea_immune_apply_trauma(
    cochlea_immune_bridge_t* bridge,
    float peak_level_db,
    uint32_t affected_channel
);

//=============================================================================
// Immune Response
//=============================================================================

nimcp_error_t cochlea_immune_get_response(
    const cochlea_immune_bridge_t* bridge,
    immune_response_t* response
);

nimcp_error_t cochlea_immune_trigger_protection(
    cochlea_immune_bridge_t* bridge
);

//=============================================================================
// Recovery
//=============================================================================

nimcp_error_t cochlea_immune_simulate_recovery(
    cochlea_immune_bridge_t* bridge,
    float hours
);

float cochlea_immune_get_recovery_progress(
    const cochlea_immune_bridge_t* bridge
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_immune_verify_bidirectional(const cochlea_immune_bridge_t* bridge);
uint64_t cochlea_immune_get_last_outbound(const cochlea_immune_bridge_t* bridge);
uint64_t cochlea_immune_get_last_inbound(const cochlea_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_IMMUNE_BRIDGE_H */
