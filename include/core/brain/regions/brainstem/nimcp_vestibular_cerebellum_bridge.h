/**
 * @file nimcp_vestibular_cerebellum_bridge.h
 * @brief Bridge between vestibular nuclei and cerebellum
 *
 * BIOLOGICAL CONTEXT:
 * The vestibulocerebellum (flocculus, nodulus, uvula) receives vestibular
 * input via mossy fibers and modulates vestibular nuclei via Purkinje cell
 * output. This forms a critical loop for VOR calibration.
 *
 * VOR CALIBRATION LOOP:
 * 1. Vestibular nuclei send mossy fiber signal to flocculus
 * 2. Flocculus receives climbing fiber error (retinal slip)
 * 3. LTD at parallel fiber-Purkinje synapses adjusts response
 * 4. Purkinje output inhibits vestibular nuclei
 * 5. VOR gain is adjusted accordingly
 *
 * FLOCCULUS vs NODULUS:
 * - Flocculus: VOR gain/phase adaptation (short-term)
 * - Nodulus: Velocity storage, tilt/translation discrimination (longer-term)
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2025-01-03
 */

#ifndef NIMCP_VESTIBULAR_CEREBELLUM_BRIDGE_H
#define NIMCP_VESTIBULAR_CEREBELLUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/brainstem/nimcp_vestibular.h"

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/* Forward declaration - cerebellum adapter */
typedef struct cerebellum_adapter cerebellum_adapter_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Default VOR-specific LTD rate (faster than general cerebellar learning) */
#define VOR_DEFAULT_LTD_RATE            0.01f

/** Maximum retinal slip that triggers adaptation (rad/s) */
#define VOR_MAX_RETINAL_SLIP            2.0f

/** Minimum retinal slip threshold for adaptation (rad/s) */
#define VOR_RETINAL_SLIP_THRESHOLD      0.02f

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge status
 */
typedef enum {
    VESTIBULAR_CEREBELLUM_STATUS_IDLE,          /**< Idle */
    VESTIBULAR_CEREBELLUM_STATUS_TRANSMITTING,  /**< Sending mossy signals */
    VESTIBULAR_CEREBELLUM_STATUS_ADAPTING,      /**< VOR adaptation in progress */
    VESTIBULAR_CEREBELLUM_STATUS_ERROR          /**< Error state */
} vestibular_cerebellum_status_t;

/**
 * @brief Bridge error codes
 */
typedef enum {
    VESTIBULAR_CEREBELLUM_ERROR_NONE = 0,       /**< No error */
    VESTIBULAR_CEREBELLUM_ERROR_INVALID_PARAM,  /**< Invalid parameter */
    VESTIBULAR_CEREBELLUM_ERROR_NOT_CONNECTED,  /**< Components not connected */
    VESTIBULAR_CEREBELLUM_ERROR_INTERNAL        /**< Internal error */
} vestibular_cerebellum_error_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Mossy fiber routing */
    uint32_t num_mossy_fibers;          /**< Number of mossy fibers from vestibular */
    float mossy_fiber_gain;             /**< Gain on mossy fiber signal */

    /* VOR adaptation */
    bool enable_vor_adaptation;         /**< Enable VOR gain adaptation */
    float vor_ltd_rate;                 /**< LTD rate for VOR (per retinal slip) */
    float retinal_slip_threshold;       /**< Minimum slip to trigger adaptation */

    /* Cerebellar targets */
    bool route_to_flocculus;            /**< Route to flocculus (VOR) */
    bool route_to_nodulus;              /**< Route to nodulus (velocity storage) */

    /* Feedback */
    bool enable_feedback_loop;          /**< Enable Purkinje->vestibular feedback */
    float feedback_weight;              /**< Weight of cerebellar feedback */
} vestibular_cerebellum_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t mossy_signals_sent;        /**< Total mossy fiber signals */
    uint64_t adaptation_triggers;       /**< Times VOR adaptation triggered */
    uint64_t feedback_events;           /**< Cerebellar feedback events */
    float total_vor_gain_change;        /**< Cumulative VOR gain change */
    float avg_retinal_slip;             /**< Average retinal slip */
    float current_flocculus_output;     /**< Current flocculus Purkinje output */
} vestibular_cerebellum_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/** Forward declaration for opaque bridge type */
typedef struct vestibular_cerebellum_bridge vestibular_cerebellum_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default bridge configuration
 *
 * @return Default configuration
 */
vestibular_cerebellum_config_t vestibular_cerebellum_default_config(void);

/**
 * @brief Create vestibular-cerebellum bridge
 *
 * @param vestibular Vestibular processor (required)
 * @param cerebellum Cerebellum adapter (required)
 * @param config Configuration (NULL for defaults)
 * @return New bridge, or NULL on failure
 */
vestibular_cerebellum_bridge_t* vestibular_cerebellum_bridge_create(
    vestibular_processor_t* vestibular,
    cerebellum_adapter_t* cerebellum,
    const vestibular_cerebellum_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void vestibular_cerebellum_bridge_destroy(vestibular_cerebellum_bridge_t* bridge);

/*=============================================================================
 * MOSSY FIBER TRANSMISSION
 *===========================================================================*/

/**
 * @brief Send vestibular mossy fiber signal to cerebellum
 *
 * Routes the current vestibular state to flocculus/nodulus via mossy fibers.
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vestibular_cerebellum_send_mossy_signal(vestibular_cerebellum_bridge_t* bridge);

/**
 * @brief Send custom mossy fiber signal
 *
 * @param bridge Bridge instance
 * @param signal Custom signal
 * @return 0 on success, -1 on error
 */
int vestibular_cerebellum_send_custom_signal(vestibular_cerebellum_bridge_t* bridge,
                                              const vestibular_mossy_signal_t* signal);

/*=============================================================================
 * VOR ADAPTATION
 *===========================================================================*/

/**
 * @brief Trigger VOR adaptation based on retinal slip
 *
 * Retinal slip triggers climbing fiber error signal to flocculus,
 * which induces LTD and adjusts VOR gain.
 *
 * @param bridge Bridge instance
 * @param retinal_slip Retinal slip (image motion during head movement)
 * @param slip_direction Direction of slip [yaw, pitch, roll]
 * @return 0 on success, -1 on error
 */
int vestibular_cerebellum_trigger_vor_adaptation(
    vestibular_cerebellum_bridge_t* bridge,
    float retinal_slip,
    const float slip_direction[3]);

/**
 * @brief Get current VOR adaptation state
 *
 * @param bridge Bridge instance
 * @param vor_gain Output: current VOR gain [yaw, pitch, roll]
 * @param adaptation_active Output: whether adaptation is active
 * @return 0 on success, -1 on error
 */
int vestibular_cerebellum_get_vor_state(const vestibular_cerebellum_bridge_t* bridge,
                                         float vor_gain[3],
                                         bool* adaptation_active);

/*=============================================================================
 * CEREBELLAR FEEDBACK
 *===========================================================================*/

/**
 * @brief Apply cerebellar Purkinje output to vestibular nuclei
 *
 * Purkinje cells inhibit vestibular nuclei to calibrate VOR.
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int vestibular_cerebellum_apply_feedback(vestibular_cerebellum_bridge_t* bridge);

/**
 * @brief Get current cerebellar modulation of vestibular nuclei
 *
 * @param bridge Bridge instance
 * @param nucleus Target nucleus
 * @param modulation Output: modulation factor [0, 2]
 * @return 0 on success, -1 on error
 */
int vestibular_cerebellum_get_modulation(const vestibular_cerebellum_bridge_t* bridge,
                                          vestibular_nucleus_type_t nucleus,
                                          float* modulation);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get bridge status
 *
 * @param bridge Bridge instance
 * @return Current status
 */
vestibular_cerebellum_status_t vestibular_cerebellum_get_status(
    const vestibular_cerebellum_bridge_t* bridge);

/**
 * @brief Get last error
 *
 * @param bridge Bridge instance
 * @return Last error code
 */
vestibular_cerebellum_error_t vestibular_cerebellum_get_last_error(
    const vestibular_cerebellum_bridge_t* bridge);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int vestibular_cerebellum_get_stats(const vestibular_cerebellum_bridge_t* bridge,
                                     vestibular_cerebellum_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VESTIBULAR_CEREBELLUM_BRIDGE_H */
