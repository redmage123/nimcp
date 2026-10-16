/**
 * @file nimcp_vestibular_cerebellum_bridge.c
 * @brief Bridge between vestibular nuclei and cerebellum implementation
 *
 * WHAT: Routes vestibular signals to cerebellum and applies cerebellar feedback
 * WHY:  Enable VOR calibration through vestibulocerebellum
 * HOW:  Mossy fiber transmission + Purkinje cell feedback
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2025-01-03
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(vestibular_cerebellum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "VESTIBULAR_CEREBELLUM_BRIDGE"


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define BRIDGE_LOG_MODULE "VEST-CB-BRIDGE"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal bridge structure
 */
struct vestibular_cerebellum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    vestibular_cerebellum_config_t config;

    /* Connected components */
    vestibular_processor_t* vestibular;
    cerebellum_adapter_t* cerebellum;

    /* Cached signals */
    vestibular_mossy_signal_t last_mossy_signal;
    float last_retinal_slip;
    float slip_direction[3];

    /* VOR adaptation state */
    float vor_gain_delta[3];        /**< Accumulated gain change */
    bool adaptation_in_progress;
    uint64_t last_adaptation_time_us;

    /* Cerebellar feedback */
    float nucleus_modulation[VESTIBULAR_NUM_NUCLEI];

    /* State */
    vestibular_cerebellum_status_t status;
    vestibular_cerebellum_error_t last_error;

    /* Statistics */
    vestibular_cerebellum_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(vestibular_cerebellum_bridge_t* bridge,
                       vestibular_cerebellum_error_t error) {
    if (!bridge) return;
    bridge->last_error = error;
    if (error != VESTIBULAR_CEREBELLUM_ERROR_NONE) {
        bridge->status = VESTIBULAR_CEREBELLUM_STATUS_ERROR;
        LOG_ERROR("[%s] Error: %d", BRIDGE_LOG_MODULE, error);
    }
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

vestibular_cerebellum_config_t vestibular_cerebellum_default_config(void) {
    vestibular_cerebellum_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_mossy_fibers = 100;
    config.mossy_fiber_gain = 1.0f;

    config.enable_vor_adaptation = true;
    config.vor_ltd_rate = VOR_DEFAULT_LTD_RATE;
    config.retinal_slip_threshold = VOR_RETINAL_SLIP_THRESHOLD;

    config.route_to_flocculus = true;
    config.route_to_nodulus = true;

    config.enable_feedback_loop = true;
    config.feedback_weight = 1.0f;

    return config;
}

vestibular_cerebellum_bridge_t* vestibular_cerebellum_bridge_create(
    vestibular_processor_t* vestibular,
    cerebellum_adapter_t* cerebellum,
    const vestibular_cerebellum_config_t* config) {

    if (!vestibular || !cerebellum) {
        LOG_ERROR("[%s] Cannot create bridge: NULL component", BRIDGE_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vestibular_cerebellum_default_config: required parameter is NULL (vestibular, cerebellum)");
        return NULL;
    }

    LOG_INFO("[%s] Creating vestibular-cerebellum bridge", BRIDGE_LOG_MODULE);

    vestibular_cerebellum_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge", BRIDGE_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vestibular_cerebellum_default_config: bridge is NULL");
        return NULL;
    }

    bridge->config = config ? *config : vestibular_cerebellum_default_config();
    bridge->vestibular = vestibular;
    bridge->cerebellum = cerebellum;

    /* Initialize modulation to neutral */
    for (int i = 0; i < VESTIBULAR_NUM_NUCLEI; i++) {
        bridge->nucleus_modulation[i] = 1.0f;
    }

    bridge->status = VESTIBULAR_CEREBELLUM_STATUS_IDLE;
    bridge->last_error = VESTIBULAR_CEREBELLUM_ERROR_NONE;

    LOG_INFO("[%s] Bridge created successfully", BRIDGE_LOG_MODULE);
    return bridge;
}

void vestibular_cerebellum_bridge_destroy(vestibular_cerebellum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "vestibular_cerebellum");

    LOG_INFO("[%s] Destroying bridge", BRIDGE_LOG_MODULE);
    nimcp_free(bridge);
}

/*=============================================================================
 * MOSSY FIBER TRANSMISSION
 *===========================================================================*/

int vestibular_cerebellum_send_mossy_signal(vestibular_cerebellum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->vestibular || !bridge->cerebellum) {
        set_error(bridge, VESTIBULAR_CEREBELLUM_ERROR_NOT_CONNECTED);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_cerebellum_send_mossy_signal: required parameter is NULL (bridge->vestibular, bridge->cerebellum)");
        return -1;
    }

    bridge->status = VESTIBULAR_CEREBELLUM_STATUS_TRANSMITTING;

    /* Get mossy fiber signal from vestibular nuclei */
    vestibular_mossy_signal_t signal;
    if (!vestibular_get_mossy_signal(bridge->vestibular, &signal)) {
        set_error(bridge, VESTIBULAR_CEREBELLUM_ERROR_INTERNAL);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vestibular_cerebellum_send_mossy_signal: vestibular_get_mossy_signal is NULL");
        return -1;
    }

    /* Cache the signal */
    bridge->last_mossy_signal = signal;

    /* Apply gain */
    for (int i = 0; i < 3; i++) {
        signal.head_velocity[i] *= bridge->config.mossy_fiber_gain;
        signal.linear_accel[i] *= bridge->config.mossy_fiber_gain;
    }

    /* Route to cerebellum (flocculus/nodulus) */
    if (bridge->config.route_to_flocculus || bridge->config.route_to_nodulus) {
        /* Send to cerebellum via vestibular input API */
        if (!cerebellum_process_vestibular_input(bridge->cerebellum, &signal)) {
            LOG_WARNING("[%s] Failed to send mossy signal to cerebellum",
                        BRIDGE_LOG_MODULE);
        }
    }

    bridge->stats.mossy_signals_sent++;
    bridge->status = VESTIBULAR_CEREBELLUM_STATUS_IDLE;

    return 0;
}

int vestibular_cerebellum_send_custom_signal(vestibular_cerebellum_bridge_t* bridge,
                                              const vestibular_mossy_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_cerebellum_send_mossy_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }

    if (!bridge->cerebellum) {
        set_error(bridge, VESTIBULAR_CEREBELLUM_ERROR_NOT_CONNECTED);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_cerebellum_send_mossy_signal: bridge->cerebellum is NULL");
        return -1;
    }

    bridge->status = VESTIBULAR_CEREBELLUM_STATUS_TRANSMITTING;

    /* Send to cerebellum */
    if (!cerebellum_process_vestibular_input(bridge->cerebellum, signal)) {
        LOG_WARNING("[%s] Failed to send custom signal to cerebellum",
                    BRIDGE_LOG_MODULE);
    }

    bridge->stats.mossy_signals_sent++;
    bridge->status = VESTIBULAR_CEREBELLUM_STATUS_IDLE;

    return 0;
}

/*=============================================================================
 * VOR ADAPTATION
 *===========================================================================*/

int vestibular_cerebellum_trigger_vor_adaptation(
    vestibular_cerebellum_bridge_t* bridge,
    float retinal_slip,
    const float slip_direction[3]) {

    if (!bridge || !slip_direction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_cerebellum_send_mossy_signal: required parameter is NULL (bridge, slip_direction)");
        return -1;
    }

    if (!bridge->config.enable_vor_adaptation) {
        return 0;  /* Adaptation disabled - not an error */
    }

    /* Check threshold */
    float slip_mag = fabsf(retinal_slip);
    if (slip_mag < bridge->config.retinal_slip_threshold) {
        return 0;  /* Slip too small */
    }

    /* Clamp slip magnitude */
    if (slip_mag > VOR_MAX_RETINAL_SLIP) {
        retinal_slip = (retinal_slip > 0) ? VOR_MAX_RETINAL_SLIP : -VOR_MAX_RETINAL_SLIP;
        slip_mag = VOR_MAX_RETINAL_SLIP;
    }

    bridge->status = VESTIBULAR_CEREBELLUM_STATUS_ADAPTING;
    bridge->adaptation_in_progress = true;
    bridge->last_retinal_slip = retinal_slip;

    for (int i = 0; i < 3; i++) {
        bridge->slip_direction[i] = slip_direction[i];
    }

    LOG_DEBUG("[%s] VOR adaptation triggered: slip=%.3f", BRIDGE_LOG_MODULE, retinal_slip);

    /*
     * VOR adaptation mechanism:
     * 1. Retinal slip = climbing fiber error to flocculus
     * 2. Climbing fiber + parallel fiber -> LTD at Purkinje synapse
     * 3. Reduced Purkinje inhibition -> increased vestibular nuclei output
     * 4. Increased VOR gain
     *
     * For simplicity, we directly adjust VOR gain here and apply
     * modulation to vestibular nuclei.
     */

    /* Compute gain adjustment */
    float ltd_rate = bridge->config.vor_ltd_rate;
    float gain_delta = retinal_slip * ltd_rate;

    /* Apply to vestibular nuclei via feedback path */
    /* MVN: Horizontal VOR (yaw) */
    if (fabsf(slip_direction[0]) > 0.1f) {
        bridge->vor_gain_delta[0] += gain_delta * slip_direction[0];
        bridge->nucleus_modulation[VESTIBULAR_NUCLEUS_MEDIAL] +=
            gain_delta * slip_direction[0] * 0.1f;
        bridge->nucleus_modulation[VESTIBULAR_NUCLEUS_MEDIAL] =
            nimcp_clampf(bridge->nucleus_modulation[VESTIBULAR_NUCLEUS_MEDIAL], 0.5f, 1.5f);
    }

    /* SVN: Vertical VOR (pitch, roll) */
    if (fabsf(slip_direction[1]) > 0.1f || fabsf(slip_direction[2]) > 0.1f) {
        float vert_slip = fabsf(slip_direction[1]) + fabsf(slip_direction[2]);
        bridge->vor_gain_delta[1] += gain_delta * slip_direction[1];
        bridge->vor_gain_delta[2] += gain_delta * slip_direction[2];
        bridge->nucleus_modulation[VESTIBULAR_NUCLEUS_SUPERIOR] +=
            gain_delta * vert_slip * 0.1f;
        bridge->nucleus_modulation[VESTIBULAR_NUCLEUS_SUPERIOR] =
            nimcp_clampf(bridge->nucleus_modulation[VESTIBULAR_NUCLEUS_SUPERIOR], 0.5f, 1.5f);
    }

    /* Report to vestibular system for direct gain adjustment */
    if (bridge->vestibular) {
        vestibular_report_retinal_slip(bridge->vestibular, retinal_slip, slip_direction);
    }

    /* Update stats */
    bridge->stats.adaptation_triggers++;
    bridge->stats.avg_retinal_slip =
        bridge->stats.avg_retinal_slip * 0.9f + slip_mag * 0.1f;
    bridge->stats.total_vor_gain_change +=
        fabsf(bridge->vor_gain_delta[0]) +
        fabsf(bridge->vor_gain_delta[1]) +
        fabsf(bridge->vor_gain_delta[2]);

    bridge->status = VESTIBULAR_CEREBELLUM_STATUS_IDLE;

    return 0;
}

int vestibular_cerebellum_get_vor_state(const vestibular_cerebellum_bridge_t* bridge,
                                         float vor_gain[3],
                                         bool* adaptation_active) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (vor_gain && bridge->vestibular) {
        vestibular_get_vor_gain(bridge->vestibular, vor_gain);
    }

    if (adaptation_active) {
        *adaptation_active = bridge->adaptation_in_progress;
    }

    return 0;
}

/*=============================================================================
 * CEREBELLAR FEEDBACK
 *===========================================================================*/

int vestibular_cerebellum_apply_feedback(vestibular_cerebellum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->config.enable_feedback_loop) {
        return 0;
    }

    if (!bridge->vestibular || !bridge->cerebellum) {
        set_error(bridge, VESTIBULAR_CEREBELLUM_ERROR_NOT_CONNECTED);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_cerebellum_apply_feedback: required parameter is NULL (bridge->vestibular, bridge->cerebellum)");
        return -1;
    }

    /*
     * Apply cerebellar modulation to vestibular nuclei.
     * In biology, flocculus Purkinje cells inhibit MVN/SVN.
     * Reduced Purkinje activity -> increased vestibular output -> higher VOR gain.
     */

    float weight = bridge->config.feedback_weight;

    for (int i = 0; i < VESTIBULAR_NUM_NUCLEI; i++) {
        float mod = 1.0f + (bridge->nucleus_modulation[i] - 1.0f) * weight;
        mod = nimcp_clampf(mod, 0.3f, 2.0f);

        vestibular_apply_cerebellar_modulation(
            bridge->vestibular,
            (vestibular_nucleus_type_t)i,
            mod);
    }

    bridge->stats.feedback_events++;

    /* Get current flocculus output from cerebellum (simplified) */
    float vor_output[3];
    if (cerebellum_get_vor_output(bridge->cerebellum, vor_output)) {
        bridge->stats.current_flocculus_output =
            (vor_output[0] + vor_output[1] + vor_output[2]) / 3.0f;
    }

    return 0;
}

int vestibular_cerebellum_get_modulation(const vestibular_cerebellum_bridge_t* bridge,
                                          vestibular_nucleus_type_t nucleus,
                                          float* modulation) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_cerebellum_apply_feedback: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    if (nucleus >= VESTIBULAR_NUM_NUCLEI) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vestibular_cerebellum_apply_feedback: capacity exceeded");
        return -1;
    }

    *modulation = bridge->nucleus_modulation[nucleus];
    return 0;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

vestibular_cerebellum_status_t vestibular_cerebellum_get_status(
    const vestibular_cerebellum_bridge_t* bridge) {
    if (!bridge) return VESTIBULAR_CEREBELLUM_STATUS_ERROR;
    return bridge->status;
}

vestibular_cerebellum_error_t vestibular_cerebellum_get_last_error(
    const vestibular_cerebellum_bridge_t* bridge) {
    if (!bridge) return VESTIBULAR_CEREBELLUM_ERROR_INTERNAL;
    return bridge->last_error;
}

int vestibular_cerebellum_get_stats(const vestibular_cerebellum_bridge_t* bridge,
                                     vestibular_cerebellum_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_cerebellum_apply_feedback: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}
