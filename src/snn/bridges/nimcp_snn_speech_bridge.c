/**
 * @file nimcp_snn_speech_bridge.c
 * @brief SNN-Speech Cortex integration bridge implementation
 *
 * WHAT: Bidirectional bridge between SNN and speech_cortex_t
 * WHY:  Enable spike-based speech processing with phoneme encoding/decoding
 * HOW:  Population coding for phonemes, temporal coding for sequences
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Gyrus (STG) contains phoneme-selective spiking neurons
 * - Speech perception relies on precise spike timing in auditory cortex
 * - Motor cortex (Broca's area) generates spike sequences for articulation
 * - Phonological working memory exhibits spike-based serial order encoding
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_speech_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_speech_bridge)

//=============================================================================
// Constants
//=============================================================================

/* Standard English phoneme formant centers (F1, F2 in Hz) */
static const float PHONEME_FORMANTS[PHONEME_COUNT][2] = {
    /* Vowels (12) */
    {270, 2290},  /* IY /i/ */
    {390, 1990},  /* IH /ɪ/ */
    {530, 1840},  /* EY /e/ */
    {660, 1720},  /* EH /ɛ/ */
    {730, 1090},  /* AE /æ/ */
    {570, 840},   /* AA /ɑ/ */
    {440, 1020},  /* AO /ɔ/ */
    {370, 950},   /* OW /o/ */
    {440, 1020},  /* UH /ʊ/ */
    {300, 870},   /* UW /u/ */
    {640, 1190},  /* AH /ʌ/ */
    {490, 1350},  /* ER /ə/ */
    /* Stops (6): P,B,T,D,K,G */
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    /* Fricatives (9): F,V,TH,DH,S,Z,SH,ZH,H */
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    /* Nasals (3): M,N,NG */
    {0, 0}, {0, 0}, {0, 0},
    /* Approximants (4): L,R,W,Y */
    {0, 0}, {0, 0}, {0, 0}, {0, 0},
    /* Affricates (2): CH,JH */
    {0, 0}, {0, 0},
    /* Special (2): SILENCE,UNKNOWN */
    {0, 0}, {0, 0}
};

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Convenient starting point for speech processing
 * HOW:  Literature-based parameter values from STG studies
 */
void snn_speech_config_default(snn_speech_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_config_default: null config pointer");
        return;
    }

    /* Encoding configuration */
    config->encoding_method = SNN_ENCODE_POPULATION;
    config->max_spike_rate = 150.0f;         /* STG phoneme neurons */
    config->min_spike_rate = 5.0f;
    config->temporal_window_ms = 50.0f;      /* Phoneme duration ~50-100ms */
    config->neurons_per_phoneme = 10;        /* Population per phoneme */

    /* Decoding configuration */
    config->decoding_method = SNN_DECODE_POPULATION;
    config->decode_window_ms = 50.0f;
    config->use_winner_take_all = true;      /* WTA for phoneme selection */

    /* Phoneme processing */
    config->num_phonemes = PHONEME_COUNT;     /* English IPA subset */
    config->num_formants = 4;                /* F1, F2, F3, F4 */
    config->encode_formants = true;
    config->encode_prosody = true;

    /* Temporal coding */
    config->use_sequence_encoding = true;
    config->use_position_encoding = true;
    config->max_sequence_length = 20;
    config->inter_phoneme_interval_ms = 80.0f;

    /* Phonological working memory */
    config->encode_buffer_position = true;
    config->buffer_capacity = 9;             /* 7±2 items */

    /* Bio-async */
    config->enable_bio_async = false;
    config->update_interval_ms = 50.0f;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Create SNN-speech bridge
 * WHY:  Initialize bidirectional speech processing
 * HOW:  Allocate buffers, create encoder/decoder, init tuning curves
 */
snn_speech_bridge_t* snn_speech_bridge_create(
    const snn_speech_config_t* config,
    snn_network_t* snn,
    speech_cortex_t* speech_cortex
) {
    /* Guard: Validate inputs */
    if (!config || !snn || !speech_cortex) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_speech_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_create: config/snn/speech_cortex is NULL");
        return NULL;
    }

    /* Allocate bridge */
    snn_speech_bridge_t* bridge = nimcp_malloc(sizeof(snn_speech_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-speech bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_speech_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize structure */
    memset(bridge, 0, sizeof(snn_speech_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_speech") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->speech_cortex = speech_cortex;
    bridge->config = *config;

    /* Calculate buffer sizes */
    uint32_t num_neurons = config->num_phonemes * config->neurons_per_phoneme;

    /* Allocate working buffers */
    bridge->formant_buffer = nimcp_malloc(config->num_formants * sizeof(float));
    bridge->phoneme_features_buffer = nimcp_malloc(num_neurons * sizeof(float));
    bridge->spike_input_buffer = nimcp_malloc(num_neurons * sizeof(float));
    bridge->spike_output_buffer = nimcp_malloc(num_neurons * sizeof(float));
    bridge->spike_mask = nimcp_malloc(num_neurons);

    if (!bridge->formant_buffer || !bridge->phoneme_features_buffer ||
        !bridge->spike_input_buffer || !bridge->spike_output_buffer ||
        !bridge->spike_mask) {
        NIMCP_LOGGING_ERROR("Failed to allocate speech bridge buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_speech_bridge_create: failed to allocate buffers");
        snn_speech_bridge_destroy(bridge);
        return NULL;
    }

    /* Allocate sequence buffers */
    bridge->phoneme_sequence = nimcp_malloc(config->max_sequence_length *
                                            sizeof(phoneme_t));
    bridge->sequence_spike_times = nimcp_malloc(config->max_sequence_length *
                                                num_neurons * sizeof(float));
    if (!bridge->phoneme_sequence || !bridge->sequence_spike_times) {
        NIMCP_LOGGING_ERROR("Failed to allocate sequence buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_speech_bridge_create: failed to allocate sequence buffers");
        snn_speech_bridge_destroy(bridge);
        return NULL;
    }

    /* Allocate phonological buffer */
    bridge->phonological_buffer = nimcp_malloc(config->buffer_capacity *
                                               sizeof(phoneme_t));
    bridge->buffer_position_encoding = nimcp_malloc(config->buffer_capacity *
                                                    num_neurons * sizeof(float));
    if (!bridge->phonological_buffer || !bridge->buffer_position_encoding) {
        NIMCP_LOGGING_ERROR("Failed to allocate phonological buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_speech_bridge_create: failed to allocate phonological buffer");
        snn_speech_bridge_destroy(bridge);
        return NULL;
    }

    /* Allocate tuning curves */
    bridge->phoneme_tuning_curves = nimcp_malloc(config->num_phonemes *
                                                  sizeof(float*));
    if (!bridge->phoneme_tuning_curves) {
        NIMCP_LOGGING_ERROR("Failed to allocate tuning curves array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_speech_bridge_create: failed to allocate tuning curves");
        snn_speech_bridge_destroy(bridge);
        return NULL;
    }

    for (uint32_t p = 0; p < config->num_phonemes; p++) {
        bridge->phoneme_tuning_curves[p] = nimcp_malloc(config->neurons_per_phoneme *
                                                        sizeof(float));
        if (!bridge->phoneme_tuning_curves[p]) {
            NIMCP_LOGGING_ERROR("Failed to allocate tuning curve for phoneme %u", p);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_speech_bridge_create: failed to allocate tuning curve");
            snn_speech_bridge_destroy(bridge);
            return NULL;
        }
    }

    bridge->phoneme_preferred_values = nimcp_malloc(num_neurons * sizeof(float));
    if (!bridge->phoneme_preferred_values) {
        NIMCP_LOGGING_ERROR("Failed to allocate preferred values");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_speech_bridge_create: failed to allocate preferred values");
        snn_speech_bridge_destroy(bridge);
        return NULL;
    }

    /* Create rate encoder - uses rate coding for speech phoneme input */
    snn_rate_encoder_config_t rate_enc_cfg;
    snn_rate_encoder_config_default(&rate_enc_cfg);
    rate_enc_cfg.max_rate = config->max_spike_rate;
    rate_enc_cfg.min_rate = config->min_spike_rate;
    rate_enc_cfg.value_min = 0.0f;
    rate_enc_cfg.value_max = 1.0f;
    rate_enc_cfg.use_poisson = true;  /* Biologically realistic */
    bridge->encoder = snn_encoder_create_rate(num_neurons, &rate_enc_cfg);

    /* Create rate decoder */
    snn_rate_decoder_config_t rate_dec_cfg;
    snn_rate_decoder_config_default(&rate_dec_cfg);
    rate_dec_cfg.time_window = config->decode_window_ms;
    rate_dec_cfg.max_rate = config->max_spike_rate;
    rate_dec_cfg.use_exponential = false;
    bridge->decoder = snn_decoder_create_rate(num_neurons, num_neurons, &rate_dec_cfg);

    /* Initialize tuning curves */
    snn_speech_bridge_init_tuning_curves(bridge);

    /* Mark as connected */
    bridge->connected = true;

    bbb_register_module("snn_speech_bridge", BBB_MODULE_TYPE_COGNITIVE);
    NIMCP_LOGGING_INFO("Created SNN-speech bridge (%u phonemes, %u neurons)",
                       config->num_phonemes, num_neurons);
    return bridge;
}

/**
 * WHAT: Destroy bridge and free resources
 * WHY:  Proper cleanup and memory management
 * HOW:  Disconnect, free all buffers, destroy encoder/decoder
 */
void snn_speech_bridge_destroy(snn_speech_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_speech_bridge_disconnect_bio_async(bridge);
    }

    /* Free working buffers */
    if (bridge->formant_buffer) nimcp_free(bridge->formant_buffer);
    if (bridge->phoneme_features_buffer) nimcp_free(bridge->phoneme_features_buffer);
    if (bridge->spike_input_buffer) nimcp_free(bridge->spike_input_buffer);
    if (bridge->spike_output_buffer) nimcp_free(bridge->spike_output_buffer);
    if (bridge->spike_mask) nimcp_free(bridge->spike_mask);

    /* Free sequence buffers */
    if (bridge->phoneme_sequence) nimcp_free(bridge->phoneme_sequence);
    if (bridge->sequence_spike_times) nimcp_free(bridge->sequence_spike_times);

    /* Free phonological buffer */
    if (bridge->phonological_buffer) nimcp_free(bridge->phonological_buffer);
    if (bridge->buffer_position_encoding) nimcp_free(bridge->buffer_position_encoding);

    /* Free tuning curves */
    if (bridge->phoneme_tuning_curves) {
        for (uint32_t p = 0; p < bridge->config.num_phonemes; p++) {
            if (bridge->phoneme_tuning_curves[p]) {
                nimcp_free(bridge->phoneme_tuning_curves[p]);
            }
        }
        nimcp_free(bridge->phoneme_tuning_curves);
    }
    if (bridge->phoneme_preferred_values) nimcp_free(bridge->phoneme_preferred_values);

    /* Destroy encoder/decoder */
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-speech bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging
 * WHY:  Enable distributed speech event coordination
 * HOW:  Register with router as BIO_MODULE_SNN_SPEECH
 */
int snn_speech_bridge_connect_bio_async(snn_speech_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_SPEECH,
        .module_name = "snn_speech_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Speech bridge connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available for speech bridge");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_speech_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int snn_speech_bridge_disconnect_bio_async(snn_speech_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Speech bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query before sending messages
 * HOW:  Return flag
 */
bool snn_speech_bridge_is_bio_async_connected(const snn_speech_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Tuning Curve Functions
//=============================================================================

/**
 * WHAT: Initialize phoneme tuning curves
 * WHY:  Model phoneme-selective neurons in STG
 * HOW:  Gaussian receptive fields in formant space (F1, F2)
 */
int snn_speech_bridge_init_tuning_curves(snn_speech_bridge_t* bridge) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_init_tuning_curves: null bridge pointer");
        return -1;
    }
    if (!bridge->phoneme_tuning_curves) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_speech_bridge_init_tuning_curves: tuning curves not allocated");
        return -1;
    }

    uint32_t npn = bridge->config.neurons_per_phoneme;
    float sigma = 100.0f;  /* Tuning curve width in Hz */

    for (uint32_t p = 0; p < bridge->config.num_phonemes; p++) {
        float f1_center = PHONEME_FORMANTS[p][0];
        float f2_center = PHONEME_FORMANTS[p][1];

        for (uint32_t n = 0; n < npn; n++) {
            /* Distribute neurons around formant center */
            float offset = (float)(n - npn/2) * sigma / 2.0f;
            bridge->phoneme_preferred_values[p * npn + n] = f1_center + offset;

            /* Initialize tuning curve amplitude */
            bridge->phoneme_tuning_curves[p][n] = 1.0f;
        }
    }

    NIMCP_LOGGING_INFO("Initialized %u phoneme tuning curves",
                       bridge->config.num_phonemes);
    return 0;
}

/**
 * WHAT: Compute population activity for phoneme features
 * WHY:  Convert features to population spike rates
 * HOW:  Gaussian activation based on feature distance
 */
int snn_speech_bridge_compute_population_activity(
    snn_speech_bridge_t* bridge,
    const phoneme_features_t* features,
    float* activities_out
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_compute_population_activity: null bridge pointer");
        return -1;
    }
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_compute_population_activity: null features pointer");
        return -1;
    }
    if (!activities_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_compute_population_activity: null activities_out pointer");
        return -1;
    }

    uint32_t npn = bridge->config.neurons_per_phoneme;
    float sigma_sq = 10000.0f;  /* Tuning width squared */

    for (uint32_t p = 0; p < bridge->config.num_phonemes; p++) {
        float f1_center = PHONEME_FORMANTS[p][0];
        float f2_center = PHONEME_FORMANTS[p][1];

        /* Skip non-vowels with zero formants */
        if (f1_center == 0.0f && f2_center == 0.0f) {
            for (uint32_t n = 0; n < npn; n++) {
                activities_out[p * npn + n] = 0.0f;
            }
            continue;
        }

        /* Compute distance in formant space */
        float d1 = features->formant_f1 - f1_center;
        float d2 = features->formant_f2 - f2_center;
        float dist_sq = d1 * d1 + d2 * d2;

        /* Gaussian activation */
        float base_activity = expf(-dist_sq / (2.0f * sigma_sq));

        for (uint32_t n = 0; n < npn; n++) {
            /* Apply tuning curve modulation */
            float activity = base_activity * bridge->phoneme_tuning_curves[p][n];
            activities_out[p * npn + n] = activity;
        }
    }

    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * WHAT: Encode phoneme to spike trains
 * WHY:  Convert phoneme features to SNN input
 * HOW:  Population coding with Gaussian tuning curves
 */
int snn_speech_bridge_encode_phoneme(
    snn_speech_bridge_t* bridge,
    phoneme_t phoneme,
    const phoneme_features_t* features,
    snn_spike_train_t** spike_trains
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_encode_phoneme: null bridge pointer");
        NIMCP_LOGGING_ERROR("Null parameters to encode_phoneme");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_encode_phoneme: null spike_trains pointer");
        NIMCP_LOGGING_ERROR("Null parameters to encode_phoneme");
        return -1;
    }

    /* Validate phoneme */
    if (phoneme >= bridge->config.num_phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_speech_bridge_encode_phoneme: invalid phoneme index");
        NIMCP_LOGGING_ERROR("Invalid phoneme: %u", phoneme);
        return -1;
    }

    uint32_t num_neurons = bridge->config.num_phonemes *
                           bridge->config.neurons_per_phoneme;

    /* Compute population activity */
    if (features) {
        int ret = snn_speech_bridge_compute_population_activity(
            bridge, features, bridge->spike_input_buffer);
        if (ret != 0) {
            return ret;
        }
    } else {
        /* Use one-hot encoding if no features */
        memset(bridge->spike_input_buffer, 0, num_neurons * sizeof(float));
        uint32_t start = phoneme * bridge->config.neurons_per_phoneme;
        for (uint32_t n = 0; n < bridge->config.neurons_per_phoneme; n++) {
            bridge->spike_input_buffer[start + n] = 1.0f;
        }
    }

    /* Encode to spike mask using rate coding */
    float dt = bridge->config.update_interval_ms;
    int ret = snn_encode_rate(bridge->encoder, bridge->spike_input_buffer,
                               dt, bridge->spike_mask);
    if (ret != 0) {
        NIMCP_LOGGING_ERROR("Failed to encode phoneme to spikes");
        return ret;
    }

    /* Allocate spike trains if needed */
    if (!*spike_trains) {
        *spike_trains = (snn_spike_train_t*)nimcp_calloc(num_neurons,
                                                          sizeof(snn_spike_train_t));
        if (!*spike_trains) {
            return SNN_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t i = 0; i < num_neurons; i++) {
            (*spike_trains)[i].neuron_id = i;
        }
    }

    /* Convert binary spike mask to spike trains */
    uint64_t current_time_us = (uint64_t)(bridge->last_update_time_ms * 1000.0f);
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (bridge->spike_mask[i]) {
            snn_spike_train_t* train = &((*spike_trains)[i]);
            train->spike_times[train->write_idx] = current_time_us;
            train->write_idx = (train->write_idx + 1) % SNN_SPIKE_BUFFER_SIZE;
            if (train->count < SNN_SPIKE_BUFFER_SIZE) {
                train->count++;
            }
            train->total_spikes++;
        }
    }

    /* Update statistics */
    bridge->encode_stats.phonemes_encoded++;
    bridge->phoneme_count++;

    return 0;
}

/**
 * WHAT: Encode phoneme sequence to spike patterns
 * WHY:  Enable SNN processing of words/sentences
 * HOW:  Temporal coding with inter-phoneme intervals
 */
int snn_speech_bridge_encode_sequence(
    snn_speech_bridge_t* bridge,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    const phoneme_features_t* features,
    snn_spike_train_t** spike_trains
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_encode_sequence: null bridge pointer");
        return -1;
    }
    if (!phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_encode_sequence: null phonemes pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_encode_sequence: null spike_trains pointer");
        return -1;
    }

    if (num_phonemes > bridge->config.max_sequence_length) {
        NIMCP_LOGGING_WARN("Sequence truncated from %u to %u",
                           num_phonemes, bridge->config.max_sequence_length);
        num_phonemes = bridge->config.max_sequence_length;
    }

    /* Store sequence */
    memcpy(bridge->phoneme_sequence, phonemes, num_phonemes * sizeof(phoneme_t));
    bridge->sequence_length = num_phonemes;

    uint32_t num_neurons = bridge->config.num_phonemes *
                           bridge->config.neurons_per_phoneme;
    float interval = bridge->config.inter_phoneme_interval_ms;

    /* Clear spike times buffer */
    memset(bridge->sequence_spike_times, 0,
           bridge->config.max_sequence_length * num_neurons * sizeof(float));

    /* Encode each phoneme with temporal offset */
    for (uint32_t i = 0; i < num_phonemes; i++) {
        phoneme_t p = phonemes[i];
        float time_offset = i * interval;

        /* Compute population activity for this phoneme */
        if (features) {
            snn_speech_bridge_compute_population_activity(
                bridge, &features[i], bridge->spike_input_buffer);
        } else {
            memset(bridge->spike_input_buffer, 0, num_neurons * sizeof(float));
            uint32_t start = p * bridge->config.neurons_per_phoneme;
            for (uint32_t n = 0; n < bridge->config.neurons_per_phoneme; n++) {
                bridge->spike_input_buffer[start + n] = 1.0f;
            }
        }

        /* Add position encoding if enabled */
        if (bridge->config.use_position_encoding) {
            float pos_factor = 1.0f - 0.1f * i;  /* Primacy effect */
            if (i == num_phonemes - 1) {
                pos_factor += 0.2f;  /* Recency effect */
            }
            for (uint32_t n = 0; n < num_neurons; n++) {
                bridge->spike_input_buffer[n] *= pos_factor;
            }
        }

        /* Store spike times with offset */
        for (uint32_t n = 0; n < num_neurons; n++) {
            if (bridge->spike_input_buffer[n] > 0.1f) {
                bridge->sequence_spike_times[i * num_neurons + n] =
                    time_offset + (1.0f - bridge->spike_input_buffer[n]) * interval * 0.5f;
            }
        }
    }

    /* Convert accumulated activity to spike mask using rate coding */
    float dt = bridge->config.update_interval_ms;
    int ret = snn_encode_rate(bridge->encoder, bridge->spike_input_buffer,
                               dt, bridge->spike_mask);
    if (ret != 0) {
        return ret;
    }

    /* Allocate spike trains if needed */
    if (!*spike_trains) {
        *spike_trains = (snn_spike_train_t*)nimcp_calloc(num_neurons,
                                                          sizeof(snn_spike_train_t));
        if (!*spike_trains) {
            return SNN_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t i = 0; i < num_neurons; i++) {
            (*spike_trains)[i].neuron_id = i;
        }
    }

    /* Convert binary spike mask to spike trains */
    uint64_t current_time_us = (uint64_t)(bridge->last_update_time_ms * 1000.0f);
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (bridge->spike_mask[i]) {
            snn_spike_train_t* train = &((*spike_trains)[i]);
            train->spike_times[train->write_idx] = current_time_us;
            train->write_idx = (train->write_idx + 1) % SNN_SPIKE_BUFFER_SIZE;
            if (train->count < SNN_SPIKE_BUFFER_SIZE) {
                train->count++;
            }
            train->total_spikes++;
        }
    }

    /* Update statistics */
    bridge->encode_stats.sequences_encoded++;
    bridge->encode_stats.avg_sequence_length =
        (bridge->encode_stats.avg_sequence_length *
         (bridge->encode_stats.sequences_encoded - 1) + num_phonemes) /
        bridge->encode_stats.sequences_encoded;

    return 0;
}

/**
 * WHAT: Encode phonological buffer to spikes
 * WHY:  Enable SNN processing of phonological working memory
 * HOW:  Position-dependent encoding of buffer items (7±2)
 */
int snn_speech_bridge_encode_phonological_buffer(
    snn_speech_bridge_t* bridge,
    const phoneme_t* buffer,
    uint32_t buffer_size,
    snn_spike_train_t** spike_trains
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_encode_phonological_buffer: null bridge pointer");
        return -1;
    }
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_encode_phonological_buffer: null buffer pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_encode_phonological_buffer: null spike_trains pointer");
        return -1;
    }

    if (buffer_size > bridge->config.buffer_capacity) {
        buffer_size = bridge->config.buffer_capacity;
    }

    /* Copy to internal buffer */
    memcpy(bridge->phonological_buffer, buffer, buffer_size * sizeof(phoneme_t));
    bridge->buffer_fill = buffer_size;

    uint32_t num_neurons = bridge->config.num_phonemes *
                           bridge->config.neurons_per_phoneme;

    /* Clear input buffer */
    memset(bridge->spike_input_buffer, 0, num_neurons * sizeof(float));

    /* Encode each buffer position with position-dependent modulation */
    for (uint32_t pos = 0; pos < buffer_size; pos++) {
        phoneme_t p = buffer[pos];
        if (p >= bridge->config.num_phonemes) continue;

        /* Position-dependent gain (primacy/recency) */
        float pos_gain = 1.0f;
        if (pos < 2) {
            pos_gain = 1.2f;  /* Primacy boost */
        } else if (pos >= buffer_size - 2) {
            pos_gain = 1.1f;  /* Recency boost */
        }

        /* Add to population activity */
        uint32_t start = p * bridge->config.neurons_per_phoneme;
        for (uint32_t n = 0; n < bridge->config.neurons_per_phoneme; n++) {
            bridge->spike_input_buffer[start + n] += pos_gain / buffer_size;
        }
    }

    /* Normalize */
    for (uint32_t n = 0; n < num_neurons; n++) {
        if (bridge->spike_input_buffer[n] > 1.0f) {
            bridge->spike_input_buffer[n] = 1.0f;
        }
    }

    /* Encode to spike mask using rate coding */
    float dt = bridge->config.update_interval_ms;
    int ret = snn_encode_rate(bridge->encoder, bridge->spike_input_buffer,
                               dt, bridge->spike_mask);
    if (ret != 0) {
        return ret;
    }

    /* Allocate spike trains if needed */
    if (!*spike_trains) {
        *spike_trains = (snn_spike_train_t*)nimcp_calloc(num_neurons,
                                                          sizeof(snn_spike_train_t));
        if (!*spike_trains) {
            return SNN_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t i = 0; i < num_neurons; i++) {
            (*spike_trains)[i].neuron_id = i;
        }
    }

    /* Convert binary spike mask to spike trains */
    uint64_t current_time_us = (uint64_t)(bridge->last_update_time_ms * 1000.0f);
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (bridge->spike_mask[i]) {
            snn_spike_train_t* train = &((*spike_trains)[i]);
            train->spike_times[train->write_idx] = current_time_us;
            train->write_idx = (train->write_idx + 1) % SNN_SPIKE_BUFFER_SIZE;
            if (train->count < SNN_SPIKE_BUFFER_SIZE) {
                train->count++;
            }
            train->total_spikes++;
        }
    }

    return 0;
}

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * WHAT: Decode spike trains to phoneme
 * WHY:  Recognize phoneme from spike activity
 * HOW:  Population vector or winner-take-all decoding
 */
int snn_speech_bridge_decode_phoneme(
    snn_speech_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    phoneme_t* phoneme_out,
    float* confidence_out
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_phoneme: null bridge pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_phoneme: null spike_trains pointer");
        return -1;
    }
    if (!phoneme_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_phoneme: null phoneme_out pointer");
        return -1;
    }

    /* Extract spike counts from spike trains into input buffer */
    for (uint32_t i = 0; i < num_trains; i++) {
        bridge->spike_input_buffer[i] = (float)spike_trains[i].count;
    }

    /* Decode spike counts to activity using rate decoding */
    int ret = snn_decode_rate(bridge->decoder, bridge->spike_input_buffer,
                               bridge->spike_output_buffer);
    if (ret != 0) {
        return ret;
    }

    /* Sum activity per phoneme (population pooling) */
    uint32_t npn = bridge->config.neurons_per_phoneme;
    float max_activity = 0.0f;
    phoneme_t winner = 0;
    float total_activity = 0.0f;

    for (uint32_t p = 0; p < bridge->config.num_phonemes; p++) {
        float sum = 0.0f;
        for (uint32_t n = 0; n < npn && p * npn + n < num_trains; n++) {
            sum += bridge->spike_output_buffer[p * npn + n];
        }

        total_activity += sum;
        if (sum > max_activity) {
            max_activity = sum;
            winner = p;
        }
    }

    *phoneme_out = winner;
    if (confidence_out) {
        *confidence_out = (total_activity > 0.0f) ?
            max_activity / total_activity : 0.0f;
    }

    /* Update statistics */
    bridge->decode_stats.phonemes_decoded++;

    return 0;
}

/**
 * WHAT: Decode spike patterns to phoneme sequence
 * WHY:  Recognize word/sentence from spike activity
 * HOW:  Temporal segmentation + phoneme decoding
 */
int snn_speech_bridge_decode_sequence(
    snn_speech_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    phoneme_t* phonemes_out,
    uint32_t max_phonemes,
    uint32_t* num_decoded
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_sequence: null bridge pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_sequence: null spike_trains pointer");
        return -1;
    }
    if (!phonemes_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_sequence: null phonemes_out pointer");
        return -1;
    }
    if (!num_decoded) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_sequence: null num_decoded pointer");
        return -1;
    }

    /* Extract spike counts from spike trains into input buffer */
    for (uint32_t i = 0; i < num_trains; i++) {
        bridge->spike_input_buffer[i] = (float)spike_trains[i].count;
    }

    /* Decode spike counts to activity using rate decoding */
    int ret = snn_decode_rate(bridge->decoder, bridge->spike_input_buffer,
                               bridge->spike_output_buffer);
    if (ret != 0) {
        return ret;
    }

    /* Simple decoding: find peaks in activity */
    uint32_t npn = bridge->config.neurons_per_phoneme;
    uint32_t decoded_count = 0;
    float threshold = 0.3f;

    for (uint32_t p = 0; p < bridge->config.num_phonemes && decoded_count < max_phonemes; p++) {
        float sum = 0.0f;
        for (uint32_t n = 0; n < npn && p * npn + n < num_trains; n++) {
            sum += bridge->spike_output_buffer[p * npn + n];
        }

        if (sum / npn > threshold) {
            phonemes_out[decoded_count++] = p;
        }
    }

    *num_decoded = decoded_count;
    return 0;
}

/**
 * WHAT: Decode spikes to phoneme features
 * WHY:  Extract acoustic features from spike representation
 * HOW:  Population vector decoding to formant space
 */
int snn_speech_bridge_decode_features(
    snn_speech_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    phoneme_features_t* features_out
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_features: null bridge pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_features: null spike_trains pointer");
        return -1;
    }
    if (!features_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_decode_features: null features_out pointer");
        return -1;
    }

    /* Extract spike counts from spike trains into input buffer */
    for (uint32_t i = 0; i < num_trains; i++) {
        bridge->spike_input_buffer[i] = (float)spike_trains[i].count;
    }

    /* Decode spike counts to activity using rate decoding */
    int ret = snn_decode_rate(bridge->decoder, bridge->spike_input_buffer,
                               bridge->spike_output_buffer);
    if (ret != 0) {
        return ret;
    }

    /* Population vector decoding for formants */
    float f1_sum = 0.0f, f2_sum = 0.0f;
    float weight_sum = 0.0f;
    uint32_t npn = bridge->config.neurons_per_phoneme;

    for (uint32_t p = 0; p < bridge->config.num_phonemes; p++) {
        float f1_center = PHONEME_FORMANTS[p][0];
        float f2_center = PHONEME_FORMANTS[p][1];

        if (f1_center == 0.0f) continue;  /* Skip consonants */

        float phoneme_activity = 0.0f;
        for (uint32_t n = 0; n < npn && p * npn + n < num_trains; n++) {
            phoneme_activity += bridge->spike_output_buffer[p * npn + n];
        }

        f1_sum += f1_center * phoneme_activity;
        f2_sum += f2_center * phoneme_activity;
        weight_sum += phoneme_activity;
    }

    if (weight_sum > 0.0f) {
        features_out->formant_f1 = f1_sum / weight_sum;
        features_out->formant_f2 = f2_sum / weight_sum;
    } else {
        features_out->formant_f1 = 0.0f;
        features_out->formant_f2 = 0.0f;
    }

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

/**
 * WHAT: Update bridge state
 * WHY:  Full update cycle for speech-SNN integration
 * HOW:  Process current speech from cortex
 */
int snn_speech_bridge_update(snn_speech_bridge_t* bridge, float dt) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_update: null bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_speech_bridge_update: bridge not connected");
        return -1;
    }

    /* Check update interval */
    bridge->last_update_time_ms += dt;
    if (bridge->last_update_time_ms < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time_ms = 0.0f;

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get encoding statistics
 * WHY:  Monitor encoding performance
 * HOW:  Copy statistics structure
 */
int snn_speech_bridge_get_encode_stats(
    const snn_speech_bridge_t* bridge,
    snn_speech_encode_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_get_encode_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_get_encode_stats: null stats pointer");
        return -1;
    }
    *stats = bridge->encode_stats;
    return 0;
}

/**
 * WHAT: Get decoding statistics
 * WHY:  Monitor decoding performance
 * HOW:  Copy statistics structure
 */
int snn_speech_bridge_get_decode_stats(
    const snn_speech_bridge_t* bridge,
    snn_speech_decode_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_get_decode_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_get_decode_stats: null stats pointer");
        return -1;
    }
    *stats = bridge->decode_stats;
    return 0;
}

/**
 * WHAT: Get current spike rate for phoneme
 * WHY:  Query individual phoneme activity
 * HOW:  Return average rate for phoneme population from input buffer
 */
float snn_speech_bridge_get_phoneme_spike_rate(
    const snn_speech_bridge_t* bridge,
    phoneme_t phoneme
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_get_phoneme_spike_rate: null bridge pointer");
        return -1.0f;
    }
    if (!bridge->spike_input_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_speech_bridge_get_phoneme_spike_rate: input buffer not allocated");
        return -1.0f;
    }
    if (phoneme >= bridge->config.num_phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_speech_bridge_get_phoneme_spike_rate: invalid phoneme index");
        return -1.0f;
    }

    uint32_t start = phoneme * bridge->config.neurons_per_phoneme;
    float sum = 0.0f;

    for (uint32_t n = 0; n < bridge->config.neurons_per_phoneme; n++) {
        float normalized = bridge->spike_input_buffer[start + n];
        float rate_range = bridge->config.max_spike_rate - bridge->config.min_spike_rate;
        sum += bridge->config.min_spike_rate + normalized * rate_range;
    }

    return sum / bridge->config.neurons_per_phoneme;
}

/**
 * WHAT: Check if bridge is active
 * WHY:  Validate bridge state
 * HOW:  Return connected flag
 */
bool snn_speech_bridge_is_active(const snn_speech_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->connected;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * WHAT: Reset bridge statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero all counters
 */
void snn_speech_bridge_reset_stats(snn_speech_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_speech_bridge_reset_stats: null bridge pointer");
        return;
    }

    memset(&bridge->encode_stats, 0, sizeof(snn_speech_encode_stats_t));
    memset(&bridge->decode_stats, 0, sizeof(snn_speech_decode_stats_t));
    bridge->phoneme_count = 0;
}

//=============================================================================
// Language Bridge Integration (Phase 8.6)
//=============================================================================

#include "snn/bridges/nimcp_snn_language_bridge.h"

/** Phoneme to approximate character mapping for word reconstruction */
static const char* phoneme_to_char(phoneme_t p) {
    switch (p) {
        /* Vowels */
        case PHONEME_AA: return "a";  case PHONEME_AE: return "a";
        case PHONEME_AH: return "u";  case PHONEME_AO: return "o";
        case PHONEME_EH: return "e";  case PHONEME_ER: return "er";
        case PHONEME_EY: return "ay"; case PHONEME_IH: return "i";
        case PHONEME_IY: return "ee"; case PHONEME_OW: return "o";
        case PHONEME_UH: return "u";  case PHONEME_UW: return "oo";
        /* Consonants: stops */
        case PHONEME_P: return "p"; case PHONEME_B: return "b";
        case PHONEME_T: return "t"; case PHONEME_D: return "d";
        case PHONEME_K: return "k"; case PHONEME_G: return "g";
        /* Consonants: fricatives */
        case PHONEME_F: return "f"; case PHONEME_V: return "v";
        case PHONEME_TH: return "th"; case PHONEME_DH: return "th";
        case PHONEME_S: return "s"; case PHONEME_Z: return "z";
        case PHONEME_SH: return "sh"; case PHONEME_ZH: return "zh";
        case PHONEME_H: return "h";
        /* Consonants: nasals + approximants */
        case PHONEME_M: return "m"; case PHONEME_N: return "n";
        case PHONEME_NG: return "ng";
        case PHONEME_L: return "l"; case PHONEME_R: return "r";
        case PHONEME_W: return "w"; case PHONEME_Y: return "y";
        /* Affricates */
        case PHONEME_CH: return "ch"; case PHONEME_JH: return "j";
        default: return "";
    }
}

int snn_speech_bridge_set_language_bridge(
    snn_speech_bridge_t* bridge,
    struct snn_language_bridge* lang_bridge)
{
    if (!bridge) return -1;
    bridge->lang_bridge = lang_bridge;
    bridge->phoneme_accum_count = 0;
    bridge->last_phoneme_time_ms = 0.0f;
    return 0;
}

/** Try to match accumulated phonemes to a registered word on the language bridge */
static int try_fire_word(snn_speech_bridge_t* bridge, float time_ms) {
    if (!bridge->lang_bridge || bridge->phoneme_accum_count == 0) return -1;

    /* Reconstruct approximate word from phoneme sequence */
    char word_buf[128] = {0};
    size_t pos = 0;
    for (uint32_t i = 0; i < bridge->phoneme_accum_count && pos < 120; i++) {
        const char* ch = phoneme_to_char(bridge->phoneme_accum[i]);
        size_t len = strlen(ch);
        memcpy(word_buf + pos, ch, len);
        pos += len;
    }
    word_buf[pos] = '\0';

    /* Fire word spike on language bridge — the bridge will look up the word pop */
    /* We use a simple hash to find the word population index */
    /* The language bridge's comprehend function handles word→pop lookup internally */
    float concepts[1] = {0};
    uint32_t activated = 0;
    float conf = 0;
    if (pos > 0) {
        snn_language_bridge_comprehend(bridge->lang_bridge, word_buf,
                                       concepts, 1, &activated, &conf);
    }

    bridge->phoneme_accum_count = 0;
    return (activated > 0) ? 0 : -1;
}

int snn_speech_bridge_accumulate_phoneme(
    snn_speech_bridge_t* bridge,
    phoneme_t phoneme,
    float time_ms)
{
    if (!bridge) return -1;

    /* Silence or unknown = word boundary */
    if (phoneme == PHONEME_SILENCE || phoneme == PHONEME_UNKNOWN) {
        int result = try_fire_word(bridge, time_ms);
        bridge->last_phoneme_time_ms = time_ms;
        return result;
    }

    /* Time gap > 200ms = word boundary */
    if (bridge->phoneme_accum_count > 0 &&
        (time_ms - bridge->last_phoneme_time_ms) > 200.0f) {
        try_fire_word(bridge, time_ms);
    }

    /* Accumulate phoneme */
    if (bridge->phoneme_accum_count < 64) {
        bridge->phoneme_accum[bridge->phoneme_accum_count++] = phoneme;
    }
    bridge->last_phoneme_time_ms = time_ms;

    return 0;
}

int snn_speech_bridge_flush_accumulator(snn_speech_bridge_t* bridge, float time_ms) {
    if (!bridge) return -1;
    return try_fire_word(bridge, time_ms);
}

//=============================================================================
// Phase 8.6b: Word -> Phoneme Production (Reverse Path)
//=============================================================================

/**
 * @brief Simple letter-to-phoneme table for common English words
 *
 * WHAT: Lookup table mapping word population indices to phoneme sequences
 * WHY:  Fast reverse path from word concept to articulatory commands
 * HOW:  Static table of common words; fallback to letter rules for unknowns
 */
typedef struct {
    const char* word;
    const phoneme_t phonemes[12];
    uint32_t count;
} word_phoneme_entry_t;

static const word_phoneme_entry_t WORD_PHONEME_TABLE[] = {
    {"the",    {PHONEME_DH, PHONEME_AH}, 2},
    {"a",      {PHONEME_AH}, 1},
    {"is",     {PHONEME_IH, PHONEME_Z}, 2},
    {"and",    {PHONEME_AE, PHONEME_N, PHONEME_D}, 3},
    {"or",     {PHONEME_AO, PHONEME_R}, 2},
    {"not",    {PHONEME_N, PHONEME_AA, PHONEME_T}, 3},
    {"yes",    {PHONEME_Y, PHONEME_EH, PHONEME_S}, 3},
    {"no",     {PHONEME_N, PHONEME_OW}, 2},
    {"hello",  {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW}, 4},
    {"world",  {PHONEME_W, PHONEME_ER, PHONEME_L, PHONEME_D}, 4},
    {"brain",  {PHONEME_B, PHONEME_R, PHONEME_EY, PHONEME_N}, 4},
    {"think",  {PHONEME_TH, PHONEME_IH, PHONEME_NG, PHONEME_K}, 4},
    {"know",   {PHONEME_N, PHONEME_OW}, 2},
    {"see",    {PHONEME_S, PHONEME_IY}, 2},
    {"say",    {PHONEME_S, PHONEME_EY}, 2},
    {"good",   {PHONEME_G, PHONEME_UH, PHONEME_D}, 3},
    {"time",   {PHONEME_T, PHONEME_AA, PHONEME_IY, PHONEME_M}, 4},
    {"make",   {PHONEME_M, PHONEME_EY, PHONEME_K}, 3},
    {"go",     {PHONEME_G, PHONEME_OW}, 2},
    {"come",   {PHONEME_K, PHONEME_AH, PHONEME_M}, 3},
    {NULL, {0}, 0}  /* Sentinel */
};

/**
 * WHAT: Simple letter-to-phoneme fallback for unknown words
 * WHY:  Production path must handle any word, not just table entries
 * HOW:  Map each consonant/vowel letter to closest phoneme
 */
static phoneme_t letter_to_phoneme(char c)
{
    switch (c) {
    case 'a': return PHONEME_AE;
    case 'e': return PHONEME_EH;
    case 'i': return PHONEME_IH;
    case 'o': return PHONEME_OW;
    case 'u': return PHONEME_AH;
    case 'b': return PHONEME_B;
    case 'c': return PHONEME_K;
    case 'd': return PHONEME_D;
    case 'f': return PHONEME_F;
    case 'g': return PHONEME_G;
    case 'h': return PHONEME_H;
    case 'j': return PHONEME_JH;
    case 'k': return PHONEME_K;
    case 'l': return PHONEME_L;
    case 'm': return PHONEME_M;
    case 'n': return PHONEME_N;
    case 'p': return PHONEME_P;
    case 'q': return PHONEME_K;
    case 'r': return PHONEME_R;
    case 's': return PHONEME_S;
    case 't': return PHONEME_T;
    case 'v': return PHONEME_V;
    case 'w': return PHONEME_W;
    case 'x': return PHONEME_K;
    case 'y': return PHONEME_Y;
    case 'z': return PHONEME_Z;
    default:  return PHONEME_SILENCE;
    }
}

/**
 * WHAT: Look up a word's phoneme sequence by word_pop_index
 * WHY:  Word population index maps to a registered word on the language bridge
 * HOW:  Get word string from language bridge, then table lookup or letter rules
 */
int snn_speech_bridge_produce_word(
    snn_speech_bridge_t* bridge,
    uint32_t word_pop_index,
    phoneme_t* phoneme_out,
    uint32_t* num_phonemes_out,
    uint32_t max_phonemes)
{
    if (!bridge || !phoneme_out || !num_phonemes_out) return -1;
    if (max_phonemes == 0) return -1;

    *num_phonemes_out = 0;

    /* Get word string directly from language bridge by population index */
    const char* word = NULL;
    if (bridge->lang_bridge) {
        word = snn_language_bridge_get_word_form(bridge->lang_bridge,
                                                  word_pop_index);
    }

    /* Table lookup first */
    if (word) {
        for (uint32_t i = 0; WORD_PHONEME_TABLE[i].word != NULL; i++) {
            if (strcmp(WORD_PHONEME_TABLE[i].word, word) == 0) {
                uint32_t n = WORD_PHONEME_TABLE[i].count;
                if (n > max_phonemes) n = max_phonemes;
                memcpy(phoneme_out, WORD_PHONEME_TABLE[i].phonemes,
                       n * sizeof(phoneme_t));
                *num_phonemes_out = n;
                return 0;
            }
        }

        /* Fallback: letter-to-phoneme rules */
        uint32_t n = 0;
        for (uint32_t j = 0; word[j] && n < max_phonemes; j++) {
            char c = word[j];
            if (c >= 'A' && c <= 'Z') c += 32;  /* tolower */
            if (c >= 'a' && c <= 'z') {
                phoneme_out[n++] = letter_to_phoneme(c);
            }
        }
        *num_phonemes_out = n;
        return 0;
    }

    /* No word available — generate from index as simple mapping */
    uint32_t base_phoneme = word_pop_index % PHONEME_COUNT;
    phoneme_out[0] = (phoneme_t)base_phoneme;
    *num_phonemes_out = 1;

    return 0;
}

/**
 * WHAT: Encode word production as temporally-ordered spike trains
 * WHY:  Motor cortex needs sequential phoneme activations for articulation
 * HOW:  Produce phonemes, then encode each using snn_speech_bridge_encode_phoneme
 *        with sequential timing (inter_phoneme_interval_ms between each)
 */
int snn_speech_bridge_encode_word_production(
    snn_speech_bridge_t* bridge,
    uint32_t word_pop_index,
    float start_time_ms,
    snn_spike_train_t** spike_trains_out,
    uint32_t* num_trains_out)
{
    if (!bridge || !spike_trains_out || !num_trains_out) return -1;

    *spike_trains_out = NULL;
    *num_trains_out = 0;

    /* Get phoneme sequence for this word */
    phoneme_t phonemes[32];
    uint32_t num_phonemes = 0;
    int rc = snn_speech_bridge_produce_word(bridge, word_pop_index,
                                             phonemes, &num_phonemes, 32);
    if (rc != 0 || num_phonemes == 0) return -1;

    /* Allocate array to collect per-phoneme spike trains */
    snn_spike_train_t** per_phoneme_trains = (snn_spike_train_t**)nimcp_calloc(
        num_phonemes, sizeof(snn_spike_train_t*));
    if (!per_phoneme_trains) return -1;

    /* Encode each phoneme with sequential timing */
    float interval = bridge->config.inter_phoneme_interval_ms;
    if (interval < 1.0f) interval = 30.0f;  /* Default 30ms gap */

    uint32_t num_neurons_per = bridge->config.num_phonemes *
                               bridge->config.neurons_per_phoneme;
    uint32_t successful = 0;

    for (uint32_t i = 0; i < num_phonemes; i++) {
        float saved_time = bridge->last_update_time_ms;
        bridge->last_update_time_ms = start_time_ms + (float)i * interval;

        phoneme_features_t features;
        memset(&features, 0, sizeof(features));
        uint32_t idx = (uint32_t)phonemes[i];
        if (idx < PHONEME_COUNT) {
            features.formant_f1 = PHONEME_FORMANTS[idx][0];
            features.formant_f2 = PHONEME_FORMANTS[idx][1];
        }

        snn_spike_train_t* trains = NULL;
        int enc_rc = snn_speech_bridge_encode_phoneme(bridge, phonemes[i],
                                                       &features, &trains);
        bridge->last_update_time_ms = saved_time;

        if (enc_rc == 0 && trains) {
            per_phoneme_trains[successful] = trains;
            successful++;
        }
    }

    if (successful == 0) {
        nimcp_free(per_phoneme_trains);
        return -1;
    }

    /* Merge all per-phoneme spike trains into a single output array */
    uint32_t total_trains = successful * num_neurons_per;
    snn_spike_train_t* merged = (snn_spike_train_t*)nimcp_calloc(
        total_trains, sizeof(snn_spike_train_t));
    if (!merged) {
        for (uint32_t i = 0; i < successful; i++) {
            nimcp_free(per_phoneme_trains[i]);
        }
        nimcp_free(per_phoneme_trains);
        return -1;
    }

    for (uint32_t i = 0; i < successful; i++) {
        memcpy(&merged[i * num_neurons_per], per_phoneme_trains[i],
               num_neurons_per * sizeof(snn_spike_train_t));
        nimcp_free(per_phoneme_trains[i]);
    }
    nimcp_free(per_phoneme_trains);

    *spike_trains_out = merged;
    *num_trains_out = total_trains;

    return 0;
}
