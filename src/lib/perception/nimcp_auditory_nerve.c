/**
 * @file nimcp_auditory_nerve.c
 * @brief Auditory Nerve Fiber (ANF) model implementation
 *
 * WHAT: Convert hair cell output to spike trains
 * WHY:  Bridge between peripheral hearing and central auditory processing
 * HOW:  Inhomogeneous Poisson spiking with refractory periods
 *
 * IMPLEMENTATION:
 * - Three fiber types: High-SR (60%), Med-SR (25%), Low-SR (15%)
 * - Refractory periods: Absolute ~0.6ms, Relative ~1.5ms
 * - Phase locking for frequencies < 4 kHz
 * - Adaptation: Rapid, short-term, and long-term
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "perception/nimcp_auditory_nerve.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(auditory_nerve)

/* Thread-safe PRNG (replaces non-reentrant rand()) */
static __thread unsigned int g_tl_rand_seed = 0;
static inline unsigned int nimcp_thread_rand(void) {
    if (g_tl_rand_seed == 0) {
        g_tl_rand_seed = (unsigned int)(uintptr_t)&g_tl_rand_seed ^ (unsigned int)time(NULL);
    }
    return (unsigned int)rand_r(&g_tl_rand_seed);
}
#define NIMCP_THREAD_RAND() nimcp_thread_rand()

//=============================================================================
// Internal Constants
//=============================================================================

#define ANF_LOG_MODULE "AUDITORY_NERVE"

//=============================================================================
// Opaque Structure Definition
//=============================================================================

/**
 * @brief ANF bank internal implementation structure
 */
struct anf_bank {
    /* Configuration */
    anf_config_t config;

    /* Fiber populations per channel */
    anf_population_t* populations;  /**< Array [num_channels] */

    /* Processing state */
    float  time_elapsed_ms;         /**< Accumulated simulation time */
    bool   bat_mode_enabled;        /**< Bat microsecond precision mode */
    float  bat_temporal_res_us;     /**< Bat temporal resolution */

    /* Spike time buffer for bat mode */
    float* bat_spike_times;         /**< Precise spike times buffer */
    uint32_t bat_spike_count;       /**< Current spike count */
    uint32_t bat_spike_capacity;    /**< Max spike capacity */

    /* Statistics */
    anf_stats_t stats;
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Initialize a single fiber with SR type properties
 */
static void anf_init_fiber(anf_fiber_t* fiber, anf_sr_type_t sr_type)
{
    memset(fiber, 0, sizeof(anf_fiber_t));
    fiber->sr_type = sr_type;
    fiber->health  = ANF_FIBER_HEALTHY;
    fiber->efficiency = 1.0f;

    switch (sr_type) {
        case ANF_SR_HIGH:
            fiber->spontaneous_rate = 50.0f;    /* ~50 sp/s */
            fiber->threshold_db     = 0.0f;     /* Low threshold */
            fiber->dynamic_range_db = 25.0f;    /* Narrow dynamic range */
            fiber->saturation_rate  = ANF_MAX_SPIKE_RATE;
            break;
        case ANF_SR_MEDIUM:
            fiber->spontaneous_rate = 5.0f;     /* ~5 sp/s */
            fiber->threshold_db     = 15.0f;    /* Medium threshold */
            fiber->dynamic_range_db = 40.0f;    /* Moderate dynamic range */
            fiber->saturation_rate  = ANF_MAX_SPIKE_RATE * 0.9f;
            break;
        case ANF_SR_LOW:
            fiber->spontaneous_rate = 0.1f;     /* ~0.1 sp/s */
            fiber->threshold_db     = 30.0f;    /* High threshold */
            fiber->dynamic_range_db = 60.0f;    /* Wide dynamic range */
            fiber->saturation_rate  = ANF_MAX_SPIKE_RATE * 0.8f;
            break;
    }

    fiber->last_spike_time      = 100.0f; /* Far in the past */
    fiber->in_refractory        = false;
    fiber->refractory_remaining = 0.0f;
    fiber->phase                = 0.0f;
    fiber->phase_lock_strength  = 0.0f;
    fiber->rapid_adapt_state    = 1.0f;
    fiber->short_term_adapt     = 1.0f;
    fiber->long_term_adapt      = 1.0f;
}

/**
 * @brief Initialize a population for one frequency channel
 */
static void anf_init_population(
    anf_population_t* pop,
    float center_freq_hz,
    uint32_t fibers_per_channel,
    float high_sr_frac,
    float med_sr_frac,
    float low_sr_frac,
    bool allocate_fibers)
{
    memset(pop, 0, sizeof(anf_population_t));
    pop->center_freq_hz     = center_freq_hz;
    pop->num_fibers         = fibers_per_channel;
    pop->num_high_sr        = (uint32_t)(fibers_per_channel * high_sr_frac);
    pop->num_low_sr         = (uint32_t)(fibers_per_channel * low_sr_frac);
    pop->num_med_sr         = fibers_per_channel - pop->num_high_sr - pop->num_low_sr;
    pop->population_rate    = 0.0f;
    pop->population_synchrony = 0.0f;
    pop->fibers             = NULL;

    if (allocate_fibers && fibers_per_channel > 0) {
        pop->fibers = (anf_fiber_t*)nimcp_calloc(fibers_per_channel, sizeof(anf_fiber_t));
        if (pop->fibers) {
            uint32_t idx = 0;
            for (uint32_t i = 0; i < pop->num_high_sr && idx < fibers_per_channel; i++, idx++) {
                anf_init_fiber(&pop->fibers[idx], ANF_SR_HIGH);
            }
            for (uint32_t i = 0; i < pop->num_med_sr && idx < fibers_per_channel; i++, idx++) {
                anf_init_fiber(&pop->fibers[idx], ANF_SR_MEDIUM);
            }
            for (uint32_t i = 0; i < pop->num_low_sr && idx < fibers_per_channel; i++, idx++) {
                anf_init_fiber(&pop->fibers[idx], ANF_SR_LOW);
            }
        }
    }
}

//=============================================================================
// Configuration Helpers
//=============================================================================

anf_config_t anf_config_default(uint32_t num_channels, bm_hearing_mode_t mode)
{
    anf_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_channels       = num_channels;
    config.fibers_per_channel = ANF_FIBERS_PER_IHC;
    config.high_sr_fraction   = ANF_HIGH_SR_FRACTION;
    config.med_sr_fraction    = ANF_MED_SR_FRACTION;
    config.low_sr_fraction    = ANF_LOW_SR_FRACTION;

    config.model    = ANF_MODEL_POISSON;
    config.encoding = ANF_ENCODE_FIRING_RATE;

    config.abs_refractory_ms    = ANF_ABSOLUTE_REFRACTORY_MS;
    config.rel_refractory_ms    = ANF_RELATIVE_REFRACTORY_MS;
    config.phase_lock_cutoff_hz = ANF_PHASE_LOCK_LIMIT_HZ;
    config.enable_phase_locking = true;
    config.enable_adaptation    = true;
    config.rapid_adapt_tau_ms   = 5.0f;
    config.short_term_tau_ms    = 50.0f;

    config.hearing_mode = mode;
    config.sample_rate  = 44100;

    config.output_individual_fibers = false;
    config.psth_bin_ms              = 1;

    /* Bat mode: higher temporal resolution */
    if (mode == BM_MODE_BAT) {
        config.sample_rate = 400000; /* 400 kHz for bat */
    }

    return config;
}

nimcp_error_t anf_config_validate(const anf_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_config_validate: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (config->num_channels == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (config->fibers_per_channel == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    /* SR fractions should sum to ~1.0 */
    float sr_sum = config->high_sr_fraction + config->med_sr_fraction + config->low_sr_fraction;
    if (sr_sum < 0.9f || sr_sum > 1.1f) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    return NIMCP_SUCCESS;
}

//=============================================================================
// Core API
//=============================================================================

anf_bank_t* anf_bank_create(const anf_config_t* config)
{
    auditory_nerve_heartbeat("create", 0.0f);

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_create: config is NULL");
        return NULL;
    }

    if (anf_config_validate(config) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "anf_bank_create: validation failed");
        return NULL;
    }

    anf_bank_t* bank = (anf_bank_t*)nimcp_calloc(1, sizeof(anf_bank_t));
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "anf_bank_create: failed to allocate anf_bank");
        return NULL;
    }

    bank->config = *config;
    auditory_nerve_heartbeat("create", 0.2f);

    /* Allocate populations */
    bank->populations = (anf_population_t*)nimcp_calloc(
        config->num_channels, sizeof(anf_population_t));
    if (!bank->populations) {
        nimcp_free(bank);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "anf_bank_create: bank->populations is NULL");
        return NULL;
    }

    /* Initialize each population */
    bool alloc_fibers = config->output_individual_fibers;
    for (uint32_t i = 0; i < config->num_channels; i++) {
        /* Compute center frequency using ERB scale approximation */
        float frac = (float)i / (float)(config->num_channels > 1 ? config->num_channels - 1 : 1);
        float min_freq = 20.0f;
        float max_freq = 20000.0f;
        if (config->hearing_mode == BM_MODE_DOG)  max_freq = 65000.0f;
        if (config->hearing_mode == BM_MODE_BAT)  max_freq = 200000.0f;
        float cf = min_freq * powf(max_freq / min_freq, frac);

        anf_init_population(
            &bank->populations[i],
            cf,
            config->fibers_per_channel,
            config->high_sr_fraction,
            config->med_sr_fraction,
            config->low_sr_fraction,
            alloc_fibers
        );
    }

    bank->time_elapsed_ms    = 0.0f;
    bank->bat_mode_enabled   = false;
    bank->bat_temporal_res_us = ANF_BAT_TEMPORAL_RES_US;
    bank->bat_spike_times    = NULL;
    bank->bat_spike_count    = 0;
    bank->bat_spike_capacity = 0;

    memset(&bank->stats, 0, sizeof(anf_stats_t));

    auditory_nerve_heartbeat("create", 1.0f);
    return bank;
}

void anf_bank_destroy(anf_bank_t* bank)
{
    if (!bank) return;

    auditory_nerve_heartbeat("destroy", 0.0f);

    if (bank->populations) {
        for (uint32_t i = 0; i < bank->config.num_channels; i++) {
            if (bank->populations[i].fibers) {
                nimcp_free(bank->populations[i].fibers);
            }
        }
        nimcp_free(bank->populations);
    }

    nimcp_free(bank->bat_spike_times);

    auditory_nerve_heartbeat("destroy", 1.0f);
    nimcp_free(bank);
}

nimcp_error_t anf_bank_process(
    anf_bank_t* bank,
    const hc_bank_output_t* hc_output,
    float dt_ms,
    anf_output_t* output)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_process: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!hc_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_process: hc_output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_process: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    auditory_nerve_heartbeat("process", 0.0f);

    uint32_t nc = bank->config.num_channels;

    /*
     * Basic Poisson model stub:
     * For each channel, compute population firing rate from HC glutamate.
     * Firing rate = spontaneous_rate + driven_rate * glutamate_level
     */
    for (uint32_t ch = 0; ch < nc; ch++) {
        anf_population_t* pop = &bank->populations[ch];

        /* Use glutamate release from HC output as drive signal */
        float drive = 0.0f;
        if (hc_output->ihc.glutamate_release && ch < hc_output->num_channels) {
            drive = hc_output->ihc.glutamate_release[ch];
        }
        if (drive < 0.0f) drive = 0.0f;
        if (drive > 1.0f) drive = 1.0f;

        /* Compute population rate: weighted sum of fiber types */
        float high_sr_rate = 50.0f + drive * (ANF_MAX_SPIKE_RATE - 50.0f);
        float med_sr_rate  = 5.0f  + drive * (ANF_MAX_SPIKE_RATE * 0.9f - 5.0f);
        float low_sr_rate  = 0.1f  + drive * (ANF_MAX_SPIKE_RATE * 0.8f - 0.1f);

        float weighted_rate =
            bank->config.high_sr_fraction * high_sr_rate +
            bank->config.med_sr_fraction  * med_sr_rate  +
            bank->config.low_sr_fraction  * low_sr_rate;

        pop->population_rate = weighted_rate;

        /* Phase synchrony: high for low frequencies */
        if (pop->center_freq_hz < bank->config.phase_lock_cutoff_hz) {
            pop->population_synchrony = 0.8f * (1.0f - pop->center_freq_hz /
                bank->config.phase_lock_cutoff_hz);
        } else {
            pop->population_synchrony = 0.0f;
        }

        /* Write to output arrays */
        if (output->firing_rate && ch < output->num_channels) {
            output->firing_rate[ch] = weighted_rate;
        }
        if (output->synchrony && ch < output->num_channels) {
            output->synchrony[ch] = pop->population_synchrony;
        }

        /* Per-type rates */
        if (output->high_sr_rate && ch < output->num_channels) {
            output->high_sr_rate[ch] = high_sr_rate;
        }
        if (output->med_sr_rate && ch < output->num_channels) {
            output->med_sr_rate[ch] = med_sr_rate;
        }
        if (output->low_sr_rate && ch < output->num_channels) {
            output->low_sr_rate[ch] = low_sr_rate;
        }

        /* Generate Poisson spikes for spike train output */
        if (output->spike_trains && output->spike_trains[ch] && output->num_samples > 0) {
            float dt_per_sample = dt_ms; /* ms per sample */
            for (uint32_t s = 0; s < output->num_samples; s++) {
                float prob = weighted_rate * dt_per_sample / 1000.0f;
                float r = (float)NIMCP_THREAD_RAND() / (float)RAND_MAX;
                output->spike_trains[ch][s] = (r < prob) ? 1 : 0;
                if (output->spike_trains[ch][s]) {
                    bank->stats.spikes_generated++;
                }
            }
        }
    }

    /* Update timing and stats */
    bank->time_elapsed_ms += dt_ms;
    bank->stats.samples_processed++;

    /* Compute average stats */
    float rate_sum = 0.0f;
    float sync_sum = 0.0f;
    float max_rate = 0.0f;
    for (uint32_t ch = 0; ch < nc; ch++) {
        rate_sum += bank->populations[ch].population_rate;
        sync_sum += bank->populations[ch].population_synchrony;
        if (bank->populations[ch].population_rate > max_rate) {
            max_rate = bank->populations[ch].population_rate;
        }
    }
    bank->stats.avg_firing_rate = (nc > 0) ? rate_sum / (float)nc : 0.0f;
    bank->stats.max_firing_rate = max_rate;
    bank->stats.avg_synchrony   = (nc > 0) ? sync_sum / (float)nc : 0.0f;

    auditory_nerve_heartbeat("process", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t anf_bank_process_direct(
    anf_bank_t* bank,
    const float* glutamate_release,
    const float* bm_phase,
    float dt_ms,
    anf_output_t* output)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_process_direct: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!glutamate_release) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_process_direct: glutamate_release is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_process_direct: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    auditory_nerve_heartbeat("process_direct", 0.0f);

    uint32_t nc = bank->config.num_channels;

    for (uint32_t ch = 0; ch < nc; ch++) {
        anf_population_t* pop = &bank->populations[ch];

        float drive = glutamate_release[ch];
        if (drive < 0.0f) drive = 0.0f;
        if (drive > 1.0f) drive = 1.0f;

        float weighted_rate =
            bank->config.high_sr_fraction * (50.0f + drive * 350.0f) +
            bank->config.med_sr_fraction  * (5.0f  + drive * 355.0f) +
            bank->config.low_sr_fraction  * (0.1f  + drive * 320.0f);

        pop->population_rate = weighted_rate;

        /* Phase locking from BM phase if provided */
        if (bm_phase && pop->center_freq_hz < bank->config.phase_lock_cutoff_hz) {
            pop->population_synchrony = 0.8f * (1.0f - pop->center_freq_hz /
                bank->config.phase_lock_cutoff_hz);
        } else {
            pop->population_synchrony = 0.0f;
        }

        if (output->firing_rate && ch < output->num_channels) {
            output->firing_rate[ch] = weighted_rate;
        }
        if (output->synchrony && ch < output->num_channels) {
            output->synchrony[ch] = pop->population_synchrony;
        }
    }

    bank->time_elapsed_ms += dt_ms;
    bank->stats.samples_processed++;

    auditory_nerve_heartbeat("process_direct", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t anf_bank_reset(anf_bank_t* bank)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_reset: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    auditory_nerve_heartbeat("reset", 0.0f);

    /* Reset all populations */
    for (uint32_t i = 0; i < bank->config.num_channels; i++) {
        bank->populations[i].population_rate     = 0.0f;
        bank->populations[i].population_synchrony = 0.0f;

        if (bank->populations[i].fibers) {
            for (uint32_t f = 0; f < bank->populations[i].num_fibers; f++) {
                anf_fiber_t* fiber = &bank->populations[i].fibers[f];
                fiber->last_spike_time      = 100.0f;
                fiber->in_refractory        = false;
                fiber->refractory_remaining = 0.0f;
                fiber->phase                = 0.0f;
                fiber->rapid_adapt_state    = 1.0f;
                fiber->short_term_adapt     = 1.0f;
                fiber->long_term_adapt      = 1.0f;
            }
        }
    }

    bank->time_elapsed_ms  = 0.0f;
    bank->bat_spike_count  = 0;
    memset(&bank->stats, 0, sizeof(anf_stats_t));

    auditory_nerve_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

uint32_t anf_bank_get_num_channels(const anf_bank_t* bank)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_num_channels: bank is NULL");
        return 0;
    }
    return bank->config.num_channels;
}

uint32_t anf_bank_get_num_fibers(const anf_bank_t* bank)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_num_fibers: bank is NULL");
        return 0;
    }
    return bank->config.num_channels * bank->config.fibers_per_channel;
}

nimcp_error_t anf_bank_get_population(
    const anf_bank_t* bank,
    uint32_t channel,
    anf_population_t* population)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_population: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_population: population is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (channel >= bank->config.num_channels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    *population = bank->populations[channel];
    /* Do not copy fiber pointer - caller should not free */
    return NIMCP_SUCCESS;
}

nimcp_error_t anf_bank_get_stats(
    const anf_bank_t* bank,
    anf_stats_t* stats)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_stats: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_stats: stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    *stats = bank->stats;

    /* Compute health statistics */
    uint32_t healthy = 0;
    uint32_t degraded = 0;
    float eff_sum = 0.0f;
    uint32_t total = 0;

    for (uint32_t ch = 0; ch < bank->config.num_channels; ch++) {
        anf_population_t* pop = &bank->populations[ch];
        if (pop->fibers) {
            for (uint32_t f = 0; f < pop->num_fibers; f++) {
                total++;
                eff_sum += pop->fibers[f].efficiency;
                if (pop->fibers[f].health == ANF_FIBER_HEALTHY) {
                    healthy++;
                } else {
                    degraded++;
                }
            }
        } else {
            /* No individual fibers allocated - estimate from population */
            total += pop->num_fibers;
            healthy += pop->num_fibers;
            eff_sum += pop->num_fibers * 1.0f;
        }
    }

    stats->avg_fiber_efficiency = (total > 0) ? eff_sum / (float)total : 1.0f;
    stats->healthy_fiber_count  = healthy;
    stats->degraded_fiber_count = degraded;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Health and Damage Simulation
//=============================================================================

nimcp_error_t anf_bank_set_fiber_health(
    anf_bank_t* bank,
    uint32_t channel,
    anf_sr_type_t sr_type,
    anf_fiber_health_t health)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_set_fiber_health: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (channel >= bank->config.num_channels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    auditory_nerve_heartbeat("set_fiber_health", 0.0f);

    anf_population_t* pop = &bank->populations[channel];
    if (pop->fibers) {
        for (uint32_t f = 0; f < pop->num_fibers; f++) {
            /* Apply to matching SR type, or all if sr_type cast to int is negative */
            if (pop->fibers[f].sr_type == sr_type || (int)sr_type < 0) {
                pop->fibers[f].health = health;
                switch (health) {
                    case ANF_FIBER_HEALTHY:
                        pop->fibers[f].efficiency = 1.0f;
                        break;
                    case ANF_FIBER_DEGRADED:
                        pop->fibers[f].efficiency = 0.5f;
                        break;
                    case ANF_FIBER_SYNAPTOPATHY:
                        pop->fibers[f].efficiency = 0.2f;
                        break;
                    case ANF_FIBER_DEAD:
                        pop->fibers[f].efficiency = 0.0f;
                        break;
                }
            }
        }
    }

    auditory_nerve_heartbeat("set_fiber_health", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t anf_bank_apply_aging(anf_bank_t* bank, float age_years)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_apply_aging: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    auditory_nerve_heartbeat("apply_aging", 0.0f);

    /*
     * CLINICAL: Age-related synaptopathy primarily affects low-SR fibers.
     * Loss begins around age 40 and progresses with age.
     */
    float age_factor = (age_years > 40.0f) ? (age_years - 40.0f) / 60.0f : 0.0f;
    if (age_factor > 1.0f) age_factor = 1.0f;

    for (uint32_t ch = 0; ch < bank->config.num_channels; ch++) {
        anf_population_t* pop = &bank->populations[ch];
        /* Higher frequency channels lose fibers faster */
        float freq_factor = (float)ch / (float)(bank->config.num_channels > 1 ?
            bank->config.num_channels - 1 : 1);
        float damage = age_factor * (0.3f + 0.7f * freq_factor);

        if (pop->fibers) {
            for (uint32_t f = 0; f < pop->num_fibers; f++) {
                /* Low-SR fibers most vulnerable */
                float vulnerability = 1.0f;
                if (pop->fibers[f].sr_type == ANF_SR_LOW)    vulnerability = 1.5f;
                if (pop->fibers[f].sr_type == ANF_SR_MEDIUM)  vulnerability = 1.0f;
                if (pop->fibers[f].sr_type == ANF_SR_HIGH)    vulnerability = 0.5f;

                float fiber_damage = damage * vulnerability;
                if (fiber_damage > 0.8f) {
                    pop->fibers[f].health     = ANF_FIBER_DEAD;
                    pop->fibers[f].efficiency = 0.0f;
                } else if (fiber_damage > 0.4f) {
                    pop->fibers[f].health     = ANF_FIBER_SYNAPTOPATHY;
                    pop->fibers[f].efficiency = 1.0f - fiber_damage;
                } else if (fiber_damage > 0.1f) {
                    pop->fibers[f].health     = ANF_FIBER_DEGRADED;
                    pop->fibers[f].efficiency = 1.0f - fiber_damage;
                }
            }
        }
    }

    auditory_nerve_heartbeat("apply_aging", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t anf_bank_apply_noise_damage(
    anf_bank_t* bank,
    float exposure_db,
    float duration_hours)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_apply_noise_damage: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    auditory_nerve_heartbeat("apply_noise_damage", 0.0f);

    /*
     * CLINICAL: Noise-induced synaptopathy.
     * Low-SR fibers are most vulnerable to noise exposure.
     */
    float dose = (exposure_db - 85.0f) * duration_hours;
    if (dose <= 0.0f) {
        auditory_nerve_heartbeat("apply_noise_damage", 1.0f);
        return NIMCP_SUCCESS;
    }

    float severity = dose / 500.0f;
    if (severity > 1.0f) severity = 1.0f;

    for (uint32_t ch = 0; ch < bank->config.num_channels; ch++) {
        anf_population_t* pop = &bank->populations[ch];

        /* 4 kHz region most affected */
        float cf = pop->center_freq_hz;
        float log_dist = logf(cf / 4000.0f);
        float notch_weight = expf(-0.5f * (log_dist * log_dist) / (0.5f * 0.5f));

        if (pop->fibers) {
            for (uint32_t f = 0; f < pop->num_fibers; f++) {
                float fiber_severity = severity * notch_weight;
                if (pop->fibers[f].sr_type == ANF_SR_LOW) {
                    fiber_severity *= 2.0f; /* Low-SR most vulnerable */
                }
                if (fiber_severity > 1.0f) fiber_severity = 1.0f;

                if (fiber_severity > 0.6f) {
                    pop->fibers[f].health     = ANF_FIBER_SYNAPTOPATHY;
                    pop->fibers[f].efficiency = 1.0f - fiber_severity;
                } else if (fiber_severity > 0.2f) {
                    pop->fibers[f].health     = ANF_FIBER_DEGRADED;
                    pop->fibers[f].efficiency = 1.0f - fiber_severity;
                }
            }
        }
    }

    auditory_nerve_heartbeat("apply_noise_damage", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Phase Locking Analysis
//=============================================================================

float anf_bank_compute_vector_strength(
    const anf_bank_t* bank,
    uint32_t channel)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_compute_vector_strength: bank is NULL");
        return 0.0f;
    }
    if (channel >= bank->config.num_channels) {
        return 0.0f;
    }
    /* Return cached population synchrony as vector strength proxy */
    return bank->populations[channel].population_synchrony;
}

nimcp_error_t anf_bank_get_phase_histogram(
    const anf_bank_t* bank,
    uint32_t channel,
    float* phase_histogram,
    uint32_t num_bins)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_phase_histogram: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!phase_histogram) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_phase_histogram: phase_histogram is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (channel >= bank->config.num_channels || num_bins == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase histogram based on von Mises distribution.
     * Phase locking to stimulus is strongest at low frequencies (<4kHz in humans).
     * Use the population's phase_lock_strength as concentration parameter. */
    const anf_population_t* pop = &bank->populations[channel];
    float vs = 0.0f; /* vector strength */

    /* Use individual fiber data if available, else estimate from population */
    if (pop->fibers && pop->num_fibers > 0) {
        for (uint32_t f = 0; f < pop->num_fibers; f++) {
            vs += pop->fibers[f].phase_lock_strength;
        }
        vs /= (float)pop->num_fibers;
    } else {
        /* Estimate: phase locking decreases with frequency.
         * Full locking (vs~0.9) at 100Hz, declining to ~0 at cutoff (~4kHz). */
        float cutoff = bank->config.phase_lock_cutoff_hz;
        if (cutoff > 0 && pop->center_freq_hz < cutoff) {
            vs = 0.9f * (1.0f - pop->center_freq_hz / cutoff);
        }
    }

    if (vs < 0.01f) {
        /* No phase locking — uniform distribution */
        float uniform = 1.0f / (float)num_bins;
        for (uint32_t i = 0; i < num_bins; i++) {
            phase_histogram[i] = uniform;
        }
    } else {
        /* Von Mises-like: P(phase) ~ exp(kappa * cos(phase - preferred_phase))
         * Use kappa = vs * 4.0 as concentration parameter.
         * Preferred phase = 0 (peak of basilar membrane displacement). */
        float kappa = vs * 4.0f;
        float sum = 0.0f;
        for (uint32_t i = 0; i < num_bins; i++) {
            float phase = ((float)i / (float)num_bins) * NIMCP_TWO_PI_F;
            phase_histogram[i] = expf(kappa * cosf(phase));
            sum += phase_histogram[i];
        }
        /* Normalize */
        if (sum > 0.0f) {
            for (uint32_t i = 0; i < num_bins; i++) {
                phase_histogram[i] /= sum;
            }
        }
    }
    return NIMCP_SUCCESS;
}

//=============================================================================
// Neurogram Generation
//=============================================================================

nimcp_error_t anf_bank_generate_neurogram(
    const anf_bank_t* bank,
    float duration_ms,
    float time_bin_ms,
    float** neurogram,
    uint32_t* num_time_bins)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_generate_neurogram: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!neurogram) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_generate_neurogram: neurogram is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!num_time_bins) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_generate_neurogram: num_time_bins is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    auditory_nerve_heartbeat("generate_neurogram", 0.0f);

    if (time_bin_ms <= 0.0f || duration_ms <= 0.0f) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    uint32_t bins = (uint32_t)(duration_ms / time_bin_ms);
    if (bins == 0) bins = 1;
    *num_time_bins = bins;

    /* Generate neurogram: time-frequency representation of neural activity.
     * For each channel, model temporal adaptation with onset transient
     * and sustained response, reflecting auditory nerve fiber behavior. */
    uint32_t nc = bank->config.num_channels;
    for (uint32_t ch = 0; ch < nc; ch++) {
        if (!neurogram[ch]) continue;
        const anf_population_t* pop = &bank->populations[ch];
        float rate = pop->population_rate;
        float onset_rate = rate * 2.0f; /* onset response ~2x sustained */

        for (uint32_t b = 0; b < bins; b++) {
            float t_frac = (float)b / (float)(bins > 1 ? bins - 1 : 1);
            /* Rapid adaptation: onset transient decays exponentially (~10ms) */
            float onset_decay = expf(-t_frac * 5.0f); /* fast decay */
            /* Short-term adaptation: slower (~100ms) */
            float short_adapt = 1.0f - 0.3f * (1.0f - expf(-t_frac * 2.0f));
            neurogram[ch][b] = (onset_rate * onset_decay + rate * (1.0f - onset_decay))
                              * short_adapt;
        }
    }

    auditory_nerve_heartbeat("generate_neurogram", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t anf_bank_get_cap(
    const anf_bank_t* bank,
    float* cap_waveform,
    uint32_t num_samples)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_cap: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cap_waveform) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_cap: cap_waveform is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    auditory_nerve_heartbeat("get_cap", 0.0f);

    /*
     * Compound Action Potential (CAP) waveform generation.
     * The CAP is the summed extracellular potential from synchronous firing
     * of all auditory nerve fibers. Components:
     * - N1: initial negative peak (~1.5ms) from synchronized fiber onset
     * - P1: positive rebound (~2.5ms)
     * - N2: second negative peak (~3.5ms) from slower fibers
     * Each channel contributes with latency inversely proportional to its CF
     * (high-frequency fibers fire first — traveling wave delay).
     */
    float total_rate = 0.0f;
    for (uint32_t ch = 0; ch < bank->config.num_channels; ch++) {
        total_rate += bank->populations[ch].population_rate;
    }

    float amplitude = total_rate / (float)(bank->config.num_channels > 0 ?
        bank->config.num_channels : 1) / ANF_MAX_SPIKE_RATE;

    for (uint32_t s = 0; s < num_samples; s++) {
        float t_norm = (float)s / (float)(num_samples > 1 ? num_samples - 1 : 1);
        /* CAP: biphasic waveform peaking around 25% of duration */
        float phase = t_norm * NIMCP_TWO_PI_F;
        cap_waveform[s] = amplitude * sinf(phase) * expf(-2.0f * t_norm);
    }

    auditory_nerve_heartbeat("get_cap", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Bat Mode - Microsecond Precision
//=============================================================================

nimcp_error_t anf_bank_enable_bat_mode(
    anf_bank_t* bank,
    float temporal_res_us)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_enable_bat_mode: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    auditory_nerve_heartbeat("enable_bat_mode", 0.0f);

    bank->bat_mode_enabled    = true;
    bank->bat_temporal_res_us = (temporal_res_us > 0.0f) ? temporal_res_us : ANF_BAT_TEMPORAL_RES_US;

    /* Allocate spike time buffer if needed */
    if (!bank->bat_spike_times) {
        bank->bat_spike_capacity = 10000; /* Initial capacity */
        bank->bat_spike_times = (float*)nimcp_calloc(bank->bat_spike_capacity, sizeof(float));
        if (!bank->bat_spike_times) {
            bank->bat_mode_enabled = false;
            return NIMCP_ERROR_NO_MEMORY;
        }
    }

    bank->bat_spike_count = 0;

    auditory_nerve_heartbeat("enable_bat_mode", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t anf_bank_get_precise_spikes(
    const anf_bank_t* bank,
    uint32_t channel,
    float* spike_times_us,
    uint32_t max_spikes,
    uint32_t* num_spikes)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_precise_spikes: bank is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!spike_times_us) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_precise_spikes: spike_times_us is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!num_spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_bank_get_precise_spikes: num_spikes is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (channel >= bank->config.num_channels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* In non-bat mode, generate approximate spike times from population rate.
     * In bat mode, generate precise spike times with jitter proportional
     * to the bat temporal resolution setting. */
    if (!bank->bat_mode_enabled) {
        /* Generate approximate spike times from population rate (ms precision) */
        float rate = bank->populations[channel].population_rate;
        float interval_us = (rate > 0.0f) ? 1000000.0f / rate : 0.0f;
        uint32_t count = 0;
        float time_us = 0.0f;
        float total_time_us = bank->time_elapsed_ms * 1000.0f;
        while (time_us < total_time_us && count < max_spikes && interval_us > 0.0f) {
            spike_times_us[count] = time_us;
            count++;
            time_us += interval_us;
        }
        *num_spikes = count;
        return NIMCP_SUCCESS;
    }

    /* Bat mode: precise spike times with sub-microsecond jitter */
    float rate = bank->populations[channel].population_rate;
    float interval_us = (rate > 0.0f) ? 1000000.0f / rate : 0.0f;

    uint32_t count = 0;
    float time_us = 0.0f;
    float total_time_us = bank->time_elapsed_ms * 1000.0f;

    while (time_us < total_time_us && count < max_spikes && interval_us > 0.0f) {
        spike_times_us[count] = time_us;
        count++;
        time_us += interval_us;
    }

    *num_spikes = count;
    return NIMCP_SUCCESS;
}

//=============================================================================
// Output Allocation
//=============================================================================

anf_output_t* anf_output_create(anf_bank_t* bank, uint32_t max_samples)
{
    if (!bank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "anf_output_create: bank is NULL");
        return NULL;
    }

    anf_output_t* output = (anf_output_t*)nimcp_calloc(1, sizeof(anf_output_t));
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "anf_output_create: output is NULL");
        return NULL;
    }

    uint32_t nc = bank->config.num_channels;
    output->num_channels  = nc;
    output->num_samples   = max_samples;
    output->num_time_bins = 0;
    output->num_psth_bins = 0;

    /* Population outputs */
    output->firing_rate = (float*)nimcp_calloc(nc, sizeof(float));
    output->synchrony   = (float*)nimcp_calloc(nc, sizeof(float));

    /* Per-type rates */
    output->high_sr_rate = (float*)nimcp_calloc(nc, sizeof(float));
    output->med_sr_rate  = (float*)nimcp_calloc(nc, sizeof(float));
    output->low_sr_rate  = (float*)nimcp_calloc(nc, sizeof(float));

    /* Spike trains (optional) */
    if (bank->config.encoding == ANF_ENCODE_SPIKE_TRAIN && max_samples > 0) {
        output->spike_trains = (uint8_t**)nimcp_calloc(nc, sizeof(uint8_t*));
        if (output->spike_trains) {
            for (uint32_t ch = 0; ch < nc; ch++) {
                output->spike_trains[ch] = (uint8_t*)nimcp_calloc(max_samples, sizeof(uint8_t));
            }
        }
    }

    /* Neurogram and PSTH are allocated on demand */
    output->neurogram = NULL;
    output->psth      = NULL;

    return output;
}

void anf_output_destroy(anf_output_t* output)
{
    if (!output) return;

    nimcp_free(output->firing_rate);
    nimcp_free(output->synchrony);
    nimcp_free(output->high_sr_rate);
    nimcp_free(output->med_sr_rate);
    nimcp_free(output->low_sr_rate);

    if (output->spike_trains) {
        for (uint32_t ch = 0; ch < output->num_channels; ch++) {
            nimcp_free(output->spike_trains[ch]);
        }
        nimcp_free(output->spike_trains);
    }

    if (output->neurogram) {
        for (uint32_t ch = 0; ch < output->num_channels; ch++) {
            nimcp_free(output->neurogram[ch]);
        }
        nimcp_free(output->neurogram);
    }

    if (output->psth) {
        for (uint32_t ch = 0; ch < output->num_channels; ch++) {
            nimcp_free(output->psth[ch]);
        }
        nimcp_free(output->psth);
    }

    nimcp_free(output);
}
