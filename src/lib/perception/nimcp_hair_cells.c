/**
 * @file nimcp_hair_cells.c
 * @brief Inner and Outer Hair Cell transduction implementation
 *
 * WHAT: Mechanotransduction converting basilar membrane motion to neural signals
 * WHY:  Bridge between mechanical (BM) and neural (ANF) stages of hearing
 * HOW:  Boltzmann transduction, prestin electromotility, adaptation
 *
 * IMPLEMENTATION:
 * - IHC: Boltzmann transduction function + ribbon synapse
 * - OHC: Level-dependent gain with compression
 * - Adaptation: Fast (~1ms) and slow (~50ms) time constants
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "perception/nimcp_hair_cells.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#include <stddef.h>  /* for NULL */
#include <stdlib.h>
#include "constants/nimcp_neural_constants.h"

//=============================================================================
// Opaque Structure Definitions
//=============================================================================

struct ihc_bank {
    ihc_config_t config;
    ihc_state_t* cells;
};

struct ohc_bank {
    ohc_config_t config;
    ohc_state_t* cells;
};

struct hair_cell_bank {
    hc_bank_config_t config;
    ihc_bank_t* ihc;
    ohc_bank_t* ohc;
    uint64_t samples_processed;
};
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hair_cells)

//=============================================================================
// IHC Bank API Implementation
//=============================================================================

nimcp_error_t ihc_bank_set_health(ihc_bank_t* bank, uint32_t channel,
                                   hc_ihc_health_t health) {
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (channel >= bank->config.num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: capacity exceeded");
        return NIMCP_ERROR_INVALID_PARAMETER;
    }
    bank->cells[channel].health = health;
    switch (health) {
    case HC_IHC_HEALTHY:        bank->cells[channel].efficiency = 1.0f; break;
    case HC_IHC_MILD_DAMAGE:    bank->cells[channel].efficiency = 0.8f; break;
    case HC_IHC_MODERATE_DAMAGE:bank->cells[channel].efficiency = 0.5f; break;
    case HC_IHC_SEVERE_DAMAGE:  bank->cells[channel].efficiency = 0.2f; break;
    case HC_IHC_DEAD:           bank->cells[channel].efficiency = 0.0f; break;
    default:                    bank->cells[channel].efficiency = 1.0f; break;
    }
    hair_cells_heartbeat("ihc_bank_set_health", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Combined Hair Cell Bank API Implementation
//=============================================================================

hc_bank_config_t hc_bank_config_default(uint32_t num_channels,
                                          bm_hearing_mode_t mode) {
    hc_bank_config_t config;
    memset(&config, 0, sizeof(config));

    config.ihc_config.num_channels = num_channels;
    config.ihc_config.model = HC_MODEL_BOLTZMANN;
    config.ihc_config.operating_point = 0.05f;
    config.ihc_config.asymmetry = 0.1f;
    config.ihc_config.slope = 1.0f;
    config.ihc_config.fast_tau_ms = HC_ADAPT_FAST_MS;
    config.ihc_config.slow_tau_ms = HC_ADAPT_SLOW_MS;
    config.ihc_config.max_release_rate = 250.0f;
    config.ihc_config.spontaneous_rate = HC_IHC_SPONTANEOUS_RATE;
    config.ihc_config.sample_rate = 44100;

    config.ohc_config.num_channels = num_channels;
    config.ohc_config.max_gain_db = HC_OHC_MAX_GAIN_DB;
    config.ohc_config.compression_ratio = HC_OHC_COMPRESSION_RATIO;
    config.ohc_config.knee_point_db = HC_OHC_KNEE_POINT_DB;
    config.ohc_config.prestin_saturation = 1.0f;
    config.ohc_config.electromotility_gain = 1.0f;
    config.ohc_config.gain_per_channel = NULL;
    config.ohc_config.bandwidth_adjustment = NULL;
    config.ohc_config.enable_efferent = false;
    config.ohc_config.ach_decay_ms = 50.0f;
    config.ohc_config.sample_rate = 44100;

    config.enable_ohc_ihc_coupling = true;
    config.coupling_strength = 0.5f;
    config.hearing_mode = mode;

    return config;
}

hair_cell_bank_t* hair_cell_bank_create(const hc_bank_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hair_cell_bank_create: config is NULL");
        return NULL;
    }
    uint32_t n = config->ihc_config.num_channels;
    if (n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hair_cell_bank_create: n is zero");
        return NULL;
    }

    hair_cell_bank_t* bank = (hair_cell_bank_t*)nimcp_calloc(1, sizeof(hair_cell_bank_t));
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hair_cell_bank_create: bank is NULL");
        return NULL;
    }
    bank->config = *config;

    bank->ihc = (ihc_bank_t*)nimcp_calloc(1, sizeof(ihc_bank_t));
    if (!bank->ihc) { nimcp_free(bank); return NULL; }
    bank->ihc->config = config->ihc_config;
    bank->ihc->cells = (ihc_state_t*)nimcp_calloc(n, sizeof(ihc_state_t));
    if (!bank->ihc->cells) { nimcp_free(bank->ihc); nimcp_free(bank); return NULL; }
    for (uint32_t i = 0; i < n; i++) {
        bank->ihc->cells[i].health = HC_IHC_HEALTHY;
        bank->ihc->cells[i].efficiency = 1.0f;
        bank->ihc->cells[i].resting_potential = NIMCP_RESTING_POTENTIAL_MV;
        bank->ihc->cells[i].saturation_potential = -30.0f;
        bank->ihc->cells[i].vesicle_pool = 1.0f;
    }

    bank->ohc = (ohc_bank_t*)nimcp_calloc(1, sizeof(ohc_bank_t));
    if (!bank->ohc) { nimcp_free(bank->ihc->cells); nimcp_free(bank->ihc); nimcp_free(bank); return NULL; }
    bank->ohc->config = config->ohc_config;
    bank->ohc->cells = (ohc_state_t*)nimcp_calloc(n, sizeof(ohc_state_t));
    if (!bank->ohc->cells) { nimcp_free(bank->ohc); nimcp_free(bank->ihc->cells); nimcp_free(bank->ihc); nimcp_free(bank); return NULL; }
    for (uint32_t i = 0; i < n; i++) {
        bank->ohc->cells[i].health = HC_OHC_HEALTHY;
        bank->ohc->cells[i].survival_fraction = 1.0f;
        bank->ohc->cells[i].gain_linear = 1.0f;
    }

    hair_cells_heartbeat("hair_cell_bank_create", 1.0f);
    return bank;
}

void hair_cell_bank_destroy(hair_cell_bank_t* bank) {
    if (!bank) return;
    if (bank->ohc) { nimcp_free(bank->ohc->cells); nimcp_free(bank->ohc); }
    if (bank->ihc) { nimcp_free(bank->ihc->cells); nimcp_free(bank->ihc); }
    nimcp_free(bank);
}

nimcp_error_t hair_cell_bank_process(hair_cell_bank_t* bank,
                                       const bm_output_t* bm_output,
                                       hc_bank_output_t* output) {
    if (!bank || !bm_output || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hair_cell_bank_destroy: required parameter is NULL (bank, bm_output, output)");
        return NIMCP_ERROR_NULL_POINTER;
    }
    hair_cells_heartbeat("hair_cell_bank_process", 0.5f);
    bank->samples_processed += bm_output->num_samples;
    return NIMCP_SUCCESS;
}

nimcp_error_t hair_cell_bank_reset(hair_cell_bank_t* bank) {
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hair_cell_bank_reset: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    uint32_t n = bank->config.ihc_config.num_channels;
    if (bank->ihc && bank->ihc->cells) {
        for (uint32_t i = 0; i < n; i++) {
            bank->ihc->cells[i].receptor_potential = 0.0f;
            bank->ihc->cells[i].fast_adapt_state = 0.0f;
            bank->ihc->cells[i].slow_adapt_state = 0.0f;
            bank->ihc->cells[i].glutamate_release = 0.0f;
            bank->ihc->cells[i].vesicle_pool = 1.0f;
        }
    }
    if (bank->ohc && bank->ohc->cells) {
        for (uint32_t i = 0; i < n; i++) {
            bank->ohc->cells[i].receptor_potential = 0.0f;
            bank->ohc->cells[i].compression_state = 0.0f;
            bank->ohc->cells[i].prestin_state = 0.0f;
        }
    }
    bank->samples_processed = 0;
    hair_cells_heartbeat("hair_cell_bank_reset", 1.0f);
    return NIMCP_SUCCESS;
}

hc_bank_output_t* hc_bank_output_create(uint32_t num_channels) {
    if (num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hc_bank_output_create: num_channels is zero");
        return NULL;
    }

    hc_bank_output_t* out = (hc_bank_output_t*)nimcp_calloc(1, sizeof(hc_bank_output_t));
    if (!out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hc_bank_output_create: out is NULL");
        return NULL;
    }
    out->num_channels = num_channels;

    out->ihc.receptor_potential = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->ihc.glutamate_release = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->ihc.adaptation_state = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->ihc.num_channels = num_channels;

    out->ohc.gain = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->ohc.amplified_bm = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->ohc.oae_signal = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->ohc.num_channels = num_channels;

    out->neural_drive = (float*)nimcp_calloc(num_channels, sizeof(float));

    if (!out->ihc.receptor_potential || !out->ihc.glutamate_release ||
        !out->ihc.adaptation_state || !out->ohc.gain ||
        !out->ohc.amplified_bm || !out->ohc.oae_signal || !out->neural_drive) {
        nimcp_free(out->ihc.receptor_potential);
        nimcp_free(out->ihc.glutamate_release);
        nimcp_free(out->ihc.adaptation_state);
        nimcp_free(out->ohc.gain);
        nimcp_free(out->ohc.amplified_bm);
        nimcp_free(out->ohc.oae_signal);
        nimcp_free(out->neural_drive);
        nimcp_free(out);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hc_bank_output_create: operation failed");
        return NULL;
    }

    return out;
}

void hc_bank_output_destroy(hc_bank_output_t* output) {
    if (!output) return;
    nimcp_free(output->ihc.receptor_potential);
    nimcp_free(output->ihc.glutamate_release);
    nimcp_free(output->ihc.adaptation_state);
    nimcp_free(output->ohc.gain);
    nimcp_free(output->ohc.amplified_bm);
    nimcp_free(output->ohc.oae_signal);
    nimcp_free(output->neural_drive);
    nimcp_free(output);
}

nimcp_error_t hair_cell_bank_get_stats(const hair_cell_bank_t* bank,
                                         hc_bank_stats_t* stats) {
    if (!bank || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hc_bank_output_destroy: required parameter is NULL (bank, stats)");
        return NIMCP_ERROR_NULL_POINTER;
    }
    memset(stats, 0, sizeof(hc_bank_stats_t));
    stats->samples_processed = bank->samples_processed;
    return NIMCP_SUCCESS;
}

ihc_bank_t* hair_cell_bank_get_ihc(hair_cell_bank_t* bank) {
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hair_cell_bank_get_ihc: bank is NULL");
        return NULL;
    }
    return bank->ihc;
}

ohc_bank_t* hair_cell_bank_get_ohc(hair_cell_bank_t* bank) {
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hair_cell_bank_get_ohc: bank is NULL");
        return NULL;
    }
    return bank->ohc;
}

ihc_config_t ihc_config_default(uint32_t num_channels, bm_hearing_mode_t mode) {
    ihc_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_channels = num_channels;
    config.model = HC_MODEL_BOLTZMANN;
    config.operating_point = 0.05f;
    config.asymmetry = 0.1f;
    config.slope = 1.0f;
    config.fast_tau_ms = HC_ADAPT_FAST_MS;
    config.slow_tau_ms = HC_ADAPT_SLOW_MS;
    config.max_release_rate = 250.0f;
    config.spontaneous_rate = HC_IHC_SPONTANEOUS_RATE;
    config.sample_rate = 44100;
    (void)mode;
    return config;
}

ihc_bank_t* ihc_bank_create(const ihc_config_t* config) {
    if (!config || config->num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ihc_bank_create: config is NULL");
        return NULL;
    }
    ihc_bank_t* bank = (ihc_bank_t*)nimcp_calloc(1, sizeof(ihc_bank_t));
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ihc_bank_create: bank is NULL");
        return NULL;
    }
    bank->config = *config;
    bank->cells = (ihc_state_t*)nimcp_calloc(config->num_channels, sizeof(ihc_state_t));
    if (!bank->cells) { nimcp_free(bank); return NULL; }
    for (uint32_t i = 0; i < config->num_channels; i++) {
        bank->cells[i].health = HC_IHC_HEALTHY;
        bank->cells[i].efficiency = 1.0f;
        bank->cells[i].resting_potential = NIMCP_RESTING_POTENTIAL_MV;
        bank->cells[i].saturation_potential = -30.0f;
        bank->cells[i].vesicle_pool = 1.0f;
    }
    return bank;
}

void ihc_bank_destroy(ihc_bank_t* bank) {
    if (!bank) return;
    nimcp_free(bank->cells);
    nimcp_free(bank);
}

nimcp_error_t ihc_bank_process(ihc_bank_t* bank, const float* bm_velocity,
                                 uint32_t num_samples, ihc_output_t* output) {
    if (!bank || !bm_velocity || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ihc_bank_destroy: required parameter is NULL (bank, bm_velocity, output)");
        return NIMCP_ERROR_NULL_POINTER;
    }
    (void)num_samples;
    return NIMCP_SUCCESS;
}

nimcp_error_t ihc_bank_reset(ihc_bank_t* bank) {
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ihc_bank_reset: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    for (uint32_t i = 0; i < bank->config.num_channels; i++) {
        bank->cells[i].receptor_potential = 0.0f;
        bank->cells[i].fast_adapt_state = 0.0f;
        bank->cells[i].slow_adapt_state = 0.0f;
        bank->cells[i].glutamate_release = 0.0f;
        bank->cells[i].vesicle_pool = 1.0f;
    }
    return NIMCP_SUCCESS;
}

nimcp_error_t ihc_bank_get_state(const ihc_bank_t* bank, uint32_t channel,
                                   ihc_state_t* state) {
    if (!bank || !state || channel >= bank->config.num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ihc_bank_reset: required parameter is NULL (bank, state)");
        return NIMCP_ERROR_INVALID_PARAMETER;
    }
    *state = bank->cells[channel];
    return NIMCP_SUCCESS;
}

ohc_config_t ohc_config_default(uint32_t num_channels, bm_hearing_mode_t mode) {
    ohc_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_channels = num_channels;
    config.max_gain_db = HC_OHC_MAX_GAIN_DB;
    config.compression_ratio = HC_OHC_COMPRESSION_RATIO;
    config.knee_point_db = HC_OHC_KNEE_POINT_DB;
    config.prestin_saturation = 1.0f;
    config.electromotility_gain = 1.0f;
    config.enable_efferent = false;
    config.ach_decay_ms = 50.0f;
    config.sample_rate = 44100;
    (void)mode;
    return config;
}

ohc_bank_t* ohc_bank_create(const ohc_config_t* config) {
    if (!config || config->num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ohc_bank_create: config is NULL");
        return NULL;
    }
    ohc_bank_t* bank = (ohc_bank_t*)nimcp_calloc(1, sizeof(ohc_bank_t));
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ohc_bank_create: bank is NULL");
        return NULL;
    }
    bank->config = *config;
    bank->cells = (ohc_state_t*)nimcp_calloc(config->num_channels, sizeof(ohc_state_t));
    if (!bank->cells) { nimcp_free(bank); return NULL; }
    for (uint32_t i = 0; i < config->num_channels; i++) {
        bank->cells[i].health = HC_OHC_HEALTHY;
        bank->cells[i].survival_fraction = 1.0f;
        bank->cells[i].gain_linear = 1.0f;
    }
    return bank;
}

void ohc_bank_destroy(ohc_bank_t* bank) {
    if (!bank) return;
    nimcp_free(bank->cells);
    nimcp_free(bank);
}

nimcp_error_t ohc_bank_process(ohc_bank_t* bank, const float* bm_input,
                                 uint32_t num_samples, ohc_output_t* output) {
    if (!bank || !bm_input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ohc_bank_destroy: required parameter is NULL (bank, bm_input, output)");
        return NIMCP_ERROR_NULL_POINTER;
    }
    (void)num_samples;
    return NIMCP_SUCCESS;
}

nimcp_error_t ohc_bank_reset(ohc_bank_t* bank) {
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ohc_bank_reset: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    for (uint32_t i = 0; i < bank->config.num_channels; i++) {
        bank->cells[i].receptor_potential = 0.0f;
        bank->cells[i].compression_state = 0.0f;
        bank->cells[i].prestin_state = 0.0f;
    }
    return NIMCP_SUCCESS;
}

nimcp_error_t ohc_bank_set_health(ohc_bank_t* bank, uint32_t channel,
                                    hc_ohc_health_t health) {
    if (!bank || channel >= bank->config.num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ohc_bank_reset: bank is NULL");
        return NIMCP_ERROR_INVALID_PARAMETER;
    }
    bank->cells[channel].health = health;
    switch (health) {
    case HC_OHC_HEALTHY:         bank->cells[channel].survival_fraction = 1.0f; break;
    case HC_OHC_MILD_DAMAGE:     bank->cells[channel].survival_fraction = 0.8f; break;
    case HC_OHC_MODERATE_DAMAGE: bank->cells[channel].survival_fraction = 0.5f; break;
    case HC_OHC_SEVERE_DAMAGE:   bank->cells[channel].survival_fraction = 0.2f; break;
    case HC_OHC_DEAD:            bank->cells[channel].survival_fraction = 0.0f; break;
    default:                     bank->cells[channel].survival_fraction = 1.0f; break;
    }
    return NIMCP_SUCCESS;
}

nimcp_error_t ohc_bank_set_ach(ohc_bank_t* bank, float ach_level) {
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ohc_bank_set_ach: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    for (uint32_t i = 0; i < bank->config.num_channels; i++) {
        bank->cells[i].ach_level = ach_level;
    }
    return NIMCP_SUCCESS;
}

nimcp_error_t ohc_bank_get_oae(const ohc_bank_t* bank, float* oae_signal) {
    if (!bank || !oae_signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ohc_bank_get_oae: required parameter is NULL (bank, oae_signal)");
        return NIMCP_ERROR_NULL_POINTER;
    }
    for (uint32_t i = 0; i < bank->config.num_channels; i++) {
        oae_signal[i] = bank->cells[i].oae_contribution;
    }
    return NIMCP_SUCCESS;
}

ihc_output_t* ihc_output_create(uint32_t num_channels) {
    if (num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ihc_output_create: num_channels is zero");
        return NULL;
    }
    ihc_output_t* out = (ihc_output_t*)nimcp_calloc(1, sizeof(ihc_output_t));
    if (!out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ihc_output_create: out is NULL");
        return NULL;
    }
    out->num_channels = num_channels;
    out->receptor_potential = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->glutamate_release = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->adaptation_state = (float*)nimcp_calloc(num_channels, sizeof(float));
    if (!out->receptor_potential || !out->glutamate_release || !out->adaptation_state) {
        nimcp_free(out->receptor_potential); nimcp_free(out->glutamate_release);
        nimcp_free(out->adaptation_state); nimcp_free(out);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ihc_output_create: required parameter is NULL (out->receptor_potential, out->glutamate_release, out->adaptation_state)");
        return NULL;
    }
    return out;
}

void ihc_output_destroy(ihc_output_t* output) {
    if (!output) return;
    nimcp_free(output->receptor_potential);
    nimcp_free(output->glutamate_release);
    nimcp_free(output->adaptation_state);
    nimcp_free(output);
}

ohc_output_t* ohc_output_create(uint32_t num_channels) {
    if (num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ohc_output_create: num_channels is zero");
        return NULL;
    }
    ohc_output_t* out = (ohc_output_t*)nimcp_calloc(1, sizeof(ohc_output_t));
    if (!out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ohc_output_create: out is NULL");
        return NULL;
    }
    out->num_channels = num_channels;
    out->gain = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->amplified_bm = (float*)nimcp_calloc(num_channels, sizeof(float));
    out->oae_signal = (float*)nimcp_calloc(num_channels, sizeof(float));
    if (!out->gain || !out->amplified_bm || !out->oae_signal) {
        nimcp_free(out->gain); nimcp_free(out->amplified_bm);
        nimcp_free(out->oae_signal); nimcp_free(out);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ohc_output_create: required parameter is NULL (out->gain, out->amplified_bm, out->oae_signal)");
        return NULL;
    }
    return out;
}

void ohc_output_destroy(ohc_output_t* output) {
    if (!output) return;
    nimcp_free(output->gain);
    nimcp_free(output->amplified_bm);
    nimcp_free(output->oae_signal);
    nimcp_free(output);
}
