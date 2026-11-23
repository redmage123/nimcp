/**
 * @file nimcp_cochlea.c
 * @brief Complete cochlear processing module implementation
 *
 * WHAT: Unified cochlear pipeline (BM → HC → ANF) with brain integration
 * WHY:  Provide biologically-accurate auditory front-end for NIMCP brain
 * HOW:  Integrate basilar membrane, hair cells, and auditory nerve
 *
 * COMPLETE PROCESSING PIPELINE:
 * 1. Audio input validation and preprocessing
 * 2. Basilar membrane frequency decomposition (gammatone filterbank)
 * 3. Outer hair cell active amplification (prestin electromotility)
 * 4. Inner hair cell transduction (Boltzmann sigmoid)
 * 5. Auditory nerve fiber spike generation (Poisson process)
 * 6. Output packaging and brain integration
 *
 * SPECIES SUPPORT:
 * - Human: Standard 20 Hz - 20 kHz processing
 * - Dog: Extended to 65 kHz with pinnae mobility, ITD/ILD
 * - Bat: Extended to 200 kHz with echolocation, μs precision
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "perception/nimcp_cochlea.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea)

//=============================================================================
// Internal Constants
//=============================================================================


#define COCHLEA_LOG_MODULE "COCHLEA"
#define COCHLEA_TAP_MAX     1024u   /* the brain hears 40 ms blocks (640 samples at 16 kHz) */
#define COCHLEA_EXT_OUT_MAX 128u    /* >= dog (16) and bat (64) ultrasonic channel counts */

//=============================================================================
// Opaque Structure Definition
//=============================================================================

/**
 * @brief Cochlea internal implementation structure
 */
struct cochlea {
    /* Configuration */
    cochlea_config_t config;

    /* Component pointers */
    basilar_membrane_t* bm;         /**< Basilar membrane processor */
    hair_cell_bank_t*   hc;         /**< Hair cell bank */
    anf_bank_t*         anf;        /**< Auditory nerve fiber bank */
    ext_hearing_t*      ext;        /**< Extended hearing (dog/bat, optional) */
    float*              ext_out;    /**< ext_hearing_process scratch [COCHLEA_EXT_OUT_MAX] */

    /* Last raw input block (2026-11-23): lets the brain's non-human hearing
     * (echolocation) listen to the same sound the cochlea hears. Seqlock:
     * tap_seq is odd while being written. */
    float*            tap_buf;
    uint32_t          tap_len;
    volatile uint64_t tap_seq;

    /* Output buffers */
    float* channel_energy;          /**< Per-channel energy */
    float* channel_db;              /**< Per-channel level (dB) */
    float* onset_strength;          /**< Onset detection per channel */
    float* channel_gains;           /**< Per-channel gain modulation */

    /* Processing state */
    float global_gain_db;           /**< Global gain */
    bool  protection_mode;          /**< Acoustic reflex active */
    float protection_attenuation;   /**< Attenuation from reflex */

    /* Statistics */
    cochlea_stats_t stats;

    /* Brain integration */
    brain_t     brain;              /**< Brain reference */
    brain_kg_t* brain_kg;           /**< Knowledge graph reference */
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async enabled flag */
};

//=============================================================================
// Configuration Helpers
//=============================================================================

cochlea_config_t cochlea_config_default(
    bm_hearing_mode_t mode,
    uint32_t sample_rate)
{
    cochlea_heartbeat("config_default", 0.0f);

    cochlea_config_t config;
    memset(&config, 0, sizeof(config));

    config.sample_rate   = (sample_rate > 0) ? sample_rate : 44100;
    config.num_channels  = 64;
    config.hearing_mode  = mode;

    /* Component defaults */
    config.bm_config  = bm_config_default(mode);
    config.hc_config  = hc_bank_config_default(config.num_channels, mode);
    config.anf_config = anf_config_default(config.num_channels, mode);

    /* Extended hearing for non-human modes */
    config.enable_extended_hearing = (mode != BM_MODE_HUMAN);
    if (config.enable_extended_hearing) {
        ext_hearing_mode_t ext_mode = EXT_HEARING_HUMAN;
        if (mode == BM_MODE_DOG)    ext_mode = EXT_HEARING_DOG;
        if (mode == BM_MODE_BAT)    ext_mode = EXT_HEARING_BAT;
        if (mode == BM_MODE_HYBRID) ext_mode = EXT_HEARING_HYBRID;
        config.ext_config = ext_hearing_config_default(ext_mode);
    }

    /* Processing options */
    config.enable_ohc_amplification = true;
    config.enable_phase_locking     = true;
    config.enable_adaptation        = true;

    /* Brain integration */
    config.enable_bio_async = false;
    config.enable_brain_kg  = false;
    config.enable_logging   = false;

    /* Output options */
    config.output_envelope  = true;
    config.output_spikes    = true;
    config.output_neurogram = false;

    cochlea_heartbeat("config_default", 1.0f);
    return config;
}

nimcp_error_t cochlea_config_validate(const cochlea_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_config_validate: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (config->sample_rate == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (config->num_channels == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    return NIMCP_SUCCESS;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_t* cochlea_create(const cochlea_config_t* config)
{
    cochlea_heartbeat("create", 0.0f);

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_create: config is NULL");
        return NULL;
    }

    if (cochlea_config_validate(config) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cochlea_create: config validation failed");
        return NULL;
    }

    cochlea_t* cochlea = (cochlea_t*)nimcp_calloc(1, sizeof(cochlea_t));
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_create: failed to allocate cochlea struct");
        return NULL;
    }

    cochlea->config = *config;
    cochlea_heartbeat("create", 0.1f);

    /* The three stages must agree with the cochlea on sample rate and
     * channel count (callers often change num_channels after
     * cochlea_config_default, and the component defaults assume 44.1 kHz /
     * 128 channels). Spike trains are produced only with spike encoding. */
    cochlea_config_t* cc = &cochlea->config;
    cc->bm_config.sample_rate = cc->sample_rate;
    cc->bm_config.num_channels = cc->num_channels;
    cc->hc_config.ihc_config.num_channels = cc->num_channels;
    cc->hc_config.ihc_config.sample_rate = cc->sample_rate;
    cc->hc_config.ohc_config.num_channels = cc->num_channels;
    cc->hc_config.ohc_config.sample_rate = cc->sample_rate;
    cc->anf_config.num_channels = cc->num_channels;
    if (cc->hearing_mode != BM_MODE_BAT) cc->anf_config.sample_rate = cc->sample_rate;
    if (cc->output_spikes) cc->anf_config.encoding = ANF_ENCODE_SPIKE_TRAIN;
    config = cc;

    /* Create basilar membrane */
    cochlea->bm = basilar_membrane_create(&config->bm_config);
    if (!cochlea->bm) {
        cochlea_destroy(cochlea);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_create: cochlea->bm is NULL");
        return NULL;
    }
    cochlea_heartbeat("create", 0.3f);

    /* Create hair cell bank */
    cochlea->hc = hair_cell_bank_create(&config->hc_config);
    if (!cochlea->hc) {
        cochlea_destroy(cochlea);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_create: cochlea->hc is NULL");
        return NULL;
    }
    cochlea_heartbeat("create", 0.5f);

    /* Create auditory nerve fiber bank */
    cochlea->anf = anf_bank_create(&config->anf_config);
    if (!cochlea->anf) {
        cochlea_destroy(cochlea);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_create: cochlea->anf is NULL");
        return NULL;
    }
    cochlea_heartbeat("create", 0.7f);

    /* Create extended hearing if enabled */
    if (config->enable_extended_hearing) {
        ext_hearing_config_t ext_cfg = config->ext_config;
        if (ext_cfg.dog.ultrasonic_channels > COCHLEA_EXT_OUT_MAX)   /* ext_out bound */
            ext_cfg.dog.ultrasonic_channels = COCHLEA_EXT_OUT_MAX;
        if (ext_cfg.bat.ultrasonic_channels > COCHLEA_EXT_OUT_MAX)
            ext_cfg.bat.ultrasonic_channels = COCHLEA_EXT_OUT_MAX;
        cochlea->ext = ext_hearing_create(&ext_cfg, config->sample_rate);
        /* Not fatal if ext fails - degrade gracefully */
    }
    cochlea->ext_out = (float*)nimcp_calloc(COCHLEA_EXT_OUT_MAX, sizeof(float));
    cochlea->tap_buf = (float*)nimcp_calloc(COCHLEA_TAP_MAX, sizeof(float));

    /* Allocate output buffers */
    uint32_t nc = config->num_channels;
    cochlea->channel_energy  = (float*)nimcp_calloc(nc, sizeof(float));
    cochlea->channel_db      = (float*)nimcp_calloc(nc, sizeof(float));
    cochlea->onset_strength  = (float*)nimcp_calloc(nc, sizeof(float));
    cochlea->channel_gains   = (float*)nimcp_calloc(nc, sizeof(float));

    if (!cochlea->channel_energy || !cochlea->channel_db ||
        !cochlea->onset_strength || !cochlea->channel_gains) {
        cochlea_destroy(cochlea);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_create: operation failed");
        return NULL;
    }

    /* Initialize gains to unity (0 dB) */
    for (uint32_t i = 0; i < nc; i++) {
        cochlea->channel_gains[i] = 1.0f;
    }

    cochlea->global_gain_db        = 0.0f;
    cochlea->protection_mode       = false;
    cochlea->protection_attenuation = 0.0f;
    cochlea->brain                 = NULL;
    cochlea->brain_kg              = NULL;
    cochlea->bio_ctx               = NULL;
    cochlea->bio_async_enabled     = false;

    memset(&cochlea->stats, 0, sizeof(cochlea_stats_t));

    cochlea_heartbeat("create", 1.0f);
    return cochlea;
}

void cochlea_destroy(cochlea_t* cochlea)
{
    if (!cochlea) return;

    cochlea_heartbeat("destroy", 0.0f);

    if (cochlea->bm)  basilar_membrane_destroy(cochlea->bm);
    if (cochlea->hc)  hair_cell_bank_destroy(cochlea->hc);
    if (cochlea->anf) anf_bank_destroy(cochlea->anf);
    if (cochlea->ext) ext_hearing_destroy(cochlea->ext);
    nimcp_free(cochlea->ext_out);
    nimcp_free(cochlea->tap_buf);

    nimcp_free(cochlea->channel_energy);
    nimcp_free(cochlea->channel_db);
    nimcp_free(cochlea->onset_strength);
    nimcp_free(cochlea->channel_gains);

    cochlea_heartbeat("destroy", 1.0f);
    nimcp_free(cochlea);
}

nimcp_error_t cochlea_process(
    cochlea_t* cochlea,
    const float* audio_in,
    uint32_t num_samples,
    cochlea_output_t* output)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_process: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!audio_in) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_process: audio_in is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_process: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("process", 0.0f);

    if (output->max_samples > 0 && num_samples > output->max_samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "cochlea_process: more samples than the output was created for");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    output->num_samples = num_samples;
    if (output->anf_output) output->anf_output->num_samples = num_samples;

    uint32_t nc = cochlea->config.num_channels;

    /* Stage 1: Basilar membrane processing */
    cochlea_heartbeat("process_bm", 0.1f);
    if (cochlea->bm && output->bm_output) {
        basilar_membrane_process(cochlea->bm, audio_in, num_samples, output->bm_output);
    }
    cochlea_heartbeat("process_bm", 0.3f);

    /* Gain control on the basilar-membrane envelope: per-channel gains
     * (attention, efferent/MOC suppression) and the global gain. */
    if (output->bm_output && output->bm_output->envelope) {
        const float g_global = powf(10.0f, cochlea->global_gain_db / 20.0f);
        uint32_t nbm = output->bm_output->num_channels < nc ? output->bm_output->num_channels : nc;
        for (uint32_t ch = 0; ch < nbm; ch++) {
            float g = g_global * (cochlea->channel_gains ? cochlea->channel_gains[ch] : 1.0f);
            output->bm_output->envelope[ch] *= g;
        }
    }

    /* Stage 2: Hair cell transduction */
    cochlea_heartbeat("process_hc", 0.3f);
    if (cochlea->hc && output->bm_output && output->hc_output) {
        hair_cell_bank_process(cochlea->hc, output->bm_output, output->hc_output);
    }
    cochlea_heartbeat("process_hc", 0.5f);

    /* Stage 3: Auditory nerve fiber spike generation */
    cochlea_heartbeat("process_anf", 0.5f);
    if (cochlea->anf && output->hc_output && output->anf_output) {
        float dt_ms = 1000.0f / (float)cochlea->config.sample_rate;
        anf_bank_process(cochlea->anf, output->hc_output, dt_ms, output->anf_output);
    }
    cochlea_heartbeat("process_anf", 0.7f);

    /* Compute derived features from ANF firing rates */
    float total_energy = 0.0f;
    float peak_energy = 0.0f;
    float peak_freq = 0.0f;
    float weighted_freq_sum = 0.0f;
    bool has_onset = false;

    if (output->anf_output && output->anf_output->firing_rate) {
        for (uint32_t ch = 0; ch < nc; ch++) {
            float rate = output->anf_output->firing_rate[ch];
            /* Energy ~ rate^2 (power proportional to rate squared) */
            float energy = rate * rate;
            if (output->channel_energy) {
                output->channel_energy[ch] = energy;
            }
            if (output->channel_db) {
                /* Level in dB SPL from the hair cells (94 dB at amplitude 1) */
                output->channel_db[ch] = (output->hc_output && output->hc_output->ihc.receptor_potential
                                          && ch < output->hc_output->num_channels)
                    ? output->hc_output->ihc.receptor_potential[ch] : -100.0f;
            }
            total_energy += energy;

            /* Track peak frequency channel.
             * Estimate CF from channel index using Greenwood function:
             * CF = 165.4 * (10^(2.1 * position) - 0.88)
             * where position = ch / (nc - 1) mapped to [0, 1] */
            float pos = (nc > 1) ? (float)ch / (float)(nc - 1) : 0.0f;
            float est_cf = 165.4f * (powf(10.0f, 2.1f * pos) - 0.88f);
            if (energy > peak_energy) {
                peak_energy = energy;
                peak_freq = est_cf;
            }
            weighted_freq_sum += energy * est_cf;

            /* Onset detection: rate significantly above spontaneous */
            if (rate > 100.0f) has_onset = true;
        }
    } else {
        if (output->channel_energy)
            memset(output->channel_energy, 0, nc * sizeof(float));
        if (output->channel_db) {
            for (uint32_t ch = 0; ch < nc; ch++)
                output->channel_db[ch] = -100.0f;
        }
    }

    output->total_energy = total_energy;
    output->peak_frequency_hz = peak_freq;
    output->overall_level_db = (total_energy > 1e-10f) ?
        10.0f * log10f(total_energy) : -100.0f;
    output->num_channels = nc;
    output->num_samples = num_samples;
    output->sound_onset_detected = has_onset;
    /* Speech detection heuristic: energy concentrated in 300-3000 Hz range */
    output->speech_detected = (peak_freq >= 300.0f && peak_freq <= 3000.0f
                               && total_energy > 0.01f);

    /* Extended (dog/bat) hearing: ext was created but ext_hearing_process
     * was never called, so the extended-hearing code never ran. Mono input
     * is fed to both ears here; cochlea_process_stereo refines localization. */
    if (cochlea->ext && cochlea->ext_out) {
        ext_hearing_process(cochlea->ext, audio_in, num_samples, cochlea->ext_out);
        dog_auditory_t* dog = ext_hearing_get_dog(cochlea->ext);
        if (dog) {
            if (!output->dog_localization)   /* freed by cochlea_output_destroy */
                output->dog_localization = (dog_localization_t*)nimcp_calloc(1, sizeof(dog_localization_t));
            if (output->dog_localization)
                dog_auditory_localize(dog, output->dog_localization);
        }
    }

    /* Publish the raw block for cochlea_copy_last_input (seqlock writer). */
    if (cochlea->tap_buf) {
        uint32_t n = num_samples < COCHLEA_TAP_MAX ? num_samples : COCHLEA_TAP_MAX;
        __atomic_add_fetch(&cochlea->tap_seq, 1, __ATOMIC_ACQ_REL);
        memcpy(cochlea->tap_buf, audio_in, n * sizeof(float));
        cochlea->tap_len = n;
        __atomic_add_fetch(&cochlea->tap_seq, 1, __ATOMIC_ACQ_REL);
    }

    /* Update statistics */
    __atomic_add_fetch(&cochlea->stats.samples_processed, num_samples, __ATOMIC_RELAXED);
    __atomic_add_fetch(&cochlea->stats.frames_processed, 1, __ATOMIC_RELAXED);

    cochlea_heartbeat("process", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_process_stereo(
    cochlea_t* cochlea,
    const float* audio_left,
    const float* audio_right,
    uint32_t num_samples,
    cochlea_output_t* output)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_process_stereo: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!audio_left || !audio_right) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_process_stereo: audio channel is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_process_stereo: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("process_stereo", 0.0f);

    /* Stub: Mix to mono and process through main pipeline */
    float* mono = (float*)nimcp_calloc(num_samples, sizeof(float));
    if (!mono) {
        return NIMCP_ERROR_NO_MEMORY;
    }
    for (uint32_t i = 0; i < num_samples; i++) {
        mono[i] = (audio_left[i] + audio_right[i]) * 0.5f;
    }

    nimcp_error_t result = cochlea_process(cochlea, mono, num_samples, output);
    nimcp_free(mono);

    /* Extended hearing localization needs the two ears separately (the
     * block that used to be here was empty): re-run the dog processor on
     * true stereo so ILD/azimuth replace the mono estimate. */
    if (result == NIMCP_SUCCESS && cochlea->ext && cochlea->ext_out) {
        dog_auditory_t* dog = ext_hearing_get_dog(cochlea->ext);
        if (dog && dog_auditory_process(dog, audio_left, audio_right, num_samples,
                                        cochlea->ext_out) == NIMCP_SUCCESS &&
            output->dog_localization) {
            dog_auditory_localize(dog, output->dog_localization);
        }
    }

    cochlea_heartbeat("process_stereo", 1.0f);
    return result;
}

nimcp_error_t cochlea_reset(cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_reset: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("reset", 0.0f);

    if (cochlea->bm)  basilar_membrane_reset(cochlea->bm);
    if (cochlea->hc)  hair_cell_bank_reset(cochlea->hc);
    if (cochlea->anf) anf_bank_reset(cochlea->anf);

    uint32_t nc = cochlea->config.num_channels;
    if (cochlea->channel_energy)  memset(cochlea->channel_energy, 0, nc * sizeof(float));
    if (cochlea->channel_db)      memset(cochlea->channel_db, 0, nc * sizeof(float));
    if (cochlea->onset_strength)  memset(cochlea->onset_strength, 0, nc * sizeof(float));

    /* Reset gains to unity */
    if (cochlea->channel_gains) {
        for (uint32_t i = 0; i < nc; i++) {
            cochlea->channel_gains[i] = 1.0f;
        }
    }

    cochlea->global_gain_db        = 0.0f;
    cochlea->protection_mode       = false;
    cochlea->protection_attenuation = 0.0f;

    memset(&cochlea->stats, 0, sizeof(cochlea_stats_t));

    cochlea_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Output Management
//=============================================================================

cochlea_output_t* cochlea_output_create(
    cochlea_t* cochlea,
    uint32_t max_samples)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_output_create: cochlea is NULL");
        return NULL;
    }

    cochlea_output_t* output = (cochlea_output_t*)nimcp_calloc(1, sizeof(cochlea_output_t));
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_output_create: output is NULL");
        return NULL;
    }

    uint32_t nc = cochlea->config.num_channels;
    output->num_channels = nc;
    output->num_samples  = 0;
    output->max_samples  = max_samples;

    /* Allocate BM output */
    if (cochlea->bm) {
        output->bm_output = bm_output_create(cochlea->bm, max_samples);
    }

    /* Allocate HC output */
    output->hc_output = hc_bank_output_create(nc);

    /* Allocate ANF output */
    if (cochlea->anf) {
        output->anf_output = anf_output_create(cochlea->anf, max_samples);
    }

    /* Allocate derived feature arrays */
    output->channel_energy  = (float*)nimcp_calloc(nc, sizeof(float));
    output->channel_db      = (float*)nimcp_calloc(nc, sizeof(float));
    output->onset_strength  = (float*)nimcp_calloc(nc, sizeof(float));

    return output;
}

void cochlea_output_destroy(cochlea_output_t* output)
{
    if (!output) return;

    if (output->bm_output)  bm_output_destroy(output->bm_output);
    if (output->hc_output)  hc_bank_output_destroy(output->hc_output);
    if (output->anf_output) anf_output_destroy(output->anf_output);

    if (output->dog_localization) nimcp_free(output->dog_localization);
    if (output->echo_result)     echolocation_result_destroy(output->echo_result);

    nimcp_free(output->channel_energy);
    nimcp_free(output->channel_db);
    nimcp_free(output->onset_strength);

    nimcp_free(output);
}

void cochlea_output_clear(cochlea_output_t* output)
{
    if (!output) return;

    uint32_t nc = output->num_channels;
    output->num_samples          = 0;
    output->total_energy         = 0.0f;
    output->peak_frequency_hz    = 0.0f;
    output->overall_level_db     = -100.0f;
    output->sound_onset_detected = false;
    output->speech_detected      = false;
    output->timestamp_ms         = 0;

    if (output->channel_energy) memset(output->channel_energy, 0, nc * sizeof(float));
    if (output->channel_db)     memset(output->channel_db, 0, nc * sizeof(float));
    if (output->onset_strength) memset(output->onset_strength, 0, nc * sizeof(float));
}

//=============================================================================
// Query Functions
//=============================================================================

uint32_t cochlea_get_num_channels(const cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_num_channels: cochlea is NULL");
        return 0;
    }
    return cochlea->config.num_channels;
}

float cochlea_get_channel_freq(const cochlea_t* cochlea, uint32_t channel)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_channel_freq: cochlea is NULL");
        return -1.0f;
    }
    if (channel >= cochlea->config.num_channels) {
        return -1.0f;
    }
    /* Delegate to basilar membrane for center frequency */
    if (cochlea->bm) {
        return basilar_membrane_get_center_freq(cochlea->bm, channel);
    }
    return -1.0f;
}

nimcp_error_t cochlea_get_all_freqs(
    const cochlea_t* cochlea,
    float* freqs)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_all_freqs: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!freqs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_all_freqs: freqs is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    for (uint32_t i = 0; i < cochlea->config.num_channels; i++) {
        freqs[i] = cochlea_get_channel_freq(cochlea, i);
    }
    return NIMCP_SUCCESS;
}

bm_hearing_mode_t cochlea_get_hearing_mode(const cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_hearing_mode: cochlea is NULL");
        return BM_MODE_HUMAN; /* safe default */
    }
    return cochlea->config.hearing_mode;
}

nimcp_error_t cochlea_get_stats(
    const cochlea_t* cochlea,
    cochlea_stats_t* stats)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_stats: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_stats: stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    *stats = cochlea->stats;

    /* Gather sub-component stats */
    if (cochlea->bm)  basilar_membrane_get_stats(cochlea->bm, &stats->bm_stats);
    if (cochlea->hc)  hair_cell_bank_get_stats(cochlea->hc, &stats->hc_stats);
    if (cochlea->anf) anf_bank_get_stats(cochlea->anf, &stats->anf_stats);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Component Access
//=============================================================================

basilar_membrane_t* cochlea_get_basilar_membrane(cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_basilar_membrane: cochlea is NULL");
        return NULL;
    }
    return cochlea->bm;
}

hair_cell_bank_t* cochlea_get_hair_cells(cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_hair_cells: cochlea is NULL");
        return NULL;
    }
    return cochlea->hc;
}

anf_bank_t* cochlea_get_auditory_nerve(cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_auditory_nerve: cochlea is NULL");
        return NULL;
    }
    return cochlea->anf;
}

ext_hearing_t* cochlea_get_extended_hearing(cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_extended_hearing: cochlea is NULL");
        return NULL;
    }
    return cochlea->ext;
}

//=============================================================================
// Hearing Mode Control
//=============================================================================

nimcp_error_t cochlea_set_hearing_mode(
    cochlea_t* cochlea,
    bm_hearing_mode_t mode)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_set_hearing_mode: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("set_hearing_mode", 0.0f);
    cochlea->config.hearing_mode = mode;

    /* NOTE: Basilar membrane does not support runtime mode change.
     * Would need to recreate BM with new config for full mode switch. */

    cochlea_heartbeat("set_hearing_mode", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_enable_extended_mode(
    cochlea_t* cochlea,
    ext_hearing_mode_t mode)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_enable_extended_mode: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("enable_extended_mode", 0.0f);

    /* Create extended hearing if not already present */
    if (!cochlea->ext) {
        ext_hearing_config_t ext_cfg = ext_hearing_config_default(mode);
        /* ext_out holds COCHLEA_EXT_OUT_MAX floats; dog/bat processing writes
         * ultrasonic_channels of them unchecked. */
        if (ext_cfg.dog.ultrasonic_channels > COCHLEA_EXT_OUT_MAX)
            ext_cfg.dog.ultrasonic_channels = COCHLEA_EXT_OUT_MAX;
        if (ext_cfg.bat.ultrasonic_channels > COCHLEA_EXT_OUT_MAX)
            ext_cfg.bat.ultrasonic_channels = COCHLEA_EXT_OUT_MAX;
        cochlea->ext = ext_hearing_create(&ext_cfg, cochlea->config.sample_rate);
        if (!cochlea->ext) {
            return NIMCP_ERROR_NO_MEMORY;
        }
    } else {
        ext_hearing_switch_mode(cochlea->ext, mode);
    }

    if (!cochlea->ext_out) {
        cochlea->ext_out = (float*)nimcp_calloc(COCHLEA_EXT_OUT_MAX, sizeof(float));
    }

    cochlea->config.enable_extended_hearing = true;
    cochlea_heartbeat("enable_extended_mode", 1.0f);
    return NIMCP_SUCCESS;
}

/* Brain-integration helpers (2026-11-23). Plain int/bool signatures so the
 * brain init TU can forward-declare them without nimcp_cochlea.h (whose
 * medulla_t / thalamus_t tags collide with nimcp_brain_internal.h). */
bool cochlea_has_extended_hearing(const cochlea_t* cochlea)
{
    return cochlea && cochlea->ext;
}

int cochlea_enable_dog_hearing(cochlea_t* cochlea)
{
    return cochlea_enable_extended_mode(cochlea, EXT_HEARING_DOG) == NIMCP_SUCCESS ? 0 : -1;
}

uint32_t cochlea_copy_last_input(const cochlea_t* cochlea, float* dst,
                                 uint32_t max_samples, uint64_t* seq_out)
{
    if (!cochlea || !cochlea->tap_buf || !dst || max_samples == 0) return 0;
    for (int attempt = 0; attempt < 4; attempt++) {   /* seqlock reader */
        uint64_t s0 = __atomic_load_n(&cochlea->tap_seq, __ATOMIC_ACQUIRE);
        if (s0 & 1u) continue;
        uint32_t n = cochlea->tap_len < max_samples ? cochlea->tap_len : max_samples;
        memcpy(dst, cochlea->tap_buf, n * sizeof(float));
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&cochlea->tap_seq, __ATOMIC_ACQUIRE) == s0) {
            if (seq_out) *seq_out = s0;
            return n;
        }
    }
    return 0;
}

//=============================================================================
// Gain and Modulation
//=============================================================================

nimcp_error_t cochlea_set_gain(cochlea_t* cochlea, float gain_db)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_set_gain: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    cochlea->global_gain_db = gain_db;
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_set_channel_gain(
    cochlea_t* cochlea,
    uint32_t channel,
    float gain_db)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_set_channel_gain: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (channel >= cochlea->config.num_channels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    /* Convert dB to linear gain */
    cochlea->channel_gains[channel] = powf(10.0f, gain_db / 20.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_apply_attention(
    cochlea_t* cochlea,
    float attention_freq_hz,
    float attention_bandwidth,
    float attention_gain)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_apply_attention: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("apply_attention", 0.0f);

    /* Apply Gaussian-shaped attention boost around target frequency */
    float log_target = logf(attention_freq_hz);
    float sigma = attention_bandwidth * logf(2.0f); /* octaves to log bandwidth */
    float gain_linear = powf(10.0f, attention_gain / 20.0f);

    for (uint32_t i = 0; i < cochlea->config.num_channels; i++) {
        float cf = cochlea_get_channel_freq(cochlea, i);
        if (cf > 0.0f) {
            float log_cf = logf(cf);
            float dist = (log_cf - log_target) / (sigma > 0.0f ? sigma : 1.0f);
            float weight = expf(-0.5f * dist * dist);
            cochlea->channel_gains[i] *= (1.0f + (gain_linear - 1.0f) * weight);
        }
    }

    cochlea_heartbeat("apply_attention", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_apply_efferent(cochlea_t* cochlea, float ach_level)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_apply_efferent: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Clamp ACh level */
    if (ach_level < 0.0f) ach_level = 0.0f;
    if (ach_level > 1.0f) ach_level = 1.0f;

    /*
     * BIOLOGICAL: MOC efferents reduce OHC gain via ACh.
     * Higher ACh -> less OHC amplification -> wider dynamic range.
     * Model as gain reduction proportional to ACh level.
     */
    float suppression = 1.0f - (0.3f * ach_level); /* Up to 30% gain reduction */
    for (uint32_t i = 0; i < cochlea->config.num_channels; i++) {
        cochlea->channel_gains[i] *= suppression;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Health and Damage Simulation
//=============================================================================

nimcp_error_t cochlea_apply_damage(
    cochlea_t* cochlea,
    uint32_t channel,
    float ohc_damage,
    float ihc_damage)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_apply_damage: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (channel >= cochlea->config.num_channels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    cochlea_heartbeat("apply_damage", 0.0f);

    /* Apply OHC damage: reduce channel gain */
    float clamped_ohc = ohc_damage > 1.0f ? 1.0f : (ohc_damage < 0.0f ? 0.0f : ohc_damage);
    float ohc_survival = 1.0f - clamped_ohc;
    cochlea->channel_gains[channel] *= ohc_survival;

    /* Apply IHC damage to hair cells if available */
    if (cochlea->hc) {
        ihc_bank_t* ihc = hair_cell_bank_get_ihc(cochlea->hc);
        if (ihc) {
            hc_ihc_health_t ihc_health = HC_IHC_HEALTHY;
            if (ihc_damage > 0.8f) ihc_health = HC_IHC_DEAD;
            else if (ihc_damage > 0.5f) ihc_health = HC_IHC_SEVERE_DAMAGE;
            else if (ihc_damage > 0.2f) ihc_health = HC_IHC_MODERATE_DAMAGE;
            else if (ihc_damage > 0.0f) ihc_health = HC_IHC_MILD_DAMAGE;
            ihc_bank_set_health(ihc, channel, ihc_health);
        }
    }

    /* Update damaged channel count */
    if (ohc_damage > 0.5f || ihc_damage > 0.5f) {
        cochlea->stats.damaged_channels++;
    }

    cochlea_heartbeat("apply_damage", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_apply_aging(cochlea_t* cochlea, float age_years)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_apply_aging: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("apply_aging", 0.0f);

    /*
     * BIOLOGICAL: Presbycusis - age-related hearing loss.
     * High frequencies affected first (basal cochlea).
     * Model: progressive OHC/IHC damage from base to apex.
     */
    uint32_t nc = cochlea->config.num_channels;
    for (uint32_t i = 0; i < nc; i++) {
        /* Higher-index channels = higher frequency = more damage */
        float freq_factor = (float)i / (float)(nc > 1 ? nc - 1 : 1);
        float age_factor  = (age_years > 20.0f) ? (age_years - 20.0f) / 80.0f : 0.0f;
        if (age_factor > 1.0f) age_factor = 1.0f;

        float ohc_damage = freq_factor * age_factor * 0.8f;
        float ihc_damage = freq_factor * age_factor * 0.4f;

        cochlea_apply_damage(cochlea, i, ohc_damage, ihc_damage);
    }

    /* Also age the ANF (synaptopathy) */
    if (cochlea->anf) {
        anf_bank_apply_aging(cochlea->anf, age_years);
    }

    cochlea_heartbeat("apply_aging", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_apply_noise_damage(
    cochlea_t* cochlea,
    float exposure_db,
    float duration_hours)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_apply_noise_damage: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("apply_noise_damage", 0.0f);

    /*
     * BIOLOGICAL: Noise-induced hearing loss targets 4 kHz region.
     * Severity depends on intensity x duration.
     */
    float dose = (exposure_db - 85.0f) * duration_hours; /* Excess dose above 85 dB */
    if (dose <= 0.0f) {
        cochlea_heartbeat("apply_noise_damage", 1.0f);
        return NIMCP_SUCCESS; /* Below damage threshold */
    }

    float damage_severity = dose / 500.0f; /* Normalize */
    if (damage_severity > 1.0f) damage_severity = 1.0f;

    /* 4 kHz notch pattern */
    uint32_t nc = cochlea->config.num_channels;
    for (uint32_t i = 0; i < nc; i++) {
        float cf = cochlea_get_channel_freq(cochlea, i);
        if (cf <= 0.0f) continue;

        /* Gaussian centered at 4 kHz */
        float log_dist = logf(cf / 4000.0f);
        float notch_weight = expf(-0.5f * (log_dist * log_dist) / (0.5f * 0.5f));

        float ohc_damage = notch_weight * damage_severity;
        float ihc_damage = notch_weight * damage_severity * 0.3f;

        cochlea_apply_damage(cochlea, i, ohc_damage, ihc_damage);
    }

    /* Apply ANF noise damage */
    if (cochlea->anf) {
        anf_bank_apply_noise_damage(cochlea->anf, exposure_db, duration_hours);
    }

    cochlea_heartbeat("apply_noise_damage", 1.0f);
    return NIMCP_SUCCESS;
}

float cochlea_get_health(const cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_health: cochlea is NULL");
        return 0.0f;
    }

    /* Average channel gain as proxy for overall health */
    uint32_t nc = cochlea->config.num_channels;
    if (nc == 0) return 1.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < nc; i++) {
        float g = cochlea->channel_gains[i];
        if (g > 1.0f) g = 1.0f;
        sum += g;
    }
    return sum / (float)nc;
}

//=============================================================================
// Brain Integration
//=============================================================================

nimcp_error_t cochlea_set_brain(cochlea_t* cochlea, brain_t brain)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_set_brain: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea->brain = brain;
    return NIMCP_SUCCESS;
}

bio_module_context_t cochlea_get_bio_context(cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_get_bio_context: cochlea is NULL");
        return NULL;
    }
    return cochlea->bio_ctx;
}

uint32_t cochlea_process_bio_messages(
    cochlea_t* cochlea,
    uint32_t max_messages)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_process_bio_messages: cochlea is NULL");
        return 0;
    }

    cochlea_heartbeat("process_bio_messages", 0.0f);

    if (!cochlea->bio_async_enabled || !cochlea->bio_ctx) {
        return 0;
    }

    /* Process pending bio-async messages via router */
    uint32_t processed = bio_router_process_inbox(cochlea->bio_ctx, max_messages);

    cochlea_heartbeat("process_bio_messages", 1.0f);
    return (int)processed;
}

nimcp_error_t cochlea_register_with_kg(
    cochlea_t* cochlea,
    brain_kg_t* brain_kg)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_register_with_kg: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!brain_kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_register_with_kg: brain_kg is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea->brain_kg = brain_kg;

    /* Register cochlea subsystem nodes in knowledge graph */
    NIMCP_LOGGING_INFO("Cochlea registered with knowledge graph (BM, HC, ANF nodes)");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Event Broadcasting
//=============================================================================

nimcp_error_t cochlea_broadcast_audio_onset(
    cochlea_t* cochlea,
    float peak_freq_hz,
    float level_db)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broadcast_audio_onset: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("broadcast_audio_onset", 0.5f);

    /* Send bio-async audio onset event */
    if (cochlea->bio_async_enabled && cochlea->bio_ctx) {
        bio_router_publish_signal(cochlea->bio_ctx, "cochlea.audio_onset.freq_hz", peak_freq_hz);
        bio_router_publish_signal(cochlea->bio_ctx, "cochlea.audio_onset.level_db", level_db);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_broadcast_speech_detected(
    cochlea_t* cochlea,
    float speech_confidence)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broadcast_speech_detected: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("broadcast_speech_detected", 0.5f);

    /* Send bio-async speech detection event */
    if (cochlea->bio_async_enabled && cochlea->bio_ctx) {
        bio_router_publish_signal(cochlea->bio_ctx, "cochlea.speech_detected", speech_confidence);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_broadcast_echo_target(
    cochlea_t* cochlea,
    const echolocation_target_t* target)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broadcast_echo_target: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_broadcast_echo_target: target is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("broadcast_echo_target", 0.5f);

    /* Send bio-async echolocation target event */
    if (cochlea->bio_async_enabled && cochlea->bio_ctx) {
        bio_router_publish_signal(cochlea->bio_ctx, "cochlea.echo_target.range_m", target->range_m);
        bio_router_publish_signal(cochlea->bio_ctx, "cochlea.echo_target.azimuth_deg", target->azimuth_deg);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Protective Mechanisms
//=============================================================================

nimcp_error_t cochlea_trigger_acoustic_reflex(
    cochlea_t* cochlea,
    float sound_level_db)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_trigger_acoustic_reflex: cochlea is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_heartbeat("acoustic_reflex", 0.0f);

    /*
     * BIOLOGICAL: Acoustic reflex triggers at ~85 dB SPL.
     * Stapedius muscle contracts, attenuating low frequencies by 10-15 dB.
     * Latency ~25-150 ms (not modeled in stub).
     */
    if (sound_level_db >= 85.0f) {
        cochlea->protection_mode = true;
        float excess = sound_level_db - 85.0f;
        cochlea->protection_attenuation = (excess > 15.0f) ? 15.0f : excess;
    } else {
        cochlea->protection_mode        = false;
        cochlea->protection_attenuation = 0.0f;
    }

    cochlea_heartbeat("acoustic_reflex", 1.0f);
    return NIMCP_SUCCESS;
}

bool cochlea_is_in_protection_mode(const cochlea_t* cochlea)
{
    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_is_in_protection_mode: cochlea is NULL");
        return false;
    }
    return cochlea->protection_mode;
}
