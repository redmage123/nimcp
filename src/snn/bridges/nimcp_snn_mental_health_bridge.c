/**
 * @file nimcp_snn_mental_health_bridge.c
 * @brief SNN-Mental Health integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_mental_health_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_mental_health_bridge)

//=============================================================================
// Bio-Async Module ID
//=============================================================================

#define BIO_MODULE_SNN_MENTAL_HEALTH_BRIDGE 0x0630

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Convenient starting point
 * HOW:  Clinical neuroscience parameter values
 */
void snn_mental_health_config_default(snn_mental_health_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_config_default: null config pointer");
        return;
    }

    /* Stability monitoring */
    config->stability_threshold = 0.6f;      /* 60% minimum stability */
    config->stability_window_ms = 1000.0f;   /* 1s window */

    /* Risk detection thresholds */
    config->risk_detection_sensitivity = 0.7f; /* 70% sensitivity */
    config->hypoactivity_threshold = 5.0f;   /* <5 Hz is hypoactive */
    config->hyperactivity_threshold = 100.0f; /* >100 Hz is hyperactive */
    config->hypersynchrony_threshold = 0.9f;  /* >90% synchrony */
    config->desynchrony_threshold = 0.2f;     /* <20% synchrony */
    config->instability_threshold = 0.5f;     /* CV > 0.5 is unstable */

    /* Intervention parameters */
    config->enable_auto_intervention = true;
    config->intervention_strength = 0.3f;     /* 30% modulation */
    config->intervention_duration_ms = 5000.0f; /* 5s interventions */

    /* Population mapping */
    config->monitor_population_id = 0;        /* Set by user */
    config->limbic_population_id = 0;
    config->prefrontal_population_id = 0;

    /* Update timing */
    config->update_interval_ms = 100.0f;      /* 10 Hz monitoring */

    /* Bio-async */
    config->enable_bio_async = false;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Create SNN-mental health bridge
 * WHY:  Initialize monitoring
 * HOW:  Allocate, validate, connect components
 */
snn_mental_health_bridge_t* snn_mental_health_bridge_create(
    const snn_mental_health_config_t* config,
    snn_network_t* snn
) {
    /* Guard: Validate inputs */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_bridge_create: snn is NULL");
        return NULL;
    }

    /* Allocate bridge */
    snn_mental_health_bridge_t* bridge = nimcp_malloc(sizeof(snn_mental_health_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_mental_health_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize structure */
    memset(bridge, 0, sizeof(snn_mental_health_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_mental_health") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->config = *config;

    /* Get populations */
    if (config->monitor_population_id > 0) {
        bridge->monitor_pop = snn_network_get_population(snn, config->monitor_population_id);
        if (!bridge->monitor_pop) {
            NIMCP_LOGGING_WARN("Monitor population ID %u not found", config->monitor_population_id);
        }
    }

    if (config->limbic_population_id > 0) {
        bridge->limbic_pop = snn_network_get_population(snn, config->limbic_population_id);
    }

    if (config->prefrontal_population_id > 0) {
        bridge->prefrontal_pop = snn_network_get_population(snn, config->prefrontal_population_id);
    }

    /* Initialize state */
    bridge->state.risk_level = SNN_MENTAL_HEALTH_RISK_NONE;
    bridge->state.dysregulation_type = SNN_DYSREGULATION_NONE;
    bridge->state.stability_index = 1.0f;
    bridge->last_update_time = 0.0f;

    NIMCP_LOGGING_INFO("Created SNN-mental health bridge");
    return bridge;
}

/**
 * WHAT: Destroy bridge and free resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect, free memory
 */
void snn_mental_health_bridge_destroy(snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_mental_health_bridge_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-mental health bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging
 * WHY:  Enable distributed coordination
 * HOW:  Register with router
 */
int snn_mental_health_bridge_connect_bio_async(snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_MENTAL_HEALTH_BRIDGE,
        .module_name = "snn_mental_health_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_mental_health_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int snn_mental_health_bridge_disconnect_bio_async(snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query before sending messages
 * HOW:  Return flag
 */
bool snn_mental_health_bridge_is_bio_async_connected(const snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * WHAT: Update bridge state
 * WHY:  Monitor mental health
 * HOW:  Compute stability, detect dysregulation, trigger interventions
 */
int snn_mental_health_bridge_update(snn_mental_health_bridge_t* bridge, float dt) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check if update needed based on interval */
    if (bridge->last_update_time > 0 &&
        (dt < bridge->config.update_interval_ms)) {
        return 0;  /* Skip update, too soon */
    }

    /* Note: Individual functions handle NULL populations gracefully.
     * Bridge operates in degraded mode if populations not configured. */

    /* Compute stability */
    bridge->state.stability_index = snn_mental_health_compute_stability(bridge);

    /* Detect dysregulation */
    bridge->state.dysregulation_type = snn_mental_health_detect_dysregulation(bridge);
    if (bridge->state.dysregulation_type != SNN_DYSREGULATION_NONE) {
        bridge->state.dysregulation_detections++;
    }

    /* Assess risk */
    bridge->state.risk_level = snn_mental_health_assess_risk(bridge);
    bridge->state.risk_score = snn_mental_health_compute_risk_score(bridge);

    /* Trigger intervention if needed */
    if (bridge->config.enable_auto_intervention &&
        !bridge->state.intervention_active &&
        bridge->state.risk_level >= SNN_MENTAL_HEALTH_RISK_MODERATE) {
        snn_mental_health_trigger_intervention(bridge, bridge->state.dysregulation_type);
    }

    /* Update intervention timer */
    if (bridge->state.intervention_active) {
        bridge->state.intervention_time_remaining -= dt;
        if (bridge->state.intervention_time_remaining <= 0.0f) {
            snn_mental_health_stop_intervention(bridge);
        }
    }

    /* Update statistics */
    bridge->state.update_count++;
    bridge->state.avg_stability =
        (bridge->state.avg_stability * (bridge->state.update_count - 1) +
         bridge->state.stability_index) / bridge->state.update_count;

    bridge->last_update_time += dt;
    return 0;
}

//=============================================================================
// Dysregulation Detection
//=============================================================================

/**
 * WHAT: Compute stability index
 * WHY:  Overall mental health metric
 * HOW:  Combine firing rate, synchrony, variability
 */
float snn_mental_health_compute_stability(snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_compute_stability: null bridge pointer");
        return 0.0f;
    }
    if (!bridge->monitor_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_mental_health_compute_stability: monitor population not configured");
        return 0.0f;
    }

    /* Get current metrics */
    float rate = snn_network_get_population_rate(
        bridge->snn,
        bridge->config.monitor_population_id,
        bridge->config.stability_window_ms
    );
    float synchrony = bridge->monitor_pop->population_synchrony;

    bridge->state.firing_rate = rate;
    bridge->state.synchrony = synchrony;

    /* Compute variability (CV approximation) */
    float cv = (synchrony < 0.5f) ? (1.0f - 2.0f * synchrony) : 0.0f;
    bridge->state.variability = cv;

    /* Stability is high when:
     * - Rate is in healthy range (10-50 Hz)
     * - Synchrony is moderate (0.3-0.7)
     * - Variability is low (CV < 0.3)
     */
    float rate_score = 1.0f;
    if (rate < 10.0f || rate > 50.0f) {
        rate_score = 0.5f;
    }
    if (rate < 5.0f || rate > 100.0f) {
        rate_score = 0.0f;
    }

    float sync_score = 1.0f;
    if (synchrony < 0.2f || synchrony > 0.9f) {
        sync_score = 0.5f;
    }

    float var_score = 1.0f;
    if (cv > 0.3f) {
        var_score = 0.5f;
    }
    if (cv > 0.5f) {
        var_score = 0.0f;
    }

    /* Combined stability */
    float stability = (rate_score + sync_score + var_score) / 3.0f;
    return stability;
}

/**
 * WHAT: Detect dysregulation type
 * WHY:  Classify abnormal patterns
 * HOW:  Compare to thresholds
 */
snn_dysregulation_type_t snn_mental_health_detect_dysregulation(
    snn_mental_health_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_detect_dysregulation: null bridge pointer");
        return SNN_DYSREGULATION_NONE;
    }

    float rate = bridge->state.firing_rate;
    float synchrony = bridge->state.synchrony;
    float cv = bridge->state.variability;

    /* Check each dysregulation type */
    if (rate < bridge->config.hypoactivity_threshold) {
        return SNN_DYSREGULATION_HYPOACTIVITY;
    }

    if (rate > bridge->config.hyperactivity_threshold) {
        return SNN_DYSREGULATION_HYPERACTIVITY;
    }

    if (synchrony > bridge->config.hypersynchrony_threshold) {
        return SNN_DYSREGULATION_HYPERSYNCHRONY;
    }

    if (synchrony < bridge->config.desynchrony_threshold) {
        return SNN_DYSREGULATION_DESYNCHRONY;
    }

    if (cv > bridge->config.instability_threshold) {
        return SNN_DYSREGULATION_INSTABILITY;
    }

    return SNN_DYSREGULATION_NONE;
}

/**
 * WHAT: Assess risk level
 * WHY:  Guide intervention
 * HOW:  Map stability to risk levels
 */
snn_mental_health_risk_t snn_mental_health_assess_risk(
    snn_mental_health_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_assess_risk: null bridge pointer");
        return SNN_MENTAL_HEALTH_RISK_NONE;
    }

    float stability = bridge->state.stability_index;

    /* Map stability to risk */
    if (stability >= 0.8f) {
        return SNN_MENTAL_HEALTH_RISK_NONE;
    } else if (stability >= 0.6f) {
        return SNN_MENTAL_HEALTH_RISK_LOW;
    } else if (stability >= 0.4f) {
        return SNN_MENTAL_HEALTH_RISK_MODERATE;
    } else if (stability >= 0.2f) {
        return SNN_MENTAL_HEALTH_RISK_HIGH;
    } else {
        return SNN_MENTAL_HEALTH_RISK_CRITICAL;
    }
}

/**
 * WHAT: Compute risk score
 * WHY:  Continuous risk measure
 * HOW:  Weighted combination of deviations
 */
float snn_mental_health_compute_risk_score(snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_compute_risk_score: null bridge pointer");
        return 0.0f;
    }

    /* Risk is inverse of stability */
    float risk = 1.0f - bridge->state.stability_index;

    /* Apply sensitivity factor */
    risk *= bridge->config.risk_detection_sensitivity;

    /* Clamp to [0, 1] */
    if (risk < 0.0f) risk = 0.0f;
    if (risk > 1.0f) risk = 1.0f;

    return risk;
}

//=============================================================================
// Intervention Functions
//=============================================================================

/**
 * WHAT: Trigger intervention
 * WHY:  Normalize dysregulated dynamics
 * HOW:  Modulate population parameters
 */
int snn_mental_health_trigger_intervention(
    snn_mental_health_bridge_t* bridge,
    snn_dysregulation_type_t dysregulation_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_trigger_intervention: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Mark intervention as active */
    bridge->state.intervention_active = true;
    bridge->state.intervention_time_remaining = bridge->config.intervention_duration_ms;
    bridge->state.intervention_count++;

    NIMCP_LOGGING_INFO("Triggered mental health intervention for dysregulation type %d",
                       dysregulation_type);

    /* Note: Actual population modulation would be implemented here */
    /* This requires access to neuron parameters and network dynamics */
    /* Intervention strategies:
     * - Hypoactivity: Increase excitability
     * - Hyperactivity: Increase inhibition
     * - Hypersynchrony: Desynchronize via noise
     * - Desynchrony: Enhance coupling
     * - Instability: Stabilize via homeostatic mechanisms
     */

    return 0;
}

/**
 * WHAT: Stop intervention
 * WHY:  Return to baseline
 * HOW:  Reset modulation
 */
int snn_mental_health_stop_intervention(snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_stop_intervention: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->state.intervention_active = false;
    bridge->state.intervention_time_remaining = 0.0f;

    NIMCP_LOGGING_INFO("Stopped mental health intervention");
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get stability index
 * WHY:  External monitoring
 * HOW:  Return cached value
 */
float snn_mental_health_get_stability(const snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_get_stability: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.stability_index;
}

/**
 * WHAT: Get risk level
 * WHY:  Query current risk
 * HOW:  Return cached level
 */
snn_mental_health_risk_t snn_mental_health_get_risk_level(const snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_get_risk_level: null bridge pointer");
        return SNN_MENTAL_HEALTH_RISK_NONE;
    }
    return bridge->state.risk_level;
}

/**
 * WHAT: Get risk score
 * WHY:  Continuous risk monitoring
 * HOW:  Return cached score
 */
float snn_mental_health_get_risk_score(const snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_get_risk_score: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.risk_score;
}

/**
 * WHAT: Get dysregulation type
 * WHY:  Identify specific abnormality
 * HOW:  Return cached type
 */
snn_dysregulation_type_t snn_mental_health_get_dysregulation_type(
    const snn_mental_health_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_get_dysregulation_type: null bridge pointer");
        return SNN_DYSREGULATION_NONE;
    }
    return bridge->state.dysregulation_type;
}

/**
 * WHAT: Check intervention status
 * WHY:  Monitor active interventions
 * HOW:  Return flag
 */
bool snn_mental_health_is_intervention_active(const snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.intervention_active;
}

/**
 * WHAT: Get bridge state
 * WHY:  External monitoring
 * HOW:  Copy state structure
 */
int snn_mental_health_bridge_get_state(
    const snn_mental_health_bridge_t* bridge,
    snn_mental_health_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * WHAT: Get intervention statistics
 * WHY:  Monitor system performance
 * HOW:  Return computed metrics
 */
int snn_mental_health_get_stats(
    const snn_mental_health_bridge_t* bridge,
    uint32_t* intervention_count,
    uint32_t* dysregulation_count,
    float* avg_stability
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    if (intervention_count) *intervention_count = bridge->state.intervention_count;
    if (dysregulation_count) *dysregulation_count = bridge->state.dysregulation_detections;
    if (avg_stability) *avg_stability = bridge->state.avg_stability;

    return 0;
}

/**
 * WHAT: Reset statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero counters
 */
void snn_mental_health_reset_stats(snn_mental_health_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_mental_health_reset_stats: null bridge pointer");
        return;
    }

    bridge->state.update_count = 0;
    bridge->state.intervention_count = 0;
    bridge->state.dysregulation_detections = 0;
    bridge->state.avg_stability = 0.0f;
}
