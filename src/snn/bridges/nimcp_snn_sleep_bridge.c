/**
 * @file nimcp_snn_sleep_bridge.c
 * @brief SNN-Sleep integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_sleep_bridge)

//=============================================================================
// Bio-Async Module ID
//=============================================================================

#define BIO_MODULE_SNN_SLEEP_BRIDGE 0x062F

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Convenient starting point
 * HOW:  Literature-based parameter values
 */
void snn_sleep_config_default(snn_sleep_config_t* config) {
    if (!config) return;

    /* Spindle detection (12-15 Hz) */
    config->spindle_frequency = 13.5f;       /* Center of spindle band */
    config->spindle_bandwidth = 3.0f;        /* 12-15 Hz range */
    config->spindle_min_duration_ms = 500.0f; /* Min 0.5s duration */
    config->spindle_power_threshold = 0.6f;  /* 60% power threshold */

    /* Slow wave detection (<1 Hz) */
    config->slow_wave_max_freq = 1.0f;       /* Max 1 Hz */
    config->slow_wave_threshold = 0.7f;      /* Amplitude threshold */
    config->slow_wave_min_duration_ms = 1000.0f; /* Min 1s duration */

    /* REM detection */
    config->rem_density = 0.5f;              /* Moderate spike density */
    config->rem_variability_threshold = 0.4f; /* High CV for REM */
    config->rem_min_duration_ms = 5000.0f;   /* Min 5s REM */

    /* Consolidation */
    config->enable_replay = true;
    config->replay_speed_factor = 10.0f;     /* 10x speed replay */
    config->consolidation_strength = 1.2f;   /* 20% strengthening */

    /* Population mapping */
    config->cortical_population_id = 0;      /* Set by user */

    /* Update timing */
    config->update_interval_ms = 100.0f;     /* 10 Hz update */

    /* Bio-async */
    config->enable_bio_async = false;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Create SNN-sleep bridge
 * WHY:  Initialize bidirectional integration
 * HOW:  Allocate, validate, connect components
 */
snn_sleep_bridge_t* snn_sleep_bridge_create(
    const snn_sleep_config_t* config,
    snn_network_t* snn
) {
    /* Guard: Validate inputs */
    if (!config || !snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null parameters to snn_sleep_bridge_create: config=%p, snn=%p",
                             (void*)config, (void*)snn);
        return NULL;
    }

    /* Allocate bridge */
    snn_sleep_bridge_t* bridge = nimcp_malloc(sizeof(snn_sleep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_sleep_bridge_t),
                          "Failed to allocate SNN-sleep bridge");
        return NULL;
    }

    /* Initialize structure */
    memset(bridge, 0, sizeof(snn_sleep_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_sleep") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->config = *config;

    /* Get cortical population */
    if (config->cortical_population_id > 0) {
        bridge->cortical_pop = snn_network_get_population(snn, config->cortical_population_id);
        if (!bridge->cortical_pop) {
            NIMCP_LOGGING_WARN("Cortical population ID %u not found", config->cortical_population_id);
        }
    }

    /* Initialize state */
    bridge->state.sleep_stage = SNN_SLEEP_WAKE;
    bridge->last_update_time = 0.0f;
    bridge->total_time = 0.0f;

    NIMCP_LOGGING_INFO("Created SNN-sleep bridge");
    return bridge;
}

/**
 * WHAT: Destroy bridge and free resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect, free memory
 */
void snn_sleep_bridge_destroy(snn_sleep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_sleep_bridge_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-sleep bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging
 * WHY:  Enable distributed coordination
 * HOW:  Register with router
 */
int snn_sleep_bridge_connect_bio_async(snn_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_SLEEP_BRIDGE,
        .module_name = "snn_sleep_bridge",
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
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_sleep_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int snn_sleep_bridge_disconnect_bio_async(snn_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
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
bool snn_sleep_bridge_is_bio_async_connected(const snn_sleep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * WHAT: Update bridge state
 * WHY:  Detect sleep patterns
 * HOW:  Analyze population, classify stage
 */
int snn_sleep_bridge_update(snn_sleep_bridge_t* bridge, float dt) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null bridge in snn_sleep_bridge_update");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check if update needed based on interval */
    if (bridge->last_update_time > 0 &&
        (dt < bridge->config.update_interval_ms)) {
        return 0;  /* Skip update, too soon */
    }

    if (!bridge->cortical_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "snn_sleep_bridge_update: cortical population not configured");
        return SNN_ERROR_INVALID_STATE;
    }

    /* Detect sleep patterns */
    bridge->state.spindle_active = snn_sleep_detect_spindle(bridge, bridge->cortical_pop);
    if (bridge->state.spindle_active) {
        bridge->state.spindle_count++;
    }

    bridge->state.slow_wave_active = snn_sleep_detect_slow_wave(bridge, bridge->cortical_pop);
    if (bridge->state.slow_wave_active) {
        bridge->state.slow_wave_count++;
    }

    bool rem_detected = snn_sleep_detect_rem(bridge, bridge->cortical_pop);
    if (rem_detected) {
        bridge->state.rem_activity = 1.0f;
    } else {
        bridge->state.rem_activity *= 0.95f;  /* Decay */
    }

    /* Classify sleep stage */
    snn_sleep_stage_t prev_stage = bridge->state.sleep_stage;
    bridge->state.sleep_stage = snn_sleep_classify_stage(bridge);

    /* Update stage duration */
    if (bridge->state.sleep_stage == prev_stage) {
        bridge->state.stage_duration_ms += dt;
    } else {
        bridge->state.stage_duration_ms = 0.0f;
    }

    /* Update time in stage statistics */
    if (bridge->state.sleep_stage < 6) {
        bridge->state.time_in_stage_ms[bridge->state.sleep_stage] += dt;
    }

    /* Trigger consolidation during NREM2/3 */
    if (bridge->state.sleep_stage == SNN_SLEEP_NREM2 ||
        bridge->state.sleep_stage == SNN_SLEEP_NREM3) {
        snn_sleep_consolidate_memory(bridge);
    }

    bridge->state.update_count++;
    bridge->last_update_time += dt;
    bridge->total_time += dt;

    return 0;
}

//=============================================================================
// Sleep Pattern Detection
//=============================================================================

/**
 * WHAT: Detect sleep spindles
 * WHY:  Spindles mark stage 2 sleep
 * HOW:  Check population synchrony in spindle band
 */
bool snn_sleep_detect_spindle(
    snn_sleep_bridge_t* bridge,
    snn_population_t* population
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_detect_spindle: bridge is NULL");
        return false;
    }
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_detect_spindle: population is NULL");
        return false;
    }

    /* Simplified: Check if population synchrony is high */
    float synchrony = population->population_synchrony;
    float power = synchrony * synchrony;  /* Power approximation */

    bridge->state.spindle_power = power;

    /* Detect spindle if power exceeds threshold */
    return power >= bridge->config.spindle_power_threshold;
}

/**
 * WHAT: Detect slow waves
 * WHY:  Slow waves mark deep sleep
 * HOW:  Check for low frequency, high amplitude oscillations
 */
bool snn_sleep_detect_slow_wave(
    snn_sleep_bridge_t* bridge,
    snn_population_t* population
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_detect_slow_wave: bridge is NULL");
        return false;
    }
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_detect_slow_wave: population is NULL");
        return false;
    }

    /* Simplified: Check if synchrony is very high (down state) */
    float synchrony = population->population_synchrony;
    bridge->state.slow_wave_power = synchrony;

    /* Slow wave if very high synchrony */
    return synchrony >= bridge->config.slow_wave_threshold;
}

/**
 * WHAT: Detect REM sleep
 * WHY:  REM shows desynchronized, variable activity
 * HOW:  High spike variability, low synchrony
 */
bool snn_sleep_detect_rem(
    snn_sleep_bridge_t* bridge,
    snn_population_t* population
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_detect_rem: bridge is NULL");
        return false;
    }
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_detect_rem: population is NULL");
        return false;
    }

    /* Compute spike variability (coefficient of variation) */
    float rate = snn_network_get_population_rate(
        bridge->snn,
        bridge->config.cortical_population_id,
        bridge->config.update_interval_ms
    );

    /* Simplified CV approximation */
    float synchrony = population->population_synchrony;
    float cv = (1.0f - synchrony) * 0.5f;  /* Low synchrony = high CV */

    bridge->state.spike_variability = cv;

    /* REM if high variability and moderate rate */
    return (cv >= bridge->config.rem_variability_threshold &&
            rate >= 10.0f && rate <= 50.0f);
}

/**
 * WHAT: Classify sleep stage
 * WHY:  Integrate multiple features
 * HOW:  Decision tree
 */
snn_sleep_stage_t snn_sleep_classify_stage(snn_sleep_bridge_t* bridge) {
    if (!bridge) return SNN_SLEEP_UNKNOWN;

    /* REM: High variability, desynchronized */
    if (bridge->state.rem_activity > 0.5f) {
        return SNN_SLEEP_REM;
    }

    /* NREM3: Slow waves present */
    if (bridge->state.slow_wave_active) {
        return SNN_SLEEP_NREM3;
    }

    /* NREM2: Spindles present */
    if (bridge->state.spindle_active) {
        return SNN_SLEEP_NREM2;
    }

    /* NREM1: Low activity, some theta */
    float rate = snn_network_get_population_rate(
        bridge->snn,
        bridge->config.cortical_population_id,
        bridge->config.update_interval_ms
    );
    if (rate < 20.0f) {
        return SNN_SLEEP_NREM1;
    }

    /* Default: Wake */
    return SNN_SLEEP_WAKE;
}

//=============================================================================
// Memory Consolidation
//=============================================================================

/**
 * WHAT: Enhance synaptic weights during sleep
 * WHY:  Sleep consolidates memories
 * HOW:  Strengthen recently active synapses
 */
int snn_sleep_consolidate_memory(snn_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_consolidate_memory: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Update consolidation progress */
    float increment = 0.01f;  /* 1% per update */
    bridge->state.consolidation_progress += increment;
    if (bridge->state.consolidation_progress > 1.0f) {
        bridge->state.consolidation_progress = 1.0f;
    }

    /* Note: Actual synaptic strengthening would be implemented here */
    /* This requires access to synapse structures and weight modification */

    return 0;
}

/**
 * WHAT: Replay recent spike sequences
 * WHY:  Replay consolidates episodic memories
 * HOW:  Replay at accelerated speed
 */
int snn_sleep_replay_sequence(
    snn_sleep_bridge_t* bridge,
    uint32_t sequence_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_replay_sequence: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_replay) {
        return 0;  /* Replay disabled */
    }

    /* Update replay count */
    bridge->state.replay_count++;

    /* Note: Actual spike replay would be implemented here */
    /* This requires sequence storage and replay mechanisms */

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get current sleep stage
 * WHY:  External monitoring
 * HOW:  Return cached stage
 */
snn_sleep_stage_t snn_sleep_get_stage(const snn_sleep_bridge_t* bridge) {
    return bridge ? bridge->state.sleep_stage : SNN_SLEEP_UNKNOWN;
}

/**
 * WHAT: Get stage duration
 * WHY:  Monitor stage stability
 * HOW:  Return accumulated time
 */
float snn_sleep_get_stage_duration(const snn_sleep_bridge_t* bridge) {
    return bridge ? bridge->state.stage_duration_ms : 0.0f;
}

/**
 * WHAT: Get spindle count
 * WHY:  Monitor NREM2 quality
 * HOW:  Return counter
 */
uint32_t snn_sleep_get_spindle_count(const snn_sleep_bridge_t* bridge) {
    return bridge ? bridge->state.spindle_count : 0;
}

/**
 * WHAT: Get slow wave count
 * WHY:  Monitor deep sleep quality
 * HOW:  Return counter
 */
uint32_t snn_sleep_get_slow_wave_count(const snn_sleep_bridge_t* bridge) {
    return bridge ? bridge->state.slow_wave_count : 0;
}

/**
 * WHAT: Get REM activity
 * WHY:  Monitor REM intensity
 * HOW:  Return activity index
 */
float snn_sleep_get_rem_activity(const snn_sleep_bridge_t* bridge) {
    return bridge ? bridge->state.rem_activity : 0.0f;
}

/**
 * WHAT: Get bridge state
 * WHY:  External monitoring
 * HOW:  Copy state structure
 */
int snn_sleep_bridge_get_state(
    const snn_sleep_bridge_t* bridge,
    snn_sleep_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * WHAT: Get sleep architecture statistics
 * WHY:  Analyze sleep quality
 * HOW:  Return time in each stage
 */
int snn_sleep_get_architecture(
    const snn_sleep_bridge_t* bridge,
    float* total_time,
    float* time_in_stage
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_sleep_get_architecture: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    if (total_time) {
        *total_time = bridge->total_time;
    }

    if (time_in_stage) {
        memcpy(time_in_stage, bridge->state.time_in_stage_ms, 6 * sizeof(float));
    }

    return 0;
}

/**
 * WHAT: Reset statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero counters
 */
void snn_sleep_reset_stats(snn_sleep_bridge_t* bridge) {
    if (!bridge) return;

    bridge->state.update_count = 0;
    bridge->state.spindle_count = 0;
    bridge->state.slow_wave_count = 0;
    bridge->state.replay_count = 0;
    bridge->state.consolidation_progress = 0.0f;
    bridge->total_time = 0.0f;
    memset(bridge->state.time_in_stage_ms, 0, 6 * sizeof(float));
}
