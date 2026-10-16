/**
 * @file nimcp_security_memory_fep_bridge.c
 * @brief Implementation of Security Memory FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for memory system security
 * WHY:  Memory anomalies are high-surprise events in FEP framework
 * HOW:  Map memory security metrics to free energy, use prediction errors for detection
 *
 * ATTACK DETECTION MECHANISMS:
 * - Memory Corruption: High FE from integrity deviation
 * - False Memory Injection: High surprise from unexpected content
 * - Replay Attacks: Temporal prediction errors
 * - Retrieval Manipulation: Access pattern anomalies
 */

#include "security/memory/nimcp_security_memory_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_memory_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_free_energy_from_metrics(
    float integrity_score,
    float content_score,
    float temporal_score,
    float retrieval_score,
    const sec_mem_fep_config_t* config
);

static float compute_surprise_from_anomaly(
    float observed_value,
    float expected_value
);

static float compute_memory_type_weight(
    security_mem_system_type_t memory_type,
    const sec_mem_fep_config_t* config
);

static sec_mem_fep_threat_level_t classify_threat_level(
    float free_energy,
    const sec_mem_fep_config_t* config
);

static sec_mem_fep_attack_type_t determine_attack_type(
    float corruption_fe,
    float injection_fe,
    float replay_fe,
    float retrieval_fe
);

static sec_mem_fep_response_t determine_response(
    sec_mem_fep_threat_level_t threat,
    sec_mem_fep_attack_type_t attack,
    float urgency
);

static void update_running_averages(
    sec_mem_fep_bridge_t* bridge,
    float free_energy,
    float surprise,
    float pred_error
);

static void update_memory_type_stats(
    sec_mem_fep_bridge_t* bridge,
    security_mem_system_type_t memory_type
);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for security memory FEP bridge
 * WHY:  Provide sensible starting point for most deployments
 * HOW:  Set biologically-plausible defaults for all parameters
 */
int sec_mem_fep_default_config(sec_mem_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->free_energy_threshold = SEC_MEM_FEP_ATTACK_THRESHOLD;
    config->surprise_threshold = SEC_MEM_FEP_SURPRISE_ANOMALY;
    config->precision_learning_rate = 0.05f;

    /* Detection parameters */
    config->use_fep_detection = true;
    config->enable_precision_modulation = true;
    config->normal_fe_threshold = SEC_MEM_FEP_NORMAL_THRESHOLD;
    config->critical_fe_threshold = SEC_MEM_FEP_CRITICAL_THRESHOLD;

    /* Attack-specific detection */
    config->detect_corruption = true;
    config->detect_false_injection = true;
    config->detect_replay_attacks = true;
    config->detect_retrieval_manipulation = true;

    /* Memory integrity mapping */
    config->integrity_to_fe_scale = 10.0f;
    config->access_pattern_pe_weight = 0.25f;
    config->content_surprise_weight = 0.35f;
    config->temporal_pe_weight = 0.20f;

    /* Per-memory system weights */
    config->working_memory_weight = 1.0f;
    config->episodic_memory_weight = 1.2f;   /* Episodic slightly more sensitive */
    config->semantic_memory_weight = 0.9f;
    config->procedural_memory_weight = 0.8f;

    /* Active inference settings */
    config->enable_active_inference = true;
    config->response_threshold = SEC_MEM_FEP_SUSPICIOUS_THRESHOLD;
    config->action_temperature = 1.0f;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;
    config->learn_from_false_positives = true;

    /* Bio-async integration */
    config->enable_bio_async = true;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create security memory FEP bridge
 * WHY:  Initialize FEP integration for memory security detection
 * HOW:  Allocate structure, initialize base, apply configuration
 */
sec_mem_fep_bridge_t* sec_mem_fep_create(
    const sec_mem_fep_config_t* config,
    security_mem_bridge_t* security_mem,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!security_mem || !fep_system) {
        NIMCP_LOGGING_ERROR("Security Memory FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_create: required parameter is NULL (security_mem, fep_system)");
        return NULL;
    }

    /* Allocate bridge structure */
    sec_mem_fep_bridge_t* bridge = (sec_mem_fep_bridge_t*)nimcp_malloc(
        sizeof(sec_mem_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Security Memory FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_mem_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(sec_mem_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sec_mem_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->security_mem = security_mem;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "security_memory_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Security Memory FEP bridge: mutex creation failed");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = SEC_MEM_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_threat = SEC_MEM_FEP_THREAT_NONE;
    bridge->state.last_attack = SEC_MEM_FEP_ATTACK_NONE;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();

    /* Initialize FEP effects */
    bridge->fep_effects.threat_level = SEC_MEM_FEP_THREAT_NONE;
    bridge->fep_effects.attack_type = SEC_MEM_FEP_ATTACK_NONE;
    bridge->fep_effects.detection_sensitivity = SEC_MEM_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.integrity_estimate = 1.0f;
    bridge->fep_effects.recommended_response = SEC_MEM_FEP_RESPONSE_NONE;

    /* Initialize security effects */
    bridge->security_effects.avg_integrity_score = 1.0f;
    bridge->security_effects.access_pattern_health = 1.0f;
    bridge->security_effects.content_validity = 1.0f;
    bridge->security_effects.temporal_consistency = 1.0f;
    bridge->security_effects.working_mem_health = 1.0f;
    bridge->security_effects.episodic_mem_health = 1.0f;
    bridge->security_effects.semantic_mem_health = 1.0f;
    bridge->security_effects.procedural_mem_health = 1.0f;

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_SECURITY_MEMORY_FEP;
    bridge->base.module_name = "sec_mem_fep_bridge";

    NIMCP_LOGGING_INFO("Security Memory FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy security memory FEP bridge
 * WHY:  Clean up all resources to prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void sec_mem_fep_destroy(sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sec_mem_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    /* Free bridge memory */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Security Memory FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections and config
 */
int sec_mem_fep_reset(sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = SEC_MEM_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_threat = SEC_MEM_FEP_THREAT_NONE;
    bridge->state.last_attack = SEC_MEM_FEP_ATTACK_NONE;
    bridge->state.last_threat_time = 0;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();

    /* Reset FEP effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_sec_mem_effects_t));
    bridge->fep_effects.integrity_estimate = 1.0f;
    bridge->fep_effects.detection_sensitivity = SEC_MEM_FEP_DEFAULT_PRECISION;

    /* Reset security effects */
    memset(&bridge->security_effects, 0, sizeof(sec_mem_to_fep_effects_t));
    bridge->security_effects.avg_integrity_score = 1.0f;
    bridge->security_effects.access_pattern_health = 1.0f;
    bridge->security_effects.content_validity = 1.0f;
    bridge->security_effects.temporal_consistency = 1.0f;
    bridge->security_effects.working_mem_health = 1.0f;
    bridge->security_effects.episodic_mem_health = 1.0f;
    bridge->security_effects.semantic_mem_health = 1.0f;
    bridge->security_effects.procedural_mem_health = 1.0f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_mem_fep_stats_t));
    bridge->stats.current_precision = SEC_MEM_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Security Memory FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge configuration
 * WHY:  Allow inspection of current settings
 * HOW:  Copy configuration to output structure
 */
int sec_mem_fep_get_config(
    const sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

/**
 * WHAT: Set bridge configuration
 * WHY:  Allow runtime tuning of detection parameters
 * HOW:  Validate and copy new configuration
 */
int sec_mem_fep_set_config(
    sec_mem_fep_bridge_t* bridge,
    const sec_mem_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Validate critical parameters */
    if (config->free_energy_threshold <= 0.0f ||
        config->surprise_threshold <= 0.0f) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_mem_fep_set_config: operation failed");
        return -1;
    }

    bridge->config = *config;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

/**
 * WHAT: Compute FEP effects on security
 * WHY:  Derive threat detection metrics from FEP state
 * HOW:  Query FEP system for free energy, surprise, prediction error
 */
int sec_mem_fep_compute_effects(sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->state.active) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current FEP metrics */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.surprise_level = surprise;
    bridge->fep_effects.prediction_error = pred_error;

    /* Classify threat level based on free energy */
    bridge->fep_effects.threat_level = classify_threat_level(
        current_fe, &bridge->config
    );

    /* Compute threat confidence based on precision and stability */
    float confidence = 1.0f - (pred_error / 10.0f);
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;
    bridge->fep_effects.threat_confidence = confidence * bridge->state.current_precision;

    /* Compute detection sensitivity from precision */
    bridge->fep_effects.detection_sensitivity = bridge->state.current_precision;

    /* Estimate memory integrity from FEP (inverted relationship) */
    float integrity_estimate = 1.0f - (current_fe / bridge->config.integrity_to_fe_scale);
    if (integrity_estimate < 0.0f) integrity_estimate = 0.0f;
    if (integrity_estimate > 1.0f) integrity_estimate = 1.0f;
    bridge->fep_effects.integrity_estimate = integrity_estimate;

    /* Determine recommended response */
    float urgency = current_fe / bridge->config.critical_fe_threshold;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.threat_level,
        bridge->fep_effects.attack_type,
        urgency
    );

    /* Update statistics (avg_free_energy maintained by update_running_averages) */
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Update FEP from security detection results
 * WHY:  Feed security observations back to update generative model
 * HOW:  Convert detection to FEP observation, update beliefs and precision
 */
int sec_mem_fep_update_from_detection(
    sec_mem_fep_bridge_t* bridge,
    bool attack_detected,
    sec_mem_fep_attack_type_t attack_type,
    float integrity_score,
    security_mem_system_type_t memory_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update security effects */
    if (attack_detected) {
        bridge->security_effects.attacks_detected++;
        bridge->security_effects.under_attack = true;
        bridge->security_effects.last_attack_time = nimcp_platform_time_monotonic_ms();
        bridge->stats.threats_detected++;

        /* Update per-attack type counts */
        switch (attack_type) {
            case SEC_MEM_FEP_ATTACK_CORRUPTION:
                bridge->security_effects.corruption_attacks++;
                bridge->stats.corruption_detections++;
                break;
            case SEC_MEM_FEP_ATTACK_FALSE_INJECTION:
                bridge->security_effects.injection_attacks++;
                bridge->stats.injection_detections++;
                break;
            case SEC_MEM_FEP_ATTACK_REPLAY:
                bridge->security_effects.replay_attacks++;
                bridge->stats.replay_detections++;
                break;
            case SEC_MEM_FEP_ATTACK_RETRIEVAL_MANIP:
                bridge->security_effects.retrieval_attacks++;
                bridge->stats.retrieval_detections++;
                break;
            case SEC_MEM_FEP_ATTACK_MULTIPLE:
                /* Count as multiple */
                bridge->stats.corruption_detections++;
                bridge->stats.injection_detections++;
                break;
            default:
                break;
        }
    } else {
        bridge->security_effects.normal_operations++;
        bridge->security_effects.under_attack = false;
    }

    /* Update average integrity score (exponential moving average) */
    bridge->security_effects.avg_integrity_score =
        0.9f * bridge->security_effects.avg_integrity_score +
        0.1f * integrity_score;

    /* Update per-memory system health */
    float* mem_health = NULL;
    switch (memory_type) {
        case SEC_MEM_TYPE_WORKING:
            mem_health = &bridge->security_effects.working_mem_health;
            break;
        case SEC_MEM_TYPE_EPISODIC:
            mem_health = &bridge->security_effects.episodic_mem_health;
            break;
        case SEC_MEM_TYPE_SEMANTIC:
            mem_health = &bridge->security_effects.semantic_mem_health;
            break;
        case SEC_MEM_TYPE_PROCEDURAL:
            mem_health = &bridge->security_effects.procedural_mem_health;
            break;
        default:
            break;
    }
    if (mem_health) {
        *mem_health = 0.9f * (*mem_health) + 0.1f * integrity_score;
    }

    /* Update memory type statistics */
    update_memory_type_stats(bridge, memory_type);

    /* Compute current threat level */
    bridge->security_effects.current_threat_level =
        1.0f - bridge->security_effects.avg_integrity_score;

    /* Update FEP system if online learning enabled */
    if (bridge->config.enable_online_learning) {
        if (attack_detected) {
            /*
             * Attack detected = high-surprise observation
             * Increase precision for better detection
             */
            fep_update_precision(bridge->fep_system);

            /* Create observation vector representing attack */
            float observation[16];
            for (int i = 0; i < 16; i++) {
                observation[i] = (1.0f - integrity_score) * (1.0f + (float)i * 0.1f);
            }
            fep_process_observation(bridge->fep_system, observation, 16);
        } else {
            /*
             * Normal operation = update generative model
             * This refines expectations for normal memory behavior
             */
            fep_update_beliefs(bridge->fep_system);
        }
    }

    bridge->state.detection_count++;

    /* Track last threat */
    if (attack_detected) {
        bridge->state.last_threat = bridge->fep_effects.threat_level;
        bridge->state.last_attack = attack_type;
        bridge->state.last_threat_time = nimcp_platform_time_monotonic_ms();
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Main update loop for bridge synchronization
 * WHY:  Maintain bidirectional effects between security and FEP
 * HOW:  Compute effects, apply precision modulation, update state
 */
int sec_mem_fep_update(sec_mem_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Suppress unused parameter warning */
    (void)delta_ms;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update timestamp */
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Compute FEP effects on security */
    int result = sec_mem_fep_compute_effects(bridge);
    if (result != 0) {
        return result;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Process bio-async messages if connected */
    if (bridge->base.bio_async_enabled && bridge->base.bio_ctx) {
        bio_router_process_inbox(bridge->base.bio_ctx, 0);  /* Process all pending */
    }

    /* Apply precision modulation if enabled */
    if (bridge->config.enable_precision_modulation) {
        /*
         * Adapt precision based on detection performance
         * High attack rate -> increase precision (more sensitive)
         * Low attack rate -> decrease precision (fewer false positives)
         */
        float attack_rate = 0.0f;
        if (bridge->state.detection_count > 0) {
            attack_rate = (float)bridge->security_effects.attacks_detected /
                         (float)bridge->state.detection_count;
        }

        float target_precision = SEC_MEM_FEP_DEFAULT_PRECISION;
        if (attack_rate > 0.2f) {
            target_precision = SEC_MEM_FEP_MAX_PRECISION;
        } else if (attack_rate < 0.05f && bridge->state.detection_count > 10) {
            target_precision = SEC_MEM_FEP_MIN_PRECISION + 0.5f;
        }

        /* Smooth adaptation */
        float alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - alpha) * bridge->state.current_precision +
            alpha * target_precision;

        /* Clamp precision */
        if (bridge->state.current_precision < SEC_MEM_FEP_MIN_PRECISION) {
            bridge->state.current_precision = SEC_MEM_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > SEC_MEM_FEP_MAX_PRECISION) {
            bridge->state.current_precision = SEC_MEM_FEP_MAX_PRECISION;
        }

        bridge->stats.precision_adaptations++;
    }

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Attack Detection Implementation
 * ============================================================================ */

/**
 * WHAT: Detect memory corruption attack
 * WHY:  Corruption produces high free energy from integrity deviation
 * HOW:  Compare current integrity to expected, compute prediction error
 */
int sec_mem_fep_detect_corruption(
    sec_mem_fep_bridge_t* bridge,
    float integrity_score,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    float* confidence_out
) {
    if (!bridge || !threat_level_out || !confidence_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_detect_corruption: required parameter is NULL (bridge, threat_level_out, confidence_out)");
        return -1;
    }

    if (!bridge->config.detect_corruption) {
        *threat_level_out = SEC_MEM_FEP_THREAT_NONE;
        *confidence_out = 0.0f;
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute free energy from integrity deviation */
    float expected_integrity = bridge->security_effects.avg_integrity_score;
    float integrity_deviation = expected_integrity - integrity_score;
    if (integrity_deviation < 0.0f) integrity_deviation = 0.0f;

    /* Apply memory type weight */
    float weight = compute_memory_type_weight(memory_type, &bridge->config);

    /* Compute corruption free energy */
    float corruption_fe = bridge->config.integrity_to_fe_scale *
                         integrity_deviation * weight *
                         SEC_MEM_FEP_CORRUPTION_WEIGHT;

    /* Classify threat */
    sec_mem_fep_threat_level_t threat = classify_threat_level(
        corruption_fe, &bridge->config
    );

    /* Compute confidence */
    float confidence = bridge->state.current_precision *
                      (1.0f - integrity_score) * weight;
    if (confidence > 1.0f) confidence = 1.0f;

    *threat_level_out = threat;
    *confidence_out = confidence;

    /* Update effects */
    bridge->fep_effects.corruption_confidence = confidence;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Detect false memory injection attack
 * WHY:  Injected memories produce high surprise (unexpected content)
 * HOW:  Check content consistency against generative model
 */
int sec_mem_fep_detect_false_injection(
    sec_mem_fep_bridge_t* bridge,
    float content_score,
    float source_validity,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    float* confidence_out
) {
    if (!bridge || !threat_level_out || !confidence_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_detect_false_injection: required parameter is NULL (bridge, threat_level_out, confidence_out)");
        return -1;
    }

    if (!bridge->config.detect_false_injection) {
        *threat_level_out = SEC_MEM_FEP_THREAT_NONE;
        *confidence_out = 0.0f;
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute surprise from unexpected content */
    float content_surprise = compute_surprise_from_anomaly(
        content_score, bridge->security_effects.content_validity
    );

    /* Invalid source increases surprise */
    float source_surprise = compute_surprise_from_anomaly(
        source_validity, 1.0f
    );

    /* Apply memory type weight */
    float weight = compute_memory_type_weight(memory_type, &bridge->config);

    /* Combined injection free energy */
    float injection_fe = (content_surprise * bridge->config.content_surprise_weight +
                         source_surprise * 0.5f) * weight *
                         SEC_MEM_FEP_INJECTION_WEIGHT * 5.0f;

    /* Classify threat */
    sec_mem_fep_threat_level_t threat = classify_threat_level(
        injection_fe, &bridge->config
    );

    /* Compute confidence */
    float confidence = bridge->state.current_precision *
                      (1.0f - content_score * source_validity);
    if (confidence > 1.0f) confidence = 1.0f;

    *threat_level_out = threat;
    *confidence_out = confidence;

    /* Update effects */
    bridge->fep_effects.injection_confidence = confidence;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Detect memory replay attack
 * WHY:  Replay attacks produce temporal prediction errors
 * HOW:  Check temporal consistency, detect duplicate patterns
 */
int sec_mem_fep_detect_replay_attack(
    sec_mem_fep_bridge_t* bridge,
    float temporal_score,
    bool replay_detected,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    float* confidence_out
) {
    if (!bridge || !threat_level_out || !confidence_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_detect_replay_attack: required parameter is NULL (bridge, threat_level_out, confidence_out)");
        return -1;
    }

    if (!bridge->config.detect_replay_attacks) {
        *threat_level_out = SEC_MEM_FEP_THREAT_NONE;
        *confidence_out = 0.0f;
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute temporal prediction error */
    float temporal_pe = 1.0f - temporal_score;

    /* Apply memory type weight */
    float weight = compute_memory_type_weight(memory_type, &bridge->config);

    /* Replay detection boosts free energy significantly */
    float replay_boost = replay_detected ? 2.0f : 1.0f;

    /* Compute replay free energy */
    float replay_fe = temporal_pe * bridge->config.temporal_pe_weight *
                     weight * replay_boost * SEC_MEM_FEP_REPLAY_WEIGHT * 20.0f;

    /* Classify threat */
    sec_mem_fep_threat_level_t threat = classify_threat_level(
        replay_fe, &bridge->config
    );

    /* Compute confidence */
    float confidence = bridge->state.current_precision * temporal_pe;
    if (replay_detected) {
        confidence = fminf(confidence * 1.5f, 1.0f);
    }

    *threat_level_out = threat;
    *confidence_out = confidence;

    /* Update effects */
    bridge->fep_effects.replay_confidence = confidence;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Detect retrieval manipulation attack
 * WHY:  Manipulated retrieval produces access pattern anomalies
 * HOW:  Check retrieval patterns against expected model
 */
int sec_mem_fep_detect_retrieval_manipulation(
    sec_mem_fep_bridge_t* bridge,
    float retrieval_score,
    float access_pattern_score,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    float* confidence_out
) {
    if (!bridge || !threat_level_out || !confidence_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_detect_retrieval_manipulation: required parameter is NULL (bridge, threat_level_out, confidence_out)");
        return -1;
    }

    if (!bridge->config.detect_retrieval_manipulation) {
        *threat_level_out = SEC_MEM_FEP_THREAT_NONE;
        *confidence_out = 0.0f;
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute prediction error from access pattern deviation */
    float access_pe = 1.0f - access_pattern_score;

    /* Compute retrieval anomaly */
    float retrieval_anomaly = 1.0f - retrieval_score;

    /* Apply memory type weight */
    float weight = compute_memory_type_weight(memory_type, &bridge->config);

    /* Combined retrieval manipulation free energy */
    float retrieval_fe = (access_pe * bridge->config.access_pattern_pe_weight +
                         retrieval_anomaly * 0.4f) * weight *
                         SEC_MEM_FEP_RETRIEVAL_WEIGHT * 10.0f;

    /* Classify threat */
    sec_mem_fep_threat_level_t threat = classify_threat_level(
        retrieval_fe, &bridge->config
    );

    /* Compute confidence */
    float confidence = bridge->state.current_precision *
                      (access_pe + retrieval_anomaly) / 2.0f;
    if (confidence > 1.0f) confidence = 1.0f;

    *threat_level_out = threat;
    *confidence_out = confidence;

    /* Update effects */
    bridge->fep_effects.retrieval_confidence = confidence;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Comprehensive threat detection
 * WHY:  Combined analysis for complete threat assessment
 * HOW:  Run all detectors, compute combined free energy
 */
int sec_mem_fep_detect_threat(
    sec_mem_fep_bridge_t* bridge,
    float integrity_score,
    float content_score,
    float temporal_score,
    float retrieval_score,
    security_mem_system_type_t memory_type,
    sec_mem_fep_threat_level_t* threat_level_out,
    sec_mem_fep_attack_type_t* attack_type_out,
    float* confidence_out
) {
    if (!bridge || !threat_level_out || !attack_type_out || !confidence_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_detect_threat: required parameter is NULL (bridge, threat_level_out, attack_type_out, confidence_out)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute combined free energy */
    float combined_fe = compute_free_energy_from_metrics(
        integrity_score, content_score, temporal_score, retrieval_score,
        &bridge->config
    );

    /* Apply memory type weight */
    float weight = compute_memory_type_weight(memory_type, &bridge->config);
    combined_fe *= weight;

    /* Compute per-attack type free energies */
    float corruption_fe = bridge->config.integrity_to_fe_scale *
                         (1.0f - integrity_score) * SEC_MEM_FEP_CORRUPTION_WEIGHT;
    float injection_fe = compute_surprise_from_anomaly(content_score, 1.0f) *
                        SEC_MEM_FEP_INJECTION_WEIGHT * 2.0f;
    float replay_fe = (1.0f - temporal_score) * SEC_MEM_FEP_REPLAY_WEIGHT * 10.0f;
    float retrieval_fe = (1.0f - retrieval_score) * SEC_MEM_FEP_RETRIEVAL_WEIGHT * 5.0f;

    /* Determine attack type from individual FEs */
    sec_mem_fep_attack_type_t attack = determine_attack_type(
        corruption_fe, injection_fe, replay_fe, retrieval_fe
    );

    /* Classify overall threat level */
    sec_mem_fep_threat_level_t threat = classify_threat_level(
        combined_fe, &bridge->config
    );

    /* Compute combined confidence */
    float confidence = bridge->state.current_precision;
    float anomaly_score = (1.0f - integrity_score + 1.0f - content_score +
                          1.0f - temporal_score + 1.0f - retrieval_score) / 4.0f;
    confidence *= anomaly_score;
    if (confidence > 1.0f) confidence = 1.0f;

    *threat_level_out = threat;
    *attack_type_out = attack;
    *confidence_out = confidence;

    /* Update FEP effects */
    bridge->fep_effects.free_energy = combined_fe;
    bridge->fep_effects.threat_level = threat;
    bridge->fep_effects.attack_type = attack;
    bridge->fep_effects.threat_confidence = confidence;

    /* Update per-memory system FE */
    switch (memory_type) {
        case SEC_MEM_TYPE_WORKING:
            bridge->fep_effects.working_mem_fe = combined_fe;
            break;
        case SEC_MEM_TYPE_EPISODIC:
            bridge->fep_effects.episodic_mem_fe = combined_fe;
            break;
        case SEC_MEM_TYPE_SEMANTIC:
            bridge->fep_effects.semantic_mem_fe = combined_fe;
            break;
        case SEC_MEM_TYPE_PROCEDURAL:
            bridge->fep_effects.procedural_mem_fe = combined_fe;
            break;
        default:
            break;
    }

    /* Update statistics */
    bridge->stats.fep_detections++;
    if (threat >= SEC_MEM_FEP_THREAT_MODERATE) {
        bridge->stats.threats_detected++;
        bridge->stats.true_positive_count++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Response Implementation
 * ============================================================================ */

/**
 * WHAT: Get recommended protective response via active inference
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluate threat level and urgency to select optimal response
 */
int sec_mem_fep_get_response(
    sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_response_t* response_out,
    float* urgency_out
) {
    if (!bridge || !response_out || !urgency_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_get_response: required parameter is NULL (bridge, response_out, urgency_out)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current threat metrics */
    float free_energy = bridge->fep_effects.free_energy;
    sec_mem_fep_threat_level_t threat = bridge->fep_effects.threat_level;
    sec_mem_fep_attack_type_t attack = bridge->fep_effects.attack_type;

    /* Compute urgency */
    float urgency = free_energy / bridge->config.critical_fe_threshold;
    if (urgency > 1.0f) urgency = 1.0f;

    /* Determine response */
    sec_mem_fep_response_t response = determine_response(threat, attack, urgency);

    *response_out = response;
    *urgency_out = urgency;

    /* Track if response is taken */
    if (response != SEC_MEM_FEP_RESPONSE_NONE) {
        bridge->stats.protective_responses++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get response for specific attack type
 * WHY:  Different attacks need different countermeasures
 * HOW:  Select response based on attack type and severity
 */
int sec_mem_fep_get_attack_response(
    sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_attack_type_t attack_type,
    float severity,
    sec_mem_fep_response_t* response_out
) {
    if (!bridge || !response_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_get_attack_response: required parameter is NULL (bridge, response_out)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sec_mem_fep_response_t response = SEC_MEM_FEP_RESPONSE_NONE;

    switch (attack_type) {
        case SEC_MEM_FEP_ATTACK_CORRUPTION:
            if (severity > 0.8f) {
                response = SEC_MEM_FEP_RESPONSE_RESTORE;
            } else if (severity > 0.5f) {
                response = SEC_MEM_FEP_RESPONSE_ISOLATE;
            } else {
                response = SEC_MEM_FEP_RESPONSE_VERIFY;
            }
            break;

        case SEC_MEM_FEP_ATTACK_FALSE_INJECTION:
            if (severity > 0.8f) {
                response = SEC_MEM_FEP_RESPONSE_QUARANTINE;
            } else if (severity > 0.5f) {
                response = SEC_MEM_FEP_RESPONSE_VERIFY;
            } else {
                response = SEC_MEM_FEP_RESPONSE_MONITOR;
            }
            break;

        case SEC_MEM_FEP_ATTACK_REPLAY:
            if (severity > 0.7f) {
                response = SEC_MEM_FEP_RESPONSE_PROTECT;
            } else if (severity > 0.4f) {
                response = SEC_MEM_FEP_RESPONSE_VERIFY;
            } else {
                response = SEC_MEM_FEP_RESPONSE_MONITOR;
            }
            break;

        case SEC_MEM_FEP_ATTACK_RETRIEVAL_MANIP:
            if (severity > 0.8f) {
                response = SEC_MEM_FEP_RESPONSE_LOCKDOWN;
            } else if (severity > 0.5f) {
                response = SEC_MEM_FEP_RESPONSE_PROTECT;
            } else {
                response = SEC_MEM_FEP_RESPONSE_MONITOR;
            }
            break;

        case SEC_MEM_FEP_ATTACK_MULTIPLE:
            if (severity > 0.6f) {
                response = SEC_MEM_FEP_RESPONSE_LOCKDOWN;
            } else {
                response = SEC_MEM_FEP_RESPONSE_ISOLATE;
            }
            break;

        default:
            response = SEC_MEM_FEP_RESPONSE_MONITOR;
            break;
    }

    *response_out = response;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report false positive detection
 * WHY:  Reduce precision to prevent similar false positives
 * HOW:  Decrease precision, update statistics
 */
int sec_mem_fep_report_false_positive(sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->security_effects.false_positives++;
    bridge->stats.false_positive_count++;

    /* Reduce precision if learning from FPs enabled */
    if (bridge->config.learn_from_false_positives) {
        float reduction = 0.9f;
        bridge->state.current_precision *= reduction;

        if (bridge->state.current_precision < SEC_MEM_FEP_MIN_PRECISION) {
            bridge->state.current_precision = SEC_MEM_FEP_MIN_PRECISION;
        }
    }

    /* Update detection accuracy */
    uint64_t total_positives = bridge->stats.true_positive_count +
                               bridge->stats.false_positive_count;
    if (total_positives > 0) {
        bridge->stats.detection_accuracy =
            (float)bridge->stats.true_positive_count / (float)total_positives;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report true positive detection
 * WHY:  Increase precision for confirmed detections
 * HOW:  Boost precision, update model
 */
int sec_mem_fep_report_true_positive(
    sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_attack_type_t attack_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->stats.true_positive_count++;

    /* Increase precision for confirmed detections */
    float boost = 1.05f;
    bridge->state.current_precision *= boost;

    if (bridge->state.current_precision > SEC_MEM_FEP_MAX_PRECISION) {
        bridge->state.current_precision = SEC_MEM_FEP_MAX_PRECISION;
    }

    /* Update per-attack statistics */
    switch (attack_type) {
        case SEC_MEM_FEP_ATTACK_CORRUPTION:
            bridge->stats.corruption_detections++;
            break;
        case SEC_MEM_FEP_ATTACK_FALSE_INJECTION:
            bridge->stats.injection_detections++;
            break;
        case SEC_MEM_FEP_ATTACK_REPLAY:
            bridge->stats.replay_detections++;
            break;
        case SEC_MEM_FEP_ATTACK_RETRIEVAL_MANIP:
            bridge->stats.retrieval_detections++;
            break;
        default:
            break;
    }

    /* Update detection accuracy */
    uint64_t total_positives = bridge->stats.true_positive_count +
                               bridge->stats.false_positive_count;
    if (total_positives > 0) {
        bridge->stats.detection_accuracy =
            (float)bridge->stats.true_positive_count / (float)total_positives;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int sec_mem_fep_get_fep_effects(
    const sec_mem_fep_bridge_t* bridge,
    fep_to_sec_mem_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_get_fep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    *effects = bridge->fep_effects;
    return 0;
}

int sec_mem_fep_get_security_effects(
    const sec_mem_fep_bridge_t* bridge,
    sec_mem_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_get_security_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    *effects = bridge->security_effects;
    return 0;
}

int sec_mem_fep_get_state(
    const sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int sec_mem_fep_get_stats(
    const sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

float sec_mem_fep_get_free_energy(const sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.free_energy;
}

float sec_mem_fep_get_surprise(const sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.surprise_level;
}

sec_mem_fep_threat_level_t sec_mem_fep_get_threat_level(
    const sec_mem_fep_bridge_t* bridge
) {
    if (!bridge) {
        return SEC_MEM_FEP_THREAT_NONE;
    }
    return bridge->fep_effects.threat_level;
}

sec_mem_fep_attack_type_t sec_mem_fep_get_attack_type(
    const sec_mem_fep_bridge_t* bridge
) {
    if (!bridge) {
        return SEC_MEM_FEP_ATTACK_NONE;
    }
    return bridge->fep_effects.attack_type;
}

float sec_mem_fep_get_memory_type_fe(
    const sec_mem_fep_bridge_t* bridge,
    security_mem_system_type_t memory_type
) {
    if (!bridge) {
        return -1.0f;
    }

    switch (memory_type) {
        case SEC_MEM_TYPE_WORKING:
            return bridge->fep_effects.working_mem_fe;
        case SEC_MEM_TYPE_EPISODIC:
            return bridge->fep_effects.episodic_mem_fe;
        case SEC_MEM_TYPE_SEMANTIC:
            return bridge->fep_effects.semantic_mem_fe;
        case SEC_MEM_TYPE_PROCEDURAL:
            return bridge->fep_effects.procedural_mem_fe;
        default:
            return -1.0f;
    }
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module with router, setup inbox
 */
int sec_mem_fep_connect_bio_async(sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_MEMORY_FEP,
        .module_name = "sec_mem_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security Memory FEP bridge connected to bio-async");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of inter-module communication
 * HOW:  Unregister module from router
 */
int sec_mem_fep_disconnect_bio_async(sec_mem_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security Memory FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Process bio-async messages
 * WHY:  Handle security notifications from other modules
 * HOW:  Use bio_router_process_inbox() for message processing
 */
int sec_mem_fep_process_messages(sec_mem_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    if (!bridge->base.bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_process_messages: bridge->base is NULL");
        return -1;
    }

    /* Process all pending messages */
    uint32_t processed = bio_router_process_inbox(bridge->base.bio_ctx, 0);
    return (int)processed;
}

bool sec_mem_fep_is_bio_async_connected(const sec_mem_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/**
 * WHAT: Send security alert via bio-async
 * WHY:  Notify system of detected threats
 * HOW:  Broadcast through bio-async router
 */
int sec_mem_fep_send_alert(
    sec_mem_fep_bridge_t* bridge,
    sec_mem_fep_threat_level_t threat_level,
    sec_mem_fep_attack_type_t attack_type
) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_send_alert: required parameter is NULL (bridge, bridge->base)");
        return -1;
    }

    if (!bridge->base.bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_send_alert: bridge->base is NULL");
        return -1;
    }

    /* Create a simple security alert structure */
    struct {
        uint32_t message_type;
        uint32_t threat_level;
        uint32_t attack_type;
        float confidence;
        float free_energy;
    } alert_msg;

    alert_msg.message_type = BIO_MSG_SECURITY_ALERT;
    alert_msg.threat_level = (uint32_t)threat_level;
    alert_msg.attack_type = (uint32_t)attack_type;
    alert_msg.confidence = bridge->fep_effects.threat_confidence;
    alert_msg.free_energy = bridge->fep_effects.free_energy;

    /* Broadcast alert to all listeners */
    nimcp_error_t result = bio_router_broadcast(
        bridge->base.bio_ctx,
        &alert_msg,
        sizeof(alert_msg)
    );
    return (result == NIMCP_SUCCESS) ? 0 : -1;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* sec_mem_fep_threat_level_name(sec_mem_fep_threat_level_t level) {
    switch (level) {
        case SEC_MEM_FEP_THREAT_NONE:
            return "None";
        case SEC_MEM_FEP_THREAT_SUSPICIOUS:
            return "Suspicious";
        case SEC_MEM_FEP_THREAT_MODERATE:
            return "Moderate";
        case SEC_MEM_FEP_THREAT_HIGH:
            return "High";
        case SEC_MEM_FEP_THREAT_CRITICAL:
            return "Critical";
        default:
            return "Unknown";
    }
}

const char* sec_mem_fep_attack_type_name(sec_mem_fep_attack_type_t attack) {
    switch (attack) {
        case SEC_MEM_FEP_ATTACK_NONE:
            return "None";
        case SEC_MEM_FEP_ATTACK_CORRUPTION:
            return "Memory Corruption";
        case SEC_MEM_FEP_ATTACK_FALSE_INJECTION:
            return "False Memory Injection";
        case SEC_MEM_FEP_ATTACK_REPLAY:
            return "Replay Attack";
        case SEC_MEM_FEP_ATTACK_RETRIEVAL_MANIP:
            return "Retrieval Manipulation";
        case SEC_MEM_FEP_ATTACK_MULTIPLE:
            return "Multiple Attacks";
        default:
            return "Unknown";
    }
}

const char* sec_mem_fep_response_name(sec_mem_fep_response_t response) {
    switch (response) {
        case SEC_MEM_FEP_RESPONSE_NONE:
            return "None";
        case SEC_MEM_FEP_RESPONSE_MONITOR:
            return "Monitor";
        case SEC_MEM_FEP_RESPONSE_VERIFY:
            return "Verify";
        case SEC_MEM_FEP_RESPONSE_QUARANTINE:
            return "Quarantine";
        case SEC_MEM_FEP_RESPONSE_PROTECT:
            return "Protect";
        case SEC_MEM_FEP_RESPONSE_ISOLATE:
            return "Isolate";
        case SEC_MEM_FEP_RESPONSE_RESTORE:
            return "Restore";
        case SEC_MEM_FEP_RESPONSE_LOCKDOWN:
            return "Lockdown";
        default:
            return "Unknown";
    }
}

void sec_mem_fep_print_summary(const sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Security Memory FEP Bridge: NULL\n");
        return;
    }

    printf("=== Security Memory FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Detections: %lu\n", (unsigned long)bridge->state.detection_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Surprise: %.3f\n", bridge->fep_effects.surprise_level);
    printf("  Prediction Error: %.3f\n", bridge->fep_effects.prediction_error);
    printf("  Threat Level: %s\n",
           sec_mem_fep_threat_level_name(bridge->fep_effects.threat_level));
    printf("  Attack Type: %s\n",
           sec_mem_fep_attack_type_name(bridge->fep_effects.attack_type));
    printf("  Threat Confidence: %.3f\n", bridge->fep_effects.threat_confidence);
    printf("  Integrity Estimate: %.3f\n", bridge->fep_effects.integrity_estimate);
    printf("  Recommended Response: %s\n",
           sec_mem_fep_response_name(bridge->fep_effects.recommended_response));
    printf("\n");
    printf("Per-Attack Confidence:\n");
    printf("  Corruption: %.3f\n", bridge->fep_effects.corruption_confidence);
    printf("  Injection: %.3f\n", bridge->fep_effects.injection_confidence);
    printf("  Replay: %.3f\n", bridge->fep_effects.replay_confidence);
    printf("  Retrieval: %.3f\n", bridge->fep_effects.retrieval_confidence);
    printf("\n");
    printf("Per-Memory System FE:\n");
    printf("  Working: %.3f\n", bridge->fep_effects.working_mem_fe);
    printf("  Episodic: %.3f\n", bridge->fep_effects.episodic_mem_fe);
    printf("  Semantic: %.3f\n", bridge->fep_effects.semantic_mem_fe);
    printf("  Procedural: %.3f\n", bridge->fep_effects.procedural_mem_fe);
    printf("\n");
    printf("Security Effects:\n");
    printf("  Attacks Detected: %lu\n",
           (unsigned long)bridge->security_effects.attacks_detected);
    printf("  Normal Operations: %lu\n",
           (unsigned long)bridge->security_effects.normal_operations);
    printf("  Under Attack: %s\n",
           bridge->security_effects.under_attack ? "yes" : "no");
    printf("  Avg Integrity: %.3f\n", bridge->security_effects.avg_integrity_score);
    printf("\n");
    printf("Attack Counts:\n");
    printf("  Corruption: %lu\n",
           (unsigned long)bridge->security_effects.corruption_attacks);
    printf("  Injection: %lu\n",
           (unsigned long)bridge->security_effects.injection_attacks);
    printf("  Replay: %lu\n",
           (unsigned long)bridge->security_effects.replay_attacks);
    printf("  Retrieval: %lu\n",
           (unsigned long)bridge->security_effects.retrieval_attacks);
    printf("=============================================\n");
}

void sec_mem_fep_print_stats(const sec_mem_fep_stats_t* stats) {
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("=== Security Memory FEP Statistics ===\n");
    printf("Total Updates: %lu\n", (unsigned long)stats->total_updates);
    printf("FEP Detections: %lu\n", (unsigned long)stats->fep_detections);
    printf("Threats Detected: %lu\n", (unsigned long)stats->threats_detected);
    printf("Protective Responses: %lu\n", (unsigned long)stats->protective_responses);
    printf("Precision Adaptations: %lu\n", (unsigned long)stats->precision_adaptations);
    printf("\n");
    printf("Per-Attack Detections:\n");
    printf("  Corruption: %lu\n", (unsigned long)stats->corruption_detections);
    printf("  Injection: %lu\n", (unsigned long)stats->injection_detections);
    printf("  Replay: %lu\n", (unsigned long)stats->replay_detections);
    printf("  Retrieval: %lu\n", (unsigned long)stats->retrieval_detections);
    printf("\n");
    printf("Per-Memory Detections:\n");
    printf("  Working: %lu\n", (unsigned long)stats->working_mem_detections);
    printf("  Episodic: %lu\n", (unsigned long)stats->episodic_mem_detections);
    printf("  Semantic: %lu\n", (unsigned long)stats->semantic_mem_detections);
    printf("  Procedural: %lu\n", (unsigned long)stats->procedural_mem_detections);
    printf("\n");
    printf("Averages:\n");
    printf("  Free Energy: %.3f\n", stats->avg_free_energy);
    printf("  Surprise: %.3f\n", stats->avg_surprise);
    printf("  Prediction Error: %.3f\n", stats->avg_prediction_error);
    printf("  Precision: %.3f\n", stats->current_precision);
    printf("\n");
    printf("Maximums:\n");
    printf("  Free Energy: %.3f\n", stats->max_free_energy);
    printf("  Surprise: %.3f\n", stats->max_surprise);
    printf("\n");
    printf("Detection Performance:\n");
    printf("  True Positives: %lu\n", (unsigned long)stats->true_positive_count);
    printf("  False Positives: %lu\n", (unsigned long)stats->false_positive_count);
    printf("  Accuracy: %.3f\n", stats->detection_accuracy);
    printf("=========================================\n");
}

int sec_mem_fep_reset_stats(sec_mem_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(sec_mem_fep_stats_t));
    bridge->stats.current_precision = bridge->state.current_precision;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from memory security metrics
 * WHY:  Map security domain to FEP domain
 * HOW:  Weighted combination of integrity, content, temporal, retrieval
 */
static float compute_free_energy_from_metrics(
    float integrity_score,
    float content_score,
    float temporal_score,
    float retrieval_score,
    const sec_mem_fep_config_t* config
) {
    /*
     * Free energy increases as scores decrease (inverted relationship)
     * F = integrity_weight * (1-integrity) + content_weight * (1-content) + ...
     */
    float integrity_fe = config->integrity_to_fe_scale * (1.0f - integrity_score);
    float content_fe = config->content_surprise_weight * (1.0f - content_score) * 10.0f;
    float temporal_fe = config->temporal_pe_weight * (1.0f - temporal_score) * 10.0f;
    float retrieval_fe = config->access_pattern_pe_weight * (1.0f - retrieval_score) * 10.0f;

    return integrity_fe + content_fe + temporal_fe + retrieval_fe;
}

/**
 * WHAT: Compute surprise from anomaly magnitude
 * WHY:  Surprise = -log(probability of observation)
 * HOW:  Approximate using deviation from expected
 */
static float compute_surprise_from_anomaly(
    float observed_value,
    float expected_value
) {
    float deviation = fabsf(observed_value - expected_value);
    float normalized_dev = deviation / (expected_value + 0.01f);

    /* Apply log-like transformation for surprise */
    float surprise = -logf(1.0f - fminf(normalized_dev, 0.99f) + 0.01f);

    if (surprise < 0.0f) surprise = 0.0f;
    if (surprise > 20.0f) surprise = 20.0f;

    return surprise;
}

/**
 * WHAT: Get weight for memory type
 * WHY:  Different memory systems have different security sensitivities
 * HOW:  Return configured weight for memory type
 */
static float compute_memory_type_weight(
    security_mem_system_type_t memory_type,
    const sec_mem_fep_config_t* config
) {
    switch (memory_type) {
        case SEC_MEM_TYPE_WORKING:
            return config->working_memory_weight;
        case SEC_MEM_TYPE_EPISODIC:
            return config->episodic_memory_weight;
        case SEC_MEM_TYPE_SEMANTIC:
            return config->semantic_memory_weight;
        case SEC_MEM_TYPE_PROCEDURAL:
            return config->procedural_memory_weight;
        default:
            return 1.0f;
    }
}

/**
 * WHAT: Classify threat level from free energy
 * WHY:  Map continuous FE to discrete threat categories
 * HOW:  Threshold-based classification
 */
static sec_mem_fep_threat_level_t classify_threat_level(
    float free_energy,
    const sec_mem_fep_config_t* config
) {
    if (free_energy >= config->critical_fe_threshold) {
        return SEC_MEM_FEP_THREAT_CRITICAL;
    } else if (free_energy >= config->free_energy_threshold) {
        return SEC_MEM_FEP_THREAT_HIGH;
    } else if (free_energy >= SEC_MEM_FEP_SUSPICIOUS_THRESHOLD) {
        return SEC_MEM_FEP_THREAT_MODERATE;
    } else if (free_energy >= config->normal_fe_threshold) {
        return SEC_MEM_FEP_THREAT_SUSPICIOUS;
    } else {
        return SEC_MEM_FEP_THREAT_NONE;
    }
}

/**
 * WHAT: Determine attack type from individual free energies
 * WHY:  Identify primary attack vector
 * HOW:  Compare per-attack FEs, return highest
 */
static sec_mem_fep_attack_type_t determine_attack_type(
    float corruption_fe,
    float injection_fe,
    float replay_fe,
    float retrieval_fe
) {
    float max_fe = corruption_fe;
    sec_mem_fep_attack_type_t attack = SEC_MEM_FEP_ATTACK_CORRUPTION;

    if (injection_fe > max_fe) {
        max_fe = injection_fe;
        attack = SEC_MEM_FEP_ATTACK_FALSE_INJECTION;
    }
    if (replay_fe > max_fe) {
        max_fe = replay_fe;
        attack = SEC_MEM_FEP_ATTACK_REPLAY;
    }
    if (retrieval_fe > max_fe) {
        max_fe = retrieval_fe;
        attack = SEC_MEM_FEP_ATTACK_RETRIEVAL_MANIP;
    }

    /* Check for multiple attacks (if multiple are above threshold) */
    int attack_count = 0;
    if (corruption_fe > SEC_MEM_FEP_SUSPICIOUS_THRESHOLD) attack_count++;
    if (injection_fe > SEC_MEM_FEP_SUSPICIOUS_THRESHOLD) attack_count++;
    if (replay_fe > SEC_MEM_FEP_SUSPICIOUS_THRESHOLD) attack_count++;
    if (retrieval_fe > SEC_MEM_FEP_SUSPICIOUS_THRESHOLD) attack_count++;

    if (attack_count > 1) {
        return SEC_MEM_FEP_ATTACK_MULTIPLE;
    }

    /* Return no attack if max FE is below threshold */
    if (max_fe < SEC_MEM_FEP_NORMAL_THRESHOLD) {
        return SEC_MEM_FEP_ATTACK_NONE;
    }

    return attack;
}

/**
 * WHAT: Determine appropriate response based on threat, attack, and urgency
 * WHY:  Active inference selects actions to minimize expected FE
 * HOW:  Map threat level, attack type, and urgency to response type
 */
static sec_mem_fep_response_t determine_response(
    sec_mem_fep_threat_level_t threat,
    sec_mem_fep_attack_type_t attack,
    float urgency
) {
    /* Critical threats always get maximum response */
    if (threat == SEC_MEM_FEP_THREAT_CRITICAL) {
        if (attack == SEC_MEM_FEP_ATTACK_MULTIPLE) {
            return SEC_MEM_FEP_RESPONSE_LOCKDOWN;
        }
        return SEC_MEM_FEP_RESPONSE_RESTORE;
    }

    /* High threats based on attack type */
    if (threat == SEC_MEM_FEP_THREAT_HIGH) {
        switch (attack) {
            case SEC_MEM_FEP_ATTACK_CORRUPTION:
                return SEC_MEM_FEP_RESPONSE_ISOLATE;
            case SEC_MEM_FEP_ATTACK_FALSE_INJECTION:
                return SEC_MEM_FEP_RESPONSE_QUARANTINE;
            case SEC_MEM_FEP_ATTACK_REPLAY:
                return SEC_MEM_FEP_RESPONSE_PROTECT;
            case SEC_MEM_FEP_ATTACK_RETRIEVAL_MANIP:
                return SEC_MEM_FEP_RESPONSE_PROTECT;
            case SEC_MEM_FEP_ATTACK_MULTIPLE:
                return SEC_MEM_FEP_RESPONSE_ISOLATE;
            default:
                return SEC_MEM_FEP_RESPONSE_PROTECT;
        }
    }

    /* Moderate threats get verification or protection */
    if (threat == SEC_MEM_FEP_THREAT_MODERATE) {
        if (urgency > 0.7f) {
            return SEC_MEM_FEP_RESPONSE_PROTECT;
        }
        return SEC_MEM_FEP_RESPONSE_VERIFY;
    }

    /* Suspicious patterns get monitoring */
    if (threat == SEC_MEM_FEP_THREAT_SUSPICIOUS) {
        return SEC_MEM_FEP_RESPONSE_MONITOR;
    }

    return SEC_MEM_FEP_RESPONSE_NONE;
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    sec_mem_fep_bridge_t* bridge,
    float free_energy,
    float surprise,
    float pred_error
) {
    const float alpha = 0.1f;

    bridge->state.avg_surprise =
        (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;

    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * pred_error;

    bridge->stats.avg_free_energy =
        (1.0f - alpha) * bridge->stats.avg_free_energy + alpha * free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
}

/**
 * WHAT: Update statistics for specific memory type
 * WHY:  Track per-memory system detection counts
 * HOW:  Increment appropriate counter
 */
static void update_memory_type_stats(
    sec_mem_fep_bridge_t* bridge,
    security_mem_system_type_t memory_type
) {
    switch (memory_type) {
        case SEC_MEM_TYPE_WORKING:
            bridge->stats.working_mem_detections++;
            break;
        case SEC_MEM_TYPE_EPISODIC:
            bridge->stats.episodic_mem_detections++;
            break;
        case SEC_MEM_TYPE_SEMANTIC:
            bridge->stats.semantic_mem_detections++;
            break;
        case SEC_MEM_TYPE_PROCEDURAL:
            bridge->stats.procedural_mem_detections++;
            break;
        default:
            break;
    }
}
