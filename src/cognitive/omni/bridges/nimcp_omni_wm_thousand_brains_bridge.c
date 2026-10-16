/**
 * @file nimcp_omni_wm_thousand_brains_bridge.c
 * @brief World Model Thousand Brains Bridge — Implementation
 * @version 1.0.0
 * @date 2026-03-17
 *
 * WHAT: Bidirectional bridge connecting Omnidirectional World Model (RSSM) with
 *       Hawkins' Thousand Brains cortical column systems.
 * WHY:  Ground the world model in multi-column spatial/object/temporal representations.
 * HOW:  Reference frames → spatial WM state; voting → object WM state;
 *       dendritic sequences → temporal WM state; WM predictions → top-down columns.
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_thousand_brains_bridge.h"
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "core/cortical_columns/nimcp_column_reference_frame.h"
#include "core/cortical_columns/nimcp_column_voting.h"
#include "core/cortical_columns/nimcp_dendritic_sequence.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_tb_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(wm_tb_bridge)

/* ============================================================================
 * Mesh Participant Registration
 * ============================================================================ */

static mesh_participant_id_t g_wm_tb_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_wm_tb_bridge_mesh_registry = NULL;

nimcp_error_t wm_tb_bridge_mesh_register(void* registry) {
    mesh_participant_registry_t* reg = (mesh_participant_registry_t*)registry;
    if (!reg) return NIMCP_ERROR_NULL_POINTER;
    if (g_wm_tb_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "wm_thousand_brains_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "wm_thousand_brains_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(reg, &iface, &config, &g_wm_tb_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_wm_tb_bridge_mesh_registry = reg;
    return err;
}

void wm_tb_bridge_mesh_unregister(void) {
    if (g_wm_tb_bridge_mesh_registry && g_wm_tb_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_wm_tb_bridge_mesh_registry, g_wm_tb_bridge_mesh_id);
        g_wm_tb_bridge_mesh_id = 0;
        g_wm_tb_bridge_mesh_registry = NULL;
    }
}

/* ============================================================================
 * Health Agent
 * ============================================================================ */

static inline void wm_tb_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_wm_tb_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_wm_tb_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_wm_tb_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void wm_tb_bridge_set_instance_health_agent(wm_thousand_brains_bridge_t* bridge,
                                             nimcp_health_agent_t* agent) {
    if (!bridge) return;
    bridge->health_agent = agent;
}

/* ============================================================================
 * Training Hooks
 * ============================================================================ */

int wm_tb_bridge_training_begin(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wm_tb_bridge_training_begin: NULL bridge");
        return -1;
    }
    NIMCP_LOGGING_INFO("wm_tb_bridge: training begin");
    return 0;
}

int wm_tb_bridge_training_end(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wm_tb_bridge_training_end: NULL bridge");
        return -1;
    }
    NIMCP_LOGGING_INFO("wm_tb_bridge: training end — spatial_updates=%lu, consensus=%lu, temporal=%lu",
                       (unsigned long)bridge->stats.spatial_updates,
                       (unsigned long)bridge->stats.consensus_updates,
                       (unsigned long)bridge->stats.temporal_updates);
    return 0;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

void wm_tb_bridge_config_default(wm_tb_bridge_config_t* config) {
    if (!config) return;
    config->spatial_weight = WM_TB_DEFAULT_SPATIAL_WEIGHT;
    config->object_weight = WM_TB_DEFAULT_OBJECT_WEIGHT;
    config->temporal_weight = WM_TB_DEFAULT_TEMPORAL_WEIGHT;
    config->topdown_gain = WM_TB_DEFAULT_TOPDOWN_GAIN;
    config->surprise_lr_scale = WM_TB_DEFAULT_SURPRISE_LR_SCALE;
    config->update_interval_ms = WM_TB_DEFAULT_UPDATE_INTERVAL_MS;
    config->enable_spatial_integration = true;
    config->enable_voting_integration = true;
    config->enable_sequence_integration = true;
    config->enable_topdown_predictions = true;
    config->enable_surprise_modulation = true;
    config->enable_movement_priors = true;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

wm_thousand_brains_bridge_t* wm_tb_bridge_create(const wm_tb_bridge_config_t* config) {
    wm_thousand_brains_bridge_t* bridge = nimcp_calloc(1, sizeof(wm_thousand_brains_bridge_t));
    if (!bridge) return NULL;

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_THOUSAND_BRAINS_BRIDGE,
                         "wm_thousand_brains_bridge") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        wm_tb_bridge_config_default(&bridge->config);
    }

    /* Default WM state dim: spatial + object + temporal encodings */
    bridge->wm_state_dim = WM_TB_MAX_SPATIAL_DIM + WM_TB_MAX_OBJECT_DIM + WM_TB_MAX_TEMPORAL_DIM;
    bridge->wm_state_buffer = nimcp_calloc(bridge->wm_state_dim, sizeof(float));
    if (!bridge->wm_state_buffer) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("wm_tb_bridge: created (spatial=%.1f, object=%.1f, temporal=%.1f, "
                       "topdown_gain=%.2f, state_dim=%u)",
                       bridge->config.spatial_weight, bridge->config.object_weight,
                       bridge->config.temporal_weight, bridge->config.topdown_gain,
                       bridge->wm_state_dim);
    return bridge;
}

void wm_tb_bridge_destroy(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) return;
    nimcp_free(bridge->wm_state_buffer);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * System Connections
 * ============================================================================ */

nimcp_error_t wm_tb_bridge_connect_world_model(wm_thousand_brains_bridge_t* bridge,
                                                omni_world_model_t* wm) {
    if (!bridge || !wm) return NIMCP_ERROR_NULL_POINTER;
    bridge->world_model = wm;
    bridge->base.system_a = wm;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = bridge->base.system_b_connected;
    NIMCP_LOGGING_DEBUG("wm_tb_bridge: connected to world model");
    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_connect_ref_frames(wm_thousand_brains_bridge_t* bridge,
                                               column_ref_frame_manager_t* ref_frames) {
    if (!bridge || !ref_frames) return NIMCP_ERROR_NULL_POINTER;
    bridge->ref_frames = ref_frames;
    /* Mark system_b connected when at least one TB component is available */
    bridge->base.system_b = ref_frames;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected;
    NIMCP_LOGGING_DEBUG("wm_tb_bridge: connected to reference frames");
    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_connect_voting(wm_thousand_brains_bridge_t* bridge,
                                           column_voting_manager_t* voting) {
    if (!bridge || !voting) return NIMCP_ERROR_NULL_POINTER;
    bridge->voting = voting;
    NIMCP_LOGGING_DEBUG("wm_tb_bridge: connected to column voting");
    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_connect_sequences(wm_thousand_brains_bridge_t* bridge,
                                              dendritic_sequence_mgr_t* sequences) {
    if (!bridge || !sequences) return NIMCP_ERROR_NULL_POINTER;
    bridge->sequences = sequences;
    NIMCP_LOGGING_DEBUG("wm_tb_bridge: connected to dendritic sequences");
    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_connect_workspace(wm_thousand_brains_bridge_t* bridge,
                                              global_workspace_t* workspace) {
    if (!bridge || !workspace) return NIMCP_ERROR_NULL_POINTER;
    bridge->workspace = workspace;
    NIMCP_LOGGING_DEBUG("wm_tb_bridge: connected to global workspace");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal: State Gathering
 * ============================================================================ */

/**
 * @brief Aggregate spatial state from all active reference frames.
 *
 * Computes mean location + compressed encoding across all bound frames.
 */
static void gather_spatial_state(wm_thousand_brains_bridge_t* bridge) {
    wm_tb_spatial_state_t* spatial = &bridge->current_state.spatial;
    memset(spatial, 0, sizeof(wm_tb_spatial_state_t));

    if (!bridge->ref_frames) return;

    column_ref_frame_manager_t* mgr = bridge->ref_frames;
    if (mgr->num_frames == 0) return;

    /* Accumulate location + encoding across frames */
    float loc_sum[3] = {0.0f, 0.0f, 0.0f};
    float enc_sum[WM_TB_MAX_SPATIAL_DIM];
    memset(enc_sum, 0, sizeof(enc_sum));
    float orient_sum = 0.0f;

    uint32_t enc_dim = WM_TB_MAX_SPATIAL_DIM;
    if (mgr->encoding_dim < enc_dim) enc_dim = mgr->encoding_dim;

    for (uint32_t i = 0; i < mgr->num_frames; i++) {
        column_reference_frame_t* frame = &mgr->frames[i];
        for (uint32_t d = 0; d < 3; d++) {
            loc_sum[d] += frame->location[d];
        }
        orient_sum += frame->orientation;

        /* Accumulate location encoding (capped to bridge dim) */
        float enc_buf[COL_REF_FRAME_ENCODING_DIM];
        if (column_ref_frame_get_location_encoding(mgr, i, enc_buf, COL_REF_FRAME_ENCODING_DIM) == 0) {
            for (uint32_t e = 0; e < enc_dim; e++) {
                enc_sum[e] += enc_buf[e];
            }
        }
    }

    /* Average */
    float inv_n = 1.0f / (float)mgr->num_frames;
    for (uint32_t d = 0; d < 3; d++) {
        spatial->location[d] = loc_sum[d] * inv_n;
    }
    spatial->orientation = orient_sum * inv_n;
    for (uint32_t e = 0; e < enc_dim; e++) {
        spatial->encoding[e] = enc_sum[e] * inv_n;
    }
    spatial->encoding_dim = enc_dim;
    spatial->num_active_frames = mgr->num_frames;

    /* Confidence: based on number of active frames and encoding variance */
    float var = 0.0f;
    for (uint32_t e = 0; e < enc_dim; e++) {
        float mean = spatial->encoding[e];
        float sq_sum = 0.0f;
        for (uint32_t i = 0; i < mgr->num_frames; i++) {
            float enc_buf[COL_REF_FRAME_ENCODING_DIM];
            if (column_ref_frame_get_location_encoding(mgr, i, enc_buf, COL_REF_FRAME_ENCODING_DIM) == 0) {
                float diff = enc_buf[e] - mean;
                sq_sum += diff * diff;
            }
        }
        var += sq_sum / (float)mgr->num_frames;
    }
    /* Low variance = high confidence */
    spatial->confidence = 1.0f / (1.0f + var);
}

/**
 * @brief Gather object state from column voting consensus.
 */
static void gather_object_state(wm_thousand_brains_bridge_t* bridge) {
    wm_tb_object_state_t* object = &bridge->current_state.object;
    memset(object, 0, sizeof(wm_tb_object_state_t));

    if (!bridge->voting) return;

    column_voting_manager_t* mgr = bridge->voting;

    object->has_consensus = column_voting_has_consensus(mgr);
    object->agreement_ratio = column_voting_get_agreement_ratio(mgr);

    if (object->has_consensus) {
        uint32_t obj_id = 0;
        float conf = 0.0f;
        if (column_voting_get_consensus(mgr, &obj_id, &conf) == 0) {
            object->object_id = obj_id;
            object->confidence = conf;
        }
    }

    /* Create distributed object embedding from object_id using multi-scale
     * hash spreading. Sparse one-hot would give poor gradient signal to WM;
     * instead we spread activation across ~25% of dims for smoother gradients
     * and better WM learning. Similar objects (nearby IDs) will have partially
     * overlapping embeddings, enabling generalization. */
    if (object->has_consensus && object->object_id > 0) {
        uint32_t h = object->object_id;
        float conf = object->confidence;
        /* Fill ~8 slots (25% of 32-dim) with decaying activations */
        for (uint32_t i = 0; i < 8; i++) {
            h = h * 2654435761u + i * 0x9E3779B9u; /* Knuth + golden ratio hash */
            uint32_t slot = h % WM_TB_MAX_OBJECT_DIM;
            float activation = conf * (1.0f - (float)i * 0.1f); /* Decay per slot */
            if (activation < 0.1f) activation = 0.1f;
            object->object_embedding[slot] += activation;
            /* Clamp to prevent unbounded accumulation */
            if (object->object_embedding[slot] > 1.0f)
                object->object_embedding[slot] = 1.0f;
        }
        /* Also encode agreement_ratio as a meta-signal in last dim */
        object->object_embedding[WM_TB_MAX_OBJECT_DIM - 1] = object->agreement_ratio;
        object->embedding_dim = WM_TB_MAX_OBJECT_DIM;
    }

    column_voting_stats_t vstats;
    if (column_voting_get_stats(mgr, &vstats) == 0) {
        object->rounds_to_consensus = (uint32_t)vstats.mean_rounds_to_consensus;
    }
}

/**
 * @brief Gather temporal prediction state from dendritic sequences.
 */
static void gather_temporal_state(wm_thousand_brains_bridge_t* bridge) {
    wm_tb_temporal_state_t* temporal = &bridge->current_state.temporal;
    memset(temporal, 0, sizeof(wm_tb_temporal_state_t));

    if (!bridge->sequences) return;

    dendritic_sequence_mgr_t* mgr = bridge->sequences;

    temporal->prediction_accuracy = dendritic_seq_get_prediction_accuracy(mgr);
    temporal->surprise_rate = dendritic_seq_get_surprise_rate(mgr);

    /* Get predicted cells and compress into temporal encoding */
    uint32_t max_predicted = WM_TB_MAX_TEMPORAL_DIM;
    dendritic_seq_get_predicted_cells(mgr, temporal->predicted_cells,
                                      max_predicted, &temporal->num_predicted);

    /* Create temporal encoding: normalized cell ID histogram.
     * Map predicted cell IDs into encoding bins. */
    if (temporal->num_predicted > 0 && mgr->num_cells > 0) {
        float inv_cells = 1.0f / (float)mgr->num_cells;
        for (uint32_t i = 0; i < temporal->num_predicted; i++) {
            uint32_t bin = (temporal->predicted_cells[i] * WM_TB_MAX_TEMPORAL_DIM) / mgr->num_cells;
            if (bin >= WM_TB_MAX_TEMPORAL_DIM) bin = WM_TB_MAX_TEMPORAL_DIM - 1;
            temporal->temporal_encoding[bin] += inv_cells;
        }
        temporal->encoding_dim = WM_TB_MAX_TEMPORAL_DIM;
    }
}

/* ============================================================================
 * Internal: State Mapping
 * ============================================================================ */

/**
 * @brief Map combined TB state into a flat WM state vector.
 *
 * Layout: [spatial_encoding | object_embedding | temporal_encoding]
 * Each section weighted by config weights.
 */
static void map_tb_to_wm_state(wm_thousand_brains_bridge_t* bridge) {
    float* buf = bridge->wm_state_buffer;
    uint32_t offset = 0;
    const wm_tb_bridge_config_t* cfg = &bridge->config;
    const wm_tb_combined_state_t* st = &bridge->current_state;

    /* Spatial component */
    for (uint32_t i = 0; i < WM_TB_MAX_SPATIAL_DIM && offset < bridge->wm_state_dim; i++) {
        buf[offset++] = st->spatial.encoding[i] * cfg->spatial_weight * st->spatial.confidence;
    }

    /* Object component */
    for (uint32_t i = 0; i < WM_TB_MAX_OBJECT_DIM && offset < bridge->wm_state_dim; i++) {
        buf[offset++] = st->object.object_embedding[i] * cfg->object_weight;
    }

    /* Temporal component */
    for (uint32_t i = 0; i < WM_TB_MAX_TEMPORAL_DIM && offset < bridge->wm_state_dim; i++) {
        buf[offset++] = st->temporal.temporal_encoding[i] * cfg->temporal_weight;
    }

    /* Zero-pad remainder */
    while (offset < bridge->wm_state_dim) {
        buf[offset++] = 0.0f;
    }
}

/**
 * @brief Extract top-down expectations from WM predicted state.
 *
 * Takes the WM's forward prediction and decomposes it back into
 * feature/movement/object expectations for cortical columns.
 */
static void extract_topdown_from_wm(wm_thousand_brains_bridge_t* bridge,
                                     const omni_wm_state_t* predicted) {
    wm_tb_topdown_t* td = &bridge->topdown;
    memset(td, 0, sizeof(wm_tb_topdown_t));

    if (!predicted || !predicted->values) return;

    float gain = bridge->config.topdown_gain;

    /* Extract feature expectation from spatial region of predicted state */
    uint32_t feat_dim = WM_TB_MAX_FEATURE_DIM;
    if (feat_dim > predicted->dim) feat_dim = predicted->dim;
    for (uint32_t i = 0; i < feat_dim; i++) {
        td->expected_feature[i] = predicted->values[i] * gain;
    }
    td->feature_dim = feat_dim;

    /* Extract movement expectation from spatial dims 0-2 */
    if (predicted->dim >= 3) {
        for (uint32_t d = 0; d < 3; d++) {
            td->expected_movement[d] = predicted->values[d] * gain;
        }
    }

    /* Extract object prediction from object region */
    if (predicted->dim > WM_TB_MAX_SPATIAL_DIM) {
        /* Find peak in object embedding region */
        uint32_t obj_start = WM_TB_MAX_SPATIAL_DIM;
        uint32_t obj_end = obj_start + WM_TB_MAX_OBJECT_DIM;
        if (obj_end > predicted->dim) obj_end = predicted->dim;

        float best_val = -1.0f;
        uint32_t best_idx = 0;
        for (uint32_t i = obj_start; i < obj_end; i++) {
            if (predicted->values[i] > best_val) {
                best_val = predicted->values[i];
                best_idx = i - obj_start;
            }
        }
        td->expected_object_id = best_idx;
    }

    td->prediction_confidence = 1.0f - predicted->uncertainty;
    if (td->prediction_confidence < 0.0f) td->prediction_confidence = 0.0f;

    /* Learning rate modulation: high surprise → boost LR */
    float surprise = bridge->current_state.temporal.surprise_rate;
    td->learning_rate_modulation = 1.0f + surprise * (bridge->config.surprise_lr_scale - 1.0f);
}

/* ============================================================================
 * Core Update Phases
 * ============================================================================ */

nimcp_error_t wm_tb_bridge_update_spatial(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->config.enable_spatial_integration) return NIMCP_SUCCESS;
    if (!bridge->ref_frames) return NIMCP_SUCCESS;

    gather_spatial_state(bridge);
    bridge->stats.spatial_updates++;

    /* Update running mean */
    float n = (float)bridge->stats.spatial_updates;
    bridge->stats.mean_spatial_confidence =
        bridge->stats.mean_spatial_confidence * ((n - 1.0f) / n) +
        bridge->current_state.spatial.confidence / n;

    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_update_consensus(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->config.enable_voting_integration) return NIMCP_SUCCESS;
    if (!bridge->voting) return NIMCP_SUCCESS;

    gather_object_state(bridge);
    bridge->stats.consensus_updates++;

    /* Update running mean consensus confidence */
    if (bridge->current_state.object.has_consensus) {
        float n = (float)bridge->stats.consensus_updates;
        bridge->stats.mean_consensus_confidence =
            bridge->stats.mean_consensus_confidence * ((n - 1.0f) / n) +
            bridge->current_state.object.confidence / n;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_update_temporal(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->config.enable_sequence_integration) return NIMCP_SUCCESS;
    if (!bridge->sequences) return NIMCP_SUCCESS;

    gather_temporal_state(bridge);
    bridge->stats.temporal_updates++;

    /* Update running means */
    float n = (float)bridge->stats.temporal_updates;
    bridge->stats.mean_prediction_accuracy =
        bridge->stats.mean_prediction_accuracy * ((n - 1.0f) / n) +
        bridge->current_state.temporal.prediction_accuracy / n;
    bridge->stats.mean_surprise_rate =
        bridge->stats.mean_surprise_rate * ((n - 1.0f) / n) +
        bridge->current_state.temporal.surprise_rate / n;

    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_push_to_world_model(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->world_model) return NIMCP_SUCCESS; /* No WM yet — skip silently */

    /* Guard: don't push empty/zero state to WM — this would collapse WM state.
     * Require at least one TB component to have produced non-trivial data. */
    bool has_spatial = bridge->ref_frames && bridge->current_state.spatial.num_active_frames > 0;
    bool has_object = bridge->voting && bridge->current_state.object.has_consensus;
    bool has_temporal = bridge->sequences && bridge->current_state.temporal.num_predicted > 0;

    if (!has_spatial && !has_object && !has_temporal) {
        /* No TB data available — don't zero out WM state */
        return NIMCP_SUCCESS;
    }

    /* Map TB state → flat WM state vector */
    map_tb_to_wm_state(bridge);

    /* Create WM state from our buffer and push it */
    omni_wm_state_t* tb_state = omni_wm_state_from_values(bridge->wm_state_buffer,
                                                            bridge->wm_state_dim);
    if (!tb_state) return NIMCP_ERROR_OUT_OF_MEMORY;

    /* Get current WM state for the transition update */
    const omni_wm_state_t* current_wm = omni_wm_get_state(bridge->world_model);

    if (current_wm) {
        /* Provide a transition observation: null action, zero reward.
         * The TB state serves as a grounding observation for the WM. */
        float null_action[1] = {0.0f};
        omni_wm_update(bridge->world_model, current_wm, null_action, 1, tb_state, 0.0f);
    }

    /* Also set as current state (TB grounds the WM).
     * NOTE: omni_wm_set_state() copies the state internally (verified),
     * so it's safe to destroy tb_state after this call. */
    omni_wm_set_state(bridge->world_model, tb_state);
    omni_wm_state_destroy(tb_state);

    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_generate_topdown(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->config.enable_topdown_predictions) return NIMCP_SUCCESS;
    if (!bridge->world_model) return NIMCP_SUCCESS;

    /* Get WM forward prediction (null action = what happens without intervention) */
    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));

    float null_action[1] = {0.0f};
    nimcp_error_t err = omni_wm_predict_forward(bridge->world_model,
                                                  null_action, 1, &transition);
    if (err != NIMCP_SUCCESS) {
        /* WM prediction failed — clear top-down */
        memset(&bridge->topdown, 0, sizeof(wm_tb_topdown_t));
        return NIMCP_SUCCESS; /* Non-fatal */
    }

    /* Extract top-down expectations from predicted state */
    extract_topdown_from_wm(bridge, transition.next_state);
    bridge->stats.topdown_predictions++;

    /* If movement priors are enabled, feed predicted movement to reference frames */
    if (bridge->config.enable_movement_priors && bridge->ref_frames) {
        for (uint32_t i = 0; i < bridge->ref_frames->num_frames; i++) {
            column_ref_frame_update_location(bridge->ref_frames, i,
                                              bridge->topdown.expected_movement);
        }
        bridge->stats.movement_priors_sent++;
    }

    /* Clean up transition result if it allocated state */
    if (transition.next_state) {
        omni_wm_state_destroy(transition.next_state);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_modulate_surprise(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->config.enable_surprise_modulation) return NIMCP_SUCCESS;
    if (!bridge->world_model) return NIMCP_SUCCESS;

    float surprise = bridge->current_state.temporal.surprise_rate;

    /* High surprise → boost WM learning rate (more to learn from unexpected inputs) */
    if (surprise > 0.5f) {
        /* Modulate WM learning rate: base_lr * (1 + surprise * scale) */
        float lr_mod = bridge->topdown.learning_rate_modulation;
        /* We don't know the base LR, so just apply a multiplicative boost.
         * The WM set_learning_rate API expects absolute value — but we
         * don't want to override a carefully tuned LR. Instead, we log
         * the signal for the training loop to pick up. */
        bridge->stats.surprise_modulations++;

        NIMCP_LOGGING_DEBUG("wm_tb_bridge: surprise=%.3f, lr_mod=%.2f",
                            surprise, lr_mod);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Full Step
 * ============================================================================ */

nimcp_error_t wm_tb_bridge_step(wm_thousand_brains_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    wm_tb_bridge_heartbeat_instance(bridge->health_agent, "step", 0.0f);

    nimcp_error_t err;

    /* Phase 1: Gather spatial from reference frames */
    err = wm_tb_bridge_update_spatial(bridge);
    if (err != NIMCP_SUCCESS) return err;

    wm_tb_bridge_heartbeat_instance(bridge->health_agent, "step", 0.2f);

    /* Phase 2: Gather consensus from column voting */
    err = wm_tb_bridge_update_consensus(bridge);
    if (err != NIMCP_SUCCESS) return err;

    wm_tb_bridge_heartbeat_instance(bridge->health_agent, "step", 0.4f);

    /* Phase 3: Gather temporal from dendritic sequences */
    err = wm_tb_bridge_update_temporal(bridge);
    if (err != NIMCP_SUCCESS) return err;

    wm_tb_bridge_heartbeat_instance(bridge->health_agent, "step", 0.6f);

    /* Phase 4: Push combined state to world model */
    err = wm_tb_bridge_push_to_world_model(bridge);
    if (err != NIMCP_SUCCESS) return err;

    wm_tb_bridge_heartbeat_instance(bridge->health_agent, "step", 0.8f);

    /* Phase 5: Generate top-down predictions back to columns */
    err = wm_tb_bridge_generate_topdown(bridge);
    if (err != NIMCP_SUCCESS) return err;

    /* Phase 6: Modulate learning based on surprise */
    err = wm_tb_bridge_modulate_surprise(bridge);
    if (err != NIMCP_SUCCESS) return err;

    /* Update timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    bridge->current_state.timestamp =
        (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;

    bridge->base.total_updates++;

    wm_tb_bridge_heartbeat_instance(bridge->health_agent, "step", 1.0f);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

nimcp_error_t wm_tb_bridge_get_state(const wm_thousand_brains_bridge_t* bridge,
                                      wm_tb_combined_state_t* state) {
    if (!bridge || !state) return NIMCP_ERROR_NULL_POINTER;
    *state = bridge->current_state;
    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_get_topdown(const wm_thousand_brains_bridge_t* bridge,
                                        wm_tb_topdown_t* topdown) {
    if (!bridge || !topdown) return NIMCP_ERROR_NULL_POINTER;
    *topdown = bridge->topdown;
    return NIMCP_SUCCESS;
}

nimcp_error_t wm_tb_bridge_get_stats(const wm_thousand_brains_bridge_t* bridge,
                                      wm_tb_bridge_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_NULL_POINTER;
    *stats = bridge->stats;
    return NIMCP_SUCCESS;
}
