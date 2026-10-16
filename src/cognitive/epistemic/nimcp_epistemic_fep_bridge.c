/**
 * @file nimcp_epistemic_fep_bridge.c
 * @brief Free Energy Principle - Epistemic Filter Integration Bridge Implementation
 *
 * WHAT: Implements bidirectional integration between FEP and epistemic filtering
 * WHY:  Epistemic value computation guides information-seeking; uncertainty estimation
 *       enables bias detection; FEP precision weights evidence quality
 * HOW:  FEP uncertainty -> epistemic value; epistemic filtering -> precision updates;
 *       bias detection -> model revision
 *
 * BIOLOGICAL BASIS:
 * - Epistemic foraging = active inference minimizing expected uncertainty
 * - Curiosity = expected free energy reduction through information gain
 * - Bias detection = mismatch between prior precision and evidence precision
 */

#include "cognitive/epistemic/nimcp_epistemic_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "epistemic_fep_bridge"

/**
 * Inline EWMA update macro - equivalent to nimcp_ewma_update() from streaming statistics.
 * Formula: EWMA_t = alpha * x_t + (1 - alpha) * EWMA_{t-1}
 * Using alpha=0.1 for responsive tracking (equivalent ~9-sample window).
 * @see include/utils/statistics/nimcp_streaming_statistics.h for full EWMA API
 */
#define NIMCP_EWMA_UPDATE(avg, new_val, alpha) \
    ((avg) = ((avg) * (1.0f - (alpha))) + ((new_val) * (alpha)))
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(epistemic_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_epistemic_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_epistemic_fep_bridge_mesh_registry = NULL;

nimcp_error_t epistemic_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_epistemic_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "epistemic_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "epistemic_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_epistemic_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_epistemic_fep_bridge_mesh_registry = registry;
    return err;
}

void epistemic_fep_bridge_mesh_unregister(void) {
    if (g_epistemic_fep_bridge_mesh_registry && g_epistemic_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_epistemic_fep_bridge_mesh_registry, g_epistemic_fep_bridge_mesh_id);
        g_epistemic_fep_bridge_mesh_id = 0;
        g_epistemic_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from epistemic_fep_bridge module (instance-level) */
static inline void epistemic_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_epistemic_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_epistemic_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_epistemic_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int epistemic_fep_bridge_default_config(epistemic_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP -> Epistemic */
    config->uncertainty_epistemic_threshold = EPISTEMIC_FEP_HIGH_UNCERTAINTY_THRESHOLD;
    config->bias_precision_penalty = EPISTEMIC_FEP_BIAS_PRECISION_PENALTY;
    config->evidence_quality_weight = 1.0f;
    config->enable_uncertainty_seeking = true;
    config->enable_bias_detection = true;
    config->enable_evidence_weighting = true;

    /* Epistemic -> FEP */
    config->information_gain_sensitivity = 1.0f;
    config->source_reliability_weight = 1.0f;
    config->enable_source_precision = true;
    config->enable_quality_updates = true;

    /* Sensitivity */
    config->fe_sensitivity = 1.0f;
    config->epistemic_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

epistemic_fep_bridge_t* epistemic_fep_bridge_create(
    const epistemic_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_create", 0.0f);


    epistemic_fep_bridge_t* bridge = nimcp_malloc(sizeof(epistemic_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate epistemic FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(epistemic_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        epistemic_fep_bridge_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "epistemic_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize defaults */
    bridge->epistemic_effects.source_reliability_weight = 1.0f;

    NIMCP_LOGGING_INFO("Created epistemic FEP bridge");
    return bridge;
}

void epistemic_fep_bridge_destroy(epistemic_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        epistemic_fep_bridge_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed epistemic FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int epistemic_fep_bridge_connect_fep(
    epistemic_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to epistemic bridge");
    return 0;
}

int epistemic_fep_bridge_connect_epistemic(
    epistemic_fep_bridge_t* bridge,
    epistemic_filter_t filter
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_connect_epistemic", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->epistemic_filter = filter;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected epistemic filter to FEP bridge");
    return 0;
}

int epistemic_fep_bridge_disconnect(epistemic_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    memset(&bridge->epistemic_filter, 0, sizeof(epistemic_filter_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from epistemic FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP -> Epistemic Direction
 * ============================================================================ */

int epistemic_fep_compute_epistemic_value(
    epistemic_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_epistemic_fep_comput", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_uncertainty_seeking) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Epistemic value = expected information gain
     * High uncertainty -> high epistemic value (seek information)
     * Epistemic foraging is active inference minimizing expected uncertainty
     */
    float uncertainty = bridge->state.current_uncertainty;
    float epistemic_value = uncertainty * bridge->config.epistemic_sensitivity;

    /* Epistemic value also depends on expected information gain */
    float info_gain = bridge->config.information_gain_sensitivity * uncertainty;
    epistemic_value = (epistemic_value + info_gain) / 2.0f;

    if (epistemic_value > 1.0f) epistemic_value = 1.0f;

    bridge->fep_effects.epistemic_value = epistemic_value;
    bridge->fep_effects.expected_information_gain = info_gain;
    bridge->fep_effects.current_uncertainty = uncertainty;

    /* Trigger information seeking if uncertainty is high */
    if (uncertainty > bridge->config.uncertainty_epistemic_threshold) {
        bridge->fep_effects.information_seeking_active = true;
        bridge->stats.information_seeking_events++;
    } else {
        bridge->fep_effects.information_seeking_active = false;
    }

    /* Update state */
    bridge->state.current_epistemic_value = epistemic_value;
    bridge->state.seeking_information = bridge->fep_effects.information_seeking_active;

    /* Update stats using EWMA (alpha=0.1, ~9-sample window for responsive tracking) */
    NIMCP_EWMA_UPDATE(bridge->stats.avg_epistemic_value, epistemic_value, 0.1f);
    NIMCP_EWMA_UPDATE(bridge->stats.avg_uncertainty, uncertainty, 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Computed epistemic value: %f, uncertainty: %f, seeking: %s",
                        epistemic_value, uncertainty,
                        bridge->fep_effects.information_seeking_active ? "yes" : "no");
    return 0;
}

int epistemic_fep_detect_bias_from_precision(
    epistemic_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_epistemic_fep_detect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bias_detection) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Bias = mismatch between prior precision and evidence precision
     * If we're very confident but evidence is weak, that's a bias indicator
     * If we're uncertain but evidence is strong, we're underweighting
     */
    float evidence_precision = bridge->epistemic_effects.evidence_precision_update;
    float current_uncertainty = bridge->state.current_uncertainty;
    float prior_precision = 1.0f - current_uncertainty;  /* Higher certainty = higher precision */

    /* Compute precision mismatch */
    float precision_mismatch = fabsf(prior_precision - evidence_precision);

    if (precision_mismatch > 0.3f) {
        /* Significant mismatch -> potential bias */
        bridge->fep_effects.bias_detected_magnitude = precision_mismatch;
        bridge->state.biases_detected++;
        bridge->stats.bias_detections++;
        NIMCP_LOGGING_INFO("Bias detected: precision mismatch = %f", precision_mismatch);
    } else {
        bridge->fep_effects.bias_detected_magnitude = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int epistemic_fep_trigger_information_seeking(
    epistemic_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_epistemic_fep_trigge", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Force information seeking mode */
    bridge->fep_effects.information_seeking_active = true;
    bridge->state.seeking_information = true;
    bridge->stats.information_seeking_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Triggered information seeking mode");
    return 0;
}

/* ============================================================================
 * Epistemic -> FEP Direction
 * ============================================================================ */

int epistemic_fep_update_evidence_precision(
    epistemic_fep_bridge_t* bridge,
    float evidence_quality
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_epistemic_fep_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_quality_updates) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Evidence quality determines precision update
     * High quality evidence -> high precision
     * Low quality evidence -> low precision
     */
    float precision_update = evidence_quality * bridge->config.evidence_quality_weight;
    if (precision_update > 1.0f) precision_update = 1.0f;
    if (precision_update < 0.0f) precision_update = 0.0f;

    bridge->epistemic_effects.evidence_precision_update = precision_update;
    bridge->epistemic_effects.precision_updated = true;

    /* Update uncertainty based on evidence quality */
    float uncertainty_reduction = evidence_quality * 0.1f;
    bridge->state.current_uncertainty -= uncertainty_reduction;
    if (bridge->state.current_uncertainty < 0.0f) bridge->state.current_uncertainty = 0.0f;

    /* Update stats */
    bridge->stats.precision_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Updated evidence precision: %f, new uncertainty: %f",
                        precision_update, bridge->state.current_uncertainty);
    return 0;
}

int epistemic_fep_revise_priors_from_bias(
    epistemic_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_epistemic_fep_revise", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bias_detection) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* If bias was detected, revise priors */
    float bias_magnitude = bridge->fep_effects.bias_detected_magnitude;
    if (bias_magnitude > 0.0f) {
        /* Prior revision magnitude proportional to bias */
        float revision = bias_magnitude * bridge->config.bias_precision_penalty;
        bridge->epistemic_effects.prior_revision_magnitude = revision;

        /* Increase uncertainty to encourage updating */
        bridge->state.current_uncertainty += revision * 0.2f;
        if (bridge->state.current_uncertainty > 1.0f) {
            bridge->state.current_uncertainty = 1.0f;
        }

        NIMCP_LOGGING_INFO("Revised priors from bias: revision magnitude = %f", revision);
    } else {
        bridge->epistemic_effects.prior_revision_magnitude = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int epistemic_fep_weight_by_source_reliability(
    epistemic_fep_bridge_t* bridge,
    float reliability
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_epistemic_fep_weight", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_source_precision) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Source reliability modulates precision weighting
     * Reliable sources get higher precision weight
     * Unreliable sources get lower precision weight
     */
    float weight = reliability * bridge->config.source_reliability_weight;
    if (weight > 2.0f) weight = 2.0f;
    if (weight < 0.1f) weight = 0.1f;

    bridge->epistemic_effects.source_reliability_weight = weight;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Weighted by source reliability: weight = %f", weight);
    return 0;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int epistemic_fep_bridge_update(
    epistemic_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Compute epistemic value from current uncertainty */
    epistemic_fep_compute_epistemic_value(bridge);

    /* Detect bias from precision mismatch */
    epistemic_fep_detect_bias_from_precision(bridge);

    /* Revise priors if bias detected */
    epistemic_fep_revise_priors_from_bias(bridge);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Uncertainty naturally increases over time (world changes) */
    float uncertainty_increase = 0.0001f * (float)delta_ms;
    bridge->state.current_uncertainty += uncertainty_increase;
    if (bridge->state.current_uncertainty > 1.0f) {
        bridge->state.current_uncertainty = 1.0f;
    }

    /* Update timestamp */
    bridge->state.last_information_seek_time += delta_ms;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int epistemic_fep_bridge_get_state(
    const epistemic_fep_bridge_t* bridge,
    epistemic_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int epistemic_fep_bridge_get_stats(
    const epistemic_fep_bridge_t* bridge,
    epistemic_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int epistemic_fep_bridge_connect_bio_async(
    epistemic_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EPISTEMIC_BRIDGE,
        .module_name = "epistemic_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    return 0;
}

int epistemic_fep_bridge_disconnect_bio_async(
    epistemic_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_disconnect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool epistemic_fep_bridge_is_bio_async_connected(
    const epistemic_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int epistemic_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    epistemic_fep_bridge_heartbeat("epistemic_fe_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Epistemic_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                epistemic_fep_bridge_heartbeat("epistemic_fe_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Epistemic_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Epistemic_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void epistemic_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_epistemic_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int epistemic_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    epistemic_fep_bridge_heartbeat_instance(NULL, "epistemic_fep_bridge_training_begin", 0.0f);
    return 0;
}

int epistemic_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_fep_bridge_training_end: NULL argument");
        return -1;
    }
    epistemic_fep_bridge_heartbeat_instance(NULL, "epistemic_fep_bridge_training_end", 1.0f);
    return 0;
}

int epistemic_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    epistemic_fep_bridge_heartbeat_instance(NULL, "epistemic_fep_bridge_training_step", progress);
    return 0;
}
