/**
 * @file nimcp_cochlea_extended.c
 * @brief Dog and Bat auditory extensions implementation
 *
 * WHAT: Non-human auditory capabilities (ultrasonic, echolocation)
 * WHY:  Enable extended hearing modes beyond human range
 * HOW:  Species-specific filterbanks, pinnae models, echolocation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "perception/nimcp_cochlea_extended.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_extended)

//=============================================================================
// Internal Constants
//=============================================================================


#define EXT_LOG_MODULE "COCHLEA_EXTENDED"

//=============================================================================
// Opaque Structure Definitions
//=============================================================================

/**
 * @brief Dog auditory processor internal structure
 */
struct dog_auditory {
    dog_auditory_config_t config;
    uint32_t sample_rate;

    /* Pinnae model */
    dog_pinnae_t pinnae;

    /* Localization state */
    dog_localization_t last_localization;

    /* Ultrasonic detection state */
    bool ultrasonic_detected;
    float ultrasonic_freq;
    float ultrasonic_level;

    /* Processing buffers */
    float* ultrasonic_buffer;   /**< Buffer for ultrasonic channels */
    uint32_t num_channels;      /**< Total channels including ultrasonic */
};

/**
 * @brief Bat auditory processor internal structure
 */
struct bat_auditory {
    bat_auditory_config_t config;
    uint32_t sample_rate;

    /* Echolocation state */
    echolocation_mode_t current_mode;
    echolocation_call_t last_call;

    /* Doppler processing */
    doppler_state_t doppler;

    /* Processing buffers */
    float* call_buffer;         /**< Buffer for generating calls */
    uint32_t call_buffer_size;
    float* echo_correlator;     /**< Cross-correlation buffer */
    uint32_t correlator_size;

    /* Echo tracking */
    float last_call_time_ms;    /**< Time of last emitted call */
    uint32_t num_channels;      /**< Total channels including ultrasonic */
};

/**
 * @brief Extended hearing processor internal structure
 */
struct ext_hearing {
    ext_hearing_config_t config;
    uint32_t sample_rate;

    /* Sub-processors */
    dog_auditory_t* dog;
    bat_auditory_t* bat;

    /* Mode state */
    ext_hearing_mode_t active_mode;
    float mode_transition_progress; /**< 0-1 during mode switch */
    bool in_transition;
};

//=============================================================================
// Dog Auditory - Configuration
//=============================================================================

dog_auditory_config_t dog_auditory_config_default(dog_breed_t breed)
{
    dog_auditory_config_t config;
    memset(&config, 0, sizeof(config));

    config.breed = breed;
    config.enable_pinnae       = true;
    config.enable_localization = true;
    config.itd_resolution_us   = DOG_ITD_RESOLUTION_US;
    config.ild_sensitivity_db  = DOG_ILD_SENSITIVITY_DB;
    config.ultrasonic_channels = 16;

    /* Breed-specific max frequency */
    switch (breed) {
        case DOG_BREED_GERMAN_SHEPHERD:
            config.max_freq_hz            = 60000.0f;
            config.ultrasonic_sensitivity = 0.9f;
            break;
        case DOG_BREED_BORDER_COLLIE:
            config.max_freq_hz            = 65000.0f;
            config.ultrasonic_sensitivity = 0.95f;
            break;
        case DOG_BREED_DALMATIAN:
            config.max_freq_hz            = 50000.0f;
            config.ultrasonic_sensitivity = 0.6f; /* Prone to hearing issues */
            break;
        case DOG_BREED_LABRADOR:
            config.max_freq_hz            = 55000.0f;
            config.ultrasonic_sensitivity = 0.8f;
            break;
        default:
            config.max_freq_hz            = 55000.0f;
            config.ultrasonic_sensitivity = 0.8f;
            break;
    }

    return config;
}

//=============================================================================
// Dog Auditory - Core API
//=============================================================================

dog_auditory_t* dog_auditory_create(
    const dog_auditory_config_t* config,
    uint32_t sample_rate)
{
    cochlea_extended_heartbeat("dog_create", 0.0f);

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dog_auditory_create: config is NULL");
        return NULL;
    }

    dog_auditory_t* dog = (dog_auditory_t*)nimcp_calloc(1, sizeof(dog_auditory_t));
    if (!dog) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dog_auditory_create: failed to allocate dog_auditory");
        return NULL;
    }

    dog->config      = *config;
    dog->sample_rate = sample_rate;
    dog->num_channels = config->ultrasonic_channels;

    /* Initialize pinnae */
    memset(&dog->pinnae, 0, sizeof(dog_pinnae_t));
    dog->pinnae.left_state       = DOG_PINNAE_NEUTRAL;
    dog->pinnae.right_state      = DOG_PINNAE_NEUTRAL;
    dog->pinnae.left_gain_factor  = 1.0f;
    dog->pinnae.right_gain_factor = 1.0f;

    /* Allocate HRTF arrays */
    if (config->enable_pinnae && config->ultrasonic_channels > 0) {
        dog->pinnae.num_hrtf_freqs = config->ultrasonic_channels;
        dog->pinnae.left_hrtf  = (float*)nimcp_calloc(config->ultrasonic_channels, sizeof(float));
        dog->pinnae.right_hrtf = (float*)nimcp_calloc(config->ultrasonic_channels, sizeof(float));
        if (dog->pinnae.left_hrtf) {
            for (uint32_t i = 0; i < config->ultrasonic_channels; i++) {
                dog->pinnae.left_hrtf[i]  = 1.0f;
                dog->pinnae.right_hrtf[i] = 1.0f;
            }
        }
    }

    /* Initialize localization */
    memset(&dog->last_localization, 0, sizeof(dog_localization_t));

    /* Ultrasonic detection */
    dog->ultrasonic_detected = false;
    dog->ultrasonic_freq     = 0.0f;
    dog->ultrasonic_level    = 0.0f;

    /* Allocate ultrasonic buffer */
    if (config->ultrasonic_channels > 0) {
        dog->ultrasonic_buffer = (float*)nimcp_calloc(
            config->ultrasonic_channels, sizeof(float));
    }

    cochlea_extended_heartbeat("dog_create", 1.0f);
    return dog;
}

void dog_auditory_destroy(dog_auditory_t* dog)
{
    if (!dog) return;

    cochlea_extended_heartbeat("dog_destroy", 0.0f);

    nimcp_free(dog->pinnae.left_hrtf);
    nimcp_free(dog->pinnae.right_hrtf);
    nimcp_free(dog->ultrasonic_buffer);

    cochlea_extended_heartbeat("dog_destroy", 1.0f);
    nimcp_free(dog);
}

nimcp_error_t dog_auditory_process(
    dog_auditory_t* dog,
    const float* audio_left,
    const float* audio_right,
    uint32_t num_samples,
    float* output)
{
    if (!dog) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dog_auditory_process: dog is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!audio_left || !audio_right) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dog_auditory_process: audio input is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dog_auditory_process: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_extended_heartbeat("dog_process", 0.0f);

    /*
     * Stub: Zero outputs for ultrasonic channels.
     * A full implementation would apply extended filterbank for 20-65 kHz,
     * pinnae directional gain, and ITD/ILD computation.
     */
    for (uint32_t ch = 0; ch < dog->num_channels; ch++) {
        output[ch] = 0.0f;
    }

    /* Rudimentary ultrasonic detection: check for energy above 20 kHz */
    dog->ultrasonic_detected = false;
    dog->ultrasonic_freq     = 0.0f;
    dog->ultrasonic_level    = 0.0f;

    /* Compute simple ITD/ILD for localization update */
    if (dog->config.enable_localization && num_samples > 0) {
        float left_energy  = 0.0f;
        float right_energy = 0.0f;
        for (uint32_t s = 0; s < num_samples; s++) {
            left_energy  += audio_left[s] * audio_left[s];
            right_energy += audio_right[s] * audio_right[s];
        }
        left_energy  /= (float)num_samples;
        right_energy /= (float)num_samples;

        /* ILD: level difference between ears */
        float ild_db = 0.0f;
        if (left_energy > 1e-10f && right_energy > 1e-10f) {
            ild_db = 10.0f * log10f(left_energy / right_energy);
        }

        dog->last_localization.ild_db = ild_db;
        dog->last_localization.azimuth_deg = ild_db * 10.0f; /* Simple mapping */
        if (dog->last_localization.azimuth_deg > 90.0f)
            dog->last_localization.azimuth_deg = 90.0f;
        if (dog->last_localization.azimuth_deg < -90.0f)
            dog->last_localization.azimuth_deg = -90.0f;
        dog->last_localization.confidence = 0.5f; /* Stub confidence */
    }

    cochlea_extended_heartbeat("dog_process", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t dog_auditory_set_pinnae(
    dog_auditory_t* dog,
    float left_azimuth,
    float left_elevation,
    float right_azimuth,
    float right_elevation)
{
    if (!dog) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dog_auditory_set_pinnae: dog is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    dog->pinnae.left_azimuth_deg    = left_azimuth;
    dog->pinnae.left_elevation_deg  = left_elevation;
    dog->pinnae.right_azimuth_deg   = right_azimuth;
    dog->pinnae.right_elevation_deg = right_elevation;

    /* Update gain factors based on orientation */
    /* BIOLOGICAL: Ears oriented toward source get gain boost */
    float left_align  = cosf(left_azimuth  * (float)M_PI / 180.0f);
    float right_align = cosf(right_azimuth * (float)M_PI / 180.0f);
    dog->pinnae.left_gain_factor  = 0.5f + 0.5f * (left_align > 0.0f ? left_align : 0.0f);
    dog->pinnae.right_gain_factor = 0.5f + 0.5f * (right_align > 0.0f ? right_align : 0.0f);

    return NIMCP_SUCCESS;
}

nimcp_error_t dog_auditory_localize(
    dog_auditory_t* dog,
    dog_localization_t* result)
{
    if (!dog) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dog_auditory_localize: dog is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dog_auditory_localize: result is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *result = dog->last_localization;
    return NIMCP_SUCCESS;
}

bool dog_auditory_detect_ultrasonic(
    dog_auditory_t* dog,
    float* freq_hz,
    float* level)
{
    if (!dog) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dog_auditory_detect_ultrasonic: dog is NULL");
        return false;
    }

    if (freq_hz) *freq_hz = dog->ultrasonic_freq;
    if (level)   *level   = dog->ultrasonic_level;
    return dog->ultrasonic_detected;
}

//=============================================================================
// Bat Auditory - Configuration
//=============================================================================

bat_auditory_config_t bat_auditory_config_default(bat_species_t species)
{
    bat_auditory_config_t config;
    memset(&config, 0, sizeof(config));

    config.species             = species;
    config.enable_echolocation = true;
    config.enable_doppler      = true;
    config.range_resolution_cm = BAT_RANGE_RES_CM;
    config.ultrasonic_channels = 64;

    switch (species) {
        case BAT_SPECIES_HORSESHOE:
            config.max_freq_hz           = 120000.0f;
            config.echolocation_band_min = 60000.0f;
            config.echolocation_band_max = 90000.0f;
            config.call_type             = ECHO_CALL_CF_FM;
            config.temporal_resolution_us = 20.0f;
            config.doppler_sensitivity   = 30.0f;
            break;
        case BAT_SPECIES_VESPERTILIONID:
            config.max_freq_hz           = 150000.0f;
            config.echolocation_band_min = 20000.0f;
            config.echolocation_band_max = 100000.0f;
            config.call_type             = ECHO_CALL_FM;
            config.temporal_resolution_us = 15.0f;
            config.doppler_sensitivity   = 20.0f;
            break;
        case BAT_SPECIES_MOLOSSID:
            config.max_freq_hz           = 60000.0f;
            config.echolocation_band_min = 10000.0f;
            config.echolocation_band_max = 40000.0f;
            config.call_type             = ECHO_CALL_FM;
            config.temporal_resolution_us = 30.0f;
            config.doppler_sensitivity   = 15.0f;
            break;
        case BAT_SPECIES_PTEROPODID:
            config.max_freq_hz           = 50000.0f;
            config.echolocation_band_min = 20000.0f;
            config.echolocation_band_max = 40000.0f;
            config.call_type             = ECHO_CALL_CLICK;
            config.temporal_resolution_us = 50.0f;
            config.doppler_sensitivity   = 10.0f;
            config.enable_echolocation   = false; /* Limited */
            break;
        default: /* GENERIC or PHYLLOSTOMID */
            config.max_freq_hz           = 200000.0f;
            config.echolocation_band_min = BAT_ECHOLOCATION_MIN_HZ;
            config.echolocation_band_max = 100000.0f;
            config.call_type             = ECHO_CALL_FM;
            config.temporal_resolution_us = BAT_TEMPORAL_RES_US;
            config.doppler_sensitivity   = 25.0f;
            break;
    }

    return config;
}

//=============================================================================
// Bat Auditory - Core API
//=============================================================================

bat_auditory_t* bat_auditory_create(
    const bat_auditory_config_t* config,
    uint32_t sample_rate)
{
    cochlea_extended_heartbeat("bat_create", 0.0f);

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_create: config is NULL");
        return NULL;
    }

    bat_auditory_t* bat = (bat_auditory_t*)nimcp_calloc(1, sizeof(bat_auditory_t));
    if (!bat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "bat_auditory_create: failed to allocate bat_auditory");
        return NULL;
    }

    bat->config      = *config;
    bat->sample_rate = sample_rate;
    bat->num_channels = config->ultrasonic_channels;

    /* Initialize echolocation */
    bat->current_mode = ECHO_MODE_SEARCHING;
    memset(&bat->last_call, 0, sizeof(echolocation_call_t));

    /* Initialize Doppler */
    memset(&bat->doppler, 0, sizeof(doppler_state_t));
    bat->doppler.reference_freq_hz = config->echolocation_band_min;
    bat->doppler.enable_dsc = (config->species == BAT_SPECIES_HORSESHOE);

    /* Allocate call buffer (1ms at sample rate) */
    bat->call_buffer_size = sample_rate / 1000;
    if (bat->call_buffer_size < 256) bat->call_buffer_size = 256;
    bat->call_buffer = (float*)nimcp_calloc(bat->call_buffer_size, sizeof(float));

    /* Allocate echo correlator buffer */
    bat->correlator_size = bat->call_buffer_size * 2;
    bat->echo_correlator = (float*)nimcp_calloc(bat->correlator_size, sizeof(float));

    bat->last_call_time_ms = 0.0f;

    cochlea_extended_heartbeat("bat_create", 1.0f);
    return bat;
}

void bat_auditory_destroy(bat_auditory_t* bat)
{
    if (!bat) return;

    cochlea_extended_heartbeat("bat_destroy", 0.0f);

    nimcp_free(bat->call_buffer);
    nimcp_free(bat->echo_correlator);
    nimcp_free(bat->last_call.harmonic_levels);

    cochlea_extended_heartbeat("bat_destroy", 1.0f);
    nimcp_free(bat);
}

nimcp_error_t bat_auditory_process(
    bat_auditory_t* bat,
    const float* audio_in,
    uint32_t num_samples,
    float* output)
{
    if (!bat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_process: bat is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!audio_in) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_process: audio_in is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_process: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_extended_heartbeat("bat_process", 0.0f);

    /*
     * Stub: Zero outputs for ultrasonic channels.
     * Full implementation would apply extended filterbank for echolocation band.
     */
    for (uint32_t ch = 0; ch < bat->num_channels; ch++) {
        output[ch] = 0.0f;
    }

    (void)num_samples;

    cochlea_extended_heartbeat("bat_process", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t bat_auditory_generate_call(
    bat_auditory_t* bat,
    const echolocation_call_t* call,
    float* audio_out,
    uint32_t buffer_size,
    uint32_t* num_samples_out)
{
    if (!bat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_generate_call: bat is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!call) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_generate_call: call is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!audio_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_generate_call: audio_out is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!num_samples_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_generate_call: num_samples_out is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_extended_heartbeat("bat_generate_call", 0.0f);

    /* Save call parameters */
    bat->last_call = *call;
    bat->last_call.harmonic_levels = NULL; /* Do not copy pointer */

    /* Compute number of samples for call duration */
    uint32_t call_samples = (uint32_t)(call->duration_ms * (float)bat->sample_rate / 1000.0f);
    if (call_samples > buffer_size) call_samples = buffer_size;

    /*
     * Generate simple FM sweep or CF call based on type.
     * Stub: generate a basic sine sweep.
     */
    float amplitude = powf(10.0f, (call->intensity_db - 100.0f) / 20.0f); /* Normalize */
    if (amplitude > 1.0f) amplitude = 1.0f;

    for (uint32_t s = 0; s < call_samples; s++) {
        float t = (float)s / (float)bat->sample_rate;
        float t_frac = (float)s / (float)(call_samples > 1 ? call_samples - 1 : 1);
        float freq = 0.0f;

        switch (call->type) {
            case ECHO_CALL_FM:
                /* Downward FM sweep */
                freq = call->start_freq_hz + t_frac * (call->end_freq_hz - call->start_freq_hz);
                break;
            case ECHO_CALL_CF:
                freq = call->cf_freq_hz;
                break;
            case ECHO_CALL_CF_FM:
                /* CF portion (first 70%) then FM sweep (last 30%) */
                if (t_frac < 0.7f) {
                    freq = call->cf_freq_hz;
                } else {
                    float fm_frac = (t_frac - 0.7f) / 0.3f;
                    freq = call->cf_freq_hz + fm_frac * (call->end_freq_hz - call->cf_freq_hz);
                }
                break;
            case ECHO_CALL_FM_CF_FM:
                if (t_frac < 0.2f) {
                    float fm1_frac = t_frac / 0.2f;
                    freq = call->start_freq_hz + fm1_frac * (call->cf_freq_hz - call->start_freq_hz);
                } else if (t_frac < 0.8f) {
                    freq = call->cf_freq_hz;
                } else {
                    float fm2_frac = (t_frac - 0.8f) / 0.2f;
                    freq = call->cf_freq_hz + fm2_frac * (call->end_freq_hz - call->cf_freq_hz);
                }
                break;
            case ECHO_CALL_CLICK:
                /* Short broadband pulse */
                freq = call->start_freq_hz;
                amplitude *= (t_frac < 0.1f) ? 1.0f : expf(-10.0f * (t_frac - 0.1f));
                break;
        }

        float phase = 2.0f * (float)M_PI * freq * t;
        audio_out[s] = amplitude * sinf(phase);
    }

    *num_samples_out = call_samples;

    cochlea_extended_heartbeat("bat_generate_call", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t bat_auditory_process_echo(
    bat_auditory_t* bat,
    const float* echo_audio,
    uint32_t num_samples,
    echolocation_result_t* result)
{
    if (!bat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_process_echo: bat is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!echo_audio) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_process_echo: echo_audio is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_process_echo: result is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_extended_heartbeat("bat_process_echo", 0.0f);

    /*
     * Stub: Simple energy-based target detection.
     * Full implementation would cross-correlate with emitted call.
     */
    float max_energy = 0.0f;
    uint32_t max_idx = 0;
    for (uint32_t s = 0; s < num_samples; s++) {
        float e = echo_audio[s] * echo_audio[s];
        if (e > max_energy) {
            max_energy = e;
            max_idx = s;
        }
    }

    result->num_targets = 0;

    if (max_energy > 1e-6f && result->max_targets > 0 && result->targets) {
        /* Simple target from echo delay */
        float delay_s = (float)max_idx / (float)bat->sample_rate;
        float range_m = delay_s * SPEED_OF_SOUND_MPS / 2.0f; /* Round trip */

        result->targets[0].range_m              = range_m;
        result->targets[0].azimuth_deg          = 0.0f;
        result->targets[0].elevation_deg        = 0.0f;
        result->targets[0].velocity_mps         = bat->doppler.velocity_estimate_mps;
        result->targets[0].target_strength      = sqrtf(max_energy);
        result->targets[0].size_estimate_cm     = range_m * 2.0f; /* Rough estimate */
        result->targets[0].detection_confidence = (max_energy > 1e-4f) ? 0.8f : 0.3f;
        result->targets[0].range_confidence     = 0.5f;

        result->num_targets      = 1;
        result->nearest_target_m = range_m;
        result->clutter_level    = 0.1f;
    }

    /* Suggest mode based on nearest target */
    if (result->num_targets > 0 && result->nearest_target_m < 1.0f) {
        result->suggested_mode = ECHO_MODE_TERMINAL;
    } else if (result->num_targets > 0 && result->nearest_target_m < 5.0f) {
        result->suggested_mode = ECHO_MODE_APPROACH;
    } else {
        result->suggested_mode = ECHO_MODE_SEARCHING;
    }

    cochlea_extended_heartbeat("bat_process_echo", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t bat_auditory_set_mode(
    bat_auditory_t* bat,
    echolocation_mode_t mode)
{
    if (!bat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_set_mode: bat is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    bat->current_mode = mode;
    return NIMCP_SUCCESS;
}

nimcp_error_t bat_auditory_get_doppler(
    const bat_auditory_t* bat,
    doppler_state_t* state)
{
    if (!bat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_get_doppler: bat is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_get_doppler: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    *state = bat->doppler;
    return NIMCP_SUCCESS;
}

nimcp_error_t bat_auditory_enable_dsc(bat_auditory_t* bat, bool enable)
{
    if (!bat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "bat_auditory_enable_dsc: bat is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    bat->doppler.enable_dsc = enable;
    return NIMCP_SUCCESS;
}

//=============================================================================
// Echolocation Result Management
//=============================================================================

echolocation_result_t* echolocation_result_create(uint32_t max_targets)
{
    echolocation_result_t* result = (echolocation_result_t*)nimcp_calloc(
        1, sizeof(echolocation_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "echolocation_result_create: result is NULL");
        return NULL;
    }

    result->max_targets = max_targets;
    result->num_targets = 0;
    result->nearest_target_m = 0.0f;
    result->clutter_level    = 0.0f;
    result->suggested_mode   = ECHO_MODE_SEARCHING;

    if (max_targets > 0) {
        result->targets = (echolocation_target_t*)nimcp_calloc(
            max_targets, sizeof(echolocation_target_t));
        if (!result->targets) {
            nimcp_free(result);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "echolocation_result_create: result->targets is NULL");
            return NULL;
        }
    }

    return result;
}

void echolocation_result_destroy(echolocation_result_t* result)
{
    if (!result) return;
    nimcp_free(result->targets);
    nimcp_free(result);
}

void echolocation_result_clear(echolocation_result_t* result)
{
    if (!result) return;
    result->num_targets      = 0;
    result->nearest_target_m = 0.0f;
    result->clutter_level    = 0.0f;
    result->suggested_mode   = ECHO_MODE_SEARCHING;
    if (result->targets && result->max_targets > 0) {
        memset(result->targets, 0, result->max_targets * sizeof(echolocation_target_t));
    }
}

//=============================================================================
// Extended Hearing - Configuration
//=============================================================================

ext_hearing_config_t ext_hearing_config_default(ext_hearing_mode_t mode)
{
    ext_hearing_config_t config;
    memset(&config, 0, sizeof(config));

    config.mode = mode;
    config.enable_mode_switching = true;
    config.mode_transition_ms    = 50.0f;

    switch (mode) {
        case EXT_HEARING_DOG:
            config.dog = dog_auditory_config_default(DOG_BREED_GENERIC);
            config.dog_weight = 1.0f;
            config.bat_weight = 0.0f;
            break;
        case EXT_HEARING_BAT:
            config.bat = bat_auditory_config_default(BAT_SPECIES_GENERIC);
            config.dog_weight = 0.0f;
            config.bat_weight = 1.0f;
            break;
        case EXT_HEARING_HYBRID:
            config.dog = dog_auditory_config_default(DOG_BREED_GENERIC);
            config.bat = bat_auditory_config_default(BAT_SPECIES_GENERIC);
            config.dog_weight = 0.5f;
            config.bat_weight = 0.5f;
            break;
        default: /* HUMAN or CUSTOM */
            config.dog_weight = 0.0f;
            config.bat_weight = 0.0f;
            break;
    }

    return config;
}

//=============================================================================
// Extended Hearing - Core API
//=============================================================================

ext_hearing_t* ext_hearing_create(
    const ext_hearing_config_t* config,
    uint32_t sample_rate)
{
    cochlea_extended_heartbeat("ext_create", 0.0f);

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ext_hearing_create: config is NULL");
        return NULL;
    }

    ext_hearing_t* ext = (ext_hearing_t*)nimcp_calloc(1, sizeof(ext_hearing_t));
    if (!ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "ext_hearing_create: failed to allocate ext_hearing");
        return NULL;
    }

    ext->config      = *config;
    ext->sample_rate = sample_rate;
    ext->active_mode = config->mode;
    ext->mode_transition_progress = 1.0f; /* Fully transitioned */
    ext->in_transition = false;

    /* Create sub-processors based on mode */
    if (config->mode == EXT_HEARING_DOG || config->mode == EXT_HEARING_HYBRID) {
        ext->dog = dog_auditory_create(&config->dog, sample_rate);
    }
    if (config->mode == EXT_HEARING_BAT || config->mode == EXT_HEARING_HYBRID) {
        ext->bat = bat_auditory_create(&config->bat, sample_rate);
    }

    cochlea_extended_heartbeat("ext_create", 1.0f);
    return ext;
}

void ext_hearing_destroy(ext_hearing_t* ext)
{
    if (!ext) return;

    cochlea_extended_heartbeat("ext_destroy", 0.0f);

    if (ext->dog) dog_auditory_destroy(ext->dog);
    if (ext->bat) bat_auditory_destroy(ext->bat);

    cochlea_extended_heartbeat("ext_destroy", 1.0f);
    nimcp_free(ext);
}

nimcp_error_t ext_hearing_process(
    ext_hearing_t* ext,
    const float* audio_in,
    uint32_t num_samples,
    float* output)
{
    if (!ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ext_hearing_process: ext is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!audio_in) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ext_hearing_process: audio_in is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ext_hearing_process: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_extended_heartbeat("ext_process", 0.0f);

    /*
     * Route to appropriate sub-processor based on active mode.
     * For stereo-requiring dog mode, pass mono to both channels.
     */
    switch (ext->active_mode) {
        case EXT_HEARING_DOG:
            if (ext->dog) {
                dog_auditory_process(ext->dog, audio_in, audio_in, num_samples, output);
            }
            break;
        case EXT_HEARING_BAT:
            if (ext->bat) {
                bat_auditory_process(ext->bat, audio_in, num_samples, output);
            }
            break;
        case EXT_HEARING_HYBRID:
            /* Process through both, blend outputs */
            /* Stub: process through whichever is available */
            if (ext->bat) {
                bat_auditory_process(ext->bat, audio_in, num_samples, output);
            } else if (ext->dog) {
                dog_auditory_process(ext->dog, audio_in, audio_in, num_samples, output);
            }
            break;
        case EXT_HEARING_HUMAN:
        case EXT_HEARING_CUSTOM:
        default:
            /* No extended processing */
            break;
    }

    cochlea_extended_heartbeat("ext_process", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t ext_hearing_switch_mode(
    ext_hearing_t* ext,
    ext_hearing_mode_t mode)
{
    if (!ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ext_hearing_switch_mode: ext is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_extended_heartbeat("ext_switch_mode", 0.0f);

    if (!ext->config.enable_mode_switching) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Create sub-processor if needed for new mode */
    if ((mode == EXT_HEARING_DOG || mode == EXT_HEARING_HYBRID) && !ext->dog) {
        dog_auditory_config_t dog_cfg = dog_auditory_config_default(DOG_BREED_GENERIC);
        ext->dog = dog_auditory_create(&dog_cfg, ext->sample_rate);
    }
    if ((mode == EXT_HEARING_BAT || mode == EXT_HEARING_HYBRID) && !ext->bat) {
        bat_auditory_config_t bat_cfg = bat_auditory_config_default(BAT_SPECIES_GENERIC);
        ext->bat = bat_auditory_create(&bat_cfg, ext->sample_rate);
    }

    ext->active_mode = mode;
    ext->config.mode = mode;
    ext->in_transition = true;
    ext->mode_transition_progress = 0.0f;

    cochlea_extended_heartbeat("ext_switch_mode", 1.0f);
    return NIMCP_SUCCESS;
}

ext_hearing_mode_t ext_hearing_get_mode(const ext_hearing_t* ext)
{
    if (!ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ext_hearing_get_mode: ext is NULL");
        return EXT_HEARING_HUMAN; /* safe default */
    }
    return ext->active_mode;
}

dog_auditory_t* ext_hearing_get_dog(ext_hearing_t* ext)
{
    if (!ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ext_hearing_get_dog: ext is NULL");
        return NULL;
    }
    return ext->dog;
}

bat_auditory_t* ext_hearing_get_bat(ext_hearing_t* ext)
{
    if (!ext) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ext_hearing_get_bat: ext is NULL");
        return NULL;
    }
    return ext->bat;
}
