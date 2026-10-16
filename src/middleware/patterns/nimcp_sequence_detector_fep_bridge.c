/**
 * @file nimcp_sequence_detector_fep_bridge.c
 * @brief Free Energy Principle - Sequence Detector Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "middleware/patterns/nimcp_sequence_detector_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sequence_detector_fep_bridge)

int sequence_detector_fep_bridge_default_config(
    sequence_detector_fep_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }
    config->enable_prediction_priming = true;
    config->enable_precision_tolerance = true;
    config->enable_sequence_pe = true;
    config->enable_replay_consolidation = true;
    config->tolerance_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->pe_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

sequence_detector_fep_bridge_t* sequence_detector_fep_bridge_create(
    const sequence_detector_fep_config_t* config
) {
    sequence_detector_fep_bridge_t* bridge = (sequence_detector_fep_bridge_t*)
        nimcp_calloc(1, sizeof(sequence_detector_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate sequence-FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    sequence_detector_fep_config_t default_cfg;
    if (!config) {
        sequence_detector_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    bridge->effects.detection_threshold = 0.5f;
    bridge->effects.temporal_tolerance = SEQUENCE_TEMPORAL_TOLERANCE_MS;
    bridge->state.current_precision = 0.5f;

    if (bridge_base_init(&bridge->base, 0, "sequence_detector_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        sequence_detector_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Sequence-FEP bridge created");
    return bridge;
}

void sequence_detector_fep_bridge_destroy(
    sequence_detector_fep_bridge_t* bridge
) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        sequence_detector_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Sequence-FEP bridge destroyed");
}

int sequence_detector_fep_bridge_connect_detector(
    sequence_detector_fep_bridge_t* bridge,
    sequence_detector_t* detector
) {
    if (!bridge || !detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_fep_bridge_connect_detector: required parameter is NULL (bridge, detector)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->sequence_detector = detector;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Sequence detector connected to FEP bridge");
    return 0;
}

int sequence_detector_fep_bridge_connect_fep(
    sequence_detector_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_fep_bridge_connect_fep: required parameter is NULL (bridge, fep)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("FEP system connected to sequence bridge");
    return 0;
}

int sequence_detector_fep_bridge_disconnect(
    sequence_detector_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->sequence_detector = NULL;
    bridge->fep_system = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Sequence-FEP bridge disconnected");
    return 0;
}

int sequence_detector_fep_prime_expected_sequence(
    sequence_detector_fep_bridge_t* bridge,
    uint32_t template_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_prediction_priming) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->effects.expected_template_id = template_id;
    bridge->effects.priming_active = true;
    bridge->effects.detection_threshold *= 0.8f;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Expected sequence primed: template %u", template_id);
    return 0;
}

int sequence_detector_fep_adjust_tolerance(
    sequence_detector_fep_bridge_t* bridge,
    float precision
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_precision_tolerance) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float tolerance = (precision > 0.7f) ? FEP_PRECISION_HIGH_TOLERANCE :
                      (precision > 0.3f) ? 0.5f : FEP_PRECISION_LOW_TOLERANCE;
    tolerance *= bridge->config.tolerance_sensitivity;

    bridge->state.current_precision = precision;
    bridge->effects.temporal_tolerance = SEQUENCE_TEMPORAL_TOLERANCE_MS * (1.0f / tolerance);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Tolerance adjusted: precision=%.3f → tolerance=%.3f",
                        precision, bridge->effects.temporal_tolerance);
    return 0;
}

int sequence_detector_fep_report_detection(
    sequence_detector_fep_bridge_t* bridge,
    const sequence_detection_t* detection
) {
    if (!bridge || !detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_fep_report_detection: required parameter is NULL (bridge, detection)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->state.sequences_detected++;
    bridge->stats.sequence_detections++;
    bridge->stats.avg_sequence_strength =
        (bridge->stats.avg_sequence_strength * (bridge->stats.sequence_detections - 1) +
         detection->strength) / bridge->stats.sequence_detections;

    /* Clear priming if expected sequence detected */
    if (bridge->effects.priming_active &&
        detection->template_id == bridge->effects.expected_template_id) {
        bridge->effects.priming_active = false;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Sequence detected: template %u, strength %.3f",
                        detection->template_id, detection->strength);
    return 0;
}

int sequence_detector_fep_report_violation(
    sequence_detector_fep_bridge_t* bridge,
    uint32_t expected_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_sequence_pe) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->state.violations_detected++;
    bridge->stats.sequence_violations++;

    float pe = FEP_SEQUENCE_VIOLATION_PE * bridge->config.pe_sensitivity;
    bridge->stats.avg_pe =
        (bridge->stats.avg_pe * (bridge->stats.sequence_violations - 1) + pe) /
        bridge->stats.sequence_violations;

    /* Clear priming */
    bridge->effects.priming_active = false;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Sequence violation: expected %u, PE=%.3f", expected_id, pe);
    return 0;
}

int sequence_detector_fep_report_replay(
    sequence_detector_fep_bridge_t* bridge,
    const sequence_detection_t* replay
) {
    if (!bridge || !replay) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_fep_report_replay: required parameter is NULL (bridge, replay)");
        return -1;
    }
    if (!bridge->config.enable_replay_consolidation) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->stats.replay_events++;

    /* Replay contributes to consolidation */
    float consolidation_increment = replay->strength * 0.1f;
    bridge->state.replay_consolidation_progress += consolidation_increment;
    bridge->state.replay_consolidation_progress = nimcp_clampf(
        bridge->state.replay_consolidation_progress, 0.0f, 1.0f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Replay detected: %s, compression=%.3f",
                        replay->is_forward ? "forward" : "backward",
                        replay->compression_factor);
    return 0;
}

int sequence_detector_fep_bridge_update(
    sequence_detector_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    return 0;
}

int sequence_detector_fep_bridge_get_state(
    const sequence_detector_fep_bridge_t* bridge,
    sequence_detector_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    *state = bridge->state;
    nimcp_platform_mutex_unlock((void*)bridge->base.mutex);
    return 0;
}

int sequence_detector_fep_bridge_get_stats(
    const sequence_detector_fep_bridge_t* bridge,
    sequence_detector_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock((void*)bridge->base.mutex);
    return 0;
}

int sequence_detector_fep_bridge_connect_bio_async(
    sequence_detector_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SEQUENCE_DETECTOR_BRIDGE,
        .module_name = "sequence_detector_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;
}

int sequence_detector_fep_bridge_disconnect_bio_async(
    sequence_detector_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool sequence_detector_fep_bridge_is_bio_async_connected(
    const sequence_detector_fep_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
