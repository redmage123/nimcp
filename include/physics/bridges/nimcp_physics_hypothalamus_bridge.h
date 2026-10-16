//=============================================================================
// nimcp_physics_hypothalamus_bridge.h - Physics Layer to Hypothalamus Bridge
//=============================================================================
/**
 * @file nimcp_physics_hypothalamus_bridge.h
 * @brief Bridge connecting Phase 1 Physics modules with Hypothalamus Homeostasis
 *
 * WHAT: Provides bidirectional integration between physics layer modules
 *       (Thermodynamics, HH) and the hypothalamus homeostatic control system.
 *
 * WHY:  Physics-hypothalamus coupling is essential for biologically realistic
 *       simulation:
 *       - Temperature regulation (physics temp ↔ hypothalamic thermoregulation)
 *       - Energy homeostasis (ATP pools ↔ metabolic drives)
 *       - Circadian rhythm effects on neural dynamics
 *
 * HOW:  - Reports physics state to hypothalamus homeostatic controllers
 *       - Receives homeostatic control signals and modulates physics parameters
 *       - Implements PID-based control loop integration
 *
 * BIOLOGICAL BASIS:
 * ```
 * PHYSICS MEASUREMENT                 HYPOTHALAMUS RESPONSE
 * ─────────────────────────────────────────────────────────────────────────
 * Thermodynamics temp too high   →   Anterior hypothalamus activates cooling
 * Thermodynamics temp too low    →   Posterior hypothalamus activates heating
 * ATP depletion                  →   Arcuate nucleus hunger signals
 * High metabolic rate            →   Energy conservation behaviors
 * Circadian phase                →   SCN clock synchronization
 *
 * HYPOTHALAMUS OUTPUT                 PHYSICS MODULATION
 * ─────────────────────────────────────────────────────────────────────────
 * Thermoregulation signal        →   Q10 scaling, metabolic rate adjustment
 * Energy conservation signal     →   Reduced conductance, slower dynamics
 * Arousal signal                 →   Enhanced excitability
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_HYPOTHALAMUS_BRIDGE_H
#define NIMCP_PHYSICS_HYPOTHALAMUS_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_HYPO_MODULE_NAME    "physics_hypo_bridge"

/** Default temperature setpoint (Celsius) */
#define PHYSICS_HYPO_TEMP_SETPOINT  37.0f

/** Default temperature range (Celsius) */
#define PHYSICS_HYPO_TEMP_MIN       35.0f
#define PHYSICS_HYPO_TEMP_MAX       42.0f

/** Default ATP setpoint (fraction) */
#define PHYSICS_HYPO_ATP_SETPOINT   0.8f

/** Default ATP critical level (fraction) */
#define PHYSICS_HYPO_ATP_CRITICAL   0.2f

/** Default circadian period (hours) */
#define PHYSICS_HYPO_CIRCADIAN_PERIOD  24.0f

/** Maximum modulation factor for physics parameters */
#define PHYSICS_HYPO_MAX_MOD        1.5f

/** Minimum modulation factor for physics parameters */
#define PHYSICS_HYPO_MIN_MOD        0.5f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Physics-hypothalamus coupling types
 */
typedef enum {
    /** Temperature coupling */
    PHYSICS_HYPO_COUPLING_TEMPERATURE = 0,

    /** Energy/ATP coupling */
    PHYSICS_HYPO_COUPLING_ENERGY,

    /** Circadian rhythm coupling */
    PHYSICS_HYPO_COUPLING_CIRCADIAN,

    /** Arousal/excitability coupling */
    PHYSICS_HYPO_COUPLING_AROUSAL,

    PHYSICS_HYPO_COUPLING_COUNT
} physics_hypo_coupling_t;

/**
 * @brief Thermoregulation direction
 */
typedef enum {
    PHYSICS_HYPO_THERMO_NEUTRAL = 0,    /**< No action needed */
    PHYSICS_HYPO_THERMO_HEATING,         /**< Increase temperature */
    PHYSICS_HYPO_THERMO_COOLING          /**< Decrease temperature */
} physics_hypo_thermo_dir_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Enable temperature coupling */
    bool enable_temperature;

    /** Enable energy coupling */
    bool enable_energy;

    /** Enable circadian coupling */
    bool enable_circadian;

    /** Enable arousal coupling */
    bool enable_arousal;

    /** Temperature update interval (ms) */
    float temp_update_interval_ms;

    /** Energy update interval (ms) */
    float energy_update_interval_ms;

    /** Circadian phase update interval (ms) */
    float circadian_update_interval_ms;

    /** Temperature setpoint (Celsius) */
    float temp_setpoint;

    /** ATP setpoint (0.0-1.0) */
    float atp_setpoint;

    /** Circadian period (hours) */
    float circadian_period_hours;

    /** Modulation strength (0.0-1.0) */
    float modulation_strength;

    /** Enable PID control integration */
    bool use_pid_control;
} physics_hypo_config_t;

/**
 * @brief Physics state reported to hypothalamus
 */
typedef struct {
    /** Current temperature (Celsius) */
    float temperature;

    /** Temperature trend (degrees/second) */
    float temp_trend;

    /** ATP level (0.0-1.0) */
    float atp_level;

    /** ATP consumption rate (fraction/second) */
    float atp_consumption_rate;

    /** Metabolic rate (arbitrary units) */
    float metabolic_rate;

    /** Entropy production rate */
    float entropy_rate;

    /** Average neural firing rate (Hz) */
    float avg_firing_rate;

    /** Current circadian phase (0.0-24.0 hours) */
    float circadian_phase;

    /** Timestamp (ms) */
    float timestamp_ms;
} physics_hypo_state_t;

/**
 * @brief Hypothalamus modulation applied to physics
 */
typedef struct {
    /** Q10 temperature coefficient modifier */
    float q10_modifier;

    /** Metabolic rate modifier */
    float metabolic_modifier;

    /** Conductance modifier (affects all channels) */
    float conductance_modifier;

    /** Threshold modifier (excitability) */
    float threshold_modifier;

    /** Time constant modifier (dynamics speed) */
    float tau_modifier;

    /** Thermoregulation direction */
    physics_hypo_thermo_dir_t thermo_direction;

    /** Thermoregulation strength (0.0-1.0) */
    float thermo_strength;

    /** Energy conservation mode active */
    bool energy_conservation;

    /** Arousal level (0.0-1.0) */
    float arousal_level;

    /** Circadian multiplier */
    float circadian_multiplier;

    /** Source controller output */
    float controller_output;
} physics_hypo_modulation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /** Total physics → hypothalamus reports */
    uint64_t physics_to_hypo_count;

    /** Total hypothalamus → physics modulations */
    uint64_t hypo_to_physics_count;

    /** Temperature deviations from setpoint */
    uint64_t temp_deviations;

    /** ATP critical events */
    uint64_t atp_critical_events;

    /** Heating activations */
    uint64_t heating_activations;

    /** Cooling activations */
    uint64_t cooling_activations;

    /** Average temperature deviation */
    float avg_temp_deviation;

    /** Average ATP level */
    float avg_atp_level;

    /** Current circadian phase */
    float current_circadian_phase;

    /** Last update timestamp */
    float last_update_ms;
} physics_hypo_stats_t;

/**
 * @brief Opaque bridge structure
 */
typedef struct physics_hypo_bridge_struct physics_hypo_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_default_config(physics_hypo_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create physics-hypothalamus bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
NIMCP_EXPORT physics_hypo_bridge_t* physics_hypo_bridge_create(
    const physics_hypo_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void physics_hypo_bridge_destroy(physics_hypo_bridge_t* bridge);

/**
 * @brief Connect bridge to physics modules
 *
 * @param bridge Bridge instance
 * @param thermo Thermodynamics system (may be NULL)
 * @param hh_pop HH population (may be NULL)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_connect_physics(
    physics_hypo_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo,
    nimcp_hh_population_t* hh_pop
);

/**
 * @brief Connect bridge to hypothalamus homeostasis
 *
 * @param bridge Bridge instance
 * @param homeostasis Hypothalamus homeostasis system
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_connect_homeostasis(
    physics_hypo_bridge_t* bridge,
    hypo_homeostasis_handle_t* homeostasis
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_reset(physics_hypo_bridge_t* bridge);

//=============================================================================
// Physics -> Hypothalamus API
//=============================================================================

/**
 * @brief Report physics state to hypothalamus
 *
 * WHAT: Collects current physics state and reports to homeostasis
 * WHY:  Hypothalamus needs physics measurements for control decisions
 * HOW:  Sample thermodynamics, HH and send to homeostatic controllers
 *
 * @param bridge Bridge instance
 * @param state Output physics state (optional, may be NULL)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_report_state(
    physics_hypo_bridge_t* bridge,
    physics_hypo_state_t* state
);

/**
 * @brief Report temperature to hypothalamus
 *
 * @param bridge Bridge instance
 * @param temperature Temperature (Celsius)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_report_temperature(
    physics_hypo_bridge_t* bridge,
    float temperature
);

/**
 * @brief Report ATP level to hypothalamus
 *
 * @param bridge Bridge instance
 * @param atp_level ATP level (0.0-1.0)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_report_atp(
    physics_hypo_bridge_t* bridge,
    float atp_level
);

//=============================================================================
// Hypothalamus -> Physics API
//=============================================================================

/**
 * @brief Get current modulation from hypothalamus
 *
 * WHAT: Extract homeostatic control outputs as physics modulation
 * WHY:  Hypothalamus drives thermoregulation and energy management
 * HOW:  Read PID controller outputs, map to physics parameters
 *
 * @param bridge Bridge instance
 * @param modulation Output modulation values
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_get_modulation(
    physics_hypo_bridge_t* bridge,
    physics_hypo_modulation_t* modulation
);

/**
 * @brief Apply modulation to physics parameters
 *
 * @param bridge Bridge instance
 * @param modulation Modulation to apply
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_apply_modulation(
    physics_hypo_bridge_t* bridge,
    const physics_hypo_modulation_t* modulation
);

//=============================================================================
// Circadian API
//=============================================================================

/**
 * @brief Set circadian phase
 *
 * @param bridge Bridge instance
 * @param phase Phase (0.0-24.0 hours)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_set_circadian_phase(
    physics_hypo_bridge_t* bridge,
    float phase
);

/**
 * @brief Advance circadian phase by time
 *
 * @param bridge Bridge instance
 * @param dt Time delta (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_advance_circadian(
    physics_hypo_bridge_t* bridge,
    float dt
);

/**
 * @brief Get circadian multiplier for current phase
 *
 * @param bridge Bridge instance
 * @return Multiplier (typically 0.5-1.5)
 */
NIMCP_EXPORT float physics_hypo_get_circadian_multiplier(
    const physics_hypo_bridge_t* bridge
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Full bridge update cycle
 *
 * WHAT: Perform complete physics ↔ hypothalamus synchronization
 * WHY:  Single call for bidirectional integration
 * HOW:  Report state → Update controllers → Get modulation → Apply
 *
 * @param bridge Bridge instance
 * @param dt Time step (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_update(
    physics_hypo_bridge_t* bridge,
    float dt
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current physics state
 *
 * @param bridge Bridge instance
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_get_state(
    const physics_hypo_bridge_t* bridge,
    physics_hypo_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_get_stats(
    const physics_hypo_bridge_t* bridge,
    physics_hypo_stats_t* stats
);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge instance
 * @return true if physics and homeostasis are connected
 */
NIMCP_EXPORT bool physics_hypo_is_connected(
    const physics_hypo_bridge_t* bridge
);

/**
 * @brief Get thermoregulation status
 *
 * @param bridge Bridge instance
 * @param direction Output direction
 * @param strength Output strength
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_hypo_get_thermo_status(
    const physics_hypo_bridge_t* bridge,
    physics_hypo_thermo_dir_t* direction,
    float* strength
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_HYPOTHALAMUS_BRIDGE_H */
