/**
 * @file nimcp_audio_dsp_bridge.c
 * @brief Audio DSP Bridge — proper FFT-based mel extraction for brain audio path
 */

#include "cognitive/physics/nimcp_audio_dsp_bridge.h"
#include "cognitive/math/nimcp_dsp.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "AUDIO_DSP"

/* ============================================================================
 * Public API
 * ============================================================================ */

audio_dsp_config_t audio_dsp_default_config(void) {
    return (audio_dsp_config_t){
        .fft_size = 512,
        .hop_size = 256,
        .num_mel_bins = 128,
        .num_mfcc = 13,
        .sample_rate = 16000.0,
        .output_type = AUDIO_DSP_OUTPUT_MEL_POWER,
        .pre_emphasis = true,
        .pre_emphasis_coeff = 0.97,
        .normalize = true,
    };
}

audio_dsp_bridge_t* audio_dsp_bridge_create(const audio_dsp_config_t* config) {
    audio_dsp_config_t cfg = config ? *config : audio_dsp_default_config();

    audio_dsp_bridge_t* b = nimcp_calloc(1, sizeof(*b));
    if (!b) return NULL;

    b->config = cfg;

    /* Create DSP engine */
    dsp_config_t dsp_cfg = dsp_default_config();
    dsp_cfg.default_fft_size = cfg.fft_size;
    dsp_cfg.default_hop_size = cfg.hop_size;
    dsp_cfg.default_sample_rate = cfg.sample_rate;
    b->dsp = dsp_create(&dsp_cfg);

    /* Allocate scratch buffers */
    uint32_t num_bins = cfg.fft_size / 2 + 1;
    b->frame_buffer = nimcp_calloc(cfg.fft_size, sizeof(double));
    b->power_spectrum = nimcp_calloc(num_bins, sizeof(double));
    b->mel_energies = nimcp_calloc(cfg.num_mel_bins, sizeof(double));
    b->output_buffer = nimcp_calloc(cfg.num_mel_bins, sizeof(float));

    if (!b->dsp || !b->frame_buffer || !b->power_spectrum ||
        !b->mel_energies || !b->output_buffer) {
        audio_dsp_bridge_destroy(b);
        return NULL;
    }

    b->initialized = true;
    LOG_INFO(LOG_TAG, "Audio DSP bridge: fft=%u, hop=%u, mel=%u, sr=%.0f, "
             "output=%s, pre_emph=%s",
             cfg.fft_size, cfg.hop_size, cfg.num_mel_bins, cfg.sample_rate,
             cfg.output_type == AUDIO_DSP_OUTPUT_MFCC ? "MFCC" :
             cfg.output_type == AUDIO_DSP_OUTPUT_MEL_POWER ? "mel_power" : "mel_raw",
             cfg.pre_emphasis ? "yes" : "no");
    return b;
}

void audio_dsp_bridge_destroy(audio_dsp_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->dsp) dsp_destroy(bridge->dsp);
    nimcp_free(bridge->frame_buffer);
    nimcp_free(bridge->power_spectrum);
    nimcp_free(bridge->mel_energies);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge);
}

int audio_dsp_process(audio_dsp_bridge_t* bridge,
                        const float* samples, uint32_t num_samples,
                        float* out_features) {
    if (!bridge || !samples || !out_features || num_samples == 0)
        return -1;

    uint32_t fft_size = bridge->config.fft_size;
    uint32_t hop_size = bridge->config.hop_size;
    uint32_t num_mel = bridge->config.num_mel_bins;
    uint32_t num_bins = fft_size / 2 + 1;
    double sr = bridge->config.sample_rate;

    /* Convert float32 input to double and apply pre-emphasis if enabled */
    uint32_t buf_len = num_samples < fft_size * 4 ? fft_size * 4 : num_samples;
    double* signal = nimcp_calloc(buf_len, sizeof(double));
    if (!signal) return -1;

    for (uint32_t i = 0; i < num_samples; i++)
        signal[i] = (double)samples[i];

    if (bridge->config.pre_emphasis && num_samples > 1) {
        /* y[n] = x[n] - α·x[n-1] — boosts high frequencies */
        for (uint32_t i = num_samples - 1; i > 0; i--)
            signal[i] -= bridge->config.pre_emphasis_coeff * signal[i - 1];
    }

    /* Process frames: window → FFT → power → mel, then average across frames */
    memset(bridge->mel_energies, 0, num_mel * sizeof(double));
    uint32_t num_frames = 0;

    double rms_total = 0;
    double centroid_total = 0;
    double peak_freq = 0;
    double peak_power = 0;

    for (uint32_t start = 0; start + fft_size <= num_samples; start += hop_size) {
        /* Copy frame and apply Hamming window */
        for (uint32_t i = 0; i < fft_size; i++)
            bridge->frame_buffer[i] = signal[start + i];
        dsp_apply_window(bridge->frame_buffer, fft_size, DSP_WINDOW_HAMMING);

        /* FFT → power spectrum */
        dsp_complex_t* spectrum = nimcp_calloc(fft_size, sizeof(dsp_complex_t));
        if (!spectrum) { nimcp_free(signal); return -1; }

        for (uint32_t i = 0; i < fft_size; i++) {
            spectrum[i].re = bridge->frame_buffer[i];
            spectrum[i].im = 0.0;
        }
        dsp_fft(spectrum, fft_size);

        /* Power spectrum: |X[k]|² */
        for (uint32_t k = 0; k < num_bins; k++) {
            bridge->power_spectrum[k] = spectrum[k].re * spectrum[k].re
                                       + spectrum[k].im * spectrum[k].im;
        }

        /* Spectral analysis stats */
        double frame_rms = 0, weighted_freq = 0, total_power = 0;
        for (uint32_t k = 0; k < num_bins; k++) {
            double freq = (double)k * sr / (double)fft_size;
            double power = bridge->power_spectrum[k];
            frame_rms += power;
            weighted_freq += freq * power;
            total_power += power;
            if (power > peak_power) { peak_power = power; peak_freq = freq; }
        }
        rms_total += frame_rms / (double)num_bins;
        if (total_power > 1e-20)
            centroid_total += weighted_freq / total_power;

        /* Mel filterbank */
        double frame_mel[128];
        dsp_mel_filterbank(bridge->power_spectrum, num_bins, sr,
                             frame_mel, num_mel);

        /* Accumulate across frames */
        for (uint32_t m = 0; m < num_mel; m++)
            bridge->mel_energies[m] += frame_mel[m];

        nimcp_free(spectrum);
        num_frames++;
    }

    /* Handle short signals (< fft_size): pad and do one frame */
    if (num_frames == 0) {
        for (uint32_t i = 0; i < fft_size; i++)
            bridge->frame_buffer[i] = (i < num_samples) ? signal[i] : 0.0;
        dsp_apply_window(bridge->frame_buffer, fft_size, DSP_WINDOW_HAMMING);

        dsp_complex_t* spectrum = nimcp_calloc(fft_size, sizeof(dsp_complex_t));
        if (spectrum) {
            for (uint32_t i = 0; i < fft_size; i++) {
                spectrum[i].re = bridge->frame_buffer[i];
                spectrum[i].im = 0.0;
            }
            dsp_fft(spectrum, fft_size);
            for (uint32_t k = 0; k < num_bins; k++)
                bridge->power_spectrum[k] = spectrum[k].re * spectrum[k].re
                                           + spectrum[k].im * spectrum[k].im;
            dsp_mel_filterbank(bridge->power_spectrum, num_bins, sr,
                                 bridge->mel_energies, num_mel);
            nimcp_free(spectrum);
            num_frames = 1;
        }
    }

    nimcp_free(signal);

    /* Average across frames */
    if (num_frames > 1) {
        for (uint32_t m = 0; m < num_mel; m++)
            bridge->mel_energies[m] /= (double)num_frames;
    }

    /* Apply log (mel power) or DCT (MFCC) */
    if (bridge->config.output_type == AUDIO_DSP_OUTPUT_MEL_POWER ||
        bridge->config.output_type == AUDIO_DSP_OUTPUT_MFCC) {
        for (uint32_t m = 0; m < num_mel; m++) {
            double e = bridge->mel_energies[m];
            bridge->mel_energies[m] = (e > 1e-20) ? log(e) : -46.05;  /* log(1e-20) */
        }
    }

    if (bridge->config.output_type == AUDIO_DSP_OUTPUT_MFCC) {
        /* DCT-II to get MFCC from log mel energies */
        uint32_t n_mfcc = bridge->config.num_mfcc;
        if (n_mfcc > num_mel) n_mfcc = num_mel;
        double pi_over_mel = 3.14159265358979323846 / (double)num_mel;
        for (uint32_t k = 0; k < n_mfcc; k++) {
            double sum = 0;
            for (uint32_t m = 0; m < num_mel; m++)
                sum += bridge->mel_energies[m] * cos(pi_over_mel * ((double)m + 0.5) * (double)k);
            /* Store MFCC in first n_mfcc positions, zero rest */
            bridge->mel_energies[k] = sum;
        }
        for (uint32_t k = n_mfcc; k < num_mel; k++)
            bridge->mel_energies[k] = 0.0;
    }

    /* Normalize to [0,1] if requested */
    if (bridge->config.normalize) {
        double max_val = -1e30, min_val = 1e30;
        for (uint32_t m = 0; m < num_mel; m++) {
            if (bridge->mel_energies[m] > max_val) max_val = bridge->mel_energies[m];
            if (bridge->mel_energies[m] < min_val) min_val = bridge->mel_energies[m];
        }
        double range = max_val - min_val;
        if (range > 1e-10) {
            for (uint32_t m = 0; m < num_mel; m++)
                bridge->mel_energies[m] = (bridge->mel_energies[m] - min_val) / range;
        }
    }

    /* Convert to float32 output */
    for (uint32_t m = 0; m < num_mel; m++)
        out_features[m] = (float)bridge->mel_energies[m];

    /* Update stats */
    bridge->stats.frames_processed += num_frames;
    bridge->stats.samples_processed += num_samples;
    bridge->stats.rms_level = sqrt(rms_total / (double)(num_frames > 0 ? num_frames : 1));
    bridge->stats.peak_frequency = peak_freq;
    bridge->stats.spectral_centroid = centroid_total / (double)(num_frames > 0 ? num_frames : 1);

    return 0;
}

int audio_dsp_process_and_submit(audio_dsp_bridge_t* bridge,
                                   struct nimcp_brain_handle* brain,
                                   const float* samples, uint32_t num_samples) {
    if (!bridge || !brain) return -1;

    float features[128];
    int rc = audio_dsp_process(bridge, samples, num_samples, features);
    if (rc != 0) return rc;

    /* Submit to brain's staged sensory system */
    extern int nimcp_brain_submit_sensory_audio(
        struct nimcp_brain_handle* brain, const float* data, uint32_t size)
        __attribute__((weak));

    if (nimcp_brain_submit_sensory_audio)
        return nimcp_brain_submit_sensory_audio(brain, features, bridge->config.num_mel_bins);

    return -1;  /* API not available */
}

audio_dsp_stats_t audio_dsp_get_stats(const audio_dsp_bridge_t* bridge) {
    if (!bridge) return (audio_dsp_stats_t){0};
    return bridge->stats;
}
