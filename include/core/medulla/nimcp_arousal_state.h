/**
 * @file nimcp_arousal_state.h
 * @brief Unified arousal state management with hysteresis for the brainstem
 *
 * WHAT: Implements arousal state transitions with hysteresis to prevent oscillation
 * WHY: Models the reticular activating system (RAS) which regulates consciousness levels
 * HOW: Tracks continuous arousal level (0-1) and discrete states with sustained thresholds
 *
 * Biological Basis:
 * - RAS modulates cortical activation from brainstem
 * - Arousal states range from coma to panic
 * - Hysteresis prevents rapid state oscillations (neural stability)
 * - Multiple neuromodulator systems (NE, 5-HT, ACh, DA) converge on arousal
 */

#ifndef NIMCP_AROUSAL_STATE_H
#define NIMCP_AROUSAL_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WHAT: Discrete arousal states mapping to consciousness levels
 * WHY: Backward-compatible labels for logging, display, and serialization
 * HOW: Ordered from lowest (coma) to highest (panic) arousal.
 *      Labels are derived FROM the continuous arousal_level via
 *      arousal_state_label_from_level(). All behavioral dispatch
 *      should use the continuous level, not these discrete labels.
 */
typedef enum {
    AROUSAL_STATE_COMA = 0,          // 0.00-0.05: Unconscious, no response
    AROUSAL_STATE_DEEP_SLEEP,         // 0.05-0.15: Slow-wave sleep, minimal awareness
    AROUSAL_STATE_LIGHT_SLEEP,        // 0.15-0.30: REM sleep, dream awareness
    AROUSAL_STATE_DROWSY,             // 0.30-0.40: Transitional, reduced vigilance
    AROUSAL_STATE_RELAXED,            // 0.40-0.55: Calm wakefulness, default state
    AROUSAL_STATE_ALERT,              // 0.55-0.70: Normal vigilance, task-ready
    AROUSAL_STATE_VIGILANT,           // 0.70-0.85: Heightened awareness, scanning
    AROUSAL_STATE_HYPERAROUSED,       // 0.85-0.95: Stress response, fight/flight
    AROUSAL_STATE_PANIC,              // 0.95-1.00: Extreme arousal, survival mode
    AROUSAL_STATE_COUNT
} arousal_state_enum_t;

/**
 * WHAT: Continuous parameters computed from arousal level
 * WHY: Replaces discrete dispatch with smooth, continuous scaling
 * HOW: All parameters vary smoothly as functions of arousal_level (0-1).
 *      No step functions -- uses linear interpolation, sigmoid, and
 *      inverted-U curves to model biologically-plausible arousal effects.
 *
 * Biological Basis:
 * - Cognitive gain follows Yerkes-Dodson inverted-U (optimal at moderate arousal)
 * - Cortical activation increases monotonically with arousal (sigmoid)
 * - Sensory gating relaxes at high arousal (threat detection) and low (sleep)
 * - Muscle tone varies: atonia during deep sleep, hypertonia during panic
 * - Metabolic rate scales with activation demand
 */
typedef struct {
    float cortical_activation;   // Sigmoid: 0.0 (coma) -> 1.0 (panic)
    float cognitive_gain;        // Inverted-U: peaks ~0.6, drops at extremes
    float sensory_gating;        // U-shape: tight at moderate, open at extremes
    float muscle_tone;           // Sigmoid: 0.0 (atonia) -> 1.0 (rigid)
    float metabolic_rate;        // Linear: 0.3 (coma) -> 1.0 (panic)
    float vigilance_factor;      // Exponential rise: low->high, saturates
    float emotional_reactivity;  // Exponential: increases sharply above 0.7
    float memory_consolidation;  // Inverted-U: best at moderate-low (sleep/relaxed)
    float free_energy;           // Inverted-U from optimal (alert ~0.6): deviation = energy
} arousal_params_t;

/**
 * WHAT: Configuration for arousal state system behavior
 * WHY: Tunable parameters for hysteresis, rate-limiting, and state stability
 * HOW: Defines thresholds, time constants, and transition constraints
 */
typedef struct {
    float hysteresis_margin;          // Threshold offset to prevent oscillation (default: 0.05)
    float min_dwell_time_ms;          // Minimum time in state before transition (default: 500ms)
    float max_rate_per_sec;           // Maximum arousal level change per second (default: 0.5)
    float stimulus_decay_rate;        // Decay rate for external stimuli (default: 0.1/sec)
    bool enable_hysteresis;           // Whether to use hysteresis (default: true)
    bool enable_rate_limiting;        // Whether to rate-limit changes (default: true)
} arousal_state_config_t;

/**
 * WHAT: Internal state transition tracking
 * WHY: Implements hysteresis by tracking threshold crossing duration
 * HOW: Records when threshold first crossed and sustained duration
 */
typedef struct {
    arousal_state_enum_t target_state; // State we're transitioning toward
    uint64_t threshold_cross_time_ms;  // When we first crossed threshold
    bool threshold_sustained;          // Whether we've met dwell time requirement
} arousal_transition_t;

/**
 * WHAT: Main arousal state manager structure
 * WHY: Encapsulates all state for unified arousal management
 * HOW: Tracks current/target states, continuous level, transitions, and thread safety
 */
typedef struct arousal_state_struct {
    // Core state
    arousal_state_enum_t current_state;  // Current discrete state
    float arousal_level;                  // Continuous level (0.0-1.0)

    // Transition management
    arousal_transition_t transition;      // Pending transition state
    uint64_t last_transition_time_ms;     // When we last changed states
    uint64_t last_update_time_ms;         // When we last updated arousal level

    // External modulation
    float stimulus_accumulator;           // Accumulated external stimuli

    // Configuration
    arousal_state_config_t config;        // System configuration

    // Bio-async integration
    bio_module_context_t bio_ctx;         // Bio-async module context
    bool bio_async_enabled;               // Whether bio-async is active

    // Thread safety
    nimcp_platform_mutex_t* mutex;        // Mutex for thread-safe access
} arousal_state_t;

/**
 * WHAT: Creates and initializes an arousal state manager
 * WHY: Allocates resources and sets up initial state
 * HOW: Allocates struct, initializes to RELAXED state, creates mutex
 *
 * @param config Configuration (NULL for defaults)
 * @return Pointer to arousal state manager, NULL on failure
 */
arousal_state_t* arousal_state_create(const arousal_state_config_t* config);

/**
 * WHAT: Destroys an arousal state manager and frees resources
 * WHY: Prevents memory leaks
 * HOW: Disconnects bio-async, destroys mutex, frees struct
 *
 * @param state Arousal state manager to destroy
 */
void arousal_state_destroy(arousal_state_t* state);

/**
 * WHAT: Populates configuration with default values
 * WHY: Provides biologically-plausible starting parameters
 * HOW: Sets hysteresis margin, dwell time, rate limits based on RAS dynamics
 *
 * @param config Configuration struct to populate
 * @return 0 on success, negative on error
 */
int arousal_state_default_config(arousal_state_config_t* config);

/**
 * WHAT: Updates arousal state with hysteresis and rate-limiting
 * WHY: Applies state transition logic with biological stability constraints
 * HOW: Checks thresholds, applies hysteresis, updates continuous level, handles transitions
 *
 * @param state Arousal state manager
 * @param delta_ms Time elapsed since last update (milliseconds)
 * @return 0 on success, negative on error
 */
int arousal_state_update(arousal_state_t* state, float delta_ms);

/**
 * WHAT: Gets current continuous arousal level
 * WHY: Allows external systems to read arousal state
 * HOW: Thread-safe read of arousal_level field
 *
 * @param state Arousal state manager
 * @param out_level Output pointer for arousal level (0.0-1.0)
 * @return 0 on success, negative on error
 */
int arousal_state_get_level(const arousal_state_t* state, float* out_level);

/**
 * WHAT: Gets current discrete arousal state
 * WHY: Allows external systems to read state classification
 * HOW: Thread-safe read of current_state field
 *
 * @param state Arousal state manager
 * @param out_state Output pointer for current state
 * @return 0 on success, negative on error
 */
int arousal_state_get_state(const arousal_state_t* state, arousal_state_enum_t* out_state);

/**
 * WHAT: Sets target arousal level (modulates toward over time)
 * WHY: Allows external systems to influence arousal
 * HOW: Updates arousal level with rate-limiting applied in next update()
 *
 * @param state Arousal state manager
 * @param target_level Target arousal level (0.0-1.0)
 * @return 0 on success, negative on error
 */
int arousal_state_set_target(arousal_state_t* state, float target_level);

/**
 * WHAT: Applies external stimulus to arousal system
 * WHY: Models neuromodulator input (e.g., threat detection, pain)
 * HOW: Adds stimulus to accumulator, which decays over time
 *
 * @param state Arousal state manager
 * @param stimulus_magnitude Stimulus strength (-1.0 to +1.0)
 * @return 0 on success, negative on error
 */
int arousal_state_apply_stimulus(arousal_state_t* state, float stimulus_magnitude);

/**
 * WHAT: Gets human-readable name for arousal state
 * WHY: Enables logging and debugging
 * HOW: Returns string constant for each state enum
 *
 * @param state_enum Arousal state enum value
 * @return String name of state (e.g., "ALERT")
 */
const char* arousal_state_get_state_name(arousal_state_enum_t state_enum);

/**
 * WHAT: Connects arousal state manager to bio-async router
 * WHY: Enables inter-module messaging for arousal state changes
 * HOW: Registers module with bio-async router
 *
 * @param state Arousal state manager
 * @return 0 on success, negative on error
 */
int arousal_state_connect_bio_async(arousal_state_t* state);

/**
 * WHAT: Disconnects arousal state manager from bio-async router
 * WHY: Cleanup before destruction
 * HOW: Unregisters module from bio-async router
 *
 * @param state Arousal state manager
 * @return 0 on success, negative on error
 */
int arousal_state_disconnect_bio_async(arousal_state_t* state);

/**
 * WHAT: Checks if arousal state manager is connected to bio-async
 * WHY: Allows conditional messaging logic
 * HOW: Returns bio_async_enabled flag
 *
 * @param state Arousal state manager
 * @return true if connected, false otherwise
 */
bool arousal_state_is_bio_async_connected(const arousal_state_t* state);

/**
 * WHAT: Computes continuous behavioral parameters from arousal level
 * WHY: Replaces discrete state dispatch with smooth mathematical functions
 * HOW: Uses sigmoid, inverted-U, exponential, and linear curves to compute
 *      biologically-plausible parameters directly from the continuous level.
 *      All outputs vary smoothly -- no step functions or binary jumps.
 *
 * @param arousal_level Continuous arousal level (0.0-1.0), clamped internally
 * @param out Output parameter struct
 * @return 0 on success, negative on error
 */
int arousal_compute_parameters(float arousal_level, arousal_params_t* out);

/**
 * WHAT: Derives a discrete label from a continuous arousal level
 * WHY: Backward-compatible labeling for logging, display, and serialization.
 *      The label is derived FROM the continuous value, not the other way around.
 * HOW: Maps level to the appropriate enum based on threshold boundaries
 *
 * @param level Continuous arousal level (0.0-1.0)
 * @return Corresponding arousal state label
 */
arousal_state_enum_t arousal_state_label_from_level(float level);

/**
 * WHAT: Gets continuous parameters from arousal state manager
 * WHY: Convenience wrapper that reads the current level and computes params
 * HOW: Thread-safe read of arousal_level, then calls arousal_compute_parameters()
 *
 * @param state Arousal state manager
 * @param out Output parameter struct
 * @return 0 on success, negative on error
 */
int arousal_state_get_parameters(const arousal_state_t* state, arousal_params_t* out);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_AROUSAL_STATE_H
