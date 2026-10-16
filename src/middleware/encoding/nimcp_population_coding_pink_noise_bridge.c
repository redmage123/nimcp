#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_population_coding_pink_noise_bridge.c - Population Coding Pink Noise Integration
//=============================================================================

#include "middleware/encoding/nimcp_population_coding_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_frequency_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(population_coding_pink_noise_bridge)

#define LOG_MODULE "POPULATION_CODING_PINK_NOISE_BRIDGE"


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Validate configuration parameters
 * WHY:  Prevent invalid bridge creation
 * HOW:  Range checks on all parameters
 */
static bool validate_config(const population_pink_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_config: config is NULL");
        return false;
    }

    // Noise parameters
    if (config->alpha < 0.0f || config->alpha > 3.0f) {
        return false;
    }
    if (config->amplitude <= 0.0f) {
        return false;
    }
    if (config->min_frequency <= 0.0f) {
        return false;
    }
    if (config->max_frequency <= config->min_frequency) {
        return false;
    }
    if (config->sample_rate < 2.0f * config->max_frequency) {
        return false;
    }

    // Strength parameters
    if (config->rate_modulation_strength < 0.0f || config->rate_modulation_strength > 1.0f) {
        return false;
    }
    if (config->tuning_modulation_strength < 0.0f || config->tuning_modulation_strength > 1.0f) {
        return false;
    }
    if (config->position_modulation_strength < 0.0f || config->position_modulation_strength > 1.0f) {
        return false;
    }
    if (config->correlation_factor < 0.0f || config->correlation_factor > 1.0f) {
        return false;
    }

    // Tuning limits
    if (config->min_tuning_width <= 0.0f) {
        return false;
    }
    if (config->max_tuning_width <= config->min_tuning_width) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

population_pink_config_t population_pink_bridge_default_config(void) {
    population_pink_config_t config;
    memset(&config, 0, sizeof(config));

    // Noise characteristics
    config.alpha = 1.0f;
    config.amplitude = 0.05f;
    config.min_frequency = 0.1f;
    config.max_frequency = 100.0f;
    config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ;

    // Application mode
    config.mode = POPULATION_PINK_MODE_HYBRID;
    config.target = POPULATION_PINK_TARGET_RATES;
    config.correlation_factor = 0.3f;

    // Rate modulation
    config.rate_modulation_strength = 0.1f;
    config.multiplicative_rate_noise = true;
    config.max_rate_change = 0.5f;

    // Tuning modulation
    config.tuning_modulation_strength = 0.05f;
    config.min_tuning_width = 0.1f;
    config.max_tuning_width = NIMCP_PI_F;

    // Position modulation
    config.position_modulation_strength = 0.01f;

    // Generator config
    config.method = PINK_NOISE_VOSS;
    config.seed = 0;

    // Control
    config.enable_noise = true;
    config.clamp_rates = true;

    return config;
}

population_pink_bridge_t* population_pink_bridge_create(
    const population_pink_config_t* config,
    uint32_t num_neurons
) {
    // Guard: validate inputs
    if (num_neurons == 0 || num_neurons > POPULATION_PINK_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_create: num_neurons is zero");
        return NULL;
    }

    population_pink_config_t cfg = config ? *config : population_pink_bridge_default_config();
    if (!validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_create: validate_config is NULL");
        return NULL;
    }

    // Allocate bridge
    population_pink_bridge_t* bridge = (population_pink_bridge_t*)nimcp_malloc(sizeof(population_pink_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(population_pink_bridge_t));
    bridge->config = cfg;
    bridge->num_neurons = num_neurons;

    // Initialize bridge base (allocates mutex, sets module name)
    if (bridge_base_init(&bridge->base, 0, "population_coding_pink_noise") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    // Create global noise generator
    pink_noise_config_t pink_cfg;
    pink_cfg.alpha = cfg.alpha;
    pink_cfg.amplitude = cfg.amplitude;
    pink_cfg.min_frequency = cfg.min_frequency;
    pink_cfg.max_frequency = cfg.max_frequency;
    pink_cfg.sample_rate = cfg.sample_rate;
    pink_cfg.method = cfg.method;
    pink_cfg.seed = cfg.seed;

    bridge->global_generator = pink_noise_create(&pink_cfg);
    if (!bridge->global_generator) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "population_pink_bridge_create: bridge->global_generator is NULL");
        return NULL;
    }

    // Create per-neuron generators if needed
    if (cfg.mode == POPULATION_PINK_MODE_PER_NEURON || cfg.mode == POPULATION_PINK_MODE_HYBRID) {
        bridge->per_neuron_generators = (pink_noise_generator_t*)nimcp_malloc(
            num_neurons * sizeof(pink_noise_generator_t)
        );
        if (!bridge->per_neuron_generators) {
            pink_noise_destroy(bridge->global_generator);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_create: bridge->per_neuron_generators is NULL");
            return NULL;
        }

        // Create generator for each neuron
        for (uint32_t i = 0; i < num_neurons; i++) {
            pink_cfg.seed = (cfg.seed == 0) ? 0 : cfg.seed + i + 1;
            bridge->per_neuron_generators[i] = pink_noise_create(&pink_cfg);
            if (!bridge->per_neuron_generators[i]) {
                // Cleanup already created generators
                for (uint32_t j = 0; j < i; j++) {
                    pink_noise_destroy(bridge->per_neuron_generators[j]);
                }
                nimcp_free(bridge->per_neuron_generators);
                pink_noise_destroy(bridge->global_generator);
                bridge_base_cleanup(&bridge->base);
                nimcp_free(bridge);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_create: bridge->per_neuron_generators is NULL");
                return NULL;
            }
        }
    }

    // Allocate noise value storage
    bridge->current_noise_values = (float*)nimcp_malloc(num_neurons * sizeof(float));
    if (!bridge->current_noise_values) {
        if (bridge->per_neuron_generators) {
            for (uint32_t i = 0; i < num_neurons; i++) {
                pink_noise_destroy(bridge->per_neuron_generators[i]);
            }
            nimcp_free(bridge->per_neuron_generators);
        }
        pink_noise_destroy(bridge->global_generator);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_create: validation failed");
        return NULL;
    }
    memset(bridge->current_noise_values, 0, num_neurons * sizeof(float));

    return bridge;
}

void population_pink_bridge_destroy(population_pink_bridge_t* bridge) {
    // Guard: NULL-safe
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "population_coding_pink_noise");

    // Destroy per-neuron generators
    if (bridge->per_neuron_generators) {
        for (uint32_t i = 0; i < bridge->num_neurons; i++) {
            pink_noise_destroy(bridge->per_neuron_generators[i]);
        }
        nimcp_free(bridge->per_neuron_generators);
    }

    // Destroy global generator
    pink_noise_destroy(bridge->global_generator);

    // Free arrays
    if (bridge->current_noise_values) {
        nimcp_free(bridge->current_noise_values);
    }

    // Cleanup bridge base (disconnects bio-async, destroys+frees mutex)
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
}

//=============================================================================
// Connection Functions
//=============================================================================

int population_pink_bridge_connect_encoder(
    population_pink_bridge_t* bridge,
    population_coding_encoder_t encoder
) {
    // Guard: validate inputs
    if (!bridge || !encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_connect_encoder: required parameter is NULL (bridge, encoder)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->encoder = encoder;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int population_pink_bridge_disconnect(population_pink_bridge_t* bridge) {
    // Guard: validate input
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->encoder = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Noise Generation Functions
//=============================================================================

int population_pink_bridge_update_noise(population_pink_bridge_t* bridge) {
    // Guard: validate input
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_update_noise: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_noise) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    // Generate global noise sample
    if (!pink_noise_generate_sample(bridge->global_generator, &bridge->global_noise_value)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_update_noise: pink_noise_generate_sample is NULL");
        return -1;
    }

    // Generate per-neuron noise based on mode
    switch (bridge->config.mode) {
        case POPULATION_PINK_MODE_GLOBAL:
            // All neurons get same global noise
            for (uint32_t i = 0; i < bridge->num_neurons; i++) {
                bridge->current_noise_values[i] = bridge->global_noise_value;
            }
            break;

        case POPULATION_PINK_MODE_PER_NEURON:
            // Each neuron gets independent noise
            for (uint32_t i = 0; i < bridge->num_neurons; i++) {
                float local_noise;
                if (!pink_noise_generate_sample(bridge->per_neuron_generators[i], &local_noise)) {
                    nimcp_mutex_unlock(bridge->base.mutex);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_update_noise: pink_noise_generate_sample is NULL");
                    return -1;
                }
                bridge->current_noise_values[i] = local_noise;
            }
            break;

        case POPULATION_PINK_MODE_HYBRID:
            // Mix global and local noise
            {
                float corr = bridge->config.correlation_factor;
                float inv_corr = 1.0f - corr;
                for (uint32_t i = 0; i < bridge->num_neurons; i++) {
                    float local_noise;
                    if (!pink_noise_generate_sample(bridge->per_neuron_generators[i], &local_noise)) {
                        nimcp_mutex_unlock(bridge->base.mutex);
                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_update_noise: pink_noise_generate_sample is NULL");
                        return -1;
                    }
                    bridge->current_noise_values[i] = corr * bridge->global_noise_value + inv_corr * local_noise;
                }
            }
            break;
    }

    // Update statistics
    float sum = 0.0f;
    float max_abs = 0.0f;
    for (uint32_t i = 0; i < bridge->num_neurons; i++) {
        float abs_val = fabsf(bridge->current_noise_values[i]);
        sum += abs_val;
        if (abs_val > max_abs) max_abs = abs_val;
    }
    bridge->stats.avg_noise_level = sum / bridge->num_neurons;
    if (max_abs > bridge->stats.max_noise_level) {
        bridge->stats.max_noise_level = max_abs;
    }
    bridge->stats.samples_generated += bridge->num_neurons;

    bridge->update_count++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float population_pink_bridge_get_noise(
    const population_pink_bridge_t* bridge,
    uint32_t neuron_idx
) {
    // Guard: validate inputs
    if (!bridge || neuron_idx >= bridge->num_neurons) return 0.0f;

    return bridge->current_noise_values[neuron_idx];
}

float population_pink_bridge_get_global_noise(const population_pink_bridge_t* bridge) {
    // Guard: validate input
    if (!bridge) return 0.0f;

    return bridge->global_noise_value;
}

//=============================================================================
// Modulation Functions
//=============================================================================

int population_pink_bridge_modulate_rates(
    population_pink_bridge_t* bridge,
    const float* rates_in,
    uint32_t num_neurons,
    float* rates_out
) {
    // Guard: validate inputs
    if (!bridge || !rates_in || !rates_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_modulate_rates: required parameter is NULL (bridge, rates_in, rates_out)");
        return -1;
    }
    if (num_neurons != bridge->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_modulate_rates: validation failed");
        return -1;
    }
    if (!bridge->config.enable_noise) {
        // If noise disabled, just copy input to output
        if (rates_in != rates_out) {
            memcpy(rates_out, rates_in, num_neurons * sizeof(float));
        }
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float strength = bridge->config.rate_modulation_strength;
    float amplitude = bridge->config.amplitude;

    for (uint32_t i = 0; i < num_neurons; i++) {
        float rate = rates_in[i];
        float noise = bridge->current_noise_values[i];
        float modulated;

        if (bridge->config.multiplicative_rate_noise) {
            // Multiplicative: rate *= (1 + strength * noise)
            modulated = rate * (1.0f + strength * noise);
        } else {
            // Additive: rate += strength * amplitude * noise
            modulated = rate + strength * amplitude * noise;
        }

        // Clamp if enabled
        if (bridge->config.clamp_rates) {
            modulated = nimcp_clampf(modulated, 0.0f, 1000.0f);  // Assume max rate of 1000 Hz
        }

        rates_out[i] = modulated;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int population_pink_bridge_modulate_tuning(
    population_pink_bridge_t* bridge,
    const tuning_curve_t* tuning_in,
    uint32_t num_neurons,
    tuning_curve_t* tuning_out
) {
    // Guard: validate inputs
    if (!bridge || !tuning_in || !tuning_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_modulate_tuning: required parameter is NULL (bridge, tuning_in, tuning_out)");
        return -1;
    }
    if (num_neurons != bridge->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_modulate_tuning: validation failed");
        return -1;
    }
    if (!bridge->config.enable_noise) {
        // If noise disabled, just copy
        if (tuning_in != tuning_out) {
            memcpy(tuning_out, tuning_in, num_neurons * sizeof(tuning_curve_t));
        }
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float strength = bridge->config.tuning_modulation_strength;

    for (uint32_t i = 0; i < num_neurons; i++) {
        tuning_out[i] = tuning_in[i];  // Copy base values

        float noise = bridge->current_noise_values[i];
        float tuning_width = tuning_in[i].tuning_width;

        // Modulate tuning width multiplicatively
        float modulated_width = tuning_width * (1.0f + strength * noise);

        // Clamp to valid range
        modulated_width = nimcp_clampf(modulated_width,
                                   bridge->config.min_tuning_width,
                                   bridge->config.max_tuning_width);

        tuning_out[i].tuning_width = modulated_width;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int population_pink_bridge_modulate_positions(
    population_pink_bridge_t* bridge,
    const vector3d_t* positions_in,
    uint32_t num_neurons,
    vector3d_t* positions_out
) {
    // Guard: validate inputs
    if (!bridge || !positions_in || !positions_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_modulate_positions: required parameter is NULL (bridge, positions_in, positions_out)");
        return -1;
    }
    if (num_neurons != bridge->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_modulate_positions: validation failed");
        return -1;
    }
    if (!bridge->config.enable_noise) {
        // If noise disabled, just copy
        if (positions_in != positions_out) {
            memcpy(positions_out, positions_in, num_neurons * sizeof(vector3d_t));
        }
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float strength = bridge->config.position_modulation_strength;

    for (uint32_t i = 0; i < num_neurons; i++) {
        float noise = bridge->current_noise_values[i];

        // Add small jitter to position
        positions_out[i].x = positions_in[i].x + strength * noise;
        positions_out[i].y = positions_in[i].y + strength * noise * 0.7f;  // Different scale for y
        positions_out[i].z = positions_in[i].z + strength * noise * 0.5f;  // Different scale for z

        // Recompute magnitude
        float mag = sqrtf(positions_out[i].x * positions_out[i].x +
                         positions_out[i].y * positions_out[i].y +
                         positions_out[i].z * positions_out[i].z);
        positions_out[i].magnitude = mag;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Control Functions
//=============================================================================

int population_pink_bridge_enable(population_pink_bridge_t* bridge) {
    // Guard: validate input
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_enable: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.enable_noise = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int population_pink_bridge_disable(population_pink_bridge_t* bridge) {
    // Guard: validate input
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_disable: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.enable_noise = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool population_pink_bridge_is_enabled(const population_pink_bridge_t* bridge) {
    // Guard: validate input
    if (!bridge) {
        return false;
    }

    return bridge->config.enable_noise;
}

int population_pink_bridge_reset(
    population_pink_bridge_t* bridge,
    uint32_t new_seed
) {
    // Guard: validate input
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t seed = (new_seed == 0) ? bridge->config.seed : new_seed;

    // Reset global generator
    if (!pink_noise_reset(bridge->global_generator, seed)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_reset: pink_noise_reset is NULL");
        return -1;
    }

    // Reset per-neuron generators
    if (bridge->per_neuron_generators) {
        for (uint32_t i = 0; i < bridge->num_neurons; i++) {
            uint32_t neuron_seed = (seed == 0) ? 0 : seed + i + 1;
            if (!pink_noise_reset(bridge->per_neuron_generators[i], neuron_seed)) {
                nimcp_mutex_unlock(bridge->base.mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_reset: pink_noise_reset is NULL");
                return -1;
            }
        }
    }

    // Clear noise values
    memset(bridge->current_noise_values, 0, bridge->num_neurons * sizeof(float));
    bridge->global_noise_value = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Configuration Update Functions
//=============================================================================

int population_pink_bridge_set_amplitude(
    population_pink_bridge_t* bridge,
    float amplitude
) {
    // Guard: validate inputs
    if (!bridge || amplitude <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_set_amplitude: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.amplitude = amplitude;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int population_pink_bridge_set_alpha(
    population_pink_bridge_t* bridge,
    float alpha
) {
    // Guard: validate inputs
    if (!bridge || alpha < 0.0f || alpha > 3.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_set_alpha: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.alpha = alpha;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int population_pink_bridge_set_correlation(
    population_pink_bridge_t* bridge,
    float correlation
) {
    // Guard: validate inputs
    if (!bridge || correlation < 0.0f || correlation > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_set_correlation: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.correlation_factor = correlation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int population_pink_bridge_set_rate_modulation(
    population_pink_bridge_t* bridge,
    float strength
) {
    // Guard: validate inputs
    if (!bridge || strength < 0.0f || strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "population_pink_bridge_set_rate_modulation: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.rate_modulation_strength = strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int population_pink_bridge_get_stats(
    const population_pink_bridge_t* bridge,
    population_pink_stats_t* stats
) {
    // Guard: validate inputs
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int population_pink_bridge_reset_stats(population_pink_bridge_t* bridge) {
    // Guard: validate input
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_pink_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(population_pink_stats_t));
    bridge->update_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}
