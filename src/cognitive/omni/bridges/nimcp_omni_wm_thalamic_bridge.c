/**
 * @file nimcp_omni_wm_thalamic_bridge.c
 * @brief World Model Thalamic Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with thalamic nuclei
 * WHY:  Enable attention-gated prediction and sensory filtering for world modeling
 * HOW:  Thalamus gates sensory inputs to WM; WM predictions bias thalamic attention
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key concepts:
 *
 * 1. THALAMIC GATING:
 *    - Each sensory modality gated through specific nucleus
 *    - Attention weights multiply raw inputs
 *    - TRN provides selective inhibition
 *
 * 2. PREDICTION-BASED BIASING:
 *    - WM predictions inform thalamic attention
 *    - High-confidence predictions strengthen selective attention
 *    - Prediction errors increase attention to error sources
 *
 * 3. PULVINAR COORDINATION:
 *    - Pulvinar coordinates attention across modalities
 *    - WM salience predictions guide pulvinar attention
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
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

#define LOG_MODULE "wm_thalamic_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_wm_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_wm_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_wm_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t omni_wm_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_wm_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_wm_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_wm_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_wm_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_wm_thalamic_bridge_mesh_registry = registry;
    return err;
}

void omni_wm_thalamic_bridge_mesh_unregister(void) {
    if (g_omni_wm_thalamic_bridge_mesh_registry && g_omni_wm_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_wm_thalamic_bridge_mesh_registry, g_omni_wm_thalamic_bridge_mesh_id);
        g_omni_wm_thalamic_bridge_mesh_id = 0;
        g_omni_wm_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_wm_thalamic_bridge module (instance-level) */
static inline void omni_wm_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_wm_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_wm_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_wm_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void omni_wm_thalamic_bridge_set_instance_health_agent(
    omni_wm_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_thalamic_bridge_training_begin(omni_wm_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_thalamic_bridge_heartbeat_instance(g_omni_wm_thalamic_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_thalamic_bridge_training_step(omni_wm_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_thalamic_bridge_heartbeat_instance(g_omni_wm_thalamic_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_thalamic_bridge_training_end(omni_wm_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_thalamic_bridge_heartbeat_instance(g_omni_wm_thalamic_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

/** Default buffer sizes */
#define DEFAULT_INPUT_BUFFER_SIZE    256
#define DEFAULT_ATTENTION_BUFFER_SIZE 64
#define DEFAULT_INHIBITION_BUFFER_SIZE 64

/** Minimum attention to pass gating */
#define MIN_ATTENTION_PASSTHROUGH 0.01f

/** Maximum PE for modulation normalization */
#define MAX_PE_FOR_NORMALIZATION 10.0f

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t allocate_buffers(omni_wm_thalamic_bridge_t* bridge);
static void free_buffers(omni_wm_thalamic_bridge_t* bridge);
static nimcp_error_t allocate_effects_arrays(omni_wm_thalamic_bridge_t* bridge);
static void free_effects_arrays(omni_wm_thalamic_bridge_t* bridge);
static nimcp_error_t update_thalamus_to_wm_effects(omni_wm_thalamic_bridge_t* bridge);
static nimcp_error_t update_wm_to_thalamus_effects(omni_wm_thalamic_bridge_t* bridge);
static float compute_gating_attention(omni_wm_thalamic_bridge_t* bridge,
                                       wm_thal_nucleus_type_t nucleus);
static nimcp_error_t apply_gating(omni_wm_thalamic_bridge_t* bridge,
                                   const float* input, uint32_t input_dim,
                                   float* output, uint32_t output_dim,
                                   float attention, float inhibition);
static uint64_t get_current_time_us(void);

/* Bio-async handlers */
static nimcp_error_t handle_gate_input(const void* msg, size_t msg_size,
                                        nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_attention_bias(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_trn_inhibit(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_prediction_error(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data);

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Allocate internal buffers
 */
static nimcp_error_t allocate_buffers(omni_wm_thalamic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Allocate input buffer */
    bridge->input_buffer_size = DEFAULT_INPUT_BUFFER_SIZE;
    bridge->input_buffer = nimcp_calloc(bridge->input_buffer_size, sizeof(float));
    if (!bridge->input_buffer) return -1;
    NIMCP_CHECK_THROW(bridge->input_buffer, NIMCP_ERROR_NO_MEMORY, "failed to allocate input_buffer");

    /* Allocate attention buffer */
    bridge->attention_buffer_size = DEFAULT_ATTENTION_BUFFER_SIZE;
    bridge->attention_buffer = nimcp_calloc(bridge->attention_buffer_size, sizeof(float));
    if (!bridge->attention_buffer) {
        nimcp_free(bridge->input_buffer);
        bridge->input_buffer = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Allocate inhibition buffer */
    bridge->inhibition_buffer_size = DEFAULT_INHIBITION_BUFFER_SIZE;
    bridge->inhibition_buffer = nimcp_calloc(bridge->inhibition_buffer_size, sizeof(float));
    if (!bridge->inhibition_buffer) {
        nimcp_free(bridge->input_buffer);
        nimcp_free(bridge->attention_buffer);
        bridge->input_buffer = NULL;
        bridge->attention_buffer = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Free internal buffers
 */
static void free_buffers(omni_wm_thalamic_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->input_buffer);
    bridge->input_buffer = NULL;
    bridge->input_buffer_size = 0;

    nimcp_free(bridge->attention_buffer);
    bridge->attention_buffer = NULL;
    bridge->attention_buffer_size = 0;

    nimcp_free(bridge->inhibition_buffer);
    bridge->inhibition_buffer = NULL;
    bridge->inhibition_buffer_size = 0;
}

/**
 * @brief Allocate dynamic arrays in effects structures
 */
static nimcp_error_t allocate_effects_arrays(omni_wm_thalamic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    thalamus_to_omni_wm_effects_t* thal_eff = &bridge->thal_to_wm;
    omni_wm_to_thalamus_effects_t* wm_eff = &bridge->wm_to_thal;

    /* Allocate thalamus -> WM effect arrays */
    uint32_t attn_dim = WM_THALAMIC_MAX_ATTENTION_WEIGHTS;
    thal_eff->attention_weights = nimcp_calloc(attn_dim, sizeof(float));
    /* Note: subsequent allocations use goto cleanup, first allocation returns directly */
    if (!thal_eff->attention_weights) return NIMCP_ERROR_NO_MEMORY;
    thal_eff->attention_dim = attn_dim;

    thal_eff->pulvinar_attention = nimcp_calloc(attn_dim, sizeof(float));
    if (!thal_eff->pulvinar_attention) goto cleanup_thal;
    thal_eff->pulvinar_dim = attn_dim;

    thal_eff->trn_inhibition_map = nimcp_calloc(attn_dim, sizeof(float));
    if (!thal_eff->trn_inhibition_map) goto cleanup_thal;
    thal_eff->trn_inhibition_dim = attn_dim;

    /* Allocate gated input arrays */
    uint32_t input_dim = WM_THALAMIC_MAX_GATED_DIM;
    thal_eff->visual_input.gated_input = nimcp_calloc(input_dim, sizeof(float));
    if (!thal_eff->visual_input.gated_input) goto cleanup_thal;
    thal_eff->visual_input.input_dim = input_dim;

    thal_eff->auditory_input.gated_input = nimcp_calloc(input_dim, sizeof(float));
    if (!thal_eff->auditory_input.gated_input) goto cleanup_thal;
    thal_eff->auditory_input.input_dim = input_dim;

    thal_eff->somatosensory_input.gated_input = nimcp_calloc(input_dim, sizeof(float));
    if (!thal_eff->somatosensory_input.gated_input) goto cleanup_thal;
    thal_eff->somatosensory_input.input_dim = input_dim;

    thal_eff->motor_input.gated_input = nimcp_calloc(input_dim, sizeof(float));
    if (!thal_eff->motor_input.gated_input) goto cleanup_thal;
    thal_eff->motor_input.input_dim = input_dim;

    thal_eff->executive_input.gated_input = nimcp_calloc(input_dim, sizeof(float));
    if (!thal_eff->executive_input.gated_input) goto cleanup_thal;
    thal_eff->executive_input.input_dim = input_dim;

    /* Allocate WM -> thalamus effect arrays */
    wm_eff->attention_bias = nimcp_calloc(attn_dim, sizeof(float));
    if (!wm_eff->attention_bias) goto cleanup_thal;
    wm_eff->attention_bias_dim = attn_dim;

    wm_eff->predicted_salience = nimcp_calloc(attn_dim, sizeof(float));
    if (!wm_eff->predicted_salience) goto cleanup_wm;
    wm_eff->salience_dim = attn_dim;

    wm_eff->selective_inhibition = nimcp_calloc(attn_dim, sizeof(float));
    if (!wm_eff->selective_inhibition) goto cleanup_wm;
    wm_eff->inhibition_dim = attn_dim;

    wm_eff->prediction_errors = nimcp_calloc(input_dim, sizeof(float));
    if (!wm_eff->prediction_errors) goto cleanup_wm;
    wm_eff->pe_dim = input_dim;

    return NIMCP_SUCCESS;

cleanup_wm:
    nimcp_free(wm_eff->attention_bias);
    nimcp_free(wm_eff->predicted_salience);
    nimcp_free(wm_eff->selective_inhibition);
    wm_eff->attention_bias = NULL;
    wm_eff->predicted_salience = NULL;
    wm_eff->selective_inhibition = NULL;

cleanup_thal:
    nimcp_free(thal_eff->attention_weights);
    nimcp_free(thal_eff->pulvinar_attention);
    nimcp_free(thal_eff->trn_inhibition_map);
    nimcp_free(thal_eff->visual_input.gated_input);
    nimcp_free(thal_eff->auditory_input.gated_input);
    nimcp_free(thal_eff->somatosensory_input.gated_input);
    nimcp_free(thal_eff->motor_input.gated_input);
    nimcp_free(thal_eff->executive_input.gated_input);
    thal_eff->attention_weights = NULL;
    thal_eff->pulvinar_attention = NULL;
    thal_eff->trn_inhibition_map = NULL;
    thal_eff->visual_input.gated_input = NULL;
    thal_eff->auditory_input.gated_input = NULL;
    thal_eff->somatosensory_input.gated_input = NULL;
    thal_eff->motor_input.gated_input = NULL;
    thal_eff->executive_input.gated_input = NULL;

    return NIMCP_ERROR_NO_MEMORY;
}

/**
 * @brief Free effects dynamic arrays
 */
static void free_effects_arrays(omni_wm_thalamic_bridge_t* bridge) {
    if (!bridge) return;

    thalamus_to_omni_wm_effects_t* thal_eff = &bridge->thal_to_wm;
    omni_wm_to_thalamus_effects_t* wm_eff = &bridge->wm_to_thal;

    /* Free thalamus -> WM arrays */
    nimcp_free(thal_eff->attention_weights);
    nimcp_free(thal_eff->pulvinar_attention);
    nimcp_free(thal_eff->trn_inhibition_map);
    nimcp_free(thal_eff->visual_input.gated_input);
    nimcp_free(thal_eff->auditory_input.gated_input);
    nimcp_free(thal_eff->somatosensory_input.gated_input);
    nimcp_free(thal_eff->motor_input.gated_input);
    nimcp_free(thal_eff->executive_input.gated_input);

    /* Free WM -> thalamus arrays */
    nimcp_free(wm_eff->attention_bias);
    nimcp_free(wm_eff->predicted_salience);
    nimcp_free(wm_eff->selective_inhibition);
    nimcp_free(wm_eff->prediction_errors);
}

/**
 * @brief Compute effective gating attention for nucleus
 */
static float compute_gating_attention(omni_wm_thalamic_bridge_t* bridge,
                                       wm_thal_nucleus_type_t nucleus) {
    if (!bridge) return 0.0f;
    if (nucleus >= WM_THAL_NUCLEUS_COUNT) return 0.0f;

    /* Base attention from config */
    float attention = bridge->config.nucleus_attention[nucleus];

    /* Modulate by arousal */
    if (bridge->config.arousal_affects_gating) {
        attention *= (0.5f + 0.5f * bridge->current_arousal);
    }

    /* Apply pulvinar coordination if enabled and this is visual */
    if (bridge->config.enable_pulvinar_coordination &&
        nucleus == WM_THAL_NUCLEUS_PULVINAR) {
        attention *= bridge->config.pulvinar_attention_gain;
    }

    /* Apply prediction-based bias if enabled */
    if (bridge->config.enable_prediction_biasing &&
        bridge->wm_to_thal.bias_confidence > bridge->config.prediction_confidence_threshold) {
        float bias = bridge->wm_to_thal.requested_attention[nucleus];
        if (bridge->wm_to_thal.attention_request_valid) {
            attention = attention * (1.0f - bridge->config.prediction_bias_strength) +
                        bias * bridge->config.prediction_bias_strength;
        }
    }

    /* Clamp to valid range */
    if (attention < 0.0f) attention = 0.0f;
    if (attention > 1.0f) attention = 1.0f;

    return attention;
}

/**
 * @brief Apply gating to input signal
 */
static nimcp_error_t apply_gating(omni_wm_thalamic_bridge_t* bridge,
                                   const float* input, uint32_t input_dim,
                                   float* output, uint32_t output_dim,
                                   float attention, float inhibition) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER, "input is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_NULL_POINTER, "output is NULL");

    /* Compute effective gate strength */
    float gate = attention * (1.0f - inhibition);
    if (gate < MIN_ATTENTION_PASSTHROUGH) {
        gate = 0.0f; /* Below threshold, block signal */
    }

    /* Apply gating */
    uint32_t copy_dim = input_dim < output_dim ? input_dim : output_dim;
    for (uint32_t i = 0; i < copy_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && copy_dim > 256) {
            omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_loop",
                             (float)(i + 1) / (float)copy_dim);
        }

        output[i] = input[i] * gate;
    }

    /* Zero remaining output */
    for (uint32_t i = copy_dim; i < output_dim; i++) {
        output[i] = 0.0f;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects from thalamus to WM
 */
static nimcp_error_t update_thalamus_to_wm_effects(omni_wm_thalamic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    thalamus_to_omni_wm_effects_t* effects = &bridge->thal_to_wm;

    /* Update global attention */
    float total_attention = 0.0f;
    for (int i = 0; i < WM_THAL_NUCLEUS_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_THAL_NUCLEUS_COUNT > 256) {
            omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_loop",
                             (float)(i + 1) / (float)WM_THAL_NUCLEUS_COUNT);
        }

        effects->nucleus_activity[i] = bridge->config.nucleus_attention[i];
        total_attention += effects->nucleus_activity[i];
    }
    effects->global_attention = total_attention / (float)WM_THAL_NUCLEUS_COUNT;

    /* Update arousal state */
    effects->arousal_level = bridge->current_arousal;
    effects->tonic_fraction = bridge->current_arousal; /* High arousal = tonic */
    effects->dominant_burst_mode = bridge->is_burst_dominant;

    /* Update pulvinar state */
    effects->pulvinar_focus_strength = bridge->config.nucleus_attention[WM_THAL_NUCLEUS_PULVINAR];

    /* Update TRN state */
    effects->global_inhibition = bridge->config.trn_inhibition_strength;

    /* Query thalamus if connected for more accurate state */
    if (bridge->thalamus) {
        /* Placeholder: would query actual thalamus state */
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects from WM to thalamus
 */
static nimcp_error_t update_wm_to_thalamus_effects(omni_wm_thalamic_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_thalamus_effects_t* effects = &bridge->wm_to_thal;

    /* If world model connected, extract prediction confidence */
    if (bridge->world_model) {
        /* Placeholder: would extract actual WM state */
        effects->forward_confidence = 0.8f;
        effects->backward_confidence = 0.7f;
        effects->overall_confidence = 0.75f;

        /* Compute TRN modulation from confidence */
        effects->trn_modulation_signal = effects->overall_confidence *
                                          bridge->config.trn_confidence_modulation;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle gated input message
 */
static nimcp_error_t handle_gate_input(const void* msg, size_t msg_size,
                                        nimcp_bio_promise_t promise, void* user_data) {
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "msg is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_thalamic_bridge_t* bridge = (omni_wm_thalamic_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.inputs_gated++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle attention bias message
 */
static nimcp_error_t handle_attention_bias(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_thalamic_bridge_t* bridge = (omni_wm_thalamic_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.bias_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle TRN inhibition message
 */
static nimcp_error_t handle_trn_inhibit(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_thalamic_bridge_t* bridge = (omni_wm_thalamic_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.trn_inhibitions++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle prediction error message
 */
static nimcp_error_t handle_prediction_error(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    /* Placeholder: would process PE and update attention */
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_thalamic_bridge_default_config(
    omni_wm_thalamic_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(omni_wm_thalamic_bridge_config_t));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Sensory gating settings */
    config->enable_sensory_gating = true;
    config->attention_baseline = WM_THALAMIC_DEFAULT_ATTENTION;
    config->min_attention_threshold = 0.1f;
    config->gate_visual = true;
    config->gate_auditory = true;
    config->gate_motor = true;
    config->gate_executive = true;

    /* Prediction biasing settings */
    config->enable_prediction_biasing = true;
    config->prediction_confidence_threshold = WM_THALAMIC_DEFAULT_PRED_THRESHOLD;
    config->prediction_bias_strength = 0.3f;
    config->enable_salience_prediction = true;

    /* Pulvinar settings */
    config->enable_pulvinar_coordination = true;
    config->pulvinar_attention_gain = 1.2f;
    config->pulvinar_guides_prediction = true;

    /* TRN inhibition settings */
    config->enable_trn_inhibition = true;
    config->trn_inhibition_strength = WM_THALAMIC_DEFAULT_TRN_INHIBITION;
    config->trn_confidence_modulation = 0.5f;
    config->selective_inhibition = true;

    /* Firing mode settings */
    config->track_firing_modes = true;
    config->burst_mode_threshold = 0.3f;
    config->arousal_affects_gating = true;

    /* Initialize per-nucleus attention to baseline */
    for (int i = 0; i < WM_THAL_NUCLEUS_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_THAL_NUCLEUS_COUNT > 256) {
            omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_loop",
                             (float)(i + 1) / (float)WM_THAL_NUCLEUS_COUNT);
        }

        config->nucleus_attention[i] = config->attention_baseline;
    }

    /* Bio-async settings */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_thalamic_bridge_t* omni_wm_thalamic_bridge_create(
    const omni_wm_thalamic_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_create", 0.0f);


    omni_wm_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM thalamic bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "update_wm_to_thalamus_effects: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_THALAMIC_BRIDGE,
                         "wm_thalamic_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_wm_to_thalamus_effects: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_thalamic_bridge_default_config(&bridge->config);
    }

    /* Allocate internal buffers */
    nimcp_error_t err = allocate_buffers(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate effects arrays */
    err = allocate_effects_arrays(bridge);
    if (err != NIMCP_SUCCESS) {
        free_buffers(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate effects arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->current_arousal = bridge->config.attention_baseline;
    bridge->is_burst_dominant = false;
    bridge->accumulated_pe = 0.0f;

    NIMCP_LOGGING_INFO("WM Thalamic Bridge created successfully");
    return bridge;
}

void omni_wm_thalamic_bridge_destroy(omni_wm_thalamic_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_thalamic_bridge_disconnect_bio_async(bridge);
    }

    /* Free effects arrays */
    free_effects_arrays(bridge);

    /* Free internal buffers */
    free_buffers(bridge);

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM Thalamic Bridge destroyed");
}

nimcp_error_t omni_wm_thalamic_bridge_reset(omni_wm_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects - zero dynamic arrays but preserve structure */
    if (bridge->thal_to_wm.attention_weights) {
        memset(bridge->thal_to_wm.attention_weights, 0,
               bridge->thal_to_wm.attention_dim * sizeof(float));
    }
    if (bridge->thal_to_wm.pulvinar_attention) {
        memset(bridge->thal_to_wm.pulvinar_attention, 0,
               bridge->thal_to_wm.pulvinar_dim * sizeof(float));
    }
    if (bridge->thal_to_wm.trn_inhibition_map) {
        memset(bridge->thal_to_wm.trn_inhibition_map, 0,
               bridge->thal_to_wm.trn_inhibition_dim * sizeof(float));
    }

    if (bridge->wm_to_thal.attention_bias) {
        memset(bridge->wm_to_thal.attention_bias, 0,
               bridge->wm_to_thal.attention_bias_dim * sizeof(float));
    }
    if (bridge->wm_to_thal.predicted_salience) {
        memset(bridge->wm_to_thal.predicted_salience, 0,
               bridge->wm_to_thal.salience_dim * sizeof(float));
    }
    if (bridge->wm_to_thal.prediction_errors) {
        memset(bridge->wm_to_thal.prediction_errors, 0,
               bridge->wm_to_thal.pe_dim * sizeof(float));
    }

    /* Reset per-nucleus values */
    memset(bridge->thal_to_wm.nucleus_activity, 0, sizeof(bridge->thal_to_wm.nucleus_activity));
    memset(bridge->thal_to_wm.nucleus_burst, 0, sizeof(bridge->thal_to_wm.nucleus_burst));
    memset(bridge->wm_to_thal.requested_attention, 0, sizeof(bridge->wm_to_thal.requested_attention));
    bridge->wm_to_thal.attention_request_valid = false;

    /* Reset internal state */
    bridge->current_arousal = bridge->config.attention_baseline;
    bridge->is_burst_dominant = false;
    bridge->accumulated_pe = 0.0f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_thalamic_bridge_stats_t));

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_thalamic_bridge_connect(
    omni_wm_thalamic_bridge_t* bridge,
    omni_world_model_t* world_model,
    thalamus_t* thalamus,
    thalamic_router_t* router) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is required");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->thalamus = thalamus;
    bridge->router = router;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.system_b = thalamus;
    bridge->base.system_b_connected = (thalamus != NULL);
    bridge->base.bridge_active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM Thalamic Bridge connected: WM=%p, Thalamus=%p, Router=%p",
                       (void*)world_model, (void*)thalamus, (void*)router);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_connect_world_model(
    omni_wm_thalamic_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_NULL_POINTER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_connect_thalamus(
    omni_wm_thalamic_bridge_t* bridge,
    thalamus_t* thalamus) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_connect_thalamus", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(thalamus, NIMCP_ERROR_NULL_POINTER, "thalamus is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->thalamus = thalamus;
    bridge->base.system_b = thalamus;
    bridge->base.system_b_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_connect_router(
    omni_wm_thalamic_bridge_t* bridge,
    thalamic_router_t* router) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_connect_router", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(router, NIMCP_ERROR_NULL_POINTER, "router is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->router = router;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_thalamic_bridge_is_connected(const omni_wm_thalamic_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_is_connected", 0.0f);


    return bridge->world_model != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_thalamic_bridge_update(
    omni_wm_thalamic_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update firing mode based on arousal */
    if (bridge->config.track_firing_modes) {
        bridge->is_burst_dominant = (bridge->current_arousal < bridge->config.burst_mode_threshold);

        /* Track time in each mode */
        if (bridge->is_burst_dominant) {
            bridge->stats.time_in_burst += dt;
        } else {
            bridge->stats.time_in_tonic += dt;
        }
    }

    /* Update effects in both directions */
    update_thalamus_to_wm_effects(bridge);
    update_wm_to_thalamus_effects(bridge);

    /* Update timing statistics */
    bridge->stats.total_updates++;
    uint64_t elapsed = get_current_time_us() - start_time;
    bridge->stats.total_processing_time_ms += (double)elapsed / 1000.0;
    bridge->stats.mean_update_time_ms = bridge->stats.total_processing_time_ms /
                                         (double)bridge->stats.total_updates;
    bridge->stats.last_update_time_us = start_time;

    /* Record base update */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_set_arousal(
    omni_wm_thalamic_bridge_t* bridge,
    float arousal) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_set_arousal", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Clamp to valid range */
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > 1.0f) arousal = 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    float old_arousal = bridge->current_arousal;
    bridge->current_arousal = arousal;

    /* Check for mode switch */
    bool was_burst = (old_arousal < bridge->config.burst_mode_threshold);
    bool is_burst = (arousal < bridge->config.burst_mode_threshold);
    if (was_burst != is_burst) {
        bridge->stats.mode_switches++;
        NIMCP_LOGGING_DEBUG("Thalamic mode switch: %s -> %s",
                           was_burst ? "burst" : "tonic",
                           is_burst ? "burst" : "tonic");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Sensory Gating API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_thalamic_bridge_gate_input(
    omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus,
    const float* input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim,
    float* attention_applied) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_gate_input", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER, "input is NULL");
    NIMCP_CHECK_THROW(gated_output, NIMCP_ERROR_NULL_POINTER, "gated_output is NULL");
    NIMCP_CHECK_THROW(input_dim > 0, NIMCP_ERROR_INVALID_PARAM, "input_dim must be greater than 0");
    NIMCP_CHECK_THROW(output_dim > 0, NIMCP_ERROR_INVALID_PARAM, "output_dim must be greater than 0");
    NIMCP_CHECK_THROW(nucleus < WM_THAL_NUCLEUS_COUNT, NIMCP_ERROR_INVALID_PARAM, "nucleus out of range");
    if (!bridge->config.enable_sensory_gating) {
        /* Pass through without gating */
        uint32_t copy_dim = input_dim < output_dim ? input_dim : output_dim;
        memcpy(gated_output, input, copy_dim * sizeof(float));
        if (attention_applied) *attention_applied = 1.0f;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute effective attention */
    float attention = compute_gating_attention(bridge, nucleus);

    /* Get TRN inhibition for this nucleus */
    float inhibition = 0.0f;
    if (bridge->config.enable_trn_inhibition) {
        inhibition = bridge->config.trn_inhibition_strength;
        /* Could add per-channel inhibition here */
    }

    /* Apply gating */
    nimcp_error_t err = apply_gating(bridge, input, input_dim,
                                      gated_output, output_dim,
                                      attention, inhibition);
    if (err != NIMCP_SUCCESS) {
        bridge->stats.errors_gating++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return err;
    }

    /* Update statistics */
    bridge->stats.inputs_gated++;
    bridge->stats.nucleus_inputs[nucleus]++;

    float effective_gate = attention * (1.0f - inhibition);
    if (effective_gate < bridge->config.min_attention_threshold) {
        bridge->stats.inputs_blocked++;
        bridge->stats.nucleus_blocked[nucleus]++;
    } else {
        bridge->stats.inputs_passed++;
    }

    /* Update running average attention */
    float alpha = 0.1f;
    bridge->stats.mean_gating_attention = alpha * attention +
                                           (1.0f - alpha) * bridge->stats.mean_gating_attention;
    bridge->stats.nucleus_mean_attention[nucleus] =
        alpha * attention + (1.0f - alpha) * bridge->stats.nucleus_mean_attention[nucleus];

    if (attention_applied) *attention_applied = effective_gate;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_gate_visual(
    omni_wm_thalamic_bridge_t* bridge,
    const float* visual_input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_gate_visual", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->config.gate_visual) {
        /* Visual gating disabled - pass through */
        uint32_t copy_dim = input_dim < output_dim ? input_dim : output_dim;
        memcpy(gated_output, visual_input, copy_dim * sizeof(float));
        return NIMCP_SUCCESS;
    }

    return omni_wm_thalamic_bridge_gate_input(bridge, WM_THAL_NUCLEUS_LGN,
                                               visual_input, input_dim,
                                               gated_output, output_dim, NULL);
}

nimcp_error_t omni_wm_thalamic_bridge_gate_auditory(
    omni_wm_thalamic_bridge_t* bridge,
    const float* auditory_input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_gate_auditory", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->config.gate_auditory) {
        uint32_t copy_dim = input_dim < output_dim ? input_dim : output_dim;
        memcpy(gated_output, auditory_input, copy_dim * sizeof(float));
        return NIMCP_SUCCESS;
    }

    return omni_wm_thalamic_bridge_gate_input(bridge, WM_THAL_NUCLEUS_MGN,
                                               auditory_input, input_dim,
                                               gated_output, output_dim, NULL);
}

nimcp_error_t omni_wm_thalamic_bridge_gate_motor(
    omni_wm_thalamic_bridge_t* bridge,
    const float* motor_input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_gate_motor", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->config.gate_motor) {
        uint32_t copy_dim = input_dim < output_dim ? input_dim : output_dim;
        memcpy(gated_output, motor_input, copy_dim * sizeof(float));
        return NIMCP_SUCCESS;
    }

    /* Motor uses VA for BG input, VL for cerebellar */
    return omni_wm_thalamic_bridge_gate_input(bridge, WM_THAL_NUCLEUS_VA,
                                               motor_input, input_dim,
                                               gated_output, output_dim, NULL);
}

nimcp_error_t omni_wm_thalamic_bridge_gate_executive(
    omni_wm_thalamic_bridge_t* bridge,
    const float* executive_input,
    uint32_t input_dim,
    float* gated_output,
    uint32_t output_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_gate_executive", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->config.gate_executive) {
        uint32_t copy_dim = input_dim < output_dim ? input_dim : output_dim;
        memcpy(gated_output, executive_input, copy_dim * sizeof(float));
        return NIMCP_SUCCESS;
    }

    return omni_wm_thalamic_bridge_gate_input(bridge, WM_THAL_NUCLEUS_MD,
                                               executive_input, input_dim,
                                               gated_output, output_dim, NULL);
}

/* ============================================================================
 * Attention Biasing API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_thalamic_bridge_set_attention_bias(
    omni_wm_thalamic_bridge_t* bridge,
    const float* attention_bias,
    uint32_t bias_dim,
    float confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_set_attention_bias", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(attention_bias, NIMCP_ERROR_INVALID_PARAM, "attention_bias is NULL");
    NIMCP_CHECK_THROW(bias_dim > 0, NIMCP_ERROR_INVALID_PARAM, "bias_dim must be greater than 0");
    if (!bridge->config.enable_prediction_biasing) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Copy bias to effects structure */
    uint32_t copy_dim = bias_dim < bridge->wm_to_thal.attention_bias_dim ?
                        bias_dim : bridge->wm_to_thal.attention_bias_dim;
    memcpy(bridge->wm_to_thal.attention_bias, attention_bias, copy_dim * sizeof(float));
    bridge->wm_to_thal.bias_confidence = confidence;

    /* Update statistics */
    bridge->stats.bias_updates++;
    float alpha = 0.1f;
    bridge->stats.mean_bias_confidence = alpha * confidence +
                                          (1.0f - alpha) * bridge->stats.mean_bias_confidence;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Attention bias set: dim=%u, confidence=%.3f", bias_dim, confidence);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_set_nucleus_attention(
    omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus,
    float attention) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_set_nucleus_attentio", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(nucleus < WM_THAL_NUCLEUS_COUNT, NIMCP_ERROR_INVALID_PARAM, "nucleus out of range");

    /* Clamp to valid range */
    if (attention < 0.0f) attention = 0.0f;
    if (attention > 1.0f) attention = 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.nucleus_attention[nucleus] = attention;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float omni_wm_thalamic_bridge_get_nucleus_attention(
    const omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus) {

    if (!bridge) return -1.0f;
    if (nucleus >= WM_THAL_NUCLEUS_COUNT) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_get_nucleus_attentio", 0.0f);


    return bridge->config.nucleus_attention[nucleus];
}

nimcp_error_t omni_wm_thalamic_bridge_set_pulvinar_attention(
    omni_wm_thalamic_bridge_t* bridge,
    const float* attention_weights,
    uint32_t weights_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_set_pulvinar_attenti", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(attention_weights, NIMCP_ERROR_INVALID_PARAM, "attention_weights is NULL");
    NIMCP_CHECK_THROW(weights_dim > 0, NIMCP_ERROR_INVALID_PARAM, "weights_dim must be greater than 0");
    if (!bridge->config.enable_pulvinar_coordination) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t copy_dim = weights_dim < bridge->thal_to_wm.pulvinar_dim ?
                        weights_dim : bridge->thal_to_wm.pulvinar_dim;
    memcpy(bridge->thal_to_wm.pulvinar_attention, attention_weights, copy_dim * sizeof(float));

    /* Update pulvinar focus strength (max attention weight) */
    float max_attn = 0.0f;
    for (uint32_t i = 0; i < copy_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && copy_dim > 256) {
            omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_loop",
                             (float)(i + 1) / (float)copy_dim);
        }

        if (attention_weights[i] > max_attn) {
            max_attn = attention_weights[i];
        }
    }
    bridge->thal_to_wm.pulvinar_focus_strength = max_attn;

    bridge->stats.pulvinar_coordination_events++;
    float alpha = 0.1f;
    bridge->stats.mean_pulvinar_focus = alpha * max_attn +
                                         (1.0f - alpha) * bridge->stats.mean_pulvinar_focus;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_predict_salience(
    omni_wm_thalamic_bridge_t* bridge,
    float* salience_out,
    uint32_t salience_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_predict_salience", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(salience_out, NIMCP_ERROR_INVALID_PARAM, "salience_out is NULL");
    NIMCP_CHECK_THROW(salience_dim > 0, NIMCP_ERROR_INVALID_PARAM, "salience_dim must be greater than 0");
    if (!bridge->config.enable_salience_prediction) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Query WM for current state
     * 2. Compute salience from prediction errors and state features
     * 3. Return salience map
     * For now, use prediction errors as proxy for salience */

    uint32_t copy_dim = salience_dim < bridge->wm_to_thal.pe_dim ?
                        salience_dim : bridge->wm_to_thal.pe_dim;

    for (uint32_t i = 0; i < copy_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && copy_dim > 256) {
            omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_loop",
                             (float)(i + 1) / (float)copy_dim);
        }

        /* High PE = high salience (surprising = salient) */
        float pe = bridge->wm_to_thal.prediction_errors[i];
        salience_out[i] = 1.0f / (1.0f + expf(-pe)); /* Sigmoid of PE */
    }

    /* Zero remaining */
    for (uint32_t i = copy_dim; i < salience_dim; i++) {
        salience_out[i] = 0.5f; /* Neutral salience */
    }

    /* Also store in effects */
    copy_dim = salience_dim < bridge->wm_to_thal.salience_dim ?
               salience_dim : bridge->wm_to_thal.salience_dim;
    memcpy(bridge->wm_to_thal.predicted_salience, salience_out, copy_dim * sizeof(float));

    bridge->stats.salience_predictions++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * TRN Inhibition API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_thalamic_bridge_apply_trn_inhibition(
    omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus,
    float inhibition_strength) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_apply_trn_inhibition", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(nucleus < WM_THAL_NUCLEUS_COUNT, NIMCP_ERROR_INVALID_PARAM, "nucleus out of range");
    if (!bridge->config.enable_trn_inhibition) return NIMCP_SUCCESS;

    /* Clamp to valid range */
    if (inhibition_strength < 0.0f) inhibition_strength = 0.0f;
    if (inhibition_strength > 1.0f) inhibition_strength = 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Store in TRN inhibition map at nucleus index */
    if (bridge->thal_to_wm.trn_inhibition_map &&
        (uint32_t)nucleus < bridge->thal_to_wm.trn_inhibition_dim) {
        bridge->thal_to_wm.trn_inhibition_map[nucleus] = inhibition_strength;
    }

    /* Update statistics */
    bridge->stats.trn_inhibitions++;
    float alpha = 0.1f;
    bridge->stats.mean_trn_inhibition = alpha * inhibition_strength +
                                         (1.0f - alpha) * bridge->stats.mean_trn_inhibition;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("TRN inhibition applied: nucleus=%s, strength=%.3f",
                       wm_thal_nucleus_type_to_string(nucleus), inhibition_strength);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_apply_selective_inhibition(
    omni_wm_thalamic_bridge_t* bridge,
    const float* inhibition_map,
    uint32_t map_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_apply_selective_inhi", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(inhibition_map, NIMCP_ERROR_INVALID_PARAM, "inhibition_map is NULL");
    NIMCP_CHECK_THROW(map_dim > 0, NIMCP_ERROR_INVALID_PARAM, "map_dim must be greater than 0");
    if (!bridge->config.enable_trn_inhibition || !bridge->config.selective_inhibition) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Copy inhibition map to internal storage */
    uint32_t copy_dim = map_dim < bridge->wm_to_thal.inhibition_dim ?
                        map_dim : bridge->wm_to_thal.inhibition_dim;
    memcpy(bridge->wm_to_thal.selective_inhibition, inhibition_map, copy_dim * sizeof(float));

    /* Also update TRN effects */
    copy_dim = map_dim < bridge->thal_to_wm.trn_inhibition_dim ?
               map_dim : bridge->thal_to_wm.trn_inhibition_dim;
    memcpy(bridge->thal_to_wm.trn_inhibition_map, inhibition_map, copy_dim * sizeof(float));

    /* Compute global inhibition */
    float sum_inhib = 0.0f;
    for (uint32_t i = 0; i < copy_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && copy_dim > 256) {
            omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_loop",
                             (float)(i + 1) / (float)copy_dim);
        }

        sum_inhib += inhibition_map[i];
    }
    bridge->thal_to_wm.global_inhibition = sum_inhib / (float)copy_dim;

    bridge->stats.trn_inhibitions++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_release_trn_inhibition(
    omni_wm_thalamic_bridge_t* bridge,
    wm_thal_nucleus_type_t nucleus) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_release_trn_inhibiti", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(nucleus < WM_THAL_NUCLEUS_COUNT, NIMCP_ERROR_INVALID_PARAM, "nucleus out of range");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Zero inhibition for this nucleus */
    if (bridge->thal_to_wm.trn_inhibition_map &&
        (uint32_t)nucleus < bridge->thal_to_wm.trn_inhibition_dim) {
        bridge->thal_to_wm.trn_inhibition_map[nucleus] = 0.0f;
    }

    bridge->stats.trn_releases++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("TRN inhibition released: nucleus=%s",
                       wm_thal_nucleus_type_to_string(nucleus));

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_modulate_trn_from_confidence(
    omni_wm_thalamic_bridge_t* bridge,
    float prediction_confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_modulate_trn_from_co", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_trn_inhibition) return NIMCP_SUCCESS;

    /* Clamp confidence */
    if (prediction_confidence < 0.0f) prediction_confidence = 0.0f;
    if (prediction_confidence > 1.0f) prediction_confidence = 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* High confidence = more selective inhibition (suppress distractors) */
    float modulation = prediction_confidence * bridge->config.trn_confidence_modulation;
    bridge->wm_to_thal.trn_modulation_signal = modulation;

    /* Update global inhibition based on confidence */
    bridge->thal_to_wm.global_inhibition =
        bridge->config.trn_inhibition_strength * (1.0f + modulation);
    if (bridge->thal_to_wm.global_inhibition > 1.0f) {
        bridge->thal_to_wm.global_inhibition = 1.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction Integration API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_thalamic_bridge_prediction_error_feedback(
    omni_wm_thalamic_bridge_t* bridge,
    const float* prediction_errors,
    uint32_t pe_dim,
    float mean_pe) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_prediction_error_fee", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(prediction_errors, NIMCP_ERROR_INVALID_PARAM, "prediction_errors is NULL");
    NIMCP_CHECK_THROW(pe_dim > 0, NIMCP_ERROR_INVALID_PARAM, "pe_dim must be greater than 0");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Store prediction errors */
    uint32_t copy_dim = pe_dim < bridge->wm_to_thal.pe_dim ?
                        pe_dim : bridge->wm_to_thal.pe_dim;
    memcpy(bridge->wm_to_thal.prediction_errors, prediction_errors, copy_dim * sizeof(float));
    bridge->wm_to_thal.mean_prediction_error = mean_pe;

    /* Accumulate PE for tracking */
    bridge->accumulated_pe += mean_pe;

    /* High PE should increase attention to error sources */
    if (bridge->config.enable_prediction_biasing) {
        /* Normalize PE to attention adjustment */
        float pe_norm = mean_pe / MAX_PE_FOR_NORMALIZATION;
        if (pe_norm > 1.0f) pe_norm = 1.0f;

        /* Boost global attention based on PE */
        float attention_boost = pe_norm * bridge->config.prediction_bias_strength;
        bridge->thal_to_wm.global_attention += attention_boost;
        if (bridge->thal_to_wm.global_attention > 1.0f) {
            bridge->thal_to_wm.global_attention = 1.0f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("PE feedback: mean=%.4f, accumulated=%.4f", mean_pe, bridge->accumulated_pe);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_update_from_gated_input(
    omni_wm_thalamic_bridge_t* bridge,
    const float* gated_input,
    uint32_t input_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_update_from_gated_in", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(gated_input, NIMCP_ERROR_INVALID_PARAM, "gated_input is NULL");
    NIMCP_CHECK_THROW(input_dim > 0, NIMCP_ERROR_INVALID_PARAM, "input_dim must be greater than 0");
    if (!bridge->world_model) return NIMCP_SUCCESS; /* No WM connected */

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Encode gated input to latent space
     * 2. Update RSSM state with new observation
     * 3. Generate new predictions
     * For now, just log */

    NIMCP_LOGGING_DEBUG("WM updated from gated input: dim=%u", input_dim);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const thalamus_to_omni_wm_effects_t* omni_wm_thalamic_bridge_get_thalamic_effects(
    const omni_wm_thalamic_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_get_thalamic_effects", 0.0f);


    return &bridge->thal_to_wm;
}

const omni_wm_to_thalamus_effects_t* omni_wm_thalamic_bridge_get_wm_effects(
    const omni_wm_thalamic_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_get_wm_effects", 0.0f);


    return &bridge->wm_to_thal;
}

nimcp_error_t omni_wm_thalamic_bridge_get_stats(
    const omni_wm_thalamic_bridge_t* bridge,
    omni_wm_thalamic_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_reset_stats(
    omni_wm_thalamic_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_wm_thalamic_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_thalamic_bridge_connect_bio_async(
    omni_wm_thalamic_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS; /* Already connected */

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_THALAMIC_BRIDGE,
        .module_name = "wm_thalamic_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register with bio-async router");
        return NIMCP_SUCCESS; /* Non-fatal */
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_THALAMIC_GATE_INPUT,
                                handle_gate_input);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_THALAMIC_ATTENTION_BIAS,
                                handle_attention_bias);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_THALAMIC_TRN_INHIBIT,
                                handle_trn_inhibit);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_THALAMIC_PRED_ERROR,
                                handle_prediction_error);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM Thalamic Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_thalamic_bridge_disconnect_bio_async(
    omni_wm_thalamic_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM Thalamic Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_thalamic_bridge_is_bio_async_connected(
    const omni_wm_thalamic_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* wm_thal_nucleus_type_to_string(wm_thal_nucleus_type_t nucleus) {
    switch (nucleus) {
        case WM_THAL_NUCLEUS_LGN:      return "LGN";
        case WM_THAL_NUCLEUS_MGN:      return "MGN";
        case WM_THAL_NUCLEUS_VPL:      return "VPL";
        case WM_THAL_NUCLEUS_VPM:      return "VPM";
        case WM_THAL_NUCLEUS_VA:       return "VA";
        case WM_THAL_NUCLEUS_VL:       return "VL";
        case WM_THAL_NUCLEUS_PULVINAR: return "Pulvinar";
        case WM_THAL_NUCLEUS_MD:       return "MD";
        case WM_THAL_NUCLEUS_TRN:      return "TRN";
        default:                        return "UNKNOWN";
    }
}

const char* omni_wm_thalamic_msg_type_to_string(omni_wm_thalamic_msg_type_t msg_type) {
    switch (msg_type) {
        case BIO_MSG_WM_THALAMIC_GATE_INPUT:       return "GATE_INPUT";
        case BIO_MSG_WM_THALAMIC_GATE_VISUAL:      return "GATE_VISUAL";
        case BIO_MSG_WM_THALAMIC_GATE_AUDITORY:    return "GATE_AUDITORY";
        case BIO_MSG_WM_THALAMIC_GATE_MOTOR:       return "GATE_MOTOR";
        case BIO_MSG_WM_THALAMIC_GATE_EXECUTIVE:   return "GATE_EXECUTIVE";
        case BIO_MSG_WM_THALAMIC_ATTENTION_BIAS:   return "ATTENTION_BIAS";
        case BIO_MSG_WM_THALAMIC_ATTENTION_UPDATE: return "ATTENTION_UPDATE";
        case BIO_MSG_WM_THALAMIC_PULVINAR_WEIGHT:  return "PULVINAR_WEIGHT";
        case BIO_MSG_WM_THALAMIC_SALIENCE_PRED:    return "SALIENCE_PRED";
        case BIO_MSG_WM_THALAMIC_TRN_INHIBIT:      return "TRN_INHIBIT";
        case BIO_MSG_WM_THALAMIC_TRN_RELEASE:      return "TRN_RELEASE";
        case BIO_MSG_WM_THALAMIC_TRN_MODULATE:     return "TRN_MODULATE";
        case BIO_MSG_WM_THALAMIC_PRED_ERROR:       return "PRED_ERROR";
        case BIO_MSG_WM_THALAMIC_PRED_CONFIDENCE:  return "PRED_CONFIDENCE";
        case BIO_MSG_WM_THALAMIC_PRED_UPDATE:      return "PRED_UPDATE";
        case BIO_MSG_WM_THALAMIC_MODE_TONIC:       return "MODE_TONIC";
        case BIO_MSG_WM_THALAMIC_MODE_BURST:       return "MODE_BURST";
        case BIO_MSG_WM_THALAMIC_AROUSAL_UPDATE:   return "AROUSAL_UPDATE";
        case BIO_MSG_WM_THALAMIC_BRIDGE_STATUS:    return "BRIDGE_STATUS";
        case BIO_MSG_WM_THALAMIC_BRIDGE_ERROR:     return "BRIDGE_ERROR";
        case BIO_MSG_WM_THALAMIC_STATS_UPDATE:     return "STATS_UPDATE";
        default:                                    return "UNKNOWN";
    }
}

nimcp_error_t omni_wm_thalamic_bridge_validate_config(
    const omni_wm_thalamic_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate attention settings */
    if (config->attention_baseline < 0.0f || config->attention_baseline > 1.0f) {
        NIMCP_LOGGING_WARN("Invalid attention_baseline: %.2f",
                          config->attention_baseline);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->min_attention_threshold < 0.0f || config->min_attention_threshold > 1.0f) {
        NIMCP_LOGGING_WARN("Invalid min_attention_threshold: %.2f",
                          config->min_attention_threshold);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate prediction biasing settings */
    if (config->enable_prediction_biasing) {
        if (config->prediction_confidence_threshold < 0.0f ||
            config->prediction_confidence_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid prediction_confidence_threshold: %.2f",
                              config->prediction_confidence_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->prediction_bias_strength < 0.0f ||
            config->prediction_bias_strength > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid prediction_bias_strength: %.2f",
                              config->prediction_bias_strength);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate pulvinar settings */
    if (config->enable_pulvinar_coordination) {
        if (config->pulvinar_attention_gain < 0.5f ||
            config->pulvinar_attention_gain > 2.0f) {
            NIMCP_LOGGING_WARN("Invalid pulvinar_attention_gain: %.2f",
                              config->pulvinar_attention_gain);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate TRN settings */
    if (config->enable_trn_inhibition) {
        if (config->trn_inhibition_strength < 0.0f ||
            config->trn_inhibition_strength > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid trn_inhibition_strength: %.2f",
                              config->trn_inhibition_strength);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->trn_confidence_modulation < 0.0f ||
            config->trn_confidence_modulation > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid trn_confidence_modulation: %.2f",
                              config->trn_confidence_modulation);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate firing mode settings */
    if (config->track_firing_modes) {
        if (config->burst_mode_threshold < 0.0f ||
            config->burst_mode_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid burst_mode_threshold: %.2f",
                              config->burst_mode_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate per-nucleus attention */
    for (int i = 0; i < WM_THAL_NUCLEUS_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WM_THAL_NUCLEUS_COUNT > 256) {
            omni_wm_thalamic_bridge_heartbeat("omni_wm_thal_loop",
                             (float)(i + 1) / (float)WM_THAL_NUCLEUS_COUNT);
        }

        if (config->nucleus_attention[i] < 0.0f ||
            config->nucleus_attention[i] > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid nucleus_attention[%d]: %.2f",
                              i, config->nucleus_attention[i]);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}
