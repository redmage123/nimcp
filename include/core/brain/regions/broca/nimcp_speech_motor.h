/**
 * @file nimcp_speech_motor.h
 * @brief Speech motor planning system for Broca's region
 *
 * WHAT: Motor planning module that converts phonemes to articulatory commands
 * WHY:  Enable speech production through coordinated motor control
 * HOW:  Phoneme-to-motor mapping with temporal coordination and coarticulation
 *
 * Mimics biological speech motor planning:
 * - Broca's area (BA44/45): Speech motor planning and sequencing
 * - Motor cortex: Articulator control (lips, tongue, jaw, larynx)
 * - Cerebellum: Timing and coordination of movements
 * - Coarticulation: Smooth transitions between phonemes
 *
 * BIOLOGICAL MOTIVATION:
 * - Speech production requires coordinating ~100 muscles
 * - Articulators must move precisely and rapidly (10-15 phonemes/sec)
 * - Coarticulation: Articulator positions influenced by surrounding phonemes
 * - Motor planning occurs ~200ms before articulation
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7
 */

#ifndef NIMCP_SPEECH_MOTOR_H
#define NIMCP_SPEECH_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

#define SPEECH_MOTOR_MAX_COMMANDS 256
#define SPEECH_MOTOR_MAX_PHONEMES 128
#define SPEECH_MOTOR_DEFAULT_PLANNING_WINDOW_MS 200.0f
#define SPEECH_MOTOR_NUM_ARTICULATORS 5

//=============================================================================
// Articulator Types
//=============================================================================

/**
 * @brief Types of speech articulators
 *
 * BIOLOGY:
 * - LIPS: Bilabial and labiodental consonants (/p/, /b/, /m/, /f/, /v/)
 * - TONGUE: Most consonants and vowels (tip, blade, dorsum movement)
 * - JAW: Vowel height and consonant manner
 * - LARYNX: Voicing (vocal fold vibration) and pitch
 * - VELUM: Nasal vs oral sounds (/m/, /n/, /ŋ/)
 */
typedef enum {
    ARTICULATOR_LIPS,      ///< Lip position and rounding
    ARTICULATOR_TONGUE,    ///< Tongue position (tip, blade, dorsum)
    ARTICULATOR_JAW,       ///< Jaw height (open/close)
    ARTICULATOR_LARYNX,    ///< Vocal fold tension and voicing
    ARTICULATOR_VELUM      ///< Velopharyngeal port (nasal coupling)
} articulator_type_t;

//=============================================================================
// Motor Command Structure
//=============================================================================

/**
 * @brief Motor command for a single articulator
 *
 * WHAT: Target position, velocity, and timing for one articulator
 * WHY:  Specify precise motor control for speech production
 * HOW:  Position (normalized), velocity (units/sec), timestamp (ms)
 */
typedef struct {
    articulator_type_t type;  ///< Which articulator to control
    float position;           ///< Target position [0.0, 1.0]
    float velocity;           ///< Movement velocity [0.0, 10.0]
    double timestamp;         ///< Execution time (ms from start)
    uint8_t phoneme;          ///< Associated phoneme (for debugging)
} motor_command_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for speech motor planner
 *
 * WHAT: Parameters controlling motor planning behavior
 * WHY:  Allow tuning of planning window, coarticulation, command capacity
 * HOW:  Set parameters at creation time
 */
typedef struct {
    uint32_t max_commands;          ///< Maximum queued commands (default: 256)
    float planning_window_ms;       ///< Planning lookahead time (default: 200ms)
    bool enable_coarticulation;     ///< Enable smooth transitions (default: true)
    float coarticulation_strength;  ///< Blending strength [0.0, 1.0] (default: 0.7)
    float default_velocity;         ///< Default movement speed (default: 5.0)
} speech_motor_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Speech motor planner instance (opaque)
 *
 * WHAT: Internal state for motor planning
 * WHY:  Encapsulation - hide implementation details
 * HOW:  Opaque pointer, access via API functions only
 */
typedef struct speech_motor_planner speech_motor_planner_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Speech motor planner statistics
 */
typedef struct {
    uint64_t phonemes_planned;      ///< Total phonemes processed
    uint64_t commands_generated;    ///< Total motor commands generated
    uint32_t queue_size;            ///< Current command queue size
    float avg_planning_time_ms;     ///< Average planning time per phoneme
} speech_motor_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Create configuration with sensible defaults
 * WHY:  Simplify initialization for common use cases
 * HOW:  Returns struct with biologically-motivated defaults
 *
 * @return Default configuration structure
 */
speech_motor_config_t speech_motor_default_config(void);

/**
 * @brief Create speech motor planner
 *
 * WHAT: Allocate and initialize motor planning system
 * WHY:  Set up phoneme-to-motor mapping and command queue
 * HOW:  Allocate structures, initialize mapping table, create command buffer
 *
 * @param config Configuration parameters (or NULL for defaults)
 * @return Motor planner instance or NULL on failure
 *
 * EXAMPLE:
 *   speech_motor_config_t config = speech_motor_default_config();
 *   config.planning_window_ms = 150.0f;
 *   speech_motor_planner_t* planner = speech_motor_create(&config);
 */
speech_motor_planner_t* speech_motor_create(const speech_motor_config_t* config);

/**
 * @brief Destroy speech motor planner
 *
 * WHAT: Free all resources associated with planner
 * WHY:  Prevent memory leaks
 * HOW:  Free command queue, mapping table, planner structure
 *
 * @param planner Motor planner instance (NULL is safe)
 */
void speech_motor_destroy(speech_motor_planner_t* planner);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Plan motor commands for a phoneme
 *
 * WHAT: Convert phoneme to articulator motor commands
 * WHY:  Generate executable motor plan for speech production
 * HOW:  Lookup phoneme features, generate per-articulator commands, apply coarticulation
 *
 * BIOLOGY:
 * - Phoneme features (place, manner, voicing) determine articulator targets
 * - Coarticulation: Current targets blend with previous/next phonemes
 * - Planning occurs in parallel (~200ms before execution)
 *
 * @param planner Motor planner instance
 * @param phoneme Phoneme ID (IPA-based encoding)
 * @return true on success, false on failure
 *
 * EXAMPLE:
 *   speech_motor_plan_phoneme(planner, 'p');  // Plan /p/ phoneme
 */
bool speech_motor_plan_phoneme(speech_motor_planner_t* planner, uint8_t phoneme);

/**
 * @brief Get planned motor commands
 *
 * WHAT: Retrieve generated motor commands from queue
 * WHY:  Execute motor plan through motor cortex
 * HOW:  Copy commands from internal queue to output buffer
 *
 * @param planner Motor planner instance
 * @param commands Output buffer for commands (must be pre-allocated)
 * @param count Input: buffer capacity; Output: actual command count
 * @return true on success, false on failure
 *
 * EXAMPLE:
 *   motor_command_t commands[64];
 *   uint32_t count = 64;
 *   if (speech_motor_get_commands(planner, commands, &count)) {
 *       // Execute commands[0..count-1]
 *   }
 */
bool speech_motor_get_commands(
    speech_motor_planner_t* planner,
    motor_command_t* commands,
    uint32_t* count
);

/**
 * @brief Reset motor planner state
 *
 * WHAT: Clear command queue and reset planning state
 * WHY:  Start fresh planning sequence or recover from errors
 * HOW:  Clear queue, reset timestamps, initialize articulators to neutral
 *
 * @param planner Motor planner instance
 * @return true on success, false on failure
 */
bool speech_motor_reset(speech_motor_planner_t* planner);

//=============================================================================
// Advanced Operations
//=============================================================================

/**
 * @brief Plan motor commands for phoneme sequence
 *
 * WHAT: Plan multiple phonemes with optimal coarticulation
 * WHY:  Enable efficient batch planning with lookahead
 * HOW:  Process sequence, apply anticipatory coarticulation
 *
 * @param planner Motor planner instance
 * @param phonemes Array of phoneme IDs
 * @param num_phonemes Number of phonemes in sequence
 * @return true on success, false on failure
 */
bool speech_motor_plan_sequence(
    speech_motor_planner_t* planner,
    const uint8_t* phonemes,
    uint32_t num_phonemes
);

/**
 * @brief Enable/disable trajectory interpolation
 *
 * WHAT: Control whether intermediate points are generated between targets
 * WHY:  Smooth articulator movements for natural speech
 * HOW:  When enabled, generates interpolated waypoints between phoneme targets
 *
 * BIOLOGY:
 * - Natural speech involves smooth trajectories, not discrete jumps
 * - Motor cortex generates continuous movements via point-to-point planning
 * - Interpolation mimics this continuous control
 *
 * @param planner Motor planner instance
 * @param enable true to enable interpolation, false for discrete targets
 * @param num_interpolation_points Number of intermediate points (2-10)
 * @return true on success, false on invalid parameters
 */
bool speech_motor_set_interpolation(
    speech_motor_planner_t* planner,
    bool enable,
    uint32_t num_interpolation_points
);

/**
 * @brief Get interpolated trajectory between two positions
 *
 * WHAT: Generate smooth trajectory between articulator positions
 * WHY:  Enable smooth movements for natural speech
 * HOW:  Use cubic Hermite spline interpolation
 *
 * @param start_pos Starting position [0-1]
 * @param end_pos Ending position [0-1]
 * @param start_vel Starting velocity
 * @param end_vel Ending velocity
 * @param t Interpolation parameter [0-1]
 * @return Interpolated position
 */
float speech_motor_interpolate_position(
    float start_pos,
    float end_pos,
    float start_vel,
    float end_vel,
    float t
);

/**
 * @brief Set articulator position manually
 *
 * WHAT: Override articulator position (for initialization or debugging)
 * WHY:  Set starting position or test specific configurations
 * HOW:  Directly set articulator state
 *
 * @param planner Motor planner instance
 * @param articulator Which articulator to set
 * @param position Target position [0.0, 1.0]
 * @return true on success, false on failure
 */
bool speech_motor_set_articulator(
    speech_motor_planner_t* planner,
    articulator_type_t articulator,
    float position
);

/**
 * @brief Get current articulator position
 *
 * WHAT: Query current position of an articulator
 * WHY:  Monitor articulator state or verify commands
 * HOW:  Return current position from internal state
 *
 * @param planner Motor planner instance
 * @param articulator Which articulator to query
 * @param position Output: current position [0.0, 1.0]
 * @return true on success, false on failure
 */
bool speech_motor_get_articulator(
    const speech_motor_planner_t* planner,
    articulator_type_t articulator,
    float* position
);

/**
 * @brief Get motor planner statistics
 *
 * WHAT: Retrieve performance and usage statistics
 * WHY:  Monitor planner performance and queue utilization
 * HOW:  Copy current stats to output structure
 *
 * @param planner Motor planner instance
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool speech_motor_get_stats(
    const speech_motor_planner_t* planner,
    speech_motor_stats_t* stats
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get articulator name as string
 *
 * WHAT: Convert articulator enum to human-readable name
 * WHY:  Debugging and logging
 * HOW:  Lookup table
 *
 * @param articulator Articulator type
 * @return String name or "UNKNOWN"
 */
const char* speech_motor_articulator_name(articulator_type_t articulator);

/**
 * @brief Validate configuration
 *
 * WHAT: Check if configuration parameters are valid
 * WHY:  Catch invalid parameters before creation
 * HOW:  Range checks and sanity validation
 *
 * @param config Configuration to validate
 * @return true if valid, false if invalid
 */
bool speech_motor_validate_config(const speech_motor_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SPEECH_MOTOR_H
