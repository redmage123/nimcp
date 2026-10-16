/**
 * @file nimcp_medulla_cerebellum_bridge.h
 * @brief Medulla-Cerebellum Bridge for Inferior Olive Error Signaling
 *
 * WHAT: Bridge connecting medulla oblongata to cerebellum via inferior olive
 * WHY:  The inferior olive (in medulla) generates climbing fiber error signals
 *       that drive cerebellar learning - this is THE primary error pathway
 * HOW:  Converts medulla state (arousal, protection, errors) to climbing fiber
 *       signals, modulates motor readiness, and adjusts learning rates
 *
 * BIOLOGICAL BASIS:
 * The inferior olive (IO) is located in the medulla oblongata and is the sole
 * source of climbing fibers to the cerebellum. Key pathways:
 *
 * 1. Inferior Olive → Climbing Fibers → Purkinje Cells
 *    - Each IO neuron contacts ~10 Purkinje cells
 *    - Climbing fiber activation triggers complex spikes
 *    - Complex spikes induce LTD at parallel fiber synapses
 *    - This is the main error signal for cerebellar learning
 *
 * 2. Reticular Formation → Deep Nuclei
 *    - Arousal state modulates motor readiness
 *    - Higher arousal = faster reactions, higher gain
 *    - Low arousal = reduced motor output
 *
 * 3. Protection Reflexes → Motor Gating
 *    - Emergency states can suppress motor output
 *    - Protective reflexes override voluntary motor control
 *
 * 4. Circadian → Learning Rate
 *    - Time of day affects motor learning efficiency
 *    - Peak alertness correlates with better acquisition
 *
 * INTEGRATION POINTS:
 * - Medulla arousal_state → Cerebellum motor readiness
 * - Medulla protection_level → Cerebellum motor gating
 * - Medulla circadian_phase → Cerebellum LTD/LTP rates
 * - Bridge inferior_olive → Cerebellum climbing_fiber_signal
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#ifndef NIMCP_MEDULLA_CEREBELLUM_BRIDGE_H
#define NIMCP_MEDULLA_CEREBELLUM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

typedef struct medulla_struct* medulla_t;
typedef struct cerebellum_adapter cerebellum_adapter_t;
typedef struct bio_router_struct* bio_router_t;

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum inferior olive neurons (each contacts ~10 Purkinje cells) */
#define MED_CEREB_MAX_IO_NEURONS 100

/** Maximum pending error signals in queue */
#define MED_CEREB_MAX_ERROR_QUEUE 64

/** Default inferior olive firing rate (Hz) - typically 1-10 Hz */
#define MED_CEREB_DEFAULT_IO_RATE 1.0f

/** Maximum climbing fiber rate (Hz) - biological limit ~10 Hz */
#define MED_CEREB_MAX_IO_RATE 10.0f

/* ============================================================================
 * ERROR TYPES (for climbing fiber signals)
 * ============================================================================ */

/**
 * @brief Error types that inferior olive can signal
 *
 * BIOLOGICAL: Different IO subdivisions process different error types
 */
typedef enum {
    /** Motor timing error - movement too early or late */
    MED_CEREB_ERROR_TIMING = 0,

    /** Force/amplitude error - movement too weak or strong */
    MED_CEREB_ERROR_AMPLITUDE = 1,

    /** Trajectory error - movement path deviation */
    MED_CEREB_ERROR_TRAJECTORY = 2,

    /** Coordination error - multi-joint synchronization failure */
    MED_CEREB_ERROR_COORDINATION = 3,

    /** Prediction error - forward model mismatch */
    MED_CEREB_ERROR_PREDICTION = 4,

    /** Protection error - emergency/reflexive correction needed */
    MED_CEREB_ERROR_PROTECTION = 5,

    /** Sequence error - action sequence violation */
    MED_CEREB_ERROR_SEQUENCE = 6,

    /** Count of error types */
    MED_CEREB_ERROR_COUNT
} med_cereb_error_type_t;

/* ============================================================================
 * MODULATION EFFECTS
 * ============================================================================ */

/**
 * @brief How arousal affects motor performance
 *
 * BIOLOGICAL: Reticular formation modulates motor readiness
 */
typedef struct {
    /** Motor gain multiplier [0.2, 2.0] - higher arousal = higher gain */
    float motor_gain;

    /** Reaction time multiplier [0.5, 2.0] - higher arousal = faster */
    float reaction_time_factor;

    /** Deep nuclei excitability [0.0, 1.0] */
    float nuclei_excitability;

    /** Fine motor precision [0.0, 1.0] - inverted U curve */
    float fine_motor_precision;

    /** Tremor amplitude [0.0, 1.0] - high at extremes */
    float tremor_amplitude;
} med_cereb_arousal_effects_t;

/**
 * @brief How protection level affects motor output
 *
 * BIOLOGICAL: Emergency states override normal motor control
 */
typedef struct {
    /** Motor output scaling [0.0, 1.0] */
    float output_scale;

    /** Whether non-essential motor is disabled */
    bool non_essential_disabled;

    /** Whether all voluntary motor is disabled */
    bool voluntary_disabled;

    /** Emergency stop active */
    bool emergency_stop;

    /** Only reflexive motor allowed */
    bool reflexes_only;
} med_cereb_protection_effects_t;

/**
 * @brief How circadian phase affects learning
 *
 * BIOLOGICAL: Motor learning efficiency varies with time of day
 */
typedef struct {
    /** LTD rate multiplier [0.3, 1.5] */
    float ltd_rate_multiplier;

    /** LTP rate multiplier [0.3, 1.5] */
    float ltp_rate_multiplier;

    /** Consolidation rate [0.0, 1.0] */
    float consolidation_rate;

    /** Memory retrieval efficiency [0.0, 1.0] */
    float retrieval_efficiency;
} med_cereb_circadian_effects_t;

/* ============================================================================
 * INFERIOR OLIVE MODEL
 * ============================================================================ */

/**
 * @brief Inferior olive neuron state
 *
 * BIOLOGICAL: IO neurons have unique electrophysiology:
 * - Subthreshold oscillations (~10 Hz)
 * - Gap junction coupling (electrical synapses)
 * - Refractory period ~100ms after firing
 */
typedef struct {
    /** Neuron ID */
    uint32_t neuron_id;

    /** Current membrane potential proxy [0, 1] */
    float activation;

    /** Accumulated error signal */
    float error_accumulator;

    /** Time since last spike (us) */
    uint64_t refractory_remaining_us;

    /** Target Purkinje cells (indices) */
    uint32_t target_purkinje[10];

    /** Number of targets */
    uint32_t num_targets;

    /** Primary error type this neuron encodes */
    med_cereb_error_type_t error_type;

    /** Oscillation phase [0, 2*PI] */
    float oscillation_phase;

    /** Whether currently in refractory period */
    bool is_refractory;
} med_cereb_io_neuron_t;

/**
 * @brief Inferior olive model
 */
typedef struct {
    /** Array of IO neurons */
    med_cereb_io_neuron_t neurons[MED_CEREB_MAX_IO_NEURONS];

    /** Number of active neurons */
    uint32_t num_neurons;

    /** Global oscillation frequency (Hz) */
    float oscillation_freq;

    /** Gap junction coupling strength [0, 1] */
    float coupling_strength;

    /** Refractory period duration (us) */
    uint64_t refractory_period_us;

    /** Error threshold for firing */
    float firing_threshold;

    /** Current simulation time (us) */
    uint64_t current_time_us;
} med_cereb_inferior_olive_t;

/* ============================================================================
 * BRIDGE CONFIGURATION
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Number of inferior olive neurons to model */
    uint32_t num_io_neurons;

    /** Enable arousal → motor modulation */
    bool enable_arousal_modulation;

    /** Enable protection → motor gating */
    bool enable_protection_gating;

    /** Enable circadian → learning rate */
    bool enable_circadian_learning;

    /** Enable inferior olive error signaling */
    bool enable_io_signaling;

    /** IO oscillation frequency (Hz) */
    float io_oscillation_freq;

    /** IO gap junction coupling strength */
    float io_coupling_strength;

    /** IO refractory period (ms) */
    float io_refractory_ms;

    /** IO firing threshold */
    float io_firing_threshold;

    /* Arousal modulation parameters */

    /** Arousal sensitivity for motor gain */
    float arousal_gain_sensitivity;

    /** Optimal arousal level for fine motor (inverted U) */
    float optimal_arousal_level;

    /* Protection parameters */

    /** Protection level at which non-essential motor stops */
    uint8_t non_essential_cutoff_level;

    /** Protection level at which voluntary motor stops */
    uint8_t voluntary_cutoff_level;

    /* Circadian parameters */

    /** Maximum circadian boost to learning */
    float max_circadian_learning_boost;

    /** Minimum circadian reduction to learning */
    float min_circadian_learning_factor;

    /* Bio-async */

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Update interval (us) */
    uint64_t update_interval_us;
} med_cereb_bridge_config_t;

/* ============================================================================
 * BRIDGE STATE
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /** Total climbing fiber signals sent */
    uint64_t climbing_signals_sent;

    /** Signals sent per error type */
    uint64_t signals_per_type[MED_CEREB_ERROR_COUNT];

    /** Motor commands modulated */
    uint64_t motor_commands_modulated;

    /** Times protection gated motor output */
    uint64_t protection_gates;

    /** Learning rate adjustments applied */
    uint64_t learning_rate_adjustments;

    /** IO spikes generated */
    uint64_t io_spikes;

    /** Errors dropped (queue full) */
    uint64_t errors_dropped;

    /** Average error magnitude */
    float avg_error_magnitude;

    /** Peak error magnitude */
    float peak_error_magnitude;
} med_cereb_bridge_stats_t;

/**
 * @brief Pending error in queue
 */
typedef struct {
    med_cereb_error_type_t error_type;
    float magnitude;
    uint64_t timestamp_us;
    uint32_t source_id;
    bool processed;
} med_cereb_pending_error_t;

/**
 * @brief Bridge state (opaque)
 */
typedef struct med_cereb_bridge_struct* med_cereb_bridge_t;

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_default_config(med_cereb_bridge_config_t* config);

/**
 * @brief Create bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
med_cereb_bridge_t med_cereb_bridge_create(const med_cereb_bridge_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void med_cereb_bridge_destroy(med_cereb_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_reset(med_cereb_bridge_t bridge);

/* ============================================================================
 * CONNECTION
 * ============================================================================ */

/**
 * @brief Connect to medulla
 *
 * @param bridge Bridge
 * @param medulla Medulla instance
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_connect_medulla(med_cereb_bridge_t bridge, medulla_t medulla);

/**
 * @brief Connect to cerebellum
 *
 * @param bridge Bridge
 * @param cerebellum Cerebellum adapter instance (pointer)
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_connect_cerebellum(med_cereb_bridge_t bridge,
                                         cerebellum_adapter_t* cerebellum);

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_connect_bio_async(med_cereb_bridge_t bridge,
                                        bio_router_t router);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge
 * @return true if medulla and cerebellum are connected
 */
bool med_cereb_bridge_is_connected(med_cereb_bridge_t bridge);

/* ============================================================================
 * INFERIOR OLIVE ERROR SIGNALING
 * ============================================================================ */

/**
 * @brief Queue an error signal for inferior olive processing
 *
 * The error will be processed during the next update cycle and may
 * generate climbing fiber signals to the cerebellum.
 *
 * @param bridge Bridge
 * @param error_type Type of error
 * @param magnitude Error magnitude [-1, 1]
 * @param source_id Optional source identifier
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_queue_error(med_cereb_bridge_t bridge,
                                  med_cereb_error_type_t error_type,
                                  float magnitude,
                                  uint32_t source_id);

/**
 * @brief Send immediate climbing fiber signal (bypasses IO model)
 *
 * Use for urgent error signals that shouldn't wait for IO processing.
 *
 * @param bridge Bridge
 * @param error_type Error type
 * @param magnitude Error magnitude [-1, 1]
 * @param target_purkinje Target Purkinje cell (0 for broadcast)
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_send_climbing_signal(med_cereb_bridge_t bridge,
                                           med_cereb_error_type_t error_type,
                                           float magnitude,
                                           uint32_t target_purkinje);

/**
 * @brief Broadcast error to all Purkinje cells
 *
 * @param bridge Bridge
 * @param error_type Error type
 * @param magnitude Error magnitude [-1, 1]
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_broadcast_error(med_cereb_bridge_t bridge,
                                      med_cereb_error_type_t error_type,
                                      float magnitude);

/* ============================================================================
 * AROUSAL MODULATION
 * ============================================================================ */

/**
 * @brief Get current arousal effects on cerebellum
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_get_arousal_effects(med_cereb_bridge_t bridge,
                                          med_cereb_arousal_effects_t* effects);

/**
 * @brief Apply arousal modulation to motor output
 *
 * Modulates motor command based on current arousal level.
 *
 * @param bridge Bridge
 * @param motor_command Input motor command array
 * @param modulated_command Output modulated command array
 * @param num_dimensions Number of motor dimensions
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_modulate_motor(med_cereb_bridge_t bridge,
                                     const float* motor_command,
                                     float* modulated_command,
                                     uint32_t num_dimensions);

/* ============================================================================
 * PROTECTION GATING
 * ============================================================================ */

/**
 * @brief Get current protection effects on cerebellum
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_get_protection_effects(med_cereb_bridge_t bridge,
                                             med_cereb_protection_effects_t* effects);

/**
 * @brief Check if motor output is allowed
 *
 * @param bridge Bridge
 * @param is_essential Whether motor command is essential
 * @param is_reflexive Whether motor command is reflexive
 * @return true if output allowed, false if gated
 */
bool med_cereb_bridge_motor_allowed(med_cereb_bridge_t bridge,
                                     bool is_essential,
                                     bool is_reflexive);

/**
 * @brief Trigger emergency motor stop
 *
 * Immediately gates all motor output and sends protection error.
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_emergency_stop(med_cereb_bridge_t bridge);

/**
 * @brief Release emergency stop
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_release_emergency(med_cereb_bridge_t bridge);

/* ============================================================================
 * CIRCADIAN LEARNING MODULATION
 * ============================================================================ */

/**
 * @brief Get current circadian effects on learning
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_get_circadian_effects(med_cereb_bridge_t bridge,
                                            med_cereb_circadian_effects_t* effects);

/**
 * @brief Get current learning rate multiplier
 *
 * Combines circadian and arousal effects on learning rate.
 *
 * @param bridge Bridge
 * @return Learning rate multiplier [0.1, 2.0]
 */
float med_cereb_bridge_get_learning_multiplier(med_cereb_bridge_t bridge);

/**
 * @brief Apply circadian modulation to cerebellum learning rates
 *
 * Updates cerebellum's LTD/LTP rates based on circadian phase.
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_apply_circadian_learning(med_cereb_bridge_t bridge);

/* ============================================================================
 * UPDATE
 * ============================================================================ */

/**
 * @brief Main update function
 *
 * Call periodically to:
 * - Process queued errors through inferior olive
 * - Update arousal modulation
 * - Check protection gating
 * - Apply circadian learning adjustments
 *
 * @param bridge Bridge
 * @param delta_us Time since last update (microseconds)
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_update(med_cereb_bridge_t bridge, uint64_t delta_us);

/**
 * @brief Process bio-async messages
 *
 * @param bridge Bridge
 * @return Number of messages processed, -1 on error
 */
int med_cereb_bridge_process_messages(med_cereb_bridge_t bridge);

/* ============================================================================
 * QUERY
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_get_stats(med_cereb_bridge_t bridge,
                                med_cereb_bridge_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_reset_stats(med_cereb_bridge_t bridge);

/**
 * @brief Get inferior olive state
 *
 * @param bridge Bridge
 * @param io_state Output IO state (caller provides storage)
 * @return 0 on success, -1 on error
 */
int med_cereb_bridge_get_io_state(med_cereb_bridge_t bridge,
                                   med_cereb_inferior_olive_t* io_state);

/**
 * @brief Get number of pending errors
 *
 * @param bridge Bridge
 * @return Number of pending errors in queue
 */
uint32_t med_cereb_bridge_pending_error_count(med_cereb_bridge_t bridge);

/* ============================================================================
 * DEBUG / DIAGNOSTICS
 * ============================================================================ */

/**
 * @brief Print bridge state to stdout
 *
 * @param bridge Bridge (NULL safe)
 */
void med_cereb_bridge_print_state(med_cereb_bridge_t bridge);

/**
 * @brief Print inferior olive state
 *
 * @param bridge Bridge (NULL safe)
 */
void med_cereb_bridge_print_io_state(med_cereb_bridge_t bridge);

/**
 * @brief Get error type name
 *
 * @param error_type Error type
 * @return String name (static)
 */
const char* med_cereb_error_type_name(med_cereb_error_type_t error_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEDULLA_CEREBELLUM_BRIDGE_H */
