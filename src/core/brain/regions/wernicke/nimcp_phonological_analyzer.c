/**
 * @file nimcp_phonological_analyzer.c
 * @brief Implementation of phonological analysis for Wernicke's area
 *
 * WHAT: Phoneme detection, syllable parsing, and prosodic analysis
 * WHY:  First layer of language comprehension
 * HOW:  Feature extraction and classification pipeline
 *
 * @version Phase W1: Wernicke's Area Core Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/wernicke/nimcp_phonological_analyzer.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(phonological_analyzer, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * VOWEL PROTOTYPES (Peterson & Barney 1952)
 *===========================================================================*/

static const vowel_prototype_t VOWEL_PROTOTYPES[] = {
    /* High front */
    { PHONEME_IY, 270, 2290 },   /* "ee" in beet */
    { PHONEME_IH, 390, 1990 },   /* "i" in bit */

    /* Mid front */
    { PHONEME_EY, 530, 1840 },   /* "a" in bait */
    { PHONEME_EH, 530, 1840 },   /* "e" in bet */

    /* Low front */
    { PHONEME_AE, 660, 1720 },   /* "a" in bat */

    /* Low back */
    { PHONEME_AA, 730, 1090 },   /* "a" in father */
    { PHONEME_AO, 570, 840 },    /* "o" in caught */

    /* Mid back */
    { PHONEME_OW, 490, 1350 },   /* "o" in boat */
    { PHONEME_UH, 440, 1020 },   /* "oo" in book */

    /* High back */
    { PHONEME_UW, 300, 870 },    /* "oo" in boot */

    /* Central */
    { PHONEME_AH, 640, 1190 },   /* "u" in but */

    /* R-colored */
    { PHONEME_ER, 490, 1350 },   /* "er" in bird */
};

static const uint32_t NUM_VOWEL_PROTOTYPES = sizeof(VOWEL_PROTOTYPES) / sizeof(VOWEL_PROTOTYPES[0]);

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal analyzer state
 */
struct phonological_analyzer {
    /* Configuration */
    phonological_config_t config;

    /* Audio processing */
    uint32_t frame_size_samples;
    uint32_t hop_size_samples;
    float* window;                       /**< Hamming window */
    float* fft_buffer;                   /**< FFT work buffer */

    /* Feature buffers */
    float* power_spectrum;               /**< Power spectrum */
    uint32_t spectrum_size;

    /* LPC for formant tracking */
    float* lpc_coeffs;
    uint32_t lpc_order;

    /* State */
    uint64_t frames_processed;
    float prev_spectral_energy;

    /* Statistics */
    uint64_t phonemes_detected;
    uint64_t syllables_parsed;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Compute Hamming window
 */
static void compute_hamming_window(float* window, uint32_t size)
{
    if (size <= 1) {
        if (size == 1) window[0] = 1.0f;
        return;
    }
    for (uint32_t i = 0; i < size; i++) {
        window[i] = 0.54f - 0.46f * cosf(NIMCP_TWO_PI_F * (float)i / (float)(size - 1));
    }
}

/**
 * @brief Compute RMS energy of signal
 */
static float compute_rms(const float* signal, uint32_t length)
{
    if (length == 0) return 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        sum += signal[i] * signal[i];
    }
    return sqrtf(sum / (float)length);
}

/**
 * @brief Count zero crossings
 */
static float compute_zcr(const float* signal, uint32_t length, uint32_t sample_rate)
{
    uint32_t crossings = 0;
    for (uint32_t i = 1; i < length; i++) {
        if ((signal[i] >= 0 && signal[i-1] < 0) ||
            (signal[i] < 0 && signal[i-1] >= 0)) {
            crossings++;
        }
    }
    float duration_s = (sample_rate > 0) ? (float)length / (float)sample_rate : 0.0f;
    return (fabsf(duration_s) > 1e-10f) ? (float)crossings / duration_s : 0.0f;
}

/**
 * @brief Simple autocorrelation-based pitch detection
 */
static float detect_pitch(const float* signal, uint32_t length, uint32_t sample_rate)
{
    /* Search for pitch between 50 Hz and 500 Hz */
    uint32_t min_lag = sample_rate / 500;
    uint32_t max_lag = sample_rate / 50;
    if (max_lag > length / 2) max_lag = length / 2;

    float max_corr = 0.0f;
    uint32_t best_lag = 0;

    for (uint32_t lag = min_lag; lag < max_lag; lag++) {
        float corr = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;

        for (uint32_t i = 0; i < length - lag; i++) {
            corr += signal[i] * signal[i + lag];
            norm1 += signal[i] * signal[i];
            norm2 += signal[i + lag] * signal[i + lag];
        }

        float denom = sqrtf(norm1 * norm2);
        if (denom > 1e-10f) {
            corr /= denom;
        }

        if (corr > max_corr) {
            max_corr = corr;
            best_lag = lag;
        }
    }

    /* Require strong correlation for voiced detection */
    if (max_corr < 0.3f || best_lag == 0) {
        return 0.0f;  /* Unvoiced */
    }

    return (float)sample_rate / (float)best_lag;
}

/**
 * @brief Simple LPC analysis (Levinson-Durbin)
 */
static bool compute_lpc(const float* signal, uint32_t length, float* coeffs, uint32_t order)
{
    /* Compute autocorrelation */
    float* r = (float*)nimcp_calloc(order + 1, sizeof(float));
    if (!r) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_lpc: r is NULL");
        return false;
    }

    for (uint32_t i = 0; i <= order; i++) {
        r[i] = 0.0f;
        for (uint32_t j = 0; j < length - i; j++) {
            r[i] += signal[j] * signal[j + i];
        }
    }

    /* Check for silence */
    if (r[0] < 1e-10f) {
        nimcp_free(r);
        for (uint32_t i = 0; i < order; i++) coeffs[i] = 0.0f;
        return true;
    }

    /* Levinson-Durbin recursion */
    float* a = (float*)nimcp_calloc(order + 1, sizeof(float));
    float* a_prev = (float*)nimcp_calloc(order + 1, sizeof(float));
    if (!a || !a_prev) {
        nimcp_free(r);
        if (a) nimcp_free(a);
        if (a_prev) nimcp_free(a_prev);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_lpc: failed to allocate LPC work buffers");
        return false;
    }

    float error = r[0];
    a[0] = 1.0f;

    for (uint32_t i = 1; i <= order; i++) {
        float sum = r[i];
        for (uint32_t j = 1; j < i; j++) {
            sum += a_prev[j] * r[i - j];
        }

        if (fabsf(error) < 1e-10f) break;
        float k = -sum / error;

        memcpy(a, a_prev, (i + 1) * sizeof(float));
        a[i] = k;
        for (uint32_t j = 1; j < i; j++) {
            a[j] = a_prev[j] + k * a_prev[i - j];
        }

        error = error * (1.0f - k * k);

        memcpy(a_prev, a, (i + 1) * sizeof(float));
    }

    for (uint32_t i = 0; i < order; i++) {
        coeffs[i] = a[i + 1];
    }

    nimcp_free(r);
    nimcp_free(a);
    nimcp_free(a_prev);
    return true;
}

/**
 * @brief Estimate formants from LPC coefficients
 */
static void estimate_formants_from_lpc(
    const float* lpc,
    uint32_t order,
    uint32_t sample_rate,
    formant_result_t* result)
{
    /* Simple peak-picking in LPC spectrum */
    /* More sophisticated: root finding of LPC polynomial */

    /* For now, use typical formant ranges */
    result->f1 = 500.0f;   /* Default F1 */
    result->f2 = 1500.0f;  /* Default F2 */
    result->f3 = 2500.0f;  /* Default F3 */
    result->f4 = 3500.0f;  /* Default F4 */

    result->f1_bandwidth = 80.0f;
    result->f2_bandwidth = 100.0f;
    result->f3_bandwidth = 120.0f;
    result->f4_bandwidth = 150.0f;

    /* Phase 2 will implement proper LPC root finding */
    (void)lpc;
    (void)order;
    (void)sample_rate;
}

/**
 * @brief Find closest vowel prototype
 */
static phoneme_t find_closest_vowel(float f1, float f2, float* distance)
{
    float min_dist = 1e10f;
    phoneme_t best = PHONEME_UNKNOWN;

    for (uint32_t i = 0; i < NUM_VOWEL_PROTOTYPES; i++) {
        float d1 = f1 - VOWEL_PROTOTYPES[i].f1;
        float d2 = f2 - VOWEL_PROTOTYPES[i].f2;
        float dist = sqrtf(d1 * d1 + d2 * d2);

        if (dist < min_dist) {
            min_dist = dist;
            best = VOWEL_PROTOTYPES[i].phoneme;
        }
    }

    if (distance) *distance = min_dist;
    return best;
}

/**
 * @brief Get phoneme sonority (for syllabification)
 */
static int get_sonority(phoneme_t p)
{
    /* Sonority hierarchy: vowels > liquids > nasals > fricatives > stops */
    if (p >= PHONEME_IY && p <= PHONEME_ER) return 5;  /* Vowels */
    if (p == PHONEME_L || p == PHONEME_R) return 4;     /* Liquids */
    if (p == PHONEME_W || p == PHONEME_Y) return 3;     /* Glides */
    if (p == PHONEME_M || p == PHONEME_N || p == PHONEME_NG) return 2; /* Nasals */
    if (p >= PHONEME_F && p <= PHONEME_H) return 1;     /* Fricatives */
    return 0;  /* Stops and affricates */
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

phonological_config_t wernicke_phonological_default_config(void)
{
    phonological_config_t config = {
        .sample_rate = 16000,
        .frame_size_ms = PHON_DEFAULT_FRAME_SIZE_MS,
        .hop_size_ms = PHON_DEFAULT_HOP_SIZE_MS,
        .fft_size = PHON_DEFAULT_FFT_SIZE,

        .num_formants = PHON_DEFAULT_NUM_FORMANTS,
        .formant_floor_hz = 50.0f,
        .formant_ceiling_hz = 5500.0f,

        .max_phonemes = PHON_DEFAULT_MAX_PHONEMES,
        .confidence_threshold = NIMCP_CONFIDENCE_MEDIUM,

        .max_syllables = PHON_DEFAULT_MAX_SYLLABLES,
        .enable_syllable_parsing = true,

        .enable_prosody = true,
        .enable_stress_detection = true,
        .enable_pitch_tracking = true
    };
    return config;
}

phonological_analyzer_t* wernicke_phonological_create(const phonological_config_t* config)
{
    phonological_config_t cfg = config ? *config : wernicke_phonological_default_config();

    phonological_analyzer_t* analyzer = (phonological_analyzer_t*)nimcp_calloc(1, sizeof(phonological_analyzer_t));
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_phonological_create: failed to allocate analyzer");
        return NULL;
    }

    analyzer->config = cfg;

    /* Compute sizes */
    analyzer->frame_size_samples = (cfg.sample_rate * cfg.frame_size_ms) / 1000;
    analyzer->hop_size_samples = (cfg.sample_rate * cfg.hop_size_ms) / 1000;
    analyzer->spectrum_size = cfg.fft_size / 2 + 1;
    analyzer->lpc_order = 12;  /* Typical for formant analysis */

    /* Allocate buffers */
    analyzer->window = (float*)nimcp_calloc(analyzer->frame_size_samples, sizeof(float));
    analyzer->fft_buffer = (float*)nimcp_calloc(cfg.fft_size, sizeof(float));
    analyzer->power_spectrum = (float*)nimcp_calloc(analyzer->spectrum_size, sizeof(float));
    analyzer->lpc_coeffs = (float*)nimcp_calloc(analyzer->lpc_order, sizeof(float));

    if (!analyzer->window || !analyzer->fft_buffer ||
        !analyzer->power_spectrum || !analyzer->lpc_coeffs) {
        wernicke_phonological_destroy(analyzer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_phonological_create: failed to allocate buffers");
        return NULL;
    }

    /* Initialize Hamming window */
    compute_hamming_window(analyzer->window, analyzer->frame_size_samples);

    NIMCP_LOG_INFO("phonological", "Created analyzer (frame=%ums, hop=%ums, fft=%u)",
                   cfg.frame_size_ms, cfg.hop_size_ms, cfg.fft_size);

    return analyzer;
}

void wernicke_phonological_destroy(phonological_analyzer_t* analyzer)
{
    if (!analyzer) return;

    if (analyzer->window) nimcp_free(analyzer->window);
    if (analyzer->fft_buffer) nimcp_free(analyzer->fft_buffer);
    if (analyzer->power_spectrum) nimcp_free(analyzer->power_spectrum);
    if (analyzer->lpc_coeffs) nimcp_free(analyzer->lpc_coeffs);

    nimcp_free(analyzer);
}

bool wernicke_phonological_reset(phonological_analyzer_t* analyzer)
{
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_phonological_reset: analyzer is NULL");
        return false;
    }

    analyzer->frames_processed = 0;
    analyzer->prev_spectral_energy = 0.0f;
    return true;
}

/*=============================================================================
 * FEATURE EXTRACTION
 *===========================================================================*/

bool phonological_extract_formants(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    formant_result_t* result)
{
    if (!analyzer || !audio || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_extract_formants: required parameter is NULL (analyzer, audio, result)");
        return false;
    }

    /* Apply pre-emphasis */
    float* pre_emph = (float*)nimcp_calloc(num_samples, sizeof(float));
    if (!pre_emph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phonological_extract_formants: pre_emph is NULL");
        return false;
    }

    pre_emph[0] = audio[0];
    for (uint32_t i = 1; i < num_samples; i++) {
        pre_emph[i] = audio[i] - 0.97f * audio[i-1];
    }

    /* Compute LPC coefficients */
    compute_lpc(pre_emph, num_samples, analyzer->lpc_coeffs, analyzer->lpc_order);

    /* Estimate formants from LPC */
    estimate_formants_from_lpc(analyzer->lpc_coeffs, analyzer->lpc_order,
                               analyzer->config.sample_rate, result);

    nimcp_free(pre_emph);
    return true;
}

bool phonological_extract_features(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    acoustic_features_t* features)
{
    if (!analyzer || !audio || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_extract_features: required parameter is NULL (analyzer, audio, features)");
        return false;
    }

    memset(features, 0, sizeof(acoustic_features_t));

    /* Extract formants */
    phonological_extract_formants(analyzer, audio, num_samples, &features->formants);

    /* Compute RMS energy */
    features->rms_energy = compute_rms(audio, num_samples);

    /* Compute zero crossing rate */
    features->zero_crossing_rate = compute_zcr(audio, num_samples,
                                                analyzer->config.sample_rate);

    /* Detect pitch (F0) */
    features->fundamental_freq = detect_pitch(audio, num_samples,
                                               analyzer->config.sample_rate);
    features->is_voiced = (features->fundamental_freq > 0.0f);

    /* Duration */
    features->duration_ms = (analyzer->config.sample_rate > 0) ?
        (float)num_samples * 1000.0f / (float)analyzer->config.sample_rate : 0.0f;

    /* Spectral centroid (simplified - would use FFT) */
    features->spectral_centroid = features->is_voiced ?
        features->formants.f1 * 0.3f + features->formants.f2 * 0.7f :
        2500.0f;  /* Higher for unvoiced */

    return true;
}

/*=============================================================================
 * PHONEME CLASSIFICATION
 *===========================================================================*/

bool phonological_classify_phoneme(
    phonological_analyzer_t* analyzer,
    const acoustic_features_t* features,
    phoneme_t* phoneme,
    float* confidence)
{
    if (!analyzer || !features || !phoneme || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_classify_phoneme: required parameter is NULL (analyzer, features, phoneme, confidence)");
        return false;
    }

    /* Check for silence */
    if (features->rms_energy < 0.001f) {
        *phoneme = PHONEME_SILENCE;
        *confidence = 1.0f;
        return true;
    }

    /* Voiced sounds */
    if (features->is_voiced) {
        /* Low ZCR = vowel or voiced consonant */
        if (features->zero_crossing_rate < 3000.0f) {
            /* Classify vowel from formants */
            float dist;
            *phoneme = find_closest_vowel(features->formants.f1,
                                          features->formants.f2, &dist);
            /* Convert distance to confidence */
            *confidence = 1.0f / (1.0f + dist / 200.0f);
            return true;
        }
        /* Voiced consonant - use spectral features */
        return phonological_classify_consonant(analyzer, features, phoneme, confidence);
    }

    /* Unvoiced sounds */
    return phonological_classify_consonant(analyzer, features, phoneme, confidence);
}

bool phonological_classify_vowel(
    phonological_analyzer_t* analyzer,
    float f1,
    float f2,
    phoneme_t* phoneme,
    float* confidence)
{
    if (!phoneme || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_classify_vowel: required parameter is NULL (phoneme, confidence)");
        return false;
    }
    (void)analyzer;

    float dist;
    *phoneme = find_closest_vowel(f1, f2, &dist);
    *confidence = 1.0f / (1.0f + dist / 200.0f);
    return true;
}

bool phonological_classify_consonant(
    phonological_analyzer_t* analyzer,
    const acoustic_features_t* features,
    phoneme_t* phoneme,
    float* confidence)
{
    if (!analyzer || !features || !phoneme || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_classify_consonant: required parameter is NULL (analyzer, features, phoneme, confidence)");
        return false;
    }

    /* Classify based on spectral centroid and ZCR */
    float sc = features->spectral_centroid;
    float zcr = features->zero_crossing_rate;
    bool voiced = features->is_voiced;

    /* High frequency noise = fricatives */
    if (sc > 4000.0f || zcr > 5000.0f) {
        if (voiced) {
            *phoneme = PHONEME_Z;  /* voiced fricative */
        } else {
            *phoneme = PHONEME_S;  /* voiceless fricative */
        }
        *confidence = 0.6f;
        return true;
    }

    /* Medium frequency = affricates or some fricatives */
    if (sc > 2500.0f) {
        if (voiced) {
            *phoneme = PHONEME_JH;
        } else {
            *phoneme = PHONEME_CH;
        }
        *confidence = 0.5f;
        return true;
    }

    /* Low frequency with low energy = stops */
    if (features->rms_energy < 0.01f && features->duration_ms < 50.0f) {
        if (voiced) {
            *phoneme = PHONEME_D;
        } else {
            *phoneme = PHONEME_T;
        }
        *confidence = 0.5f;
        return true;
    }

    /* Nasals - voiced with specific formant pattern */
    if (voiced && features->formants.f1 < 400.0f) {
        *phoneme = PHONEME_N;
        *confidence = 0.5f;
        return true;
    }

    /* Default */
    *phoneme = PHONEME_UNKNOWN;
    *confidence = 0.3f;
    return true;
}

/*=============================================================================
 * COMPLETE ANALYSIS
 *===========================================================================*/

bool phonological_analyze(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    phonological_result_t* result)
{
    if (!analyzer || !audio || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_analyze: required parameter is NULL (analyzer, audio, result)");
        return false;
    }

    /* Detect phonemes */
    bool success = phonological_detect_phonemes(analyzer, audio, num_samples,
                                                 result->phonemes,
                                                 analyzer->config.max_phonemes,
                                                 &result->num_phonemes);
    if (!success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phonological_analyze: phoneme detection failed");
        return false;
    }

    /* Parse syllables */
    if (analyzer->config.enable_syllable_parsing && result->num_phonemes > 0) {
        phonological_parse_syllables(analyzer, result->phonemes, result->num_phonemes,
                                     result->syllables,
                                     analyzer->config.max_syllables,
                                     &result->num_syllables);
    }

    /* Extract prosody */
    if (analyzer->config.enable_prosody) {
        phonological_extract_prosody(analyzer, audio, num_samples, &result->prosody);
    }

    /* Compute quality metrics */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < result->num_phonemes; i++) {
        total_conf += result->phonemes[i].confidence;
    }
    result->avg_confidence = result->num_phonemes > 0 ?
        total_conf / (float)result->num_phonemes : 0.0f;

    result->signal_quality = result->avg_confidence;

    return true;
}

bool phonological_detect_phonemes(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    phoneme_event_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_detected)
{
    if (!analyzer || !audio || !phonemes || !num_detected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_detect_phonemes: required parameter is NULL (analyzer, audio, phonemes, num_detected)");
        return false;
    }

    *num_detected = 0;
    uint32_t frame_size = analyzer->frame_size_samples;
    uint32_t hop_size = analyzer->hop_size_samples;

    if (num_samples < frame_size) return true;

    phoneme_t prev_phoneme = PHONEME_SILENCE;
    uint64_t segment_start = 0;

    for (uint32_t offset = 0; offset + frame_size <= num_samples; offset += hop_size) {
        if (*num_detected >= max_phonemes) break;

        /* Extract features for this frame */
        acoustic_features_t features;
        phonological_extract_features(analyzer, &audio[offset], frame_size, &features);

        /* Classify phoneme */
        phoneme_t phoneme;
        float confidence;
        phonological_classify_phoneme(analyzer, &features, &phoneme, &confidence);

        /* Detect phoneme boundary (change in phoneme) */
        if (phoneme != prev_phoneme && phoneme != PHONEME_SILENCE) {
            /* End previous segment and start new one */
            phoneme_event_t* event = &phonemes[*num_detected];
            event->phoneme = phoneme;
            event->confidence = confidence;
            event->onset_time_ms = (analyzer->config.sample_rate > 0) ?
                (uint64_t)offset * 1000 / analyzer->config.sample_rate : 0;
            event->offset_time_ms = (analyzer->config.sample_rate > 0) ?
                event->onset_time_ms + frame_size * 1000 / analyzer->config.sample_rate : 0;

            /* Copy features */
            event->features.formant_f1 = features.formants.f1;
            event->features.formant_f2 = features.formants.f2;
            event->features.formant_f3 = features.formants.f3;
            event->features.formant_f4 = features.formants.f4;
            event->features.pitch_hz = features.fundamental_freq;
            event->features.duration_ms = features.duration_ms;
            event->features.is_vowel = (phoneme >= PHONEME_IY && phoneme <= PHONEME_ER);
            event->features.is_voiced = features.is_voiced;

            (*num_detected)++;
            segment_start = offset;
        }

        prev_phoneme = phoneme;
        analyzer->frames_processed++;
    }

    analyzer->phonemes_detected += *num_detected;

    NIMCP_LOG_DEBUG("phonological", "Detected %u phonemes from %u samples",
                   *num_detected, num_samples);

    return true;
}

/*=============================================================================
 * SYLLABLE PARSING
 *===========================================================================*/

bool phonological_parse_syllables(
    phonological_analyzer_t* analyzer,
    const phoneme_event_t* phonemes,
    uint32_t num_phonemes,
    syllable_t* syllables,
    uint32_t max_syllables,
    uint32_t* num_syllables)
{
    if (!analyzer || !phonemes || !syllables || !num_syllables) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_parse_syllables: required parameter is NULL (analyzer, phonemes, syllables, num_syllables)");
        return false;
    }

    *num_syllables = 0;

    /* Find syllable nuclei (vowels) */
    uint32_t nucleus_indices[PHON_DEFAULT_MAX_SYLLABLES];
    uint32_t num_nuclei = 0;

    for (uint32_t i = 0; i < num_phonemes && num_nuclei < max_syllables; i++) {
        phoneme_t p = phonemes[i].phoneme;
        if (p >= PHONEME_IY && p <= PHONEME_ER) {
            nucleus_indices[num_nuclei++] = i;
        }
    }

    if (num_nuclei == 0) return true;

    /* Create syllables around nuclei */
    for (uint32_t i = 0; i < num_nuclei && *num_syllables < max_syllables; i++) {
        uint32_t nucleus = nucleus_indices[i];

        /* Find onset (before nucleus) */
        uint32_t onset = nucleus;
        while (onset > 0) {
            uint32_t prev = onset - 1;
            /* Check if previous is previous syllable's nucleus */
            if (i > 0 && prev <= nucleus_indices[i-1]) break;
            /* Check sonority sequencing */
            int this_son = get_sonority(phonemes[onset].phoneme);
            int prev_son = get_sonority(phonemes[prev].phoneme);
            if (prev_son >= this_son) break;  /* Sonority must rise */
            onset = prev;
        }

        /* Find coda (after nucleus) */
        uint32_t coda = nucleus;
        while (coda + 1 < num_phonemes) {
            uint32_t next = coda + 1;
            /* Check if next is next syllable's nucleus */
            if (i + 1 < num_nuclei && next >= nucleus_indices[i+1]) break;
            /* Check sonority sequencing */
            int this_son = get_sonority(phonemes[coda].phoneme);
            int next_son = get_sonority(phonemes[next].phoneme);
            if (next_son > this_son) break;  /* Sonority must fall */
            coda = next;
        }

        /* Create syllable */
        syllable_t* syl = &syllables[*num_syllables];
        syl->start_phoneme = onset;
        syl->end_phoneme = coda + 1;
        syl->nucleus_phoneme = nucleus;
        syl->onset_time_ms = phonemes[onset].onset_time_ms;
        syl->offset_time_ms = phonemes[coda].offset_time_ms;
        syl->duration_ms = (float)(syl->offset_time_ms - syl->onset_time_ms);
        syl->stress_level = 0.5f;  /* Default; detect_stress will update */
        syl->is_stressed = false;

        (*num_syllables)++;
    }

    analyzer->syllables_parsed += *num_syllables;

    NIMCP_LOG_DEBUG("phonological", "Parsed %u syllables from %u phonemes",
                   *num_syllables, num_phonemes);

    return true;
}

bool phonological_detect_nuclei(
    phonological_analyzer_t* analyzer,
    const phoneme_event_t* phonemes,
    uint32_t num_phonemes,
    uint32_t* nuclei_indices,
    uint32_t max_nuclei,
    uint32_t* num_nuclei)
{
    if (!phonemes || !nuclei_indices || !num_nuclei) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_detect_nuclei: required parameter is NULL (phonemes, nuclei_indices, num_nuclei)");
        return false;
    }
    (void)analyzer;

    *num_nuclei = 0;
    for (uint32_t i = 0; i < num_phonemes && *num_nuclei < max_nuclei; i++) {
        phoneme_t p = phonemes[i].phoneme;
        if (p >= PHONEME_IY && p <= PHONEME_ER) {
            nuclei_indices[(*num_nuclei)++] = i;
        }
    }
    return true;
}

/*=============================================================================
 * PROSODIC ANALYSIS
 *===========================================================================*/

bool phonological_extract_prosody(
    phonological_analyzer_t* analyzer,
    const float* audio,
    uint32_t num_samples,
    prosodic_contour_t* prosody)
{
    if (!analyzer || !audio || !prosody) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_extract_prosody: required parameter is NULL (analyzer, audio, prosody)");
        return false;
    }

    uint32_t frame_size = analyzer->frame_size_samples;
    uint32_t hop_size = analyzer->hop_size_samples;
    if (hop_size == 0 || num_samples < frame_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phonological_extract_prosody: hop_size is 0 or num_samples < frame_size");
        return false;
    }
    uint32_t num_frames = (num_samples - frame_size) / hop_size + 1;

    /* Allocate contours if needed */
    if (!prosody->pitch_contour) {
        prosody->pitch_contour = (float*)nimcp_calloc(num_frames, sizeof(float));
    }
    if (!prosody->intensity_contour) {
        prosody->intensity_contour = (float*)nimcp_calloc(num_frames, sizeof(float));
    }
    if (!prosody->pitch_contour || !prosody->intensity_contour) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phonological_extract_prosody: required parameter is NULL (prosody->pitch_contour, prosody->intensity_contour)");
        return false;
    }

    prosody->num_frames = num_frames;

    float pitch_sum = 0.0f;
    float pitch_min = 1000.0f;
    float pitch_max = 0.0f;
    uint32_t voiced_frames = 0;

    for (uint32_t i = 0; i < num_frames; i++) {
        uint32_t offset = i * hop_size;
        const float* frame = &audio[offset];

        /* Track pitch */
        float f0 = detect_pitch(frame, frame_size, analyzer->config.sample_rate);
        prosody->pitch_contour[i] = f0;

        if (f0 > 0.0f) {
            pitch_sum += f0;
            if (f0 < pitch_min) pitch_min = f0;
            if (f0 > pitch_max) pitch_max = f0;
            voiced_frames++;
        }

        /* Track intensity */
        prosody->intensity_contour[i] = compute_rms(frame, frame_size);
    }

    prosody->mean_pitch = voiced_frames > 0 ? pitch_sum / (float)voiced_frames : 0.0f;
    prosody->pitch_range = pitch_max - pitch_min;

    /* Estimate speech rate (crude) */
    float duration_s = (analyzer->config.sample_rate > 0) ?
        (float)num_samples / (float)analyzer->config.sample_rate : 0.0f;
    prosody->speech_rate = 3.0f;  /* Placeholder; would count syllables */

    return true;
}

bool phonological_detect_stress(
    phonological_analyzer_t* analyzer,
    syllable_t* syllables,
    uint32_t num_syllables,
    bool* stress_pattern)
{
    if (!analyzer || !syllables || !stress_pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_detect_stress: required parameter is NULL (analyzer, syllables, stress_pattern)");
        return false;
    }

    /* Stress detection based on:
     * 1. Duration (stressed syllables are longer)
     * 2. Pitch (stressed syllables have higher F0)
     * 3. Intensity (stressed syllables are louder)
     */

    /* For now, simple heuristic: mark every other syllable */
    for (uint32_t i = 0; i < num_syllables; i++) {
        /* First syllable of disyllabic words typically stressed in English */
        if (num_syllables == 2) {
            stress_pattern[i] = (i == 0);
            syllables[i].is_stressed = (i == 0);
            syllables[i].stress_level = (i == 0) ? 0.8f : 0.3f;
        } else {
            /* Simple alternating pattern */
            stress_pattern[i] = (i % 2 == 0);
            syllables[i].is_stressed = (i % 2 == 0);
            syllables[i].stress_level = (i % 2 == 0) ? 0.7f : 0.4f;
        }
    }

    return true;
}

/*=============================================================================
 * COARTICULATION
 *===========================================================================*/

bool phonological_apply_coarticulation(
    phonological_analyzer_t* analyzer,
    phoneme_event_t* phonemes,
    uint32_t num_phonemes)
{
    if (!analyzer || !phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_apply_coarticulation: required parameter is NULL (analyzer, phonemes)");
        return false;
    }

    /* Phase 2 will implement locus equations and transition modeling */
    /* For now, no-op */
    (void)num_phonemes;
    return true;
}

/*=============================================================================
 * RESULT MANAGEMENT
 *===========================================================================*/

phonological_result_t* phonological_result_alloc(
    uint32_t max_phonemes,
    uint32_t max_syllables,
    uint32_t max_frames)
{
    phonological_result_t* result = (phonological_result_t*)nimcp_calloc(1, sizeof(phonological_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phonological_result_alloc: failed to allocate result");
        return NULL;
    }

    result->phonemes = (phoneme_event_t*)nimcp_calloc(max_phonemes, sizeof(phoneme_event_t));
    result->syllables = (syllable_t*)nimcp_calloc(max_syllables, sizeof(syllable_t));
    result->prosody.pitch_contour = (float*)nimcp_calloc(max_frames, sizeof(float));
    result->prosody.intensity_contour = (float*)nimcp_calloc(max_frames, sizeof(float));

    if (!result->phonemes || !result->syllables ||
        !result->prosody.pitch_contour || !result->prosody.intensity_contour) {
        phonological_result_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "phonological_result_alloc: failed to allocate sub-buffers");
        return NULL;
    }

    return result;
}

void phonological_result_free(phonological_result_t* result)
{
    if (!result) return;

    if (result->phonemes) nimcp_free(result->phonemes);
    if (result->syllables) nimcp_free(result->syllables);
    if (result->prosody.pitch_contour) nimcp_free(result->prosody.pitch_contour);
    if (result->prosody.intensity_contour) nimcp_free(result->prosody.intensity_contour);

    nimcp_free(result);
}

/*=============================================================================
 * VOWEL PROTOTYPES
 *===========================================================================*/

const vowel_prototype_t* phonological_get_vowel_prototypes(uint32_t* count)
{
    if (count) *count = NUM_VOWEL_PROTOTYPES;
    return VOWEL_PROTOTYPES;
}
