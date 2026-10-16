/**
 * @file nimcp_parietal_linguistics_snn_bridge.c
 * @brief SNN Integration Bridge Implementation for Parietal Linguistics
 * @version 1.0.0
 * @date 2026-01-31
 */

#include "cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_snn_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_learning_constants.h"

/* ============================================================================
 * PRIVATE CONSTANTS
 * ============================================================================ */

#define LING_SNN_MAGIC 0x4C534E4E  /* "LSNN" */

/* Thread-local error message */
static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* Phoneme names (IPA) */
static const char* PHONEME_NAMES[LING_PHONEME_COUNT] = {
    /* Vowels */
    "IY", "IH", "EY", "EH", "AE", "AA", "AO", "OW", "UH", "UW", "AH", "ER",
    /* Stops */
    "P", "B", "T", "D", "K", "G",
    /* Fricatives */
    "F", "V", "TH", "DH", "S", "Z", "SH", "ZH", "H",
    /* Nasals */
    "M", "N", "NG",
    /* Approximants */
    "L", "R", "W", "Y",
    /* Affricates */
    "CH", "JH",
    /* Diphthongs */
    "AY", "AW", "OY", "EYR", "IYR", "UWR",
    /* Special */
    "SIL", "UNK"
};

/* Phoneme voicing */
static const bool PHONEME_VOICED[LING_PHONEME_COUNT] = {
    /* Vowels - all voiced */
    true, true, true, true, true, true, true, true, true, true, true, true,
    /* Stops - P,T,K unvoiced, B,D,G voiced */
    false, true, false, true, false, true,
    /* Fricatives */
    false, true, false, true, false, true, false, true, false,
    /* Nasals - all voiced */
    true, true, true,
    /* Approximants - all voiced */
    true, true, true, true,
    /* Affricates */
    false, true,
    /* Diphthongs - all voiced */
    true, true, true, true, true, true,
    /* Special */
    false, false
};

/* ============================================================================
 * PRIVATE TYPES
 * ============================================================================ */

/**
 * @brief Internal SNN bridge structure
 */
struct ling_snn_bridge {
    uint32_t magic;                     /**< Magic number for validation */

    /* Configuration */
    ling_snn_bridge_config_t config;

    /* Encoders */
    snn_encoder_t* phoneme_encoder;     /**< Temporal encoder for phonemes */
    snn_encoder_t* spatial_encoder;     /**< Population encoder for spatial */
    snn_encoder_t* number_encoder;      /**< Rate encoder for numbers */

    /* Decoders */
    snn_decoder_t* phoneme_decoder;
    snn_decoder_t* spatial_decoder;
    snn_decoder_t* number_decoder;

    /* Population state */
    float* phoneme_rates;               /**< Phoneme population firing rates */
    float* spatial_rates;               /**< Spatial population firing rates */
    float* number_rates;                /**< Number population firing rates */

    /* Working buffers */
    uint8_t* spike_buffer;              /**< Spike generation buffer */
    float* rate_buffer;                 /**< Rate computation buffer */

    /* Mesh integration */
    linguistics_mesh_t* mesh;
    linguistics_belief_t current_belief;
    float current_precision;

    /* Statistics */
    ling_snn_bridge_stats_t stats;

    /* RNG state */
    uint64_t rng_state;
};

/* ============================================================================
 * PRIVATE FUNCTIONS
 * ============================================================================ */

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static float random_uniform(uint64_t* state) {
    return (float)(xorshift64(state) & 0xFFFFFFFF) / (float)0xFFFFFFFF;
}

static float compute_rate_stability(const float* rates, uint32_t count) {
    if (count == 0) return 0.0f;

    float mean = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        mean += rates[i];
    }
    mean /= count;

    float variance = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = rates[i] - mean;
        variance += diff * diff;
    }
    variance /= count;

    /* Stability = 1 / (1 + coefficient of variation) */
    float cv = (mean > 0.0f) ? sqrtf(variance) / mean : 1.0f;
    return 1.0f / (1.0f + cv);
}

static float weber_fechner_rate(float magnitude, float max_rate, float min_rate) {
    /* Weber-Fechner law: perceived intensity ~ log(stimulus) */
    if (magnitude <= 0.0f) return min_rate;

    float log_mag = logf(magnitude + 1.0f);
    float max_log = logf(1001.0f);  /* Approximate upper bound */

    float normalized = log_mag / max_log;
    return min_rate + normalized * (max_rate - min_rate);
}

static float inverse_weber_fechner(float rate, float max_rate, float min_rate) {
    float normalized = (rate - min_rate) / (max_rate - min_rate);
    if (normalized <= 0.0f) return 0.0f;

    float max_log = logf(1001.0f);
    float log_mag = normalized * max_log;

    return expf(log_mag) - 1.0f;
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

ling_snn_bridge_config_t ling_snn_bridge_default_config(void) {
    ling_snn_bridge_config_t config = {
        .neurons_per_phoneme = LING_SNN_NEURONS_PER_PHONEME,
        .neurons_per_spatial = LING_SNN_NEURONS_PER_SPATIAL,
        .neurons_per_number = LING_SNN_NEURONS_PER_NUMBER,

        .phoneme_encoding = SNN_ENCODE_TEMPORAL,
        .spatial_encoding = SNN_ENCODE_POPULATION,
        .number_encoding = SNN_ENCODE_RATE,

        .phoneme_topology = SNN_TOPO_RECURRENT,
        .spatial_topology = SNN_TOPO_FEEDFORWARD,

        .max_firing_rate = 100.0f,
        .min_firing_rate = 1.0f,

        .base_precision = LING_SNN_DEFAULT_PRECISION,
        .rate_stability_weight = 0.5f,

        .dt_ms = 0.1f,
        .encoding_window_ms = 50.0f,

        .enable_mesh = true,
        .enable_stdp = true,
        .enable_health = true,
        .enable_logging = true
    };
    return config;
}

ling_snn_bridge_t* ling_snn_bridge_create(
    const ling_snn_bridge_config_t* config
) {
    ling_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(ling_snn_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate SNN bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ling_snn_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = LING_SNN_MAGIC;

    /* Use provided config or defaults */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = ling_snn_bridge_default_config();
    }

    /* Initialize RNG */
    bridge->rng_state = (uint64_t)time(NULL) ^ 0xDEADBEEFCAFEBABE;

    /* Allocate population rate buffers */
    uint32_t phoneme_pop_size = LING_SNN_NUM_PHONEMES * bridge->config.neurons_per_phoneme;
    uint32_t spatial_pop_size = LING_SNN_NUM_SPATIAL_WORDS * bridge->config.neurons_per_spatial;
    uint32_t number_pop_size = LING_SNN_NUM_NUMBER_TYPES * bridge->config.neurons_per_number;

    bridge->phoneme_rates = nimcp_calloc(phoneme_pop_size, sizeof(float));
    bridge->spatial_rates = nimcp_calloc(spatial_pop_size, sizeof(float));
    bridge->number_rates = nimcp_calloc(number_pop_size, sizeof(float));

    if (!bridge->phoneme_rates || !bridge->spatial_rates || !bridge->number_rates) {
        set_error("Failed to allocate population rate buffers");
        ling_snn_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ling_snn_bridge_create: required parameter is NULL (bridge->phoneme_rates, bridge->spatial_rates, bridge->number_rates)");
        return NULL;
    }

    /* Allocate working buffers */
    uint32_t max_pop = phoneme_pop_size;
    if (spatial_pop_size > max_pop) max_pop = spatial_pop_size;
    if (number_pop_size > max_pop) max_pop = number_pop_size;

    bridge->spike_buffer = nimcp_calloc(max_pop, sizeof(uint8_t));
    bridge->rate_buffer = nimcp_calloc(max_pop, sizeof(float));

    if (!bridge->spike_buffer || !bridge->rate_buffer) {
        set_error("Failed to allocate working buffers");
        ling_snn_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ling_snn_bridge_create: required parameter is NULL (bridge->spike_buffer, bridge->rate_buffer)");
        return NULL;
    }

    /* Create encoders */
    snn_temporal_encoder_config_t temp_config;
    snn_temporal_encoder_config_default(&temp_config);
    temp_config.t_min = 0.0f;
    temp_config.t_max = bridge->config.encoding_window_ms;
    bridge->phoneme_encoder = snn_encoder_create_temporal(
        LING_SNN_NUM_PHONEMES, &temp_config);

    snn_population_encoder_config_t pop_config;
    snn_population_encoder_config_default(&pop_config);
    pop_config.n_neurons = bridge->config.neurons_per_spatial;
    pop_config.sigma = 0.3f;
    bridge->spatial_encoder = snn_encoder_create_population(
        LING_SNN_NUM_SPATIAL_WORDS, &pop_config);

    snn_rate_encoder_config_t rate_config;
    snn_rate_encoder_config_default(&rate_config);
    rate_config.max_rate = bridge->config.max_firing_rate;
    rate_config.min_rate = bridge->config.min_firing_rate;
    bridge->number_encoder = snn_encoder_create_rate(
        LING_SNN_NUM_NUMBER_TYPES, &rate_config);

    /* Create decoders */
    snn_rate_decoder_config_t dec_config;
    snn_rate_decoder_config_default(&dec_config);
    dec_config.time_window = bridge->config.encoding_window_ms;
    bridge->phoneme_decoder = snn_decoder_create_rate(
        phoneme_pop_size, LING_SNN_NUM_PHONEMES, &dec_config);
    bridge->spatial_decoder = snn_decoder_create_rate(
        spatial_pop_size, LING_SNN_NUM_SPATIAL_WORDS, &dec_config);
    bridge->number_decoder = snn_decoder_create_rate(
        number_pop_size, LING_SNN_NUM_NUMBER_TYPES, &dec_config);

    /* Initialize precision */
    bridge->current_precision = bridge->config.base_precision;

    /* Initialize belief */
    memset(&bridge->current_belief, 0, sizeof(bridge->current_belief));

    return bridge;
}

void ling_snn_bridge_destroy(ling_snn_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy encoders */
    if (bridge->phoneme_encoder) snn_encoder_destroy(bridge->phoneme_encoder);
    if (bridge->spatial_encoder) snn_encoder_destroy(bridge->spatial_encoder);
    if (bridge->number_encoder) snn_encoder_destroy(bridge->number_encoder);

    /* Destroy decoders */
    if (bridge->phoneme_decoder) snn_decoder_destroy(bridge->phoneme_decoder);
    if (bridge->spatial_decoder) snn_decoder_destroy(bridge->spatial_decoder);
    if (bridge->number_decoder) snn_decoder_destroy(bridge->number_decoder);

    /* Free buffers */
    nimcp_free(bridge->phoneme_rates);
    nimcp_free(bridge->spatial_rates);
    nimcp_free(bridge->number_rates);
    nimcp_free(bridge->spike_buffer);
    nimcp_free(bridge->rate_buffer);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;
}

int ling_snn_bridge_register_mesh(
    ling_snn_bridge_t* bridge,
    linguistics_mesh_t* mesh
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        set_error("Invalid bridge");
        return LING_SNN_ERR_NULL;
    }

    bridge->mesh = mesh;

    if (mesh && bridge->config.enable_mesh) {
        linguistics_mesh_handler_t handler;
        int ret = ling_snn_get_mesh_handler(bridge, &handler);
        if (ret != LING_SNN_ERR_OK) {
            return ret;
        }

        ret = linguistics_mesh_register_participant(mesh, BIO_MODULE_LING_SNN_BRIDGE,
                                                     "snn_bridge", handler);
        if (ret != 0) {
            set_error("Failed to register with mesh");
            return LING_SNN_ERR_MESH_REGISTER;
        }
    }

    return LING_SNN_ERR_OK;
}

/* ============================================================================
 * PHONEME ENCODING API
 * ============================================================================ */

int ling_snn_encode_phoneme(
    ling_snn_bridge_t* bridge,
    ling_phoneme_t phoneme,
    float duration_ms,
    ling_phoneme_encoding_t* result
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        set_error("Invalid bridge");
        return LING_SNN_ERR_NULL;
    }
    if (!result) {
        set_error("NULL result pointer");
        return LING_SNN_ERR_NULL;
    }
    if (phoneme < 0 || phoneme >= LING_PHONEME_COUNT) {
        set_error("Invalid phoneme: %d", phoneme);
        return LING_SNN_ERR_INVALID_PHONEME;
    }

    memset(result, 0, sizeof(*result));
    result->phoneme = phoneme;
    result->duration_ms = duration_ms;

    /* Generate temporal spike encoding */
    uint32_t pop_offset = phoneme * bridge->config.neurons_per_phoneme;
    uint32_t num_neurons = bridge->config.neurons_per_phoneme;

    /* Generate spike times with jitter */
    float base_time = (float)phoneme / (float)LING_PHONEME_COUNT * duration_ms;
    uint32_t spike_count = 0;

    for (uint32_t i = 0; i < num_neurons && spike_count < 64; i++) {
        /* Each neuron fires at slightly different time */
        float jitter = (random_uniform(&bridge->rng_state) - 0.5f) * 2.0f;
        float spike_time = base_time + jitter;

        if (spike_time >= 0.0f && spike_time <= duration_ms) {
            result->spike_train.spike_times[spike_count] =
                (uint64_t)(spike_time * 1000.0f);  /* Convert to microseconds */
            spike_count++;

            /* Update population rate */
            bridge->phoneme_rates[pop_offset + i] =
                bridge->config.max_firing_rate * 0.8f;
        }
    }

    result->spike_train.spike_count = spike_count;
    result->spike_train.inst_rate = (spike_count > 0) ?
        (float)spike_count / (duration_ms / 1000.0f) : 0.0f;
    result->spike_train.avg_rate = result->spike_train.inst_rate;

    /* Compute encoding confidence based on spike count */
    result->encoding_confidence = (spike_count > 0) ?
        fminf(1.0f, (float)spike_count / (float)num_neurons) : 0.0f;

    /* Generate synthetic formants based on phoneme class */
    if (phoneme < 12) {  /* Vowels */
        result->formants[0] = 300.0f + phoneme * 50.0f;  /* F1 */
        result->formants[1] = 700.0f + phoneme * 100.0f; /* F2 */
        result->formants[2] = 2500.0f;
        result->formants[3] = 3500.0f;
    }

    bridge->stats.total_encodings++;
    bridge->stats.total_spikes += spike_count;

    return LING_SNN_ERR_OK;
}

int ling_snn_encode_phoneme_sequence(
    ling_snn_bridge_t* bridge,
    const ling_phoneme_t* phonemes,
    const float* durations,
    uint32_t count,
    ling_phoneme_encoding_t* results
) {
    if (!bridge || !phonemes || !durations || !results) {
        set_error("NULL pointer");
        return LING_SNN_ERR_NULL;
    }

    for (uint32_t i = 0; i < count; i++) {
        int ret = ling_snn_encode_phoneme(bridge, phonemes[i],
                                           durations[i], &results[i]);
        if (ret != LING_SNN_ERR_OK) {
            return ret;
        }
    }

    return LING_SNN_ERR_OK;
}

int ling_snn_decode_phoneme(
    ling_snn_bridge_t* bridge,
    const ling_spike_train_t* spike_train,
    ling_phoneme_t* phoneme,
    float* confidence
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        set_error("Invalid bridge");
        return LING_SNN_ERR_NULL;
    }
    if (!spike_train || !phoneme) {
        set_error("NULL pointer");
        return LING_SNN_ERR_NULL;
    }

    /* Simple decoding: map average spike time to phoneme index */
    if (spike_train->spike_count == 0) {
        *phoneme = LING_PHONEME_SILENCE;
        if (confidence) *confidence = 1.0f;
        return LING_SNN_ERR_OK;
    }

    /* Compute mean spike time */
    float mean_time = 0.0f;
    for (uint32_t i = 0; i < spike_train->spike_count; i++) {
        mean_time += spike_train->spike_times[i];
    }
    mean_time /= spike_train->spike_count;

    /* Map time to phoneme (inverse of encoding) */
    float normalized = mean_time / (bridge->config.encoding_window_ms * 1000.0f);
    int idx = (int)(normalized * LING_PHONEME_COUNT);
    if (idx < 0) idx = 0;
    if (idx >= LING_PHONEME_COUNT) idx = LING_PHONEME_COUNT - 1;

    *phoneme = (ling_phoneme_t)idx;

    if (confidence) {
        /* Confidence based on spike count and rate stability */
        *confidence = fminf(1.0f, spike_train->spike_count / 8.0f);
    }

    bridge->stats.total_decodings++;

    return LING_SNN_ERR_OK;
}

/* ============================================================================
 * SPATIAL WORD ENCODING API
 * ============================================================================ */

int ling_snn_encode_spatial_word(
    ling_snn_bridge_t* bridge,
    spatial_preposition_t preposition,
    float activation,
    ling_spatial_encoding_t* result
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        set_error("Invalid bridge");
        return LING_SNN_ERR_NULL;
    }
    if (!result) {
        set_error("NULL result pointer");
        return LING_SNN_ERR_NULL;
    }
    if (preposition < 0 || preposition >= SPATIAL_PREPOSITION_COUNT) {
        set_error("Invalid preposition: %d", preposition);
        return LING_SNN_ERR_INVALID_WORD;
    }

    memset(result, 0, sizeof(*result));
    result->preposition = preposition;

    /* Generate population activity with Gaussian tuning */
    float sigma = 2.0f;  /* Tuning curve width in preposition space */
    float target = (float)preposition;

    for (uint32_t i = 0; i < LING_SNN_NUM_SPATIAL_WORDS; i++) {
        float dist = fabsf((float)i - target);
        float gaussian = expf(-dist * dist / (2.0f * sigma * sigma));
        result->population_activity[i] = gaussian * activation;

        /* Update internal rate tracking */
        uint32_t pop_offset = i * bridge->config.neurons_per_spatial;
        for (uint32_t j = 0; j < bridge->config.neurons_per_spatial; j++) {
            bridge->spatial_rates[pop_offset + j] =
                result->population_activity[i] * bridge->config.max_firing_rate;
        }
    }

    /* Find winner */
    result->winner_idx = preposition;
    result->winner_rate = result->population_activity[preposition];

    /* Compute precision based on activity profile sharpness */
    float sum = 0.0f;
    for (uint32_t i = 0; i < LING_SNN_NUM_SPATIAL_WORDS; i++) {
        sum += result->population_activity[i];
    }
    result->encoding_precision = (sum > 0.0f) ?
        result->winner_rate / (fabsf(sum) > 1e-7f ? sum : 1e-7f) : 0.0f;

    bridge->stats.total_encodings++;

    return LING_SNN_ERR_OK;
}

int ling_snn_decode_spatial_word(
    ling_snn_bridge_t* bridge,
    const float* population_activity,
    spatial_preposition_t* preposition,
    float* confidence
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        set_error("Invalid bridge");
        return LING_SNN_ERR_NULL;
    }
    if (!population_activity || !preposition) {
        set_error("NULL pointer");
        return LING_SNN_ERR_NULL;
    }

    /* Winner-take-all decoding */
    float max_activity = -1.0f;
    uint32_t winner_idx = 0;

    for (uint32_t i = 0; i < LING_SNN_NUM_SPATIAL_WORDS; i++) {
        if (population_activity[i] > max_activity) {
            max_activity = population_activity[i];
            winner_idx = i;
        }
    }

    *preposition = (spatial_preposition_t)winner_idx;

    if (confidence) {
        /* Confidence = winner / sum (softmax-like) */
        float sum = 0.0f;
        for (uint32_t i = 0; i < LING_SNN_NUM_SPATIAL_WORDS; i++) {
            sum += population_activity[i];
        }
        *confidence = (sum > 0.0f) ? max_activity / sum : 0.0f;
    }

    bridge->stats.total_decodings++;

    return LING_SNN_ERR_OK;
}

int ling_snn_get_spatial_population(
    const ling_snn_bridge_t* bridge,
    spatial_preposition_t preposition,
    float* activity,
    uint32_t size
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        return LING_SNN_ERR_NULL;
    }
    if (!activity) {
        return LING_SNN_ERR_NULL;
    }
    if (preposition < 0 || preposition >= SPATIAL_PREPOSITION_COUNT) {
        return LING_SNN_ERR_INVALID_WORD;
    }

    uint32_t pop_offset = preposition * bridge->config.neurons_per_spatial;
    uint32_t copy_count = (size < bridge->config.neurons_per_spatial) ?
        size : bridge->config.neurons_per_spatial;

    memcpy(activity, &bridge->spatial_rates[pop_offset],
           copy_count * sizeof(float));

    return LING_SNN_ERR_OK;
}

/* ============================================================================
 * NUMBER WORD ENCODING API
 * ============================================================================ */

int ling_snn_encode_number(
    ling_snn_bridge_t* bridge,
    float magnitude,
    number_word_type_t type,
    ling_number_encoding_t* result
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        set_error("Invalid bridge");
        return LING_SNN_ERR_NULL;
    }
    if (!result) {
        set_error("NULL result pointer");
        return LING_SNN_ERR_NULL;
    }

    memset(result, 0, sizeof(*result));
    result->type = type;
    result->magnitude = magnitude;

    /* Weber-Fechner rate coding */
    result->firing_rate = weber_fechner_rate(
        magnitude,
        bridge->config.max_firing_rate,
        bridge->config.min_firing_rate
    );

    /* Weber fraction uncertainty (typically 0.15 for numbers) */
    float weber_fraction = 0.15f;
    result->uncertainty = magnitude * weber_fraction;

    /* Approximate if magnitude > subitizing range */
    result->is_approximate = (magnitude > 4.0f);

    /* Update internal rate tracking */
    uint32_t type_idx = (type < LING_SNN_NUM_NUMBER_TYPES) ? type : 0;
    uint32_t pop_offset = type_idx * bridge->config.neurons_per_number;
    for (uint32_t i = 0; i < bridge->config.neurons_per_number; i++) {
        /* Add some variability across neurons */
        float jitter = 1.0f + (random_uniform(&bridge->rng_state) - 0.5f) * 0.2f;
        bridge->number_rates[pop_offset + i] = result->firing_rate * jitter;
    }

    bridge->stats.total_encodings++;

    return LING_SNN_ERR_OK;
}

int ling_snn_decode_number(
    ling_snn_bridge_t* bridge,
    float firing_rate,
    number_word_type_t type,
    float* magnitude,
    float* uncertainty
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        set_error("Invalid bridge");
        return LING_SNN_ERR_NULL;
    }
    if (!magnitude) {
        set_error("NULL magnitude pointer");
        return LING_SNN_ERR_NULL;
    }

    /* Inverse Weber-Fechner */
    *magnitude = inverse_weber_fechner(
        firing_rate,
        bridge->config.max_firing_rate,
        bridge->config.min_firing_rate
    );

    if (uncertainty) {
        float weber_fraction = 0.15f;
        *uncertainty = (*magnitude) * weber_fraction;
    }

    bridge->stats.total_decodings++;

    return LING_SNN_ERR_OK;
}

/* ============================================================================
 * POPULATION MANAGEMENT API
 * ============================================================================ */

int ling_snn_step_phoneme_population(
    ling_snn_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        return LING_SNN_ERR_NULL;
    }

    /* Simple leaky integration for rates */
    float decay = expf(-dt_ms / 20.0f);  /* 20ms time constant */
    uint32_t pop_size = LING_SNN_NUM_PHONEMES * bridge->config.neurons_per_phoneme;

    for (uint32_t i = 0; i < pop_size; i++) {
        bridge->phoneme_rates[i] *= decay;
    }

    return LING_SNN_ERR_OK;
}

int ling_snn_step_spatial_population(
    ling_snn_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        return LING_SNN_ERR_NULL;
    }

    float decay = expf(-dt_ms / 50.0f);  /* 50ms time constant */
    uint32_t pop_size = LING_SNN_NUM_SPATIAL_WORDS * bridge->config.neurons_per_spatial;

    for (uint32_t i = 0; i < pop_size; i++) {
        bridge->spatial_rates[i] *= decay;
    }

    return LING_SNN_ERR_OK;
}

int ling_snn_step_number_population(
    ling_snn_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        return LING_SNN_ERR_NULL;
    }

    float decay = expf(-dt_ms / 100.0f);  /* 100ms time constant */
    uint32_t pop_size = LING_SNN_NUM_NUMBER_TYPES * bridge->config.neurons_per_number;

    for (uint32_t i = 0; i < pop_size; i++) {
        bridge->number_rates[i] *= decay;
    }

    return LING_SNN_ERR_OK;
}

float ling_snn_get_population_rate(
    const ling_snn_bridge_t* bridge,
    uint32_t population_id
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        return 0.0f;
    }

    const float* rates = NULL;
    uint32_t size = 0;

    switch (population_id) {
        case 0:  /* Phoneme */
            rates = bridge->phoneme_rates;
            size = LING_SNN_NUM_PHONEMES * bridge->config.neurons_per_phoneme;
            break;
        case 1:  /* Spatial */
            rates = bridge->spatial_rates;
            size = LING_SNN_NUM_SPATIAL_WORDS * bridge->config.neurons_per_spatial;
            break;
        case 2:  /* Number */
            rates = bridge->number_rates;
            size = LING_SNN_NUM_NUMBER_TYPES * bridge->config.neurons_per_number;
            break;
        default:
            return 0.0f;
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        sum += rates[i];
    }

    return (size > 0) ? sum / size : 0.0f;
}

float ling_snn_get_population_synchrony(
    const ling_snn_bridge_t* bridge,
    uint32_t population_id
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        return 0.0f;
    }

    const float* rates = NULL;
    uint32_t size = 0;

    switch (population_id) {
        case 0:
            rates = bridge->phoneme_rates;
            size = LING_SNN_NUM_PHONEMES * bridge->config.neurons_per_phoneme;
            break;
        case 1:
            rates = bridge->spatial_rates;
            size = LING_SNN_NUM_SPATIAL_WORDS * bridge->config.neurons_per_spatial;
            break;
        case 2:
            rates = bridge->number_rates;
            size = LING_SNN_NUM_NUMBER_TYPES * bridge->config.neurons_per_number;
            break;
        default:
            return 0.0f;
    }

    /* Synchrony approximated by rate stability */
    return compute_rate_stability(rates, size);
}

/* ============================================================================
 * MESH HANDLER INTERFACE
 * ============================================================================ */

int ling_snn_mesh_process(
    void* ctx,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
) {
    ling_snn_bridge_t* bridge = (ling_snn_bridge_t*)ctx;
    if (!bridge || bridge->magic != LING_SNN_MAGIC || !request || !belief) {
        return LING_SNN_ERR_NULL;
    }

    memset(belief, 0, sizeof(*belief));

    /* Process based on request type */
    switch (request->type) {
        case LING_REQUEST_PARSE_SPATIAL: {
            /* Encode spatial word and return activity as belief */
            ling_spatial_encoding_t encoding;
            spatial_preposition_t prep = request->spatial.preposition;

            int ret = ling_snn_encode_spatial_word(bridge, prep, 1.0f, &encoding);
            if (ret != LING_SNN_ERR_OK) {
                return ret;
            }

            belief->certainty = encoding.encoding_precision;
            belief->precision = bridge->current_precision;
            /* Copy activity to belief vector (first few elements) */
            for (uint32_t i = 0; i < LING_SNN_NUM_SPATIAL_WORDS &&
                                 i < LINGUISTICS_BELIEF_VEC_SIZE; i++) {
                belief->belief_vector[i] = encoding.population_activity[i];
            }
            break;
        }

        case LING_REQUEST_PARSE_NUMBER: {
            /* Encode number and return rate as belief */
            ling_number_encoding_t encoding;
            float magnitude = request->number.value;
            number_word_type_t type = request->number.type;

            int ret = ling_snn_encode_number(bridge, magnitude, type, &encoding);
            if (ret != LING_SNN_ERR_OK) {
                return ret;
            }

            belief->certainty = 1.0f - (encoding.uncertainty / (fabsf(magnitude) > 1e-7f ? magnitude : 1e-7f));
            if (belief->certainty < 0.0f) belief->certainty = 0.0f;
            belief->precision = bridge->current_precision;
            belief->belief_vector[0] = encoding.firing_rate;
            break;
        }

        default:
            belief->certainty = 0.0f;
            belief->precision = LING_SNN_PRECISION_FLOOR;
            break;
    }

    bridge->stats.mesh_contributions++;
    bridge->current_belief = *belief;

    return LING_SNN_ERR_OK;
}

int ling_snn_mesh_update(
    void* ctx,
    const linguistics_belief_t* neighbors,
    uint32_t count,
    linguistics_belief_t* updated
) {
    ling_snn_bridge_t* bridge = (ling_snn_bridge_t*)ctx;
    if (!bridge || bridge->magic != LING_SNN_MAGIC || !updated) {
        return LING_SNN_ERR_NULL;
    }

    /* Start with current belief */
    *updated = bridge->current_belief;

    if (!neighbors || count == 0) {
        return LING_SNN_ERR_OK;
    }

    /* FEP update: μ' = μ - lr × Π × ε */
    float learning_rate = NIMCP_LEARNING_RATE_COARSE;

    for (uint32_t i = 0; i < count; i++) {
        float error = neighbors[i].certainty - updated->certainty;
        float weight = neighbors[i].precision;

        updated->certainty += learning_rate * weight * error;

        /* Update belief vector */
        for (uint32_t j = 0; j < LINGUISTICS_BELIEF_VEC_SIZE; j++) {
            float vec_error = neighbors[i].belief_vector[j] - updated->belief_vector[j];
            updated->belief_vector[j] += learning_rate * weight * vec_error;
        }
    }

    /* Clamp certainty */
    if (updated->certainty < 0.0f) updated->certainty = 0.0f;
    if (updated->certainty > 1.0f) updated->certainty = 1.0f;

    bridge->current_belief = *updated;

    return LING_SNN_ERR_OK;
}

float ling_snn_mesh_get_precision(void* ctx) {
    ling_snn_bridge_t* bridge = (ling_snn_bridge_t*)ctx;
    if (!bridge || bridge->magic != LING_SNN_MAGIC) {
        return LING_SNN_PRECISION_FLOOR;
    }

    /* Precision based on population rate stability */
    float phoneme_stability = compute_rate_stability(
        bridge->phoneme_rates,
        LING_SNN_NUM_PHONEMES * bridge->config.neurons_per_phoneme
    );
    float spatial_stability = compute_rate_stability(
        bridge->spatial_rates,
        LING_SNN_NUM_SPATIAL_WORDS * bridge->config.neurons_per_spatial
    );
    float number_stability = compute_rate_stability(
        bridge->number_rates,
        LING_SNN_NUM_NUMBER_TYPES * bridge->config.neurons_per_number
    );

    /* Weighted average */
    float avg_stability = (phoneme_stability + spatial_stability + number_stability) / 3.0f;

    /* Map stability to precision */
    float precision = bridge->config.base_precision * avg_stability +
                      (1.0f - bridge->config.rate_stability_weight) *
                      bridge->config.base_precision;

    /* Clamp */
    if (precision < LING_SNN_PRECISION_FLOOR) precision = LING_SNN_PRECISION_FLOOR;
    if (precision > LING_SNN_PRECISION_CEILING) precision = LING_SNN_PRECISION_CEILING;

    bridge->current_precision = precision;

    return precision;
}

int ling_snn_get_mesh_handler(
    ling_snn_bridge_t* bridge,
    linguistics_mesh_handler_t* handler
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC || !handler) {
        return LING_SNN_ERR_NULL;
    }

    handler->process = ling_snn_mesh_process;
    handler->update = ling_snn_mesh_update;
    handler->get_precision = ling_snn_mesh_get_precision;
    handler->ctx = bridge;

    return LING_SNN_ERR_OK;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int ling_snn_bridge_get_stats(
    const ling_snn_bridge_t* bridge,
    ling_snn_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC || !stats) {
        return LING_SNN_ERR_NULL;
    }

    *stats = bridge->stats;

    /* Compute live stats */
    stats->avg_phoneme_rate = ling_snn_get_population_rate(bridge, 0);
    stats->avg_spatial_rate = ling_snn_get_population_rate(bridge, 1);
    stats->avg_number_rate = ling_snn_get_population_rate(bridge, 2);
    stats->avg_precision = bridge->current_precision;
    stats->health = SNN_STATE_HEALTHY;  /* Simplified */

    return LING_SNN_ERR_OK;
}

void ling_snn_bridge_reset_stats(ling_snn_bridge_t* bridge) {
    if (!bridge || bridge->magic != LING_SNN_MAGIC) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* ling_snn_bridge_get_last_error(void) {
    return g_last_error;
}

/* ============================================================================
 * UTILITY API
 * ============================================================================ */

const char* ling_snn_phoneme_name(ling_phoneme_t phoneme) {
    if (phoneme < 0 || phoneme >= LING_PHONEME_COUNT) {
        return "INVALID";
    }
    return PHONEME_NAMES[phoneme];
}

int ling_snn_parse_phoneme(const char* str, ling_phoneme_t* phoneme) {
    if (!str || !phoneme) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ling_snn_parse_phoneme: required parameter is NULL (str, phoneme)");
        return -1;
    }

    for (int i = 0; i < LING_PHONEME_COUNT; i++) {
        if (strcmp(str, PHONEME_NAMES[i]) == 0) {
            *phoneme = (ling_phoneme_t)i;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ling_snn_parse_phoneme: validation failed");
    return -1;
}

const char* ling_snn_phoneme_class(ling_phoneme_t phoneme) {
    if (phoneme < 0 || phoneme >= LING_PHONEME_COUNT) return "INVALID";

    if (phoneme < 12) return "VOWEL";
    if (phoneme < 18) return "STOP";
    if (phoneme < 27) return "FRICATIVE";
    if (phoneme < 30) return "NASAL";
    if (phoneme < 34) return "APPROXIMANT";
    if (phoneme < 36) return "AFFRICATE";
    if (phoneme < 42) return "DIPHTHONG";
    return "SPECIAL";
}

bool ling_snn_phoneme_is_voiced(ling_phoneme_t phoneme) {
    if (phoneme < 0 || phoneme >= LING_PHONEME_COUNT) {
        return false;
    }
    return PHONEME_VOICED[phoneme];
}
