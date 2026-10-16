//=============================================================================
// nimcp_inferior_colliculus.c - Inferior Colliculus (Auditory Midbrain)
//=============================================================================

#include "core/brain/subcortical/nimcp_inferior_colliculus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(inferior_colliculus, MESH_ADAPTER_CATEGORY_SUBCORTICAL)

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct inferior_colliculus {
    uint32_t magic;                      /* 0x1C0LL1C5 */
    ic_config_t config;

    /* Tonotopic channels */
    uint32_t num_frequency_channels;     /* default 64 */
    float* center_frequencies;           /* Hz, log-spaced 20Hz-20kHz */
    float* channel_activations;          /* per channel [0-1] */

    /* ICC - central nucleus tonotopic response */
    float* icc_response;

    /* ICX - external nucleus spatial map */
    float* icx_response;

    /* Binaural cues */
    float* itd_map;                      /* interaural time difference per ch */
    float* ild_map;                      /* interaural level difference per ch */

    /* Spatial estimates */
    float azimuth_estimate;              /* -90 to +90 degrees */
    float elevation_estimate;            /* -45 to +45 degrees */

    /* Temporal response patterns */
    float* onset_response;               /* per channel */
    float* sustained_response;           /* per channel */
    float* prev_activation;              /* previous frame for onset detect */

    /* Statistics */
    uint32_t update_count;
    uint64_t last_update_us;

    /* Thread safety */
    nimcp_mutex_t* lock;
};

/* ============================================================================
 * HELPERS
 * ============================================================================ */

static uint64_t ic_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * Compute log-spaced center frequencies from min_hz to max_hz.
 */
static void ic_compute_center_frequencies(float* freqs, uint32_t n,
                                           float min_hz, float max_hz) {
    if (n == 0) return;
    if (n == 1) {
        freqs[0] = sqrtf(min_hz * max_hz);
        return;
    }
    float log_min = logf(min_hz);
    float log_max = logf(max_hz);
    float step = (log_max - log_min) / (float)(n - 1);
    for (uint32_t i = 0; i < n; i++) {
        freqs[i] = expf(log_min + step * (float)i);
    }
}

/**
 * Simple band energy estimation for a frequency channel.
 * Uses a crude bandpass approximation based on sample energy
 * weighted by proximity to center frequency.
 */
static float ic_band_energy(const float* samples, uint32_t num_samples,
                             float center_freq, float bandwidth) {
    if (!samples || num_samples == 0) return 0.0f;
    (void)center_freq;
    (void)bandwidth;

    /* Accumulate RMS energy of samples as proxy for band energy.
     * A real implementation would use FFT or filterbank; this is a
     * biologically-inspired placeholder that responds to amplitude. */
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < num_samples; i++) {
        float s = samples[i];
        if (!isfinite(s)) continue;
        sum_sq += (double)s * (double)s;
    }
    float rms = sqrtf((float)(sum_sq / (double)num_samples));
    return isfinite(rms) ? rms : 0.0f;
}

static float ic_clampf(float val, float lo, float hi) {
    if (!isfinite(val)) return lo;
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

ic_config_t ic_default_config(void) {
    ic_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.num_frequency_channels = IC_DEFAULT_NUM_CHANNELS;
    cfg.min_freq_hz = IC_MIN_FREQ_HZ;
    cfg.max_freq_hz = IC_MAX_FREQ_HZ;

    cfg.itd_weight = 0.6f;
    cfg.ild_weight = 0.4f;
    cfg.onset_decay = 0.9f;         /* fast decay */
    cfg.sustained_adaptation = 0.01f;

    cfg.spatial_resolution = 0.8f;
    cfg.frequency_resolution = 0.9f;

    cfg.enable_cortical_modulation = true;
    cfg.enable_sc_relay = true;

    return cfg;
}

inferior_colliculus_t* ic_create(const ic_config_t* config) {
    inferior_colliculus_t* ic = nimcp_calloc(1, sizeof(inferior_colliculus_t));
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "ic_create: failed to allocate inferior_colliculus_t");
        return NULL;
    }

    /* Apply config */
    if (config) {
        ic->config = *config;
    } else {
        ic->config = ic_default_config();
    }

    uint32_t nch = ic->config.num_frequency_channels;
    if (nch == 0) nch = IC_DEFAULT_NUM_CHANNELS;
    ic->num_frequency_channels = nch;

    /* Allocate arrays */
    ic->center_frequencies = nimcp_calloc(nch, sizeof(float));
    ic->channel_activations = nimcp_calloc(nch, sizeof(float));
    ic->icc_response = nimcp_calloc(nch, sizeof(float));
    ic->icx_response = nimcp_calloc(nch, sizeof(float));
    ic->itd_map = nimcp_calloc(nch, sizeof(float));
    ic->ild_map = nimcp_calloc(nch, sizeof(float));
    ic->onset_response = nimcp_calloc(nch, sizeof(float));
    ic->sustained_response = nimcp_calloc(nch, sizeof(float));
    ic->prev_activation = nimcp_calloc(nch, sizeof(float));

    if (!ic->center_frequencies || !ic->channel_activations ||
        !ic->icc_response || !ic->icx_response ||
        !ic->itd_map || !ic->ild_map ||
        !ic->onset_response || !ic->sustained_response ||
        !ic->prev_activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "ic_create: failed to allocate channel arrays");
        ic_destroy(ic);
        return NULL;
    }

    /* Compute center frequencies */
    ic_compute_center_frequencies(ic->center_frequencies, nch,
                                  ic->config.min_freq_hz,
                                  ic->config.max_freq_hz);

    /* Create mutex */
    ic->lock = nimcp_mutex_create(NULL);
    if (!ic->lock) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "ic_create: failed to create mutex");
        ic_destroy(ic);
        return NULL;
    }

    ic->magic = 0x1C011C5U;
    ic->azimuth_estimate = 0.0f;
    ic->elevation_estimate = 0.0f;
    ic->update_count = 0;
    ic->last_update_us = ic_get_time_us();

    NIMCP_LOGGING_INFO("Inferior colliculus created: %u frequency channels "
                       "(%.0f - %.0f Hz)",
                       nch, ic->config.min_freq_hz, ic->config.max_freq_hz);

    return ic;
}

void ic_destroy(inferior_colliculus_t* ic) {
    if (!ic) return;

    ic->magic = 0;

    if (ic->lock) {
        nimcp_mutex_destroy(ic->lock);
        ic->lock = NULL;
    }

    nimcp_free(ic->center_frequencies);
    nimcp_free(ic->channel_activations);
    nimcp_free(ic->icc_response);
    nimcp_free(ic->icx_response);
    nimcp_free(ic->itd_map);
    nimcp_free(ic->ild_map);
    nimcp_free(ic->onset_response);
    nimcp_free(ic->sustained_response);
    nimcp_free(ic->prev_activation);

    nimcp_free(ic);
}

/* ============================================================================
 * PROCESSING
 * ============================================================================ */

int ic_update(inferior_colliculus_t* ic, float dt_s) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_update: ic is NULL");
        return -1;
    }
    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "ic_update: invalid dt_s");
        return -1;
    }

    nimcp_mutex_lock(ic->lock);

    uint32_t nch = ic->num_frequency_channels;
    float onset_decay = ic->config.onset_decay;
    float adapt = ic->config.sustained_adaptation;

    for (uint32_t i = 0; i < nch; i++) {
        /* Decay onset response */
        float onset = ic->onset_response[i] * onset_decay;
        ic->onset_response[i] = isfinite(onset) ? onset : 0.0f;

        /* Adapt sustained response toward channel activation */
        float sustained = ic->sustained_response[i];
        float target = ic->channel_activations[i];
        float delta = (target - sustained) * adapt;
        sustained += delta;
        ic->sustained_response[i] = isfinite(sustained) ? sustained : 0.0f;

        /* Decay channel activations */
        float act = ic->channel_activations[i] * (1.0f - 0.05f * dt_s);
        ic->channel_activations[i] = isfinite(act) ? ic_clampf(act, 0.0f, 1.0f) : 0.0f;
    }

    ic->update_count++;
    ic->last_update_us = ic_get_time_us();

    nimcp_mutex_unlock(ic->lock);
    return 0;
}

int ic_process_audio(inferior_colliculus_t* ic,
                     const float* left,
                     const float* right,
                     uint32_t num_samples) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_process_audio: ic is NULL");
        return -1;
    }
    if (!left || !right) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_process_audio: audio buffers are NULL");
        return -1;
    }
    if (num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "ic_process_audio: num_samples is 0");
        return -1;
    }

    nimcp_mutex_lock(ic->lock);

    uint32_t nch = ic->num_frequency_channels;

    /* ---- ICC: Tonotopic frequency analysis ---- */
    float peak_activation = 0.0f;
    uint32_t peak_channel = 0;

    for (uint32_t ch = 0; ch < nch; ch++) {
        /* Compute energy for left and right ears */
        float bandwidth = (ch < nch - 1)
            ? (ic->center_frequencies[ch + 1] - ic->center_frequencies[ch])
            : (ic->center_frequencies[ch] - ic->center_frequencies[ch - 1]);

        float energy_l = ic_band_energy(left, num_samples,
                                         ic->center_frequencies[ch], bandwidth);
        float energy_r = ic_band_energy(right, num_samples,
                                         ic->center_frequencies[ch], bandwidth);

        /* ICC response is the mean binaural energy */
        float binaural = (energy_l + energy_r) * 0.5f;
        ic->icc_response[ch] = isfinite(binaural) ? ic_clampf(binaural, 0.0f, 1.0f) : 0.0f;

        /* Store previous activation for onset detection */
        ic->prev_activation[ch] = ic->channel_activations[ch];
        ic->channel_activations[ch] = ic->icc_response[ch];

        /* Onset detection: sudden increase in activation */
        float diff = ic->channel_activations[ch] - ic->prev_activation[ch];
        if (diff > 0.01f) {
            ic->onset_response[ch] = isfinite(diff) ? ic_clampf(diff, 0.0f, 1.0f) : 0.0f;
        }

        /* Sustained response: EMA of sustained activation */
        float sus = ic->sustained_response[ch];
        sus += (ic->channel_activations[ch] - sus) * 0.1f;
        ic->sustained_response[ch] = isfinite(sus) ? ic_clampf(sus, 0.0f, 1.0f) : 0.0f;

        /* Track peak */
        if (ic->channel_activations[ch] > peak_activation) {
            peak_activation = ic->channel_activations[ch];
            peak_channel = ch;
        }

        /* ---- ICX: Binaural cues per channel ---- */

        /* ILD: interaural level difference (dB) */
        float eps = 1e-10f;
        float ild = 20.0f * log10f((energy_l + eps) / (energy_r + eps));
        ic->ild_map[ch] = isfinite(ild) ? ic_clampf(ild, -IC_ILD_MAX_DB, IC_ILD_MAX_DB) : 0.0f;

        /* ITD: cross-correlation lag proxy.
         * True ITD requires time-domain cross-correlation; we approximate
         * from the level difference scaled by frequency. Low frequencies
         * favor ITD, high frequencies favor ILD (duplex theory). */
        float freq = ic->center_frequencies[ch];
        float itd_proxy = 0.0f;
        if (freq < 1500.0f && freq > 0.0f) {
            /* Low freq: ITD dominates. Approximate from ILD as proxy. */
            itd_proxy = ild * (IC_ITD_MAX_US / IC_ILD_MAX_DB) * 0.5f;
        }
        ic->itd_map[ch] = isfinite(itd_proxy)
            ? ic_clampf(itd_proxy, -IC_ITD_MAX_US, IC_ITD_MAX_US)
            : 0.0f;

        /* ICX spatial response: weighted combination of ITD and ILD */
        float itd_norm = ic->itd_map[ch] / IC_ITD_MAX_US;
        float ild_norm = ic->ild_map[ch] / IC_ILD_MAX_DB;
        float spatial = ic->config.itd_weight * itd_norm
                      + ic->config.ild_weight * ild_norm;
        ic->icx_response[ch] = isfinite(spatial) ? ic_clampf(spatial, -1.0f, 1.0f) : 0.0f;
    }

    /* ---- Compute aggregate spatial estimates ---- */
    /* Weighted average of per-channel ICX responses, weighted by activation */
    float weighted_azimuth = 0.0f;
    float total_weight = 0.0f;
    for (uint32_t ch = 0; ch < nch; ch++) {
        float w = ic->channel_activations[ch];
        if (isfinite(w) && w > 0.01f) {
            weighted_azimuth += ic->icx_response[ch] * w;
            total_weight += w;
        }
    }

    if (total_weight > 0.001f) {
        float azimuth = (weighted_azimuth / total_weight) * IC_MAX_AZIMUTH_DEG;
        ic->azimuth_estimate = isfinite(azimuth)
            ? ic_clampf(azimuth, -IC_MAX_AZIMUTH_DEG, IC_MAX_AZIMUTH_DEG)
            : 0.0f;
    }

    /* Elevation is harder to compute binaurally; use spectral cues proxy.
     * Higher frequency emphasis suggests elevated sources (pinna filtering). */
    if (nch > 1 && peak_activation > 0.01f) {
        float norm_ch = (float)peak_channel / (float)(nch - 1);  /* 0..1 */
        float elev = (norm_ch - 0.5f) * 2.0f * IC_MAX_ELEVATION_DEG;
        ic->elevation_estimate = isfinite(elev)
            ? ic_clampf(elev, -IC_MAX_ELEVATION_DEG, IC_MAX_ELEVATION_DEG)
            : 0.0f;
    }

    ic->last_update_us = ic_get_time_us();
    ic->update_count++;

    nimcp_mutex_unlock(ic->lock);
    return 0;
}

/* ============================================================================
 * QUERY API
 * ============================================================================ */

float ic_get_azimuth(const inferior_colliculus_t* ic) {
    if (!ic) return 0.0f;
    /* Read is atomic for float, no lock needed for snapshot */
    return ic->azimuth_estimate;
}

float ic_get_elevation(const inferior_colliculus_t* ic) {
    if (!ic) return 0.0f;
    return ic->elevation_estimate;
}

float ic_get_channel_activation(const inferior_colliculus_t* ic, uint32_t channel) {
    if (!ic) return -1.0f;
    if (channel >= ic->num_frequency_channels) return -1.0f;
    return ic->channel_activations[channel];
}

int ic_get_icc_response(const inferior_colliculus_t* ic,
                        float* out_response,
                        uint32_t buf_size) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_get_icc_response: ic is NULL");
        return -1;
    }
    if (!out_response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_get_icc_response: out_response is NULL");
        return -1;
    }
    if (buf_size < ic->num_frequency_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "ic_get_icc_response: buffer too small");
        return -1;
    }

    nimcp_mutex_lock(((inferior_colliculus_t*)ic)->lock);
    memcpy(out_response, ic->icc_response,
           ic->num_frequency_channels * sizeof(float));
    nimcp_mutex_unlock(((inferior_colliculus_t*)ic)->lock);
    return 0;
}

int ic_get_icx_response(const inferior_colliculus_t* ic,
                        float* out_response,
                        uint32_t buf_size) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_get_icx_response: ic is NULL");
        return -1;
    }
    if (!out_response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_get_icx_response: out_response is NULL");
        return -1;
    }
    if (buf_size < ic->num_frequency_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "ic_get_icx_response: buffer too small");
        return -1;
    }

    nimcp_mutex_lock(((inferior_colliculus_t*)ic)->lock);
    memcpy(out_response, ic->icx_response,
           ic->num_frequency_channels * sizeof(float));
    nimcp_mutex_unlock(((inferior_colliculus_t*)ic)->lock);
    return 0;
}

int ic_get_stats(const inferior_colliculus_t* ic, ic_stats_t* stats) {
    if (!ic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_get_stats: ic is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "ic_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(((inferior_colliculus_t*)ic)->lock);

    stats->azimuth_estimate = ic->azimuth_estimate;
    stats->elevation_estimate = ic->elevation_estimate;
    stats->update_count = ic->update_count;
    stats->last_update_us = ic->last_update_us;

    /* Compute mean activation and peak */
    float sum = 0.0f;
    float peak = 0.0f;
    uint32_t peak_ch = 0;
    uint32_t nch = ic->num_frequency_channels;

    for (uint32_t i = 0; i < nch; i++) {
        float a = ic->channel_activations[i];
        if (isfinite(a)) {
            sum += a;
            if (a > peak) {
                peak = a;
                peak_ch = i;
            }
        }
    }

    stats->mean_activation = (nch > 0) ? (sum / (float)nch) : 0.0f;
    stats->peak_channel = peak_ch;
    stats->peak_frequency_hz = (peak_ch < nch) ? ic->center_frequencies[peak_ch] : 0.0f;

    nimcp_mutex_unlock(((inferior_colliculus_t*)ic)->lock);
    return 0;
}
