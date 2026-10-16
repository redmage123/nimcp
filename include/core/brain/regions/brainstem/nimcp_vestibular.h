/**
 * @file nimcp_vestibular.h
 * @brief Vestibular nuclei processor for balance and spatial orientation
 *
 * BIOLOGICAL CONTEXT:
 * The vestibular nuclei are four paired nuclei in the brainstem that process
 * signals from the vestibular organs (semicircular canals and otoliths).
 *
 * FOUR VESTIBULAR NUCLEI:
 * 1. Medial Vestibular Nucleus (MVN) - Horizontal VOR, head velocity
 * 2. Lateral Vestibular Nucleus (LVN) - Vestibulospinal reflex, posture
 * 3. Superior Vestibular Nucleus (SVN) - Vertical VOR
 * 4. Inferior Vestibular Nucleus (IVN) - Integration with cerebellum
 *
 * KEY PATHWAYS:
 * - Vestibulo-Ocular Reflex (VOR): Stabilizes gaze during head movement
 * - Vestibulospinal Reflex: Maintains posture and balance
 * - Vestibulocerebellum: Calibration via flocculus and nodulus
 *
 * VOR PATHWAY:
 * Semicircular canals -> Vestibular nuclei (MVN/SVN)
 *                                |
 *                                v
 *                     Mossy fibers to flocculus
 *                                |
 *                                v
 *                     Purkinje cell output (inhibitory)
 *                                |
 *                                v
 *                     Modulated vestibular nuclei
 *                                |
 *                                v
 *                     Oculomotor neurons -> Eye movement
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2025-01-03
 */

#ifndef NIMCP_VESTIBULAR_H
#define NIMCP_VESTIBULAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Number of vestibular nuclei */
#define VESTIBULAR_NUM_NUCLEI           4

/** Default VOR gain (eye velocity / head velocity) */
#define VESTIBULAR_DEFAULT_VOR_GAIN     1.0f

/** VOR gain adaptation range */
#define VESTIBULAR_MIN_VOR_GAIN         0.3f
#define VESTIBULAR_MAX_VOR_GAIN         2.0f

/** Time constant for velocity storage (seconds) */
#define VESTIBULAR_VELOCITY_STORAGE_TAU 15.0f

/** Default vestibular nuclei neuron counts */
#define VESTIBULAR_DEFAULT_MVN_NEURONS  100
#define VESTIBULAR_DEFAULT_LVN_NEURONS  80
#define VESTIBULAR_DEFAULT_SVN_NEURONS  60
#define VESTIBULAR_DEFAULT_IVN_NEURONS  40

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Vestibular nucleus type
 */
typedef enum {
    VESTIBULAR_NUCLEUS_MEDIAL,      /**< MVN: Horizontal VOR */
    VESTIBULAR_NUCLEUS_LATERAL,     /**< LVN: Vestibulospinal */
    VESTIBULAR_NUCLEUS_SUPERIOR,    /**< SVN: Vertical VOR */
    VESTIBULAR_NUCLEUS_INFERIOR     /**< IVN: Cerebellar integration */
} vestibular_nucleus_type_t;

/**
 * @brief Semicircular canal type
 */
typedef enum {
    SEMICIRCULAR_CANAL_HORIZONTAL,  /**< Horizontal (lateral) canal */
    SEMICIRCULAR_CANAL_ANTERIOR,    /**< Anterior (superior) canal */
    SEMICIRCULAR_CANAL_POSTERIOR    /**< Posterior canal */
} semicircular_canal_type_t;

/**
 * @brief Vestibular processor status
 */
typedef enum {
    VESTIBULAR_STATUS_IDLE,         /**< Waiting for input */
    VESTIBULAR_STATUS_PROCESSING,   /**< Processing vestibular signals */
    VESTIBULAR_STATUS_VOR_ACTIVE,   /**< VOR reflex active */
    VESTIBULAR_STATUS_VSR_ACTIVE,   /**< Vestibulospinal reflex active */
    VESTIBULAR_STATUS_CALIBRATING,  /**< Cerebellar calibration active */
    VESTIBULAR_STATUS_ERROR         /**< Error state */
} vestibular_status_t;

/**
 * @brief Vestibular error codes
 */
typedef enum {
    VESTIBULAR_ERROR_NONE = 0,          /**< No error */
    VESTIBULAR_ERROR_INVALID_INPUT,     /**< Invalid input parameter */
    VESTIBULAR_ERROR_NOT_INITIALIZED,   /**< Processor not initialized */
    VESTIBULAR_ERROR_CALIBRATION_FAIL,  /**< Calibration failed */
    VESTIBULAR_ERROR_INTERNAL           /**< Internal error */
} vestibular_error_t;

/*=============================================================================
 * INPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief Semicircular canal input (angular velocity sensing)
 *
 * BIOLOGICAL: Semicircular canals detect angular head velocity via
 * endolymph movement deflecting hair cells in the ampulla.
 */
typedef struct {
    float yaw_velocity;         /**< Rotation around vertical axis (rad/s) */
    float pitch_velocity;       /**< Rotation around lateral axis (rad/s) */
    float roll_velocity;        /**< Rotation around anterior axis (rad/s) */
    uint64_t timestamp_us;      /**< Timestamp in microseconds */
} semicircular_canal_input_t;

/**
 * @brief Otolith organ input (linear acceleration/gravity sensing)
 *
 * BIOLOGICAL: Otoliths (utricle and saccule) detect linear acceleration
 * and head tilt relative to gravity.
 */
typedef struct {
    float x_accel;              /**< Forward/backward acceleration (m/s^2) */
    float y_accel;              /**< Left/right acceleration (m/s^2) */
    float z_accel;              /**< Up/down acceleration (m/s^2) */
    float head_tilt_pitch;      /**< Head tilt pitch (radians) */
    float head_tilt_roll;       /**< Head tilt roll (radians) */
    uint64_t timestamp_us;      /**< Timestamp in microseconds */
} otolith_input_t;

/**
 * @brief Combined vestibular input
 */
typedef struct {
    semicircular_canal_input_t canals;  /**< Angular velocity from canals */
    otolith_input_t otoliths;           /**< Linear acceleration from otoliths */
    bool canals_valid;                  /**< Canal data is valid */
    bool otoliths_valid;                /**< Otolith data is valid */
} vestibular_input_t;

/*=============================================================================
 * NUCLEUS STATE STRUCTURES
 *===========================================================================*/

/**
 * @brief State of a single vestibular nucleus
 */
typedef struct {
    vestibular_nucleus_type_t type;     /**< Nucleus type */
    float activity[3];                  /**< Activity per axis (yaw, pitch, roll) */
    float baseline_rate;                /**< Baseline firing rate (Hz) */
    float current_rate;                 /**< Current firing rate (Hz) */
    float velocity_storage[3];          /**< Velocity storage integrator */
    float cerebellar_modulation;        /**< Modulation from cerebellum [0, 2] */
    uint32_t num_neurons;               /**< Number of neurons in nucleus */
    uint64_t last_update_us;            /**< Last update timestamp */
} vestibular_nucleus_state_t;

/**
 * @brief VOR state for gaze stabilization
 *
 * The VOR produces compensatory eye movements opposite to head movement.
 * Gain = eye velocity / head velocity (normally ~1.0)
 */
typedef struct {
    float gain[3];                  /**< VOR gain per axis (yaw, pitch, roll) */
    float phase[3];                 /**< VOR phase per axis (degrees) */
    float eye_velocity[3];          /**< Commanded eye velocity (rad/s) */
    float head_velocity[3];         /**< Input head velocity (rad/s) */
    float retinal_slip;             /**< Retinal slip error (image motion) */
    bool adaptation_active;         /**< VOR adaptation in progress */
    float adaptation_rate;          /**< Current adaptation rate */
    uint64_t last_adaptation_us;    /**< Last adaptation timestamp */
} vor_state_t;

/**
 * @brief Vestibulospinal reflex state for posture control
 */
typedef struct {
    float postural_command[3];      /**< Postural adjustment command */
    float head_position[3];         /**< Estimated head position */
    float body_tilt[2];             /**< Body tilt (pitch, roll) */
    bool reflex_active;             /**< VSR is active */
} vestibulospinal_state_t;

/*=============================================================================
 * MOSSY FIBER SIGNAL FOR CEREBELLUM
 *===========================================================================*/

/**
 * @brief Vestibular mossy fiber signal to cerebellum
 *
 * Routes to flocculus (VOR) and nodulus (velocity storage, posture)
 */
typedef struct vestibular_mossy_signal {
    float head_velocity[3];         /**< Head velocity from canals */
    float linear_accel[3];          /**< Linear acceleration from otoliths */
    float eye_velocity[3];          /**< Current eye velocity (efference copy) */
    float retinal_slip;             /**< Retinal slip (error signal) */
    vestibular_nucleus_type_t source; /**< Source nucleus */
    uint64_t timestamp_us;          /**< Signal timestamp */
} vestibular_mossy_signal_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Vestibular processor configuration
 */
typedef struct {
    /* Nucleus neuron counts */
    uint32_t mvn_neurons;           /**< Medial vestibular nucleus neurons */
    uint32_t lvn_neurons;           /**< Lateral vestibular nucleus neurons */
    uint32_t svn_neurons;           /**< Superior vestibular nucleus neurons */
    uint32_t ivn_neurons;           /**< Inferior vestibular nucleus neurons */

    /* VOR parameters */
    float initial_vor_gain;         /**< Initial VOR gain */
    float vor_adaptation_rate;      /**< VOR adaptation rate */
    bool enable_vor_adaptation;     /**< Enable VOR gain adaptation */
    float vor_time_constant_ms;     /**< VOR processing time constant */

    /* Velocity storage */
    float velocity_storage_tau_s;   /**< Velocity storage time constant (s) */
    bool enable_velocity_storage;   /**< Enable velocity storage */

    /* Vestibulospinal */
    bool enable_vestibulospinal;    /**< Enable vestibulospinal reflex */
    float vsr_gain;                 /**< Vestibulospinal gain */

    /* Cerebellar connection */
    bool enable_cerebellar_input;   /**< Accept cerebellar modulation */
    float cerebellar_weight;        /**< Weight of cerebellar input */
} vestibular_config_t;

/**
 * @brief Vestibular processor statistics
 */
typedef struct {
    uint64_t canal_inputs;          /**< Total canal inputs processed */
    uint64_t otolith_inputs;        /**< Total otolith inputs processed */
    uint64_t vor_commands;          /**< Total VOR commands generated */
    uint64_t vsr_commands;          /**< Total VSR commands generated */
    uint64_t adaptation_events;     /**< VOR adaptation events */
    float avg_retinal_slip;         /**< Average retinal slip */
    float current_vor_gain[3];      /**< Current VOR gain per axis */
    float avg_latency_us;           /**< Average processing latency */
} vestibular_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/** Forward declaration for opaque processor type */
typedef struct vestibular_processor vestibular_processor_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default vestibular configuration
 *
 * @return Default configuration with biological parameters
 */
vestibular_config_t vestibular_default_config(void);

/**
 * @brief Create vestibular processor
 *
 * @param config Configuration (NULL for defaults)
 * @return New processor, or NULL on failure
 */
vestibular_processor_t* vestibular_create(const vestibular_config_t* config);

/**
 * @brief Destroy vestibular processor
 *
 * @param processor Processor to destroy
 */
void vestibular_destroy(vestibular_processor_t* processor);

/**
 * @brief Reset processor to initial state
 *
 * @param processor Processor instance
 * @return true on success
 */
bool vestibular_reset(vestibular_processor_t* processor);

/*=============================================================================
 * INPUT PROCESSING
 *===========================================================================*/

/**
 * @brief Process vestibular input (canals + otoliths)
 *
 * @param processor Processor instance
 * @param input Combined vestibular input
 * @return true on success
 */
bool vestibular_process_input(vestibular_processor_t* processor,
                               const vestibular_input_t* input);

/**
 * @brief Process semicircular canal input only
 *
 * @param processor Processor instance
 * @param input Canal input (angular velocity)
 * @return true on success
 */
bool vestibular_process_canal_input(vestibular_processor_t* processor,
                                     const semicircular_canal_input_t* input);

/**
 * @brief Process otolith input only
 *
 * @param processor Processor instance
 * @param input Otolith input (linear acceleration)
 * @return true on success
 */
bool vestibular_process_otolith_input(vestibular_processor_t* processor,
                                       const otolith_input_t* input);

/*=============================================================================
 * VOR FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get VOR eye movement command
 *
 * @param processor Processor instance
 * @param eye_velocity Output: commanded eye velocity (yaw, pitch, roll)
 * @return true on success
 */
bool vestibular_get_vor_command(const vestibular_processor_t* processor,
                                 float eye_velocity[3]);

/**
 * @brief Set VOR gain
 *
 * @param processor Processor instance
 * @param gain New VOR gain (per axis, or single value for all)
 * @param per_axis If true, gain is array[3]; if false, single value
 * @return true on success
 */
bool vestibular_set_vor_gain(vestibular_processor_t* processor,
                              const float* gain,
                              bool per_axis);

/**
 * @brief Get current VOR gain
 *
 * @param processor Processor instance
 * @param gain Output: VOR gain per axis
 * @return true on success
 */
bool vestibular_get_vor_gain(const vestibular_processor_t* processor,
                              float gain[3]);

/**
 * @brief Report retinal slip for VOR adaptation
 *
 * Retinal slip (image motion during head movement) triggers VOR adaptation.
 * Positive slip = eyes not compensating enough (increase gain)
 * Negative slip = eyes overcompensating (decrease gain)
 *
 * @param processor Processor instance
 * @param retinal_slip Retinal slip magnitude (rad/s)
 * @param direction Slip direction (yaw, pitch, roll)
 * @return true on success
 */
bool vestibular_report_retinal_slip(vestibular_processor_t* processor,
                                     float retinal_slip,
                                     const float direction[3]);

/*=============================================================================
 * VESTIBULOSPINAL FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get vestibulospinal postural command
 *
 * @param processor Processor instance
 * @param postural_command Output: postural adjustment vector
 * @return true on success
 */
bool vestibular_get_postural_command(const vestibular_processor_t* processor,
                                      float postural_command[3]);

/**
 * @brief Get estimated head position
 *
 * @param processor Processor instance
 * @param position Output: estimated head position (yaw, pitch, roll)
 * @return true on success
 */
bool vestibular_get_head_position(const vestibular_processor_t* processor,
                                   float position[3]);

/*=============================================================================
 * CEREBELLAR INTERFACE
 *===========================================================================*/

/**
 * @brief Get mossy fiber signal for cerebellum
 *
 * Generates mossy fiber input for vestibulocerebellum (flocculus/nodulus)
 *
 * @param processor Processor instance
 * @param signal Output: mossy fiber signal
 * @return true on success
 */
bool vestibular_get_mossy_signal(const vestibular_processor_t* processor,
                                  vestibular_mossy_signal_t* signal);

/**
 * @brief Apply cerebellar modulation to vestibular nuclei
 *
 * Cerebellar Purkinje cells inhibit vestibular nuclei to calibrate VOR gain.
 *
 * @param processor Processor instance
 * @param nucleus Target nucleus
 * @param modulation Modulation factor [0, 2] (1.0 = no change)
 * @return true on success
 */
bool vestibular_apply_cerebellar_modulation(vestibular_processor_t* processor,
                                             vestibular_nucleus_type_t nucleus,
                                             float modulation);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get processor status
 *
 * @param processor Processor instance
 * @return Current status
 */
vestibular_status_t vestibular_get_status(const vestibular_processor_t* processor);

/**
 * @brief Get last error
 *
 * @param processor Processor instance
 * @return Last error code
 */
vestibular_error_t vestibular_get_last_error(const vestibular_processor_t* processor);

/**
 * @brief Get statistics
 *
 * @param processor Processor instance
 * @param stats Output: statistics
 * @return true on success
 */
bool vestibular_get_stats(const vestibular_processor_t* processor,
                           vestibular_stats_t* stats);

/**
 * @brief Get nucleus state
 *
 * @param processor Processor instance
 * @param nucleus Nucleus type
 * @param state Output: nucleus state
 * @return true on success
 */
bool vestibular_get_nucleus_state(const vestibular_processor_t* processor,
                                   vestibular_nucleus_type_t nucleus,
                                   vestibular_nucleus_state_t* state);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable description
 */
const char* vestibular_error_string(vestibular_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable description
 */
const char* vestibular_status_string(vestibular_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VESTIBULAR_H */
