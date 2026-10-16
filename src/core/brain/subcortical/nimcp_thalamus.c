//=============================================================================
// nimcp_thalamus.c - Thalamic Nuclei System Implementation
//=============================================================================
/**
 * @file nimcp_thalamus.c
 * @brief Implementation of thalamic nuclei for sensory relay and cortical gating
 *
 * WHAT: Implements biologically-inspired thalamic relay and gating
 * WHY:  Gateway to cortex - attention-modulated signal relay
 * HOW:  LGN, MGN, VPL/VPM, VA/VL, Pulvinar, MD with TRN gating
 */

#include "core/brain/subcortical/nimcp_thalamus.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(thalamus, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Sigmoid activation
 */
static inline float sigmoid_f(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Get default channel count for nucleus type
 */
static uint32_t get_default_channels(thal_nucleus_type_t type) {
    switch (type) {
        case THAL_NUCLEUS_LGN:      return 64;   /* Visual channels */
        case THAL_NUCLEUS_MGN:      return 32;   /* Auditory channels */
        case THAL_NUCLEUS_VPL:      return 32;   /* Body somatosensory */
        case THAL_NUCLEUS_VPM:      return 16;   /* Face somatosensory */
        case THAL_NUCLEUS_VA:       return 16;   /* Motor from BG */
        case THAL_NUCLEUS_VL:       return 16;   /* Motor from cerebellum */
        case THAL_NUCLEUS_PULVINAR: return 64;   /* Attention/visual */
        case THAL_NUCLEUS_MD:       return 32;   /* Executive */
        case THAL_NUCLEUS_ANTERIOR: return 16;   /* Limbic */
        case THAL_NUCLEUS_TRN:      return 128;  /* Gating all channels */
        default:                    return 32;
    }
}

/**
 * @brief Get relay order for nucleus type
 */
static thal_relay_order_t get_relay_order(thal_nucleus_type_t type) {
    switch (type) {
        case THAL_NUCLEUS_LGN:
        case THAL_NUCLEUS_MGN:
        case THAL_NUCLEUS_VPL:
        case THAL_NUCLEUS_VPM:
        case THAL_NUCLEUS_VA:
        case THAL_NUCLEUS_VL:
            return THAL_ORDER_FIRST;
        case THAL_NUCLEUS_PULVINAR:
        case THAL_NUCLEUS_MD:
        case THAL_NUCLEUS_ANTERIOR:
        default:
            return THAL_ORDER_HIGHER;
    }
}

//=============================================================================
// Nucleus Configuration
//=============================================================================

void thal_nucleus_default_config(thal_nucleus_config_t* config, thal_nucleus_type_t type) {
    if (!config) return;

    memset(config, 0, sizeof(thal_nucleus_config_t));
    config->type = type;
    config->num_neurons = THAL_DEFAULT_NEURONS;
    config->num_channels = get_default_channels(type);
    config->order = get_relay_order(type);
    config->burst_threshold = THAL_BURST_THRESHOLD;
    config->attention_weight = THAL_ATTENTION_BASELINE;
    config->trn_inhibition = THAL_TRN_INHIBITION_STRENGTH;
    config->enable_adaptation = true;
}

void thalamus_default_config(thalamus_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(thalamus_config_t));
    config->neurons_per_nucleus = THAL_DEFAULT_NEURONS;
    config->channels_per_nucleus = 32;
    config->attention_baseline = THAL_ATTENTION_BASELINE;
    config->burst_threshold = THAL_BURST_THRESHOLD;
    config->trn_strength = THAL_TRN_INHIBITION_STRENGTH;
    config->enable_trn = true;
    config->enable_mode_switching = true;
    config->enable_attention_gating = true;

    /* Initialize individual nucleus configs */
    thal_nucleus_default_config(&config->lgn_config, THAL_NUCLEUS_LGN);
    thal_nucleus_default_config(&config->mgn_config, THAL_NUCLEUS_MGN);
    thal_nucleus_default_config(&config->vpl_config, THAL_NUCLEUS_VPL);
    thal_nucleus_default_config(&config->vpm_config, THAL_NUCLEUS_VPM);
    thal_nucleus_default_config(&config->va_config, THAL_NUCLEUS_VA);
    thal_nucleus_default_config(&config->vl_config, THAL_NUCLEUS_VL);
    thal_nucleus_default_config(&config->pulvinar_config, THAL_NUCLEUS_PULVINAR);
    thal_nucleus_default_config(&config->md_config, THAL_NUCLEUS_MD);
}

//=============================================================================
// Nucleus Lifecycle
//=============================================================================

thal_nucleus_t* thal_nucleus_create(const thal_nucleus_config_t* config) {
    thal_nucleus_config_t default_config;
    if (!config) {
        thal_nucleus_default_config(&default_config, THAL_NUCLEUS_LGN);
        config = &default_config;
    }

    thal_nucleus_t* nucleus = nimcp_malloc(sizeof(thal_nucleus_t));
    if (!nucleus) {
        NIMCP_LOGGING_ERROR("Failed to allocate nucleus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "thal_nucleus_create: failed to allocate nucleus");
        return NULL;
    }
    memset(nucleus, 0, sizeof(thal_nucleus_t));

    nucleus->type = config->type;
    nucleus->order = config->order;
    nucleus->num_cells = config->num_neurons;
    nucleus->num_input_channels = config->num_channels;
    nucleus->num_output_channels = config->num_channels;
    nucleus->config = *config;
    nucleus->dominant_mode = THAL_MODE_TONIC;
    nucleus->tonic_fraction = 1.0f;
    nucleus->attention_level = config->attention_weight;
    nucleus->trn_inhibition = config->trn_inhibition;
    nucleus->output_gain = 1.0f;

    /* Allocate cells */
    nucleus->cells = nimcp_malloc(sizeof(thal_relay_cell_t) * nucleus->num_cells);
    if (!nucleus->cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "thal_nucleus_create: failed to allocate cells");
        nimcp_free(nucleus);
        return NULL;
    }
    for (uint32_t i = 0; i < nucleus->num_cells; i++) {
        nucleus->cells[i].cell_id = i;
        nucleus->cells[i].membrane_potential = -65.0f;
        nucleus->cells[i].firing_rate = 0.0f;
        nucleus->cells[i].mode = THAL_MODE_TONIC;
        nucleus->cells[i].t_channel_state = 0.0f;
        nucleus->cells[i].refractory_time = 0.0f;
        nucleus->cells[i].is_bursting = false;
        nucleus->cells[i].burst_spike_count = 0;
    }

    /* Allocate buffers */
    nucleus->input_buffer = nimcp_malloc(sizeof(float) * nucleus->num_input_channels);
    nucleus->output_buffer = nimcp_malloc(sizeof(float) * nucleus->num_output_channels);
    nucleus->channel_attention = nimcp_malloc(sizeof(float) * nucleus->num_input_channels);
    nucleus->channel_inhibition = nimcp_malloc(sizeof(float) * nucleus->num_input_channels);

    if (!nucleus->input_buffer || !nucleus->output_buffer ||
        !nucleus->channel_attention || !nucleus->channel_inhibition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "thal_nucleus_create: failed to allocate buffers");
        thal_nucleus_destroy(nucleus);
        return NULL;
    }

    /* Initialize buffers */
    memset(nucleus->input_buffer, 0, sizeof(float) * nucleus->num_input_channels);
    memset(nucleus->output_buffer, 0, sizeof(float) * nucleus->num_output_channels);
    for (uint32_t i = 0; i < nucleus->num_input_channels; i++) {
        nucleus->channel_attention[i] = config->attention_weight;
        nucleus->channel_inhibition[i] = 0.0f;
    }

    /* Create mutex */
    nucleus->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (nucleus->mutex) {
        nimcp_mutex_init(nucleus->mutex, NULL);
    }

    return nucleus;
}

void thal_nucleus_destroy(thal_nucleus_t* nucleus) {
    if (!nucleus) return;

    if (nucleus->mutex) {
        nimcp_mutex_destroy(nucleus->mutex);
        nimcp_free(nucleus->mutex);
    }

    nimcp_free(nucleus->cells);
    nimcp_free(nucleus->input_buffer);
    nimcp_free(nucleus->output_buffer);
    nimcp_free(nucleus->channel_attention);
    nimcp_free(nucleus->channel_inhibition);
    nimcp_free(nucleus);
}

//=============================================================================
// TRN Lifecycle
//=============================================================================

static thalamic_reticular_nucleus_t* trn_create(uint32_t num_channels) {
    thalamic_reticular_nucleus_t* trn = nimcp_malloc(sizeof(thalamic_reticular_nucleus_t));
    if (!trn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "trn is NULL");

        return NULL;

    }
    memset(trn, 0, sizeof(thalamic_reticular_nucleus_t));

    trn->num_channels = num_channels;
    trn->attention_gain = 1.0f;
    trn->is_active = true;

    trn->inhibition_map = nimcp_malloc(sizeof(float) * num_channels);
    trn->cortical_drive = nimcp_malloc(sizeof(float) * num_channels);
    trn->collateral_input = nimcp_malloc(sizeof(float) * num_channels);

    if (!trn->inhibition_map || !trn->cortical_drive || !trn->collateral_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "trn_create: failed to allocate buffers");
        nimcp_free(trn->inhibition_map);
        nimcp_free(trn->cortical_drive);
        nimcp_free(trn->collateral_input);
        nimcp_free(trn);
        return NULL;
    }

    memset(trn->inhibition_map, 0, sizeof(float) * num_channels);
    memset(trn->cortical_drive, 0, sizeof(float) * num_channels);
    memset(trn->collateral_input, 0, sizeof(float) * num_channels);

    trn->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (trn->mutex) {
        nimcp_mutex_init(trn->mutex, NULL);
    }

    return trn;
}

static void trn_destroy(thalamic_reticular_nucleus_t* trn) {
    if (!trn) return;

    if (trn->mutex) {
        nimcp_mutex_destroy(trn->mutex);
        nimcp_free(trn->mutex);
    }

    nimcp_free(trn->inhibition_map);
    nimcp_free(trn->cortical_drive);
    nimcp_free(trn->collateral_input);
    nimcp_free(trn);
}

//=============================================================================
// Thalamus Lifecycle
//=============================================================================

thalamus_t* thalamus_create(const thalamus_config_t* config) {
    thalamus_config_t default_config;
    if (!config) {
        thalamus_default_config(&default_config);
        config = &default_config;
    }

    thalamus_t* thal = nimcp_malloc(sizeof(thalamus_t));
    if (!thal) {
        NIMCP_LOGGING_ERROR("Failed to allocate thalamus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "thalamus_create: failed to allocate thalamus");
        return NULL;
    }
    memset(thal, 0, sizeof(thalamus_t));

    thal->config = *config;
    thal->global_arousal = 1.0f;
    thal->global_attention = config->attention_baseline;
    thal->dominant_mode = THAL_MODE_TONIC;

    /* Create individual nuclei */
    thal->lgn = thal_nucleus_create(&config->lgn_config);
    thal->mgn = thal_nucleus_create(&config->mgn_config);
    thal->vpl = thal_nucleus_create(&config->vpl_config);
    thal->vpm = thal_nucleus_create(&config->vpm_config);
    thal->va = thal_nucleus_create(&config->va_config);
    thal->vl = thal_nucleus_create(&config->vl_config);
    thal->pulvinar = thal_nucleus_create(&config->pulvinar_config);
    thal->md = thal_nucleus_create(&config->md_config);

    if (!thal->lgn || !thal->mgn || !thal->vpl || !thal->vpm ||
        !thal->va || !thal->vl || !thal->pulvinar || !thal->md) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "thalamus_create: failed to create nuclei");
        thalamus_destroy(thal);
        return NULL;
    }

    /* Create TRN if enabled */
    if (config->enable_trn) {
        uint32_t total_channels = 0;
        total_channels += thal->lgn->num_input_channels;
        total_channels += thal->mgn->num_input_channels;
        total_channels += thal->vpl->num_input_channels;
        total_channels += thal->vpm->num_input_channels;
        total_channels += thal->va->num_input_channels;
        total_channels += thal->vl->num_input_channels;
        total_channels += thal->pulvinar->num_input_channels;
        total_channels += thal->md->num_input_channels;

        thal->trn = trn_create(total_channels);
        if (!thal->trn) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "thalamus_create: failed to create TRN");
            thalamus_destroy(thal);
            return NULL;
        }
    }

    /* Create mutex */
    thal->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (thal->mutex) {
        nimcp_mutex_init(thal->mutex, NULL);
    }

    return thal;
}

void thalamus_destroy(thalamus_t* thal) {
    if (!thal) return;

    if (thal->bio_async_enabled) {
        thalamus_disconnect_bio_async(thal);
    }

    if (thal->mutex) {
        nimcp_mutex_destroy(thal->mutex);
        nimcp_free(thal->mutex);
    }

    thal_nucleus_destroy(thal->lgn);
    thal_nucleus_destroy(thal->mgn);
    thal_nucleus_destroy(thal->vpl);
    thal_nucleus_destroy(thal->vpm);
    thal_nucleus_destroy(thal->va);
    thal_nucleus_destroy(thal->vl);
    thal_nucleus_destroy(thal->pulvinar);
    thal_nucleus_destroy(thal->md);
    trn_destroy(thal->trn);

    nimcp_free(thal);
}

int thalamus_reset(thalamus_t* thal) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    thal->global_arousal = 1.0f;
    thal->global_attention = thal->config.attention_baseline;
    thal->dominant_mode = THAL_MODE_TONIC;
    memset(&thal->stats, 0, sizeof(thalamus_stats_t));

    /* Reset each nucleus */
    thal_nucleus_t* nuclei[] = {
        thal->lgn, thal->mgn, thal->vpl, thal->vpm,
        thal->va, thal->vl, thal->pulvinar, thal->md
    };

    for (int i = 0; i < 8; i++) {
        if (!nuclei[i]) continue;
        for (uint32_t j = 0; j < nuclei[i]->num_cells; j++) {
            nuclei[i]->cells[j].membrane_potential = -65.0f;
            nuclei[i]->cells[j].firing_rate = 0.0f;
            nuclei[i]->cells[j].mode = THAL_MODE_TONIC;
            nuclei[i]->cells[j].t_channel_state = 0.0f;
            nuclei[i]->cells[j].is_bursting = false;
        }
        nuclei[i]->dominant_mode = THAL_MODE_TONIC;
        nuclei[i]->tonic_fraction = 1.0f;
        nuclei[i]->avg_firing_rate = 0.0f;
        memset(nuclei[i]->input_buffer, 0, sizeof(float) * nuclei[i]->num_input_channels);
        memset(nuclei[i]->output_buffer, 0, sizeof(float) * nuclei[i]->num_output_channels);
    }

    /* Reset TRN */
    if (thal->trn) {
        memset(thal->trn->inhibition_map, 0, sizeof(float) * thal->trn->num_channels);
        thal->trn->global_inhibition = 0.0f;
    }

    return 0;
}

//=============================================================================
// Signal Relay Functions
//=============================================================================

int thal_nucleus_process_input(thal_nucleus_t* nucleus, const float* input, uint32_t size) {
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_NULL_POINTER, "nucleus is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER, "input is NULL");

    uint32_t process_size = (size < nucleus->num_input_channels) ? size : nucleus->num_input_channels;

    /* Copy input with attention and inhibition modulation */
    for (uint32_t i = 0; i < process_size; i++) {
        float attention = nucleus->channel_attention[i];
        float inhibition = nucleus->channel_inhibition[i];
        float effective_gain = attention * (1.0f - inhibition);

        /* Apply gain based on firing mode */
        if (nucleus->dominant_mode == THAL_MODE_BURST) {
            /* Burst mode: non-linear, threshold-based */
            if (input[i] > nucleus->config.burst_threshold) {
                nucleus->input_buffer[i] = input[i] * effective_gain * 1.5f;  /* Burst amplification */
            } else {
                nucleus->input_buffer[i] = 0.0f;  /* Below threshold, no relay */
            }
        } else if (nucleus->dominant_mode == THAL_MODE_INHIBITED) {
            nucleus->input_buffer[i] = 0.0f;  /* TRN suppression */
        } else {
            /* Tonic mode: linear, faithful relay */
            nucleus->input_buffer[i] = input[i] * effective_gain;
        }
    }

    return 0;
}

static int nucleus_compute_output(thal_nucleus_t* nucleus) {
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_NULL_POINTER, "nucleus is NULL");

    float total_activity = 0.0f;

    /* Compute output for each channel */
    for (uint32_t i = 0; i < nucleus->num_output_channels; i++) {
        /* Map input to output (can be 1:1 or with pooling) */
        uint32_t input_idx = i % nucleus->num_input_channels;
        float raw_output = nucleus->input_buffer[input_idx];

        /* Apply output gain */
        nucleus->output_buffer[i] = nimcp_clampf(raw_output * nucleus->output_gain, 0.0f, 1.0f);
        total_activity += nucleus->output_buffer[i];
    }

    /* Update average firing rate */
    nucleus->avg_firing_rate = total_activity / (float)nucleus->num_output_channels * 100.0f;  /* Scale to Hz */

    return 0;
}

int thalamus_relay(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_NULL_POINTER, "input is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_NULL_POINTER, "output is NULL");

    thal_nucleus_t* nucleus = thalamus_get_nucleus(thal, nucleus_type);
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    /* Process input */
    int result = thal_nucleus_process_input(nucleus, input, input_size);
    if (result < 0) return result;

    /* Compute output */
    result = nucleus_compute_output(nucleus);
    if (result < 0) return result;

    /* Copy to output buffer */
    uint32_t copy_size = (output_size < nucleus->num_output_channels) ? output_size : nucleus->num_output_channels;
    memcpy(output, nucleus->output_buffer, sizeof(float) * copy_size);

    /* Update statistics */
    thal->stats.total_signals_relayed++;
    if (nucleus_type < THAL_NUCLEUS_COUNT) {
        thal->stats.signals_per_nucleus[nucleus_type]++;
    }

    return (int)copy_size;
}

int thalamus_relay_visual(
    thalamus_t* thal,
    const float* retinal_input,
    uint32_t input_size,
    float* v1_output,
    uint32_t output_size
) {
    return thalamus_relay(thal, THAL_NUCLEUS_LGN, retinal_input, input_size, v1_output, output_size);
}

int thalamus_relay_auditory(
    thalamus_t* thal,
    const float* ic_input,
    uint32_t input_size,
    float* a1_output,
    uint32_t output_size
) {
    return thalamus_relay(thal, THAL_NUCLEUS_MGN, ic_input, input_size, a1_output, output_size);
}

int thalamus_relay_motor(
    thalamus_t* thal,
    const float* bg_input,
    uint32_t bg_size,
    float* motor_output,
    uint32_t output_size
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    NIMCP_CHECK_THROW(bg_input, NIMCP_ERROR_NULL_POINTER, "bg_input is NULL");
    NIMCP_CHECK_THROW(motor_output, NIMCP_ERROR_NULL_POINTER, "motor_output is NULL");

    /* Motor relay uses VA (from BG) */
    return thalamus_relay(thal, THAL_NUCLEUS_VA, bg_input, bg_size, motor_output, output_size);
}

int thalamus_relay_executive(
    thalamus_t* thal,
    const float* input,
    uint32_t input_size,
    float* pfc_output,
    uint32_t output_size
) {
    return thalamus_relay(thal, THAL_NUCLEUS_MD, input, input_size, pfc_output, output_size);
}

int thalamus_get_output(
    const thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    float* output,
    uint32_t size
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_NULL_POINTER, "output is NULL");

    const thal_nucleus_t* nucleus = thalamus_get_nucleus_const(thal, nucleus_type);
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    uint32_t copy_size = (size < nucleus->num_output_channels) ? size : nucleus->num_output_channels;
    memcpy(output, nucleus->output_buffer, sizeof(float) * copy_size);

    return (int)copy_size;
}

//=============================================================================
// Attention and Gating Functions
//=============================================================================

int thalamus_set_attention(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    float attention
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    thal_nucleus_t* nucleus = thalamus_get_nucleus(thal, nucleus_type);
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    attention = nimcp_clampf(attention, 0.0f, 1.0f);
    nucleus->attention_level = attention;

    /* Update all channels */
    for (uint32_t i = 0; i < nucleus->num_input_channels; i++) {
        nucleus->channel_attention[i] = attention;
    }

    return 0;
}

int thalamus_set_channel_attention(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    uint32_t channel,
    float attention
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    thal_nucleus_t* nucleus = thalamus_get_nucleus(thal, nucleus_type);
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");
    NIMCP_CHECK_THROW(channel < nucleus->num_input_channels, NIMCP_ERROR_INVALID_PARAM, "channel out of range");

    nucleus->channel_attention[channel] = nimcp_clampf(attention, 0.0f, 1.0f);
    return 0;
}

int thalamus_set_arousal(thalamus_t* thal, float arousal) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    thal->global_arousal = nimcp_clampf(arousal, 0.0f, 1.0f);

    /* High arousal → tonic mode, low arousal → burst mode */
    if (thal->config.enable_mode_switching) {
        thal_firing_mode_t new_mode = (arousal > THAL_BURST_THRESHOLD) ?
            THAL_MODE_TONIC : THAL_MODE_BURST;

        if (new_mode != thal->dominant_mode) {
            thal->dominant_mode = new_mode;

            /* Update all nuclei */
            thal_nucleus_t* nuclei[] = {
                thal->lgn, thal->mgn, thal->vpl, thal->vpm,
                thal->va, thal->vl, thal->pulvinar, thal->md
            };
            for (int i = 0; i < 8; i++) {
                if (nuclei[i]) {
                    nuclei[i]->dominant_mode = new_mode;
                }
            }
        }
    }

    return 0;
}

float thalamus_get_attention(const thalamus_t* thal, thal_nucleus_type_t nucleus_type) {
    if (!thal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamus_get_attention: thal is NULL");
        return -1.0f;
    }

    const thal_nucleus_t* nucleus = thalamus_get_nucleus_const(thal, nucleus_type);
    if (!nucleus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "thalamus_get_attention: invalid nucleus type");
        return -1.0f;
    }

    return nucleus->attention_level;
}

int thalamus_apply_trn_inhibition(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    float inhibition
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    thal_nucleus_t* nucleus = thalamus_get_nucleus(thal, nucleus_type);
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    inhibition = nimcp_clampf(inhibition, 0.0f, 1.0f);
    nucleus->trn_inhibition = inhibition;

    /* Update mode if fully inhibited */
    if (inhibition > 0.9f) {
        nucleus->dominant_mode = THAL_MODE_INHIBITED;
    } else if (thal->global_arousal > THAL_BURST_THRESHOLD) {
        nucleus->dominant_mode = THAL_MODE_TONIC;
    }

    return 0;
}

int thalamus_apply_channel_inhibition(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    uint32_t channel,
    float inhibition
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    thal_nucleus_t* nucleus = thalamus_get_nucleus(thal, nucleus_type);
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");
    NIMCP_CHECK_THROW(channel < nucleus->num_input_channels, NIMCP_ERROR_INVALID_PARAM, "channel out of range");

    nucleus->channel_inhibition[channel] = nimcp_clampf(inhibition, 0.0f, 1.0f);
    return 0;
}

//=============================================================================
// Firing Mode Functions
//=============================================================================

int thalamus_set_mode(
    thalamus_t* thal,
    thal_nucleus_type_t nucleus_type,
    thal_firing_mode_t mode
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    thal_nucleus_t* nucleus = thalamus_get_nucleus(thal, nucleus_type);
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    nucleus->dominant_mode = mode;

    /* Update cell modes */
    for (uint32_t i = 0; i < nucleus->num_cells; i++) {
        nucleus->cells[i].mode = mode;
    }

    /* Update tonic fraction */
    nucleus->tonic_fraction = (mode == THAL_MODE_TONIC) ? 1.0f : 0.0f;

    return 0;
}

thal_firing_mode_t thalamus_get_mode(
    const thalamus_t* thal,
    thal_nucleus_type_t nucleus_type
) {
    if (!thal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamus_get_mode: thal is NULL");
        return THAL_MODE_TONIC;
    }

    const thal_nucleus_t* nucleus = thalamus_get_nucleus_const(thal, nucleus_type);
    if (!nucleus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "thalamus_get_mode: invalid nucleus type");
        return THAL_MODE_TONIC;
    }

    return nucleus->dominant_mode;
}

int thalamus_trigger_burst(thalamus_t* thal, thal_nucleus_type_t nucleus_type) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    thal_nucleus_t* nucleus = thalamus_get_nucleus(thal, nucleus_type);
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_INVALID_PARAM, "invalid nucleus type");

    /* Activate T-type Ca2+ channels for burst */
    for (uint32_t i = 0; i < nucleus->num_cells; i++) {
        nucleus->cells[i].t_channel_state = 1.0f;
        nucleus->cells[i].is_bursting = true;
        nucleus->cells[i].burst_spike_count = 0;
        nucleus->cells[i].mode = THAL_MODE_BURST;
    }

    nucleus->dominant_mode = THAL_MODE_BURST;
    nucleus->tonic_fraction = 0.0f;
    thal->stats.burst_count++;

    return 0;
}

float thalamus_get_tonic_fraction(
    const thalamus_t* thal,
    thal_nucleus_type_t nucleus_type
) {
    if (!thal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamus_get_tonic_fraction: thal is NULL");
        return 0.0f;
    }

    const thal_nucleus_t* nucleus = thalamus_get_nucleus_const(thal, nucleus_type);
    if (!nucleus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "thalamus_get_tonic_fraction: invalid nucleus type");
        return 0.0f;
    }

    return nucleus->tonic_fraction;
}

//=============================================================================
// Update Functions
//=============================================================================

int thal_nucleus_step(thal_nucleus_t* nucleus, float dt) {
    NIMCP_CHECK_THROW(nucleus, NIMCP_ERROR_NULL_POINTER, "nucleus is NULL");

    float tonic_count = 0;

    for (uint32_t i = 0; i < nucleus->num_cells; i++) {
        thal_relay_cell_t* cell = &nucleus->cells[i];

        /* Update T-channel state (decay) */
        if (cell->t_channel_state > 0.0f) {
            cell->t_channel_state *= expf(-dt / 20.0f);  /* 20ms time constant */
            if (cell->t_channel_state < 0.01f) {
                cell->t_channel_state = 0.0f;
                cell->is_bursting = false;
                cell->burst_spike_count = 0;
            }
        }

        /* Update refractory time */
        if (cell->refractory_time > 0.0f) {
            cell->refractory_time -= dt;
            if (cell->refractory_time < 0.0f) cell->refractory_time = 0.0f;
        }

        /* Update firing rate based on mode */
        if (cell->mode == THAL_MODE_BURST && cell->is_bursting) {
            /* Burst: high-frequency spikes (2-7 spikes) */
            cell->firing_rate = 300.0f;  /* 300 Hz during burst */
            cell->burst_spike_count++;
            if (cell->burst_spike_count >= 5) {
                cell->is_bursting = false;
                cell->mode = THAL_MODE_TONIC;
                cell->firing_rate = 0.0f;
            }
        } else if (cell->mode == THAL_MODE_INHIBITED) {
            cell->firing_rate = 0.0f;
        } else {
            /* Tonic: moderate, sustained firing */
            cell->firing_rate = 20.0f;  /* ~20 Hz baseline */
        }

        if (cell->mode == THAL_MODE_TONIC) tonic_count++;
    }

    nucleus->tonic_fraction = tonic_count / (float)nucleus->num_cells;

    return 0;
}

int thalamus_update_trn(thalamus_t* thal) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    NIMCP_CHECK_THROW(thal->trn, NIMCP_ERROR_NULL_POINTER, "thal->trn is NULL");

    thalamic_reticular_nucleus_t* trn = thal->trn;

    /* Compute TRN inhibition based on collateral inputs */
    float total_inhibition = 0.0f;
    for (uint32_t i = 0; i < trn->num_channels; i++) {
        /* Inhibition is driven by thalamocortical activity */
        float drive = trn->collateral_input[i] * trn->attention_gain;
        /* And modulated by cortical feedback */
        drive += trn->cortical_drive[i] * 0.5f;

        trn->inhibition_map[i] = nimcp_clampf(drive, 0.0f, 1.0f);
        total_inhibition += trn->inhibition_map[i];
    }

    trn->global_inhibition = total_inhibition / (float)trn->num_channels;

    return 0;
}

int thalamus_step(thalamus_t* thal, float dt) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");

    /* Step each nucleus */
    thal_nucleus_step(thal->lgn, dt);
    thal_nucleus_step(thal->mgn, dt);
    thal_nucleus_step(thal->vpl, dt);
    thal_nucleus_step(thal->vpm, dt);
    thal_nucleus_step(thal->va, dt);
    thal_nucleus_step(thal->vl, dt);
    thal_nucleus_step(thal->pulvinar, dt);
    thal_nucleus_step(thal->md, dt);

    /* Update TRN */
    if (thal->config.enable_trn) {
        thalamus_update_trn(thal);
    }

    /* Update dominant mode based on fraction */
    float total_tonic = 0.0f;
    thal_nucleus_t* nuclei[] = {
        thal->lgn, thal->mgn, thal->vpl, thal->vpm,
        thal->va, thal->vl, thal->pulvinar, thal->md
    };
    for (int i = 0; i < 8; i++) {
        if (nuclei[i]) {
            total_tonic += nuclei[i]->tonic_fraction;
        }
    }
    thal->stats.tonic_mode_fraction = total_tonic / 8.0f;
    thal->stats.avg_attention_level = thal->global_attention;

    if (thal->trn) {
        thal->stats.avg_trn_inhibition = thal->trn->global_inhibition;
    }

    return 0;
}

//=============================================================================
// Integration Functions
//=============================================================================

int thalamus_connect_basal_ganglia(
    thalamus_t* thal,
    const float* bg_output,
    uint32_t size
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    NIMCP_CHECK_THROW(bg_output, NIMCP_ERROR_NULL_POINTER, "bg_output is NULL");

    /* Route BG output through VA nucleus to motor cortex */
    return thal_nucleus_process_input(thal->va, bg_output, size);
}

int thalamus_pulvinar_attention(
    thalamus_t* thal,
    const float* attention_signal,
    uint32_t size
) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    NIMCP_CHECK_THROW(attention_signal, NIMCP_ERROR_NULL_POINTER, "attention_signal is NULL");

    /* Process attention signal through pulvinar */
    int result = thal_nucleus_process_input(thal->pulvinar, attention_signal, size);
    if (result < 0) return result;

    if (size == 0) return 0;

    /* Pulvinar modulates other visual areas */
    float avg_attention = 0.0f;
    for (uint32_t i = 0; i < size && i < thal->pulvinar->num_input_channels; i++) {
        avg_attention += attention_signal[i];
    }
    avg_attention /= (float)size;

    /* Update LGN attention based on pulvinar */
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, avg_attention);

    return 0;
}

thal_nucleus_t* thalamus_get_nucleus(thalamus_t* thal, thal_nucleus_type_t type) {
    if (!thal) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thal is NULL");

        return NULL;

    }

    switch (type) {
        case THAL_NUCLEUS_LGN:      return thal->lgn;
        case THAL_NUCLEUS_MGN:      return thal->mgn;
        case THAL_NUCLEUS_VPL:      return thal->vpl;
        case THAL_NUCLEUS_VPM:      return thal->vpm;
        case THAL_NUCLEUS_VA:       return thal->va;
        case THAL_NUCLEUS_VL:       return thal->vl;
        case THAL_NUCLEUS_PULVINAR: return thal->pulvinar;
        case THAL_NUCLEUS_MD:       return thal->md;
        default:                    return NULL;
    }
}

const thal_nucleus_t* thalamus_get_nucleus_const(
    const thalamus_t* thal,
    thal_nucleus_type_t type
) {
    // Note: Must cast away const to call non-const getter
    // This is safe because thalamus_get_nucleus only returns a pointer, doesn't modify state
    return thalamus_get_nucleus((thalamus_t*)thal, type);
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int thalamus_connect_bio_async(thalamus_t* thal) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    if (thal->bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_THALAMUS,
        .module_name = "thalamus",
        .inbox_capacity = 64,
        .user_data = thal
    };

    thal->bio_ctx = bio_router_register_module(&info);
    if (thal->bio_ctx) {
        thal->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Thalamus connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    return 0;
}

int thalamus_disconnect_bio_async(thalamus_t* thal) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    if (!thal->bio_async_enabled) return 0;

    if (thal->bio_ctx) {
        bio_router_unregister_module(thal->bio_ctx);
        thal->bio_ctx = NULL;
    }
    thal->bio_async_enabled = false;

    return 0;
}

bool thalamus_is_bio_async_connected(const thalamus_t* thal) {
    return thal && thal->bio_async_enabled;
}

//=============================================================================
// Statistics and Debugging
//=============================================================================

int thalamus_get_stats(const thalamus_t* thal, thalamus_stats_t* stats) {
    NIMCP_CHECK_THROW(thal, NIMCP_ERROR_NULL_POINTER, "thal is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    *stats = thal->stats;
    return 0;
}

const char* thal_nucleus_name(thal_nucleus_type_t type) {
    switch (type) {
        case THAL_NUCLEUS_LGN:      return "LGN";
        case THAL_NUCLEUS_MGN:      return "MGN";
        case THAL_NUCLEUS_VPL:      return "VPL";
        case THAL_NUCLEUS_VPM:      return "VPM";
        case THAL_NUCLEUS_VA:       return "VA";
        case THAL_NUCLEUS_VL:       return "VL";
        case THAL_NUCLEUS_PULVINAR: return "Pulvinar";
        case THAL_NUCLEUS_MD:       return "MD";
        case THAL_NUCLEUS_ANTERIOR: return "Anterior";
        case THAL_NUCLEUS_TRN:      return "TRN";
        default:                    return "Unknown";
    }
}

const char* thal_mode_name(thal_firing_mode_t mode) {
    switch (mode) {
        case THAL_MODE_TONIC:     return "Tonic";
        case THAL_MODE_BURST:     return "Burst";
        case THAL_MODE_INHIBITED: return "Inhibited";
        default:                  return "Unknown";
    }
}

float thalamus_get_firing_rate(
    const thalamus_t* thal,
    thal_nucleus_type_t nucleus_type
) {
    if (!thal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamus_get_firing_rate: thal is NULL");
        return 0.0f;
    }

    const thal_nucleus_t* nucleus = thalamus_get_nucleus_const(thal, nucleus_type);
    if (!nucleus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "thalamus_get_firing_rate: invalid nucleus type");
        return 0.0f;
    }

    return nucleus->avg_firing_rate;
}
