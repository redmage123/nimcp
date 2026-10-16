#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_snn_config.c - SNN Configuration Implementation
//=============================================================================
/**
 * @file nimcp_snn_config.c
 * @brief Implementation of SNN configuration management
 *
 * WHAT: Configuration creation, validation, and preset implementations
 * WHY:  Centralize SNN configuration with sensible defaults
 * HOW:  Factory functions fill configuration structures with valid values
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#include "snn/nimcp_snn_config.h"
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_config)

//=============================================================================
// Configuration Lifecycle
//=============================================================================

int snn_config_default(snn_config_t* config) {
    /* Guard clause: null check */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_default: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Zero initialize */
    memset(config, 0, sizeof(snn_config_t));

    /* Network dimensions - minimal defaults */
    config->n_inputs = 0;       /* Must be set by user */
    config->n_outputs = 0;      /* Must be set by user */
    config->n_populations = 0;

    /* Simulation parameters - biological defaults */
    config->dt = SNN_DT_DEFAULT;        /* 0.1 ms timestep */
    config->t_ref = SNN_REFRACTORY_DEFAULT;  /* 2 ms refractory */
    config->v_thresh = -50.0f;          /* Spike threshold (mV) */
    config->v_reset = NIMCP_RESET_POTENTIAL_MV;             /* Reset after spike: -65 mV */
    config->v_rest = NIMCP_RESTING_POTENTIAL_MV;            /* Resting potential: -70 mV */
    config->tau_mem = NIMCP_MEMBRANE_TAU_MS;            /* Membrane τ (ms) */
    config->tau_syn = 5.0f;             /* Synaptic τ (ms) */

    /* Input current scaling */
    config->input_current_scale = 70.0f;  /* 70 mV/unit: avg-pooled input 0.3 → 21mV (fires) */

    /* Encoder defaults - Poisson rate coding */
    config->encoder.method = SNN_ENCODE_POISSON;
    config->encoder.max_rate = 100.0f;  /* Max 100 Hz */
    config->encoder.min_rate = 0.0f;
    config->encoder.time_window = 100.0f;  /* 100 ms window */
    config->encoder.threshold = NIMCP_DEFAULT_SYNAPSE_STRENGTH;
    config->encoder.population_size = 1;
    config->encoder.sigma = 0.1f;

    /* Decoder defaults - rate decoding */
    config->decoder.method = SNN_DECODE_RATE;
    config->decoder.time_window = 100.0f;  /* 100 ms window */
    config->decoder.decay_tau = NIMCP_MEMBRANE_TAU_MS;
    config->decoder.use_softmax = false;

    /* Training defaults - STDP */
    config->train_mode = SNN_TRAIN_STDP;
    config->surrogate = SNN_SURROGATE_FAST_SIGMOID;
    config->surrogate_beta = 10.0f;
    config->learning_rate = NIMCP_DEFAULT_LEARNING_RATE;
    config->enable_stdp = true;
    config->enable_reward_modulation = false;

    /* Integration defaults - all disabled initially */
    config->enable_bio_async = false;
    config->enable_immune = false;
    config->use_axon_delays = false;
    config->use_dendritic_integration = false;

    /* Parallelization defaults */
    config->enable_simd = true;
    config->n_threads = 1;

    NIMCP_LOGGING_DEBUG("snn_config_default: initialized with biological defaults");
    return SNN_SUCCESS;
}

int snn_config_feedforward(snn_config_t* config,
                           uint32_t n_inputs,
                           uint32_t n_hidden,
                           uint32_t n_outputs) {
    /* Guard clauses */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_feedforward: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (n_inputs == 0 || n_outputs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_feedforward: zero inputs (%u) or outputs (%u)", n_inputs, n_outputs);
        return SNN_ERROR_INVALID_DIMENSION;
    }

    /* Initialize with defaults first */
    int result = snn_config_default(config);
    if (result != SNN_SUCCESS) {
        return result;
    }

    /* Set dimensions */
    config->n_inputs = n_inputs;
    config->n_outputs = n_outputs;
    config->n_hidden = n_hidden;
    config->n_populations = (n_hidden > 0) ? 3 : 2;  /* input, [hidden], output */

    NIMCP_LOGGING_INFO("snn_config_feedforward: %u -> %u -> %u",
                       n_inputs, n_hidden, n_outputs);
    return SNN_SUCCESS;
}

int snn_config_multilayer(snn_config_t* config,
                          const uint32_t* layer_sizes,
                          uint32_t n_layers) {
    /* Guard clauses */
    if (!config || !layer_sizes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_multilayer: NULL %s", !config ? "config" : "layer_sizes");
        return SNN_ERROR_NULL_POINTER;
    }
    if (n_layers < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_multilayer: need at least 2 layers (got %u)", n_layers);
        return SNN_ERROR_INVALID_DIMENSION;
    }

    /* Initialize with defaults */
    int result = snn_config_default(config);
    if (result != SNN_SUCCESS) {
        return result;
    }

    /* Set dimensions */
    config->n_inputs = layer_sizes[0];
    config->n_outputs = layer_sizes[n_layers - 1];
    config->n_populations = n_layers;

    NIMCP_LOGGING_INFO("snn_config_multilayer: %u layers, %u -> %u",
                       n_layers, config->n_inputs, config->n_outputs);
    return SNN_SUCCESS;
}

int snn_config_reservoir(snn_config_t* config,
                         uint32_t n_inputs,
                         uint32_t n_reservoir,
                         uint32_t n_outputs,
                         float connectivity) {
    /* Guard clauses */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_reservoir: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (n_inputs == 0 || n_reservoir == 0 || n_outputs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_reservoir: zero dimension (inputs=%u, reservoir=%u, outputs=%u)",
            n_inputs, n_reservoir, n_outputs);
        return SNN_ERROR_INVALID_DIMENSION;
    }
    if (connectivity < 0.0f || connectivity > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_reservoir: connectivity %.2f must be in [0, 1]", connectivity);
        return SNN_ERROR_INVALID_CONFIG;
    }

    /* Initialize with defaults */
    int result = snn_config_default(config);
    if (result != SNN_SUCCESS) {
        return result;
    }

    /* Set dimensions */
    config->n_inputs = n_inputs;
    config->n_outputs = n_outputs;
    config->n_populations = 3;  /* input, reservoir, output */

    /* Reservoir-specific: only train readout, use STDP for reservoir */
    config->enable_stdp = true;
    config->train_mode = SNN_TRAIN_STDP;

    NIMCP_LOGGING_INFO("snn_config_reservoir: %u -> [%u @ %.1f%%] -> %u",
                       n_inputs, n_reservoir, connectivity * 100.0f, n_outputs);
    return SNN_SUCCESS;
}

int snn_config_cortical_column(snn_config_t* config,
                               uint32_t n_minicolumns,
                               uint32_t neurons_per_minicolumn) {
    /* Guard clauses */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_cortical_column: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (n_minicolumns == 0 || neurons_per_minicolumn == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_cortical_column: zero minicolumns (%u) or neurons_per_minicolumn (%u)",
            n_minicolumns, neurons_per_minicolumn);
        return SNN_ERROR_INVALID_DIMENSION;
    }

    /* Initialize with defaults */
    int result = snn_config_default(config);
    if (result != SNN_SUCCESS) {
        return result;
    }

    /* Cortical column has 6 populations (layers 1-6) */
    config->n_populations = 6;

    /* Layer 4 is primary input */
    config->n_inputs = n_minicolumns * neurons_per_minicolumn;

    /* Layer 5 is primary output */
    config->n_outputs = n_minicolumns * neurons_per_minicolumn;

    /* Enable biological features */
    config->use_axon_delays = true;
    config->use_dendritic_integration = true;
    config->enable_stdp = true;

    NIMCP_LOGGING_INFO("snn_config_cortical_column: %u minicolumns × %u neurons/layer",
                       n_minicolumns, neurons_per_minicolumn);
    return SNN_SUCCESS;
}

int snn_config_validate(const snn_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_validate: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Validate dimensions */
    if (config->n_inputs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: n_inputs is 0");
        return SNN_ERROR_INVALID_DIMENSION;
    }
    if (config->n_outputs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: n_outputs is 0");
        return SNN_ERROR_INVALID_DIMENSION;
    }

    /* Validate timestep */
    if (config->dt < SNN_DT_MIN || config->dt > SNN_DT_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: dt %.4f outside [%.4f, %.4f]",
            config->dt, SNN_DT_MIN, SNN_DT_MAX);
        return SNN_ERROR_INVALID_CONFIG;
    }

    /* Validate time constants */
    if (config->tau_mem <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: tau_mem must be > 0");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (config->tau_syn <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: tau_syn must be > 0");
        return SNN_ERROR_INVALID_CONFIG;
    }

    /* Validate voltage thresholds */
    if (config->v_thresh <= config->v_reset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: v_thresh must be > v_reset");
        return SNN_ERROR_INVALID_CONFIG;
    }

    /* Validate refractory period */
    if (config->t_ref < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: t_ref must be >= 0");
        return SNN_ERROR_INVALID_CONFIG;
    }

    /* Validate learning rate */
    if (config->learning_rate < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: learning_rate must be >= 0");
        return SNN_ERROR_INVALID_CONFIG;
    }

    /* Validate surrogate beta */
    if (config->train_mode == SNN_TRAIN_SURROGATE && config->surrogate_beta <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: surrogate_beta must be > 0 for surrogate training");
        return SNN_ERROR_INVALID_CONFIG;
    }

    /* Validate encoder */
    if (config->encoder.max_rate < config->encoder.min_rate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: encoder max_rate < min_rate");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (config->encoder.time_window <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: encoder time_window must be > 0");
        return SNN_ERROR_INVALID_CONFIG;
    }

    /* Validate decoder */
    if (config->decoder.time_window <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_validate: decoder time_window must be > 0");
        return SNN_ERROR_INVALID_CONFIG;
    }

    NIMCP_LOGGING_DEBUG("snn_config_validate: configuration valid");
    return SNN_SUCCESS;
}

void snn_config_destroy(snn_config_t* config) {
    /* Guard clause */
    if (!config) {
        return;
    }

    /* Zero the structure */
    memset(config, 0, sizeof(snn_config_t));

    NIMCP_LOGGING_DEBUG("snn_config_destroy: configuration destroyed");
}

//=============================================================================
// Encoder Configuration
//=============================================================================

int snn_config_encoder_rate(snn_config_t* config,
                            float max_rate,
                            float time_window) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_encoder_rate: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (max_rate <= 0.0f || time_window <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_encoder_rate: invalid max_rate (%.2f) or time_window (%.2f)",
            max_rate, time_window);
        return SNN_ERROR_INVALID_CONFIG;
    }

    config->encoder.method = SNN_ENCODE_RATE;
    config->encoder.max_rate = max_rate;
    config->encoder.min_rate = 0.0f;
    config->encoder.time_window = time_window;

    return SNN_SUCCESS;
}

int snn_config_encoder_population(snn_config_t* config,
                                  uint32_t population_size,
                                  float sigma) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_encoder_population: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (population_size == 0 || sigma <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_encoder_population: invalid population_size (%u) or sigma (%.2f)",
            population_size, sigma);
        return SNN_ERROR_INVALID_CONFIG;
    }

    config->encoder.method = SNN_ENCODE_POPULATION;
    config->encoder.population_size = population_size;
    config->encoder.sigma = sigma;

    return SNN_SUCCESS;
}

int snn_config_encoder_latency(snn_config_t* config,
                               float max_latency) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_encoder_latency: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (max_latency <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_encoder_latency: invalid max_latency (%.2f)", max_latency);
        return SNN_ERROR_INVALID_CONFIG;
    }

    config->encoder.method = SNN_ENCODE_LATENCY;
    config->encoder.time_window = max_latency;

    return SNN_SUCCESS;
}

//=============================================================================
// Training Configuration
//=============================================================================

int snn_config_train_stdp(snn_config_t* config,
                          float learning_rate,
                          float time_window,
                          float a_plus,
                          float a_minus) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_train_stdp: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (learning_rate < 0.0f || time_window <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_LEARNING_FAILED,
            "snn_config_train_stdp: invalid learning_rate (%.4f) or time_window (%.2f)",
            learning_rate, time_window);
        return SNN_ERROR_INVALID_CONFIG;
    }

    config->train_mode = SNN_TRAIN_STDP;
    config->learning_rate = learning_rate;
    config->enable_stdp = true;

    NIMCP_LOGGING_DEBUG("snn_config_train_stdp: lr=%.4f, window=%.1fms, A+=%.4f, A-=%.4f",
                       learning_rate, time_window, a_plus, a_minus);
    return SNN_SUCCESS;
}

int snn_config_train_rstdp(snn_config_t* config,
                           float learning_rate,
                           float eligibility_decay) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_train_rstdp: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (learning_rate < 0.0f || eligibility_decay <= 0.0f || eligibility_decay > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_LEARNING_FAILED,
            "snn_config_train_rstdp: invalid learning_rate (%.4f) or eligibility_decay (%.4f)",
            learning_rate, eligibility_decay);
        return SNN_ERROR_INVALID_CONFIG;
    }

    config->train_mode = SNN_TRAIN_R_STDP;
    config->learning_rate = learning_rate;
    config->enable_stdp = true;
    config->enable_reward_modulation = true;

    NIMCP_LOGGING_DEBUG("snn_config_train_rstdp: lr=%.4f, decay=%.4f",
                       learning_rate, eligibility_decay);
    return SNN_SUCCESS;
}

int snn_config_train_surrogate(snn_config_t* config,
                               snn_surrogate_t surrogate,
                               float beta,
                               float learning_rate) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_train_surrogate: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (beta <= 0.0f || learning_rate < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_LEARNING_FAILED,
            "snn_config_train_surrogate: invalid beta (%.2f) or learning_rate (%.4f)",
            beta, learning_rate);
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (surrogate >= SNN_SURROGATE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_config_train_surrogate: invalid surrogate type %d", surrogate);
        return SNN_ERROR_INVALID_CONFIG;
    }

    config->train_mode = SNN_TRAIN_SURROGATE;
    config->surrogate = surrogate;
    config->surrogate_beta = beta;
    config->learning_rate = learning_rate;

    NIMCP_LOGGING_DEBUG("snn_config_train_surrogate: surrogate=%d, beta=%.2f, lr=%.4f",
                       surrogate, beta, learning_rate);
    return SNN_SUCCESS;
}

int snn_config_train_eprop(snn_config_t* config,
                           float learning_rate,
                           float eligibility_decay) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_train_eprop: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (learning_rate < 0.0f || eligibility_decay <= 0.0f || eligibility_decay > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_LEARNING_FAILED,
            "snn_config_train_eprop: invalid learning_rate (%.4f) or eligibility_decay (%.4f)",
            learning_rate, eligibility_decay);
        return SNN_ERROR_INVALID_CONFIG;
    }

    config->train_mode = SNN_TRAIN_EPROP;
    config->learning_rate = learning_rate;

    NIMCP_LOGGING_DEBUG("snn_config_train_eprop: lr=%.4f, decay=%.4f",
                       learning_rate, eligibility_decay);
    return SNN_SUCCESS;
}

//=============================================================================
// Integration Configuration
//=============================================================================

int snn_config_enable_bio_async(snn_config_t* config, bool enable) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_enable_bio_async: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    config->enable_bio_async = enable;
    return SNN_SUCCESS;
}

int snn_config_enable_immune(snn_config_t* config, bool enable) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_enable_immune: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    config->enable_immune = enable;
    return SNN_SUCCESS;
}

int snn_config_enable_axon_delays(snn_config_t* config, bool enable) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_enable_axon_delays: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    config->use_axon_delays = enable;
    return SNN_SUCCESS;
}

int snn_config_enable_dendrites(snn_config_t* config, bool enable) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_enable_dendrites: NULL config pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    config->use_dendritic_integration = enable;
    return SNN_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

int snn_config_print(const snn_config_t* config,
                     char* buffer,
                     size_t buffer_size) {
    if (!config || !buffer || buffer_size == 0) {
        return 0;
    }

    static const char* encode_names[] = {
        "RATE", "TEMPORAL", "POPULATION", "LATENCY", "BURST", "PHASE", "POISSON"
    };
    static const char* decode_names[] = {
        "RATE", "FIRST_SPIKE", "POPULATION", "MEMBRANE"
    };
    static const char* train_names[] = {
        "STDP", "R-STDP", "EPROP", "SURROGATE", "SLAYER", "DECOLLE"
    };

    int written = snprintf(buffer, buffer_size,
        "SNN Configuration:\n"
        "  Dimensions: %u inputs, %u outputs, %u populations\n"
        "  Timing: dt=%.3fms, tau_mem=%.1fms, tau_syn=%.1fms, t_ref=%.1fms\n"
        "  Voltages: thresh=%.1fmV, reset=%.1fmV, rest=%.1fmV\n"
        "  Encoding: %s, max_rate=%.1fHz, window=%.1fms\n"
        "  Decoding: %s, window=%.1fms\n"
        "  Training: %s, lr=%.4f, STDP=%s\n"
        "  Integration: bio_async=%s, immune=%s, axon_delays=%s, dendrites=%s\n"
        "  Parallel: SIMD=%s, threads=%u\n",
        config->n_inputs, config->n_outputs, config->n_populations,
        config->dt, config->tau_mem, config->tau_syn, config->t_ref,
        config->v_thresh, config->v_reset, config->v_rest,
        encode_names[config->encoder.method], config->encoder.max_rate, config->encoder.time_window,
        decode_names[config->decoder.method], config->decoder.time_window,
        train_names[config->train_mode], config->learning_rate,
        config->enable_stdp ? "ON" : "OFF",
        config->enable_bio_async ? "ON" : "OFF",
        config->enable_immune ? "ON" : "OFF",
        config->use_axon_delays ? "ON" : "OFF",
        config->use_dendritic_integration ? "ON" : "OFF",
        config->enable_simd ? "ON" : "OFF",
        config->n_threads);

    return written;
}

int snn_config_clone(const snn_config_t* src, snn_config_t* dst) {
    if (!src || !dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_config_clone: NULL %s", !src ? "source" : "destination");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Simple memcpy since snn_config_t has no dynamically allocated members */
    memcpy(dst, src, sizeof(snn_config_t));

    return SNN_SUCCESS;
}
