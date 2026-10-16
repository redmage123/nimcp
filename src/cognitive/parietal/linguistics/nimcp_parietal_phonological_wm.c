/**
 * @file nimcp_parietal_phonological_wm.c
 * @brief Phonological Working Memory Module Implementation
 * @version 1.0.0
 * @date 2025-01-31
 */

#include "cognitive/parietal/linguistics/nimcp_parietal_phonological_wm.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * INTERNAL CONSTANTS
 * ============================================================================ */

/** Activation threshold for forgetting */
#define ACTIVATION_THRESHOLD    0.1f

/** Minimum activation after decay */
#define MIN_ACTIVATION          0.0f

/** Maximum activation */
#define MAX_ACTIVATION          1.0f

/* ============================================================================
 * PHONEME TABLES
 * ============================================================================ */

/** Phoneme names (ARPABET-like) */
static const char* g_phoneme_names[PHONEME_COUNT] = {
    [PHONEME_IY] = "IY", [PHONEME_IH] = "IH", [PHONEME_EY] = "EY", [PHONEME_EH] = "EH",
    [PHONEME_AE] = "AE", [PHONEME_AA] = "AA", [PHONEME_AO] = "AO", [PHONEME_OW] = "OW",
    [PHONEME_UH] = "UH", [PHONEME_UW] = "UW", [PHONEME_AH] = "AH", [PHONEME_ER] = "ER",
    [PHONEME_P] = "P", [PHONEME_B] = "B", [PHONEME_T] = "T", [PHONEME_D] = "D",
    [PHONEME_K] = "K", [PHONEME_G] = "G",
    [PHONEME_F] = "F", [PHONEME_V] = "V", [PHONEME_TH] = "TH", [PHONEME_DH] = "DH",
    [PHONEME_S] = "S", [PHONEME_Z] = "Z", [PHONEME_SH] = "SH", [PHONEME_ZH] = "ZH",
    [PHONEME_HH] = "HH",
    [PHONEME_M] = "M", [PHONEME_N] = "N", [PHONEME_NG] = "NG",
    [PHONEME_L] = "L", [PHONEME_R] = "R", [PHONEME_W] = "W", [PHONEME_Y] = "Y",
    [PHONEME_CH] = "CH", [PHONEME_JH] = "JH",
    [PHONEME_AY] = "AY", [PHONEME_AW] = "AW", [PHONEME_OY] = "OY",
    [PHONEME_EY2] = "EY2", [PHONEME_OW2] = "OW2", [PHONEME_UW2] = "UW2",
    [PHONEME_SILENCE] = "SIL", [PHONEME_UNKNOWN] = "UNK"
};

/** IPA symbols */
static const char* g_phoneme_ipa[PHONEME_COUNT] = {
    [PHONEME_IY] = "i:", [PHONEME_IH] = "I", [PHONEME_EY] = "eI", [PHONEME_EH] = "E",
    [PHONEME_AE] = "ae", [PHONEME_AA] = "A:", [PHONEME_AO] = "O:", [PHONEME_OW] = "oU",
    [PHONEME_UH] = "U", [PHONEME_UW] = "u:", [PHONEME_AH] = "V", [PHONEME_ER] = "3:",
    [PHONEME_P] = "p", [PHONEME_B] = "b", [PHONEME_T] = "t", [PHONEME_D] = "d",
    [PHONEME_K] = "k", [PHONEME_G] = "g",
    [PHONEME_F] = "f", [PHONEME_V] = "v", [PHONEME_TH] = "T", [PHONEME_DH] = "D",
    [PHONEME_S] = "s", [PHONEME_Z] = "z", [PHONEME_SH] = "S", [PHONEME_ZH] = "Z",
    [PHONEME_HH] = "h",
    [PHONEME_M] = "m", [PHONEME_N] = "n", [PHONEME_NG] = "N",
    [PHONEME_L] = "l", [PHONEME_R] = "r", [PHONEME_W] = "w", [PHONEME_Y] = "j",
    [PHONEME_CH] = "tS", [PHONEME_JH] = "dZ",
    [PHONEME_AY] = "aI", [PHONEME_AW] = "aU", [PHONEME_OY] = "OI",
    [PHONEME_EY2] = "eI", [PHONEME_OW2] = "oU", [PHONEME_UW2] = "u:",
    [PHONEME_SILENCE] = "-", [PHONEME_UNKNOWN] = "?"
};

/* ============================================================================
 * INTERNAL TYPES
 * ============================================================================ */

/**
 * @brief Phonological working memory internal state
 */
struct phonological_wm {
    /* Configuration */
    phonological_wm_config_t config;

    /* Buffer */
    phonological_trace_t buffer[LINGUISTICS_PHONOLOGICAL_BUFFER_SIZE];
    uint32_t count;
    uint32_t capacity;

    /* Rehearsal state */
    bool is_rehearsing;
    uint32_t rehearsal_position;
    uint64_t last_rehearsal_ms;

    /* Modulation state */
    float inflammation_level;
    float fatigue_level;
    float arousal_level;

    /* Derived state */
    float current_precision;
    float effective_decay_rate;
    uint32_t effective_capacity;

    /* Statistics */
    phonological_wm_stats_t stats;

    /* Time tracking */
    uint64_t last_update_ms;
};

/* ============================================================================
 * THREAD-LOCAL ERROR STORAGE
 * ============================================================================ */

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_last_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief Simple letter-to-phoneme for common patterns
 *
 * This is a simplified conversion - a full implementation would use
 * a pronunciation dictionary or neural TTS frontend.
 */
static void simple_grapheme_to_phoneme(
    const char* word,
    phoneme_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_phonemes
) {
    *num_phonemes = 0;
    const char* p = word;

    while (*p && *num_phonemes < max_phonemes) {
        char c = (char)tolower((unsigned char)*p);
        phoneme_t ph = PHONEME_UNKNOWN;

        /* Simple vowels */
        if (c == 'a') {
            /* Check for common patterns */
            if (p[1] == 'y') { ph = PHONEME_EY; p++; }
            else if (p[1] == 'i') { ph = PHONEME_AY; p++; }
            else if (p[1] == 'u' || p[1] == 'w') { ph = PHONEME_AW; p++; }
            else { ph = PHONEME_AE; }
        }
        else if (c == 'e') {
            if (p[1] == 'e') { ph = PHONEME_IY; p++; }
            else if (p[1] == 'y') { ph = PHONEME_IY; p++; }
            else if (p[1] == 'i') { ph = PHONEME_EY; p++; }
            else { ph = PHONEME_EH; }
        }
        else if (c == 'i') {
            if (p[1] == 'e') { ph = PHONEME_IY; p++; }
            else { ph = PHONEME_IH; }
        }
        else if (c == 'o') {
            if (p[1] == 'o') { ph = PHONEME_UW; p++; }
            else if (p[1] == 'i' || p[1] == 'y') { ph = PHONEME_OY; p++; }
            else if (p[1] == 'u' || p[1] == 'w') { ph = PHONEME_OW; p++; }
            else { ph = PHONEME_AA; }
        }
        else if (c == 'u') {
            ph = PHONEME_AH;
        }
        /* Consonants */
        else if (c == 'b') { ph = PHONEME_B; }
        else if (c == 'c') {
            if (p[1] == 'h') { ph = PHONEME_CH; p++; }
            else if (p[1] == 'k') { ph = PHONEME_K; p++; }
            else { ph = PHONEME_K; }
        }
        else if (c == 'd') { ph = PHONEME_D; }
        else if (c == 'f') { ph = PHONEME_F; }
        else if (c == 'g') { ph = PHONEME_G; }
        else if (c == 'h') { ph = PHONEME_HH; }
        else if (c == 'j') { ph = PHONEME_JH; }
        else if (c == 'k') { ph = PHONEME_K; }
        else if (c == 'l') { ph = PHONEME_L; }
        else if (c == 'm') { ph = PHONEME_M; }
        else if (c == 'n') {
            if (p[1] == 'g') { ph = PHONEME_NG; p++; }
            else { ph = PHONEME_N; }
        }
        else if (c == 'p') { ph = PHONEME_P; }
        else if (c == 'q') { ph = PHONEME_K; }
        else if (c == 'r') { ph = PHONEME_R; }
        else if (c == 's') {
            if (p[1] == 'h') { ph = PHONEME_SH; p++; }
            else { ph = PHONEME_S; }
        }
        else if (c == 't') {
            if (p[1] == 'h') { ph = PHONEME_TH; p++; }
            else { ph = PHONEME_T; }
        }
        else if (c == 'v') { ph = PHONEME_V; }
        else if (c == 'w') { ph = PHONEME_W; }
        else if (c == 'x') {
            if (*num_phonemes < max_phonemes - 1) {
                phonemes[(*num_phonemes)++] = PHONEME_K;
                ph = PHONEME_S;
            }
        }
        else if (c == 'y') { ph = PHONEME_Y; }
        else if (c == 'z') { ph = PHONEME_Z; }

        if (ph != PHONEME_UNKNOWN) {
            phonemes[(*num_phonemes)++] = ph;
        }
        p++;
    }
}

/**
 * @brief Get phoneme features
 */
static void get_phoneme_features(phoneme_t ph, phoneme_features_t* features) {
    memset(features, 0, sizeof(*features));

    /* Vowels */
    if (PHONEME_IS_VOWEL(ph)) {
        features->is_vowel = true;
        features->is_voiced = true;

        switch (ph) {
            case PHONEME_IY: features->height = 3; features->frontness = 2; break;
            case PHONEME_IH: features->height = 2; features->frontness = 2; break;
            case PHONEME_EY: features->height = 2; features->frontness = 2; break;
            case PHONEME_EH: features->height = 1; features->frontness = 2; break;
            case PHONEME_AE: features->height = 0; features->frontness = 2; break;
            case PHONEME_AA: features->height = 0; features->frontness = 1; break;
            case PHONEME_AO: features->height = 0; features->frontness = 0; break;
            case PHONEME_OW: features->height = 1; features->frontness = 0; break;
            case PHONEME_UH: features->height = 2; features->frontness = 0; break;
            case PHONEME_UW: features->height = 3; features->frontness = 0; break;
            case PHONEME_AH: features->height = 1; features->frontness = 1; break;
            case PHONEME_ER: features->height = 1; features->frontness = 1; break;
            default: break;
        }
    }
    /* Stops */
    else if (PHONEME_IS_STOP(ph)) {
        features->is_stop = true;
        features->is_voiced = (ph == PHONEME_B || ph == PHONEME_D || ph == PHONEME_G);

        switch (ph) {
            case PHONEME_P: case PHONEME_B: features->place = 0; break; /* Bilabial */
            case PHONEME_T: case PHONEME_D: features->place = 2; break; /* Alveolar */
            case PHONEME_K: case PHONEME_G: features->place = 5; break; /* Velar */
            default: break;
        }
    }
    /* Fricatives */
    else if (PHONEME_IS_FRICATIVE(ph)) {
        features->is_fricative = true;
        features->is_voiced = (ph == PHONEME_V || ph == PHONEME_DH ||
                               ph == PHONEME_Z || ph == PHONEME_ZH);

        switch (ph) {
            case PHONEME_F: case PHONEME_V: features->place = 1; break; /* Labiodental */
            case PHONEME_TH: case PHONEME_DH: features->place = 2; break; /* Dental */
            case PHONEME_S: case PHONEME_Z: features->place = 3; break; /* Alveolar */
            case PHONEME_SH: case PHONEME_ZH: features->place = 4; break; /* Postalveolar */
            case PHONEME_HH: features->place = 7; break; /* Glottal */
            default: break;
        }
    }
    /* Nasals */
    else if (PHONEME_IS_NASAL(ph)) {
        features->is_nasal = true;
        features->is_voiced = true;

        switch (ph) {
            case PHONEME_M: features->place = 0; break; /* Bilabial */
            case PHONEME_N: features->place = 3; break; /* Alveolar */
            case PHONEME_NG: features->place = 5; break; /* Velar */
            default: break;
        }
    }
}

/**
 * @brief Compute similarity between two phonemes
 */
static float compute_phoneme_similarity(phoneme_t a, phoneme_t b) {
    if (a == b) return 1.0f;

    phoneme_features_t fa, fb;
    get_phoneme_features(a, &fa);
    get_phoneme_features(b, &fb);

    float similarity = 0.0f;
    uint32_t comparisons = 0;

    /* Compare features */
    if (fa.is_vowel == fb.is_vowel) { similarity += 0.2f; }
    comparisons++;

    if (fa.is_voiced == fb.is_voiced) { similarity += 0.15f; }
    comparisons++;

    if (fa.is_nasal == fb.is_nasal) { similarity += 0.1f; }
    comparisons++;

    if (fa.is_stop == fb.is_stop) { similarity += 0.1f; }
    comparisons++;

    if (fa.is_fricative == fb.is_fricative) { similarity += 0.1f; }
    comparisons++;

    /* Place of articulation (closer = more similar) */
    if (fa.place == fb.place) {
        similarity += 0.2f;
    } else {
        int place_diff = abs((int)fa.place - (int)fb.place);
        similarity += 0.2f * (1.0f - place_diff / 7.0f);
    }
    comparisons++;

    /* For vowels, compare height and frontness */
    if (fa.is_vowel && fb.is_vowel) {
        int height_diff = abs((int)fa.height - (int)fb.height);
        int front_diff = abs((int)fa.frontness - (int)fb.frontness);
        similarity += 0.1f * (1.0f - height_diff / 3.0f);
        similarity += 0.05f * (1.0f - front_diff / 2.0f);
    }

    return fminf(similarity, 1.0f);
}

/**
 * @brief Find slot with lowest activation
 */
static uint32_t find_weakest_slot(const phonological_wm_t* pwm) {
    uint32_t weakest = 0;
    float min_activation = pwm->buffer[0].activation;

    for (uint32_t i = 1; i < pwm->count; i++) {
        if (pwm->buffer[i].activation < min_activation) {
            min_activation = pwm->buffer[i].activation;
            weakest = i;
        }
    }

    return weakest;
}

/**
 * @brief Update effective parameters based on modulation
 */
static void update_effective_params(phonological_wm_t* pwm) {
    /* Effective capacity decreases with inflammation and fatigue */
    float capacity_mult = 1.0f;
    capacity_mult -= pwm->inflammation_level * pwm->config.inflammation_sensitivity;
    capacity_mult -= pwm->fatigue_level * pwm->config.fatigue_sensitivity * 0.5f;

    /* Arousal has inverted-U effect: optimal at 0.5 */
    float arousal_effect = 1.0f - 2.0f * fabsf(pwm->arousal_level - 0.5f);
    capacity_mult += arousal_effect * pwm->config.arousal_sensitivity * 0.2f;

    capacity_mult = fmaxf(0.5f, fminf(1.0f, capacity_mult));
    pwm->effective_capacity = (uint32_t)(pwm->config.buffer_capacity * capacity_mult);
    pwm->effective_capacity = (pwm->effective_capacity < 3) ? 3 : pwm->effective_capacity;

    /* Effective decay rate increases with inflammation and fatigue */
    float base_decay = 1.0f / (float)pwm->config.decay_time_ms;
    float decay_mult = 1.0f;
    decay_mult += pwm->inflammation_level * pwm->config.inflammation_sensitivity;
    decay_mult += pwm->fatigue_level * pwm->config.fatigue_sensitivity;
    pwm->effective_decay_rate = base_decay * decay_mult;

    /* Precision decreases with modulation */
    pwm->current_precision = 0.8f;
    pwm->current_precision -= pwm->inflammation_level * pwm->config.inflammation_sensitivity;
    pwm->current_precision -= pwm->fatigue_level * pwm->config.fatigue_sensitivity;
    pwm->current_precision = fmaxf(LINGUISTICS_PRECISION_FLOOR, pwm->current_precision);
}

/* ============================================================================
 * LIFECYCLE API IMPLEMENTATION
 * ============================================================================ */

phonological_wm_config_t phonological_wm_default_config(void) {
    phonological_wm_config_t config = {
        .buffer_capacity = PHONOLOGICAL_WM_DEFAULT_CAPACITY,
        .decay_time_ms = PHONOLOGICAL_WM_DEFAULT_DECAY_MS,
        .rehearsal_rate = PHONOLOGICAL_WM_DEFAULT_REHEARSAL_RATE,

        .enable_similarity_effect = true,
        .enable_word_length_effect = true,
        .enable_recency_effect = true,

        .enable_automatic_rehearsal = false,
        .enable_bio_async = false,
        .enable_mesh_participation = true,
        .enable_theta_gating = false,

        .inflammation_sensitivity = 0.3f,
        .fatigue_sensitivity = 0.2f,
        .arousal_sensitivity = 0.2f
    };
    return config;
}

bool phonological_wm_validate_config(const phonological_wm_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonological_wm_validate_config: config is NULL");
        return false;
    }

    if (config->buffer_capacity < 1 || config->buffer_capacity > LINGUISTICS_PHONOLOGICAL_BUFFER_SIZE) {
        return false;
    }
    if (config->decay_time_ms < 100) {
        return false;
    }
    if (config->rehearsal_rate < 0.1f || config->rehearsal_rate > 20.0f) {
        return false;
    }
    if (config->inflammation_sensitivity < 0.0f || config->inflammation_sensitivity > 1.0f) {
        return false;
    }
    if (config->fatigue_sensitivity < 0.0f || config->fatigue_sensitivity > 1.0f) {
        return false;
    }
    if (config->arousal_sensitivity < 0.0f || config->arousal_sensitivity > 1.0f) {
        return false;
    }

    return true;
}

phonological_wm_t* phonological_wm_create(void) {
    return phonological_wm_create_custom(NULL);
}

phonological_wm_t* phonological_wm_create_custom(const phonological_wm_config_t* config) {
    phonological_wm_t* pwm = (phonological_wm_t*)nimcp_calloc(1, sizeof(phonological_wm_t));
    if (!pwm) {
        set_last_error("Failed to allocate phonological working memory");
        NIMCP_THROW_TO_IMMUNE(LING_ERR_ALLOC_FAILED, "phonological_wm_create_custom: allocation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config && phonological_wm_validate_config(config)) {
        pwm->config = *config;
    } else {
        pwm->config = phonological_wm_default_config();
    }

    /* Initialize state */
    pwm->count = 0;
    pwm->capacity = pwm->config.buffer_capacity;
    pwm->is_rehearsing = false;
    pwm->rehearsal_position = 0;

    pwm->inflammation_level = 0.0f;
    pwm->fatigue_level = 0.0f;
    pwm->arousal_level = 0.5f; /* Optimal arousal */

    pwm->last_update_ms = get_time_ms();
    pwm->last_rehearsal_ms = pwm->last_update_ms;

    update_effective_params(pwm);

    /* Reset statistics */
    memset(&pwm->stats, 0, sizeof(pwm->stats));

    return pwm;
}

void phonological_wm_destroy(phonological_wm_t* pwm) {
    if (pwm) {
        nimcp_free(pwm);
        pwm = NULL;
    }
}

/* ============================================================================
 * ENCODING API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_encode(
    phonological_wm_t* pwm,
    const char* word,
    phonological_trace_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_encode: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_encode: word is NULL");

    /* Convert word to phonemes */
    phoneme_t phonemes[LINGUISTICS_MAX_PHONEME_SEQUENCE];
    uint32_t num_phonemes = 0;

    simple_grapheme_to_phoneme(word, phonemes, LINGUISTICS_MAX_PHONEME_SEQUENCE, &num_phonemes);

    return phonological_wm_encode_phonemes(pwm, word, phonemes, num_phonemes, out);
}

int phonological_wm_encode_phonemes(
    phonological_wm_t* pwm,
    const char* word,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    phonological_trace_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_encode_phonemes: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(phonemes != NULL || num_phonemes == 0, LING_ERR_NULL_POINTER,
        "phonological_wm_encode_phonemes: phonemes is NULL");

    uint64_t current_time = get_time_ms();

    /* Find slot for new trace */
    uint32_t slot = 0;
    if (pwm->count < pwm->effective_capacity) {
        slot = pwm->count++;
    } else {
        /* Buffer full - displace weakest item */
        slot = find_weakest_slot(pwm);
        pwm->stats.overflow_events++;
    }

    /* Create trace */
    phonological_trace_t* trace = &pwm->buffer[slot];
    memset(trace, 0, sizeof(*trace));

    /* Copy phoneme sequence */
    trace->length = (num_phonemes < LINGUISTICS_MAX_PHONEME_SEQUENCE) ?
                    num_phonemes : LINGUISTICS_MAX_PHONEME_SEQUENCE;
    memcpy(trace->phonemes, phonemes, trace->length * sizeof(phoneme_t));

    /* Set default durations (100ms per phoneme) */
    for (uint32_t i = 0; i < trace->length; i++) {
        trace->durations[i] = 100.0f;
    }

    /* Copy word */
    if (word) {
        strncpy(trace->word, word, LINGUISTICS_MAX_WORD_LENGTH - 1);
        trace->word[LINGUISTICS_MAX_WORD_LENGTH - 1] = '\0';
        trace->word_length = (uint32_t)strlen(trace->word);
    }

    /* Set trace state */
    trace->activation = MAX_ACTIVATION;
    trace->decay_rate = pwm->effective_decay_rate;
    trace->encode_time_ms = current_time;
    trace->last_rehearse_ms = current_time;

    /* Word length effect: longer words have faster decay */
    if (pwm->config.enable_word_length_effect && trace->length > 5) {
        trace->decay_rate *= (1.0f + (trace->length - 5) * 0.1f);
    }

    /* Similarity susceptibility based on number of similar items in buffer */
    trace->similarity_susceptibility = 0.0f;
    if (pwm->config.enable_similarity_effect) {
        for (uint32_t i = 0; i < pwm->count; i++) {
            if (i == slot) continue;

            /* Check if similar phonemes */
            uint32_t shared_phonemes = 0;
            for (uint32_t p1 = 0; p1 < trace->length; p1++) {
                for (uint32_t p2 = 0; p2 < pwm->buffer[i].length; p2++) {
                    if (trace->phonemes[p1] == pwm->buffer[i].phonemes[p2]) {
                        shared_phonemes++;
                        break;
                    }
                }
            }
            float sim = (float)shared_phonemes / (float)trace->length;
            if (sim > trace->similarity_susceptibility) {
                trace->similarity_susceptibility = sim;
            }
        }
    }

    /* Update statistics */
    pwm->stats.words_encoded++;
    if (pwm->count > pwm->stats.peak_occupancy) {
        pwm->stats.peak_occupancy = pwm->count;
    }

    /* Return copy if requested */
    if (out) {
        *out = *trace;
    }

    return LING_ERR_OK;
}

int phonological_wm_word_to_phonemes(
    const phonological_wm_t* pwm,
    const char* word,
    phoneme_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_phonemes
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_word_to_phonemes: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_word_to_phonemes: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(phonemes != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_word_to_phonemes: phonemes is NULL");
    NIMCP_CHECK_THROW_IMMUNE(num_phonemes != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_word_to_phonemes: num_phonemes is NULL");

    (void)pwm; /* Configuration could affect conversion in future */

    simple_grapheme_to_phoneme(word, phonemes, max_phonemes, num_phonemes);

    return LING_ERR_OK;
}

/* ============================================================================
 * REHEARSAL API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_rehearse(phonological_wm_t* pwm) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_rehearse: pwm is NULL");

    if (pwm->count == 0) {
        return LING_ERR_OK;
    }

    uint64_t current_time = get_time_ms();

    /* Rehearse all items */
    for (uint32_t i = 0; i < pwm->count; i++) {
        phonological_trace_t* trace = &pwm->buffer[i];

        /* Refresh activation */
        trace->activation = MAX_ACTIVATION;
        trace->last_rehearse_ms = current_time;
    }

    pwm->last_rehearsal_ms = current_time;
    pwm->stats.rehearsals_triggered++;

    return LING_ERR_OK;
}

int phonological_wm_rehearse_item(
    phonological_wm_t* pwm,
    uint32_t index
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_rehearse_item: pwm is NULL");

    if (index >= pwm->count) {
        set_last_error("Index %u out of range (count=%u)", index, pwm->count);
        return LING_ERR_INVALID_PARAM;
    }

    uint64_t current_time = get_time_ms();

    pwm->buffer[index].activation = MAX_ACTIVATION;
    pwm->buffer[index].last_rehearse_ms = current_time;

    return LING_ERR_OK;
}

bool phonological_wm_needs_rehearsal(const phonological_wm_t* pwm) {
    if (!pwm || pwm->count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phonological_wm_needs_rehearsal: pwm is NULL");
        return false;
    }

    for (uint32_t i = 0; i < pwm->count; i++) {
        if (pwm->buffer[i].activation < 0.5f) {
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "phonological_wm_needs_rehearsal: validation failed");
    return false;
}

int phonological_wm_start_rehearsal(phonological_wm_t* pwm) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_start_rehearsal: pwm is NULL");

    pwm->is_rehearsing = true;
    pwm->rehearsal_position = 0;

    return LING_ERR_OK;
}

int phonological_wm_stop_rehearsal(phonological_wm_t* pwm) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_stop_rehearsal: pwm is NULL");

    pwm->is_rehearsing = false;

    return LING_ERR_OK;
}

/* ============================================================================
 * DECAY & UPDATE API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_update(
    phonological_wm_t* pwm,
    uint32_t elapsed_ms
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_update: pwm is NULL");

    int forgotten = 0;
    uint64_t current_time = get_time_ms();

    /* Apply decay to all traces */
    for (uint32_t i = 0; i < pwm->count; i++) {
        phonological_trace_t* trace = &pwm->buffer[i];

        /* Exponential decay */
        float decay = trace->decay_rate * (float)elapsed_ms;
        trace->activation -= decay;

        /* Similarity interference */
        if (pwm->config.enable_similarity_effect && trace->similarity_susceptibility > 0.0f) {
            trace->activation -= trace->similarity_susceptibility * decay * 0.5f;
        }

        /* Clamp */
        if (trace->activation < ACTIVATION_THRESHOLD) {
            trace->activation = MIN_ACTIVATION;
        }
    }

    /* Remove forgotten items (activation = 0) */
    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < pwm->count; read_idx++) {
        if (pwm->buffer[read_idx].activation > MIN_ACTIVATION) {
            if (write_idx != read_idx) {
                pwm->buffer[write_idx] = pwm->buffer[read_idx];
            }
            write_idx++;
        } else {
            forgotten++;
            pwm->stats.words_forgotten++;

            /* Track average lifetime */
            uint64_t lifetime = current_time - pwm->buffer[read_idx].encode_time_ms;
            pwm->stats.avg_trace_lifetime_ms =
                (pwm->stats.avg_trace_lifetime_ms * pwm->stats.words_forgotten +
                 (float)lifetime) / (pwm->stats.words_forgotten + 1);
        }
    }
    pwm->count = write_idx;

    /* Automatic rehearsal if enabled */
    if (pwm->is_rehearsing && pwm->count > 0) {
        /* Rehearse one item per time slice based on rehearsal rate */
        float items_per_ms = pwm->config.rehearsal_rate / 1000.0f;
        uint32_t items_to_rehearse = (uint32_t)(items_per_ms * elapsed_ms);
        items_to_rehearse = (items_to_rehearse < 1) ? 1 : items_to_rehearse;

        for (uint32_t i = 0; i < items_to_rehearse && pwm->count > 0; i++) {
            pwm->buffer[pwm->rehearsal_position].activation = MAX_ACTIVATION;
            pwm->buffer[pwm->rehearsal_position].last_rehearse_ms = current_time;
            pwm->rehearsal_position = (pwm->rehearsal_position + 1) % pwm->count;
        }
    }

    pwm->last_update_ms = current_time;

    return forgotten;
}

int phonological_wm_get_activation(
    const phonological_wm_t* pwm,
    uint32_t index,
    float* activation
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_activation: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(activation != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_activation: activation is NULL");

    if (index >= pwm->count) {
        return LING_ERR_INVALID_PARAM;
    }

    *activation = pwm->buffer[index].activation;
    return LING_ERR_OK;
}

void phonological_wm_clear(phonological_wm_t* pwm) {
    if (pwm) {
        pwm->count = 0;
        pwm->is_rehearsing = false;
        pwm->rehearsal_position = 0;
    }
}

/* ============================================================================
 * RETRIEVAL API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_retrieve(
    phonological_wm_t* pwm,
    const char* cue,
    phonological_trace_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_retrieve: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(cue != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_retrieve: cue is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_retrieve: out is NULL");

    if (pwm->count == 0) {
        set_last_error("Buffer empty");
        return LING_ERR_BUFFER_EMPTY;
    }

    /* Convert cue to phonemes */
    phoneme_t cue_phonemes[LINGUISTICS_MAX_PHONEME_SEQUENCE];
    uint32_t cue_length = 0;
    simple_grapheme_to_phoneme(cue, cue_phonemes, LINGUISTICS_MAX_PHONEME_SEQUENCE, &cue_length);

    /* Find best matching trace */
    float best_score = -1.0f;
    int32_t best_idx = -1;

    for (uint32_t i = 0; i < pwm->count; i++) {
        phonological_trace_t* trace = &pwm->buffer[i];

        /* Score based on phoneme overlap */
        uint32_t matches = 0;
        uint32_t min_len = (cue_length < trace->length) ? cue_length : trace->length;

        for (uint32_t p = 0; p < min_len; p++) {
            if (cue_phonemes[p] == trace->phonemes[p]) {
                matches++;
            }
        }

        float match_score = (float)matches / (float)cue_length;

        /* Also check string prefix */
        if (strncasecmp(cue, trace->word, strlen(cue)) == 0) {
            match_score += 0.5f;
        }

        /* Weight by activation (recency effect) */
        if (pwm->config.enable_recency_effect) {
            match_score *= trace->activation;
        }

        if (match_score > best_score) {
            best_score = match_score;
            best_idx = (int32_t)i;
        }
    }

    if (best_idx < 0 || best_score < 0.3f) {
        set_last_error("No matching trace found for cue: %s", cue);
        return LING_ERR_UNKNOWN_WORD;
    }

    *out = pwm->buffer[best_idx];
    pwm->stats.words_retrieved++;

    return LING_ERR_OK;
}

int phonological_wm_get_trace(
    const phonological_wm_t* pwm,
    uint32_t index,
    phonological_trace_t* out
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_trace: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(out != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_trace: out is NULL");

    if (index >= pwm->count) {
        return LING_ERR_INVALID_PARAM;
    }

    *out = pwm->buffer[index];
    return LING_ERR_OK;
}

int phonological_wm_find(
    const phonological_wm_t* pwm,
    const char* word,
    uint32_t* index
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_find: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_find: word is NULL");
    NIMCP_CHECK_THROW_IMMUNE(index != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_find: index is NULL");

    for (uint32_t i = 0; i < pwm->count; i++) {
        if (strcasecmp(word, pwm->buffer[i].word) == 0) {
            *index = i;
            return LING_ERR_OK;
        }
    }

    return LING_ERR_UNKNOWN_WORD;
}

bool phonological_wm_contains(
    const phonological_wm_t* pwm,
    const char* word
) {
    uint32_t idx = 0;
    return phonological_wm_find(pwm, word, &idx) == LING_ERR_OK;
}

/* ============================================================================
 * SIMILARITY API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_similarity(
    const phonological_wm_t* pwm,
    const char* word_a,
    const char* word_b,
    similarity_result_t* result
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_similarity: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word_a != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_similarity: word_a is NULL");
    NIMCP_CHECK_THROW_IMMUNE(word_b != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_similarity: word_b is NULL");
    NIMCP_CHECK_THROW_IMMUNE(result != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_similarity: result is NULL");

    /* Convert to phonemes */
    phoneme_t ph_a[LINGUISTICS_MAX_PHONEME_SEQUENCE];
    phoneme_t ph_b[LINGUISTICS_MAX_PHONEME_SEQUENCE];
    uint32_t len_a, len_b;

    simple_grapheme_to_phoneme(word_a, ph_a, LINGUISTICS_MAX_PHONEME_SEQUENCE, &len_a);
    simple_grapheme_to_phoneme(word_b, ph_b, LINGUISTICS_MAX_PHONEME_SEQUENCE, &len_b);

    /* Compute average phoneme similarity */
    float total_sim = 0.0f;
    uint32_t comparisons = 0;
    uint32_t feature_overlap = 0;

    uint32_t max_len = (len_a > len_b) ? len_a : len_b;
    uint32_t min_len = (len_a < len_b) ? len_a : len_b;

    for (uint32_t i = 0; i < min_len; i++) {
        float sim = compute_phoneme_similarity(ph_a[i], ph_b[i]);
        total_sim += sim;
        comparisons++;
        if (sim > 0.7f) feature_overlap++;
    }

    /* Penalize length difference */
    if (max_len > min_len) {
        total_sim *= (float)min_len / (float)max_len;
    }

    result->similarity = (comparisons > 0) ? total_sim / comparisons : 0.0f;
    result->feature_overlap = feature_overlap;
    result->total_features = comparisons;
    result->will_interfere = result->similarity > PHONOLOGICAL_WM_SIMILARITY_THRESHOLD;

    return LING_ERR_OK;
}

float phonological_wm_phoneme_similarity(
    const phonological_wm_t* pwm,
    phoneme_t phoneme_a,
    phoneme_t phoneme_b
) {
    (void)pwm;
    return compute_phoneme_similarity(phoneme_a, phoneme_b);
}

int phonological_wm_get_phoneme_features(
    const phonological_wm_t* pwm,
    phoneme_t phoneme,
    phoneme_features_t* features
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_phoneme_features: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(features != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_phoneme_features: features is NULL");

    (void)pwm;
    get_phoneme_features(phoneme, features);

    return LING_ERR_OK;
}

/* ============================================================================
 * BUFFER STATE API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_get_state(
    const phonological_wm_t* pwm,
    buffer_state_t* state
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_state: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(state != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_state: state is NULL");

    state->item_count = pwm->count;
    state->capacity = pwm->effective_capacity;
    state->is_rehearsing = pwm->is_rehearsing;
    state->rehearsal_position = pwm->rehearsal_position;

    state->total_activation = 0.0f;
    state->oldest_activation = MAX_ACTIVATION;

    for (uint32_t i = 0; i < pwm->count; i++) {
        state->total_activation += pwm->buffer[i].activation;
        if (pwm->buffer[i].activation < state->oldest_activation) {
            state->oldest_activation = pwm->buffer[i].activation;
        }
    }

    state->avg_activation = (pwm->count > 0) ? state->total_activation / pwm->count : 0.0f;

    return LING_ERR_OK;
}

uint32_t phonological_wm_count(const phonological_wm_t* pwm) {
    return pwm ? pwm->count : 0;
}

uint32_t phonological_wm_capacity(const phonological_wm_t* pwm) {
    return pwm ? pwm->effective_capacity : 0;
}

bool phonological_wm_is_full(const phonological_wm_t* pwm) {
    return pwm && pwm->count >= pwm->effective_capacity;
}

/* ============================================================================
 * MESH INTEGRATION API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_mesh_process(
    phonological_wm_t* pwm,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_mesh_process: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(request != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_mesh_process: request is NULL");
    NIMCP_CHECK_THROW_IMMUNE(belief != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_mesh_process: belief is NULL");

    phonological_trace_t trace;
    int result = LING_ERR_OK;

    switch (request->type) {
        case LING_REQUEST_ENCODE_PHONOLOGICAL:
            result = phonological_wm_encode(pwm, request->input_word, &trace);
            break;

        case LING_REQUEST_RETRIEVE:
            result = phonological_wm_retrieve(pwm, request->input_word, &trace);
            break;

        case LING_REQUEST_REHEARSE:
            result = phonological_wm_rehearse(pwm);
            memset(&trace, 0, sizeof(trace));
            break;

        default:
            return LING_ERR_INVALID_PARAM;
    }

    /* Populate belief */
    belief->belief_id = (uint32_t)(request->request_id & 0xFFFFFFFF);
    belief->source_module_id = BIO_MODULE_PHONOLOGICAL_WM;
    snprintf(belief->topic, sizeof(belief->topic),
             "phonological_%s", request->input_word);

    if (result == LING_ERR_OK) {
        belief->certainty = trace.activation;
        belief->precision = pwm->current_precision;

        /* Encode trace into belief vector */
        belief->vector_dim = 8;
        belief->belief_vector[0] = trace.activation;
        belief->belief_vector[1] = (float)trace.length / LINGUISTICS_MAX_PHONEME_SEQUENCE;
        belief->belief_vector[2] = trace.similarity_susceptibility;
        belief->belief_vector[3] = (float)pwm->count / pwm->effective_capacity;

        /* First few phonemes */
        for (uint32_t i = 0; i < 4 && i < trace.length; i++) {
            belief->belief_vector[4 + i] = (float)trace.phonemes[i] / (PHONEME_COUNT > 0 ? PHONEME_COUNT : 1);
        }
    } else {
        belief->certainty = 0.0f;
        belief->precision = LINGUISTICS_PRECISION_FLOOR;
        belief->vector_dim = 0;
    }

    belief->timestamp_ms = request->timestamp_ms;

    return result;
}

int phonological_wm_mesh_update(
    phonological_wm_t* pwm,
    const linguistics_belief_t* neighbor_beliefs,
    uint32_t neighbor_count,
    linguistics_belief_t* updated_belief
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_mesh_update: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(updated_belief != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_mesh_update: updated_belief is NULL");

    if (neighbor_count == 0 || !neighbor_beliefs) {
        return LING_ERR_OK;
    }

    float lr = LINGUISTICS_FEP_LEARNING_RATE;

    for (uint32_t i = 0; i < neighbor_count; i++) {
        const linguistics_belief_t* neighbor = &neighbor_beliefs[i];

        if (neighbor->vector_dim == 0 ||
            neighbor->vector_dim != updated_belief->vector_dim) {
            continue;
        }

        float precision = fminf(neighbor->precision, LINGUISTICS_PRECISION_CEILING);
        precision = fmaxf(precision, LINGUISTICS_PRECISION_FLOOR);

        for (uint32_t j = 0; j < updated_belief->vector_dim; j++) {
            float error = neighbor->belief_vector[j] - updated_belief->belief_vector[j];
            float delta = lr * precision * error;
            updated_belief->belief_vector[j] += delta;
        }

        float weight = precision / (precision + updated_belief->precision);
        updated_belief->certainty = (1.0f - weight) * updated_belief->certainty +
                                     weight * neighbor->certainty;
    }

    return LING_ERR_OK;
}

float phonological_wm_get_precision(const phonological_wm_t* pwm) {
    if (!pwm) return LINGUISTICS_PRECISION_FLOOR;
    return pwm->current_precision;
}

int phonological_wm_get_mesh_handler(
    phonological_wm_t* pwm,
    linguistics_mesh_handler_t* handler
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_mesh_handler: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(handler != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_mesh_handler: handler is NULL");

    handler->process = (int (*)(void*, const linguistics_request_t*, linguistics_belief_t*))
                       phonological_wm_mesh_process;
    handler->update = (int (*)(void*, const linguistics_belief_t*, uint32_t, linguistics_belief_t*))
                      phonological_wm_mesh_update;
    handler->get_precision = (float (*)(void*))phonological_wm_get_precision;
    handler->ctx = pwm;

    return LING_ERR_OK;
}

/* ============================================================================
 * MODULATION API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_set_inflammation(
    phonological_wm_t* pwm,
    float level
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_set_inflammation: pwm is NULL");

    if (level < 0.0f || level > 1.0f) {
        set_last_error("Inflammation level must be in [0,1]: %f", level);
        return LING_ERR_INVALID_PARAM;
    }

    pwm->inflammation_level = level;
    update_effective_params(pwm);
    return LING_ERR_OK;
}

int phonological_wm_set_fatigue(
    phonological_wm_t* pwm,
    float level
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_set_fatigue: pwm is NULL");

    if (level < 0.0f || level > 1.0f) {
        set_last_error("Fatigue level must be in [0,1]: %f", level);
        return LING_ERR_INVALID_PARAM;
    }

    pwm->fatigue_level = level;
    update_effective_params(pwm);
    return LING_ERR_OK;
}

int phonological_wm_set_arousal(
    phonological_wm_t* pwm,
    float level
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_set_arousal: pwm is NULL");

    if (level < 0.0f || level > 1.0f) {
        set_last_error("Arousal level must be in [0,1]: %f", level);
        return LING_ERR_INVALID_PARAM;
    }

    pwm->arousal_level = level;
    update_effective_params(pwm);
    return LING_ERR_OK;
}

/* ============================================================================
 * STATISTICS API IMPLEMENTATION
 * ============================================================================ */

int phonological_wm_get_stats(
    const phonological_wm_t* pwm,
    phonological_wm_stats_t* stats
) {
    NIMCP_CHECK_THROW_IMMUNE(pwm != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_stats: pwm is NULL");
    NIMCP_CHECK_THROW_IMMUNE(stats != NULL, LING_ERR_NULL_POINTER,
        "phonological_wm_get_stats: stats is NULL");

    *stats = pwm->stats;
    return LING_ERR_OK;
}

void phonological_wm_reset_stats(phonological_wm_t* pwm) {
    if (pwm) {
        memset(&pwm->stats, 0, sizeof(pwm->stats));
    }
}

const char* phonological_wm_get_last_error(void) {
    return g_last_error;
}

/* ============================================================================
 * UTILITY API IMPLEMENTATION
 * ============================================================================ */

const char* phonological_wm_phoneme_name(phoneme_t phoneme) {
    if (phoneme >= PHONEME_COUNT) {
        return "UNK";
    }
    return g_phoneme_names[phoneme];
}

const char* phonological_wm_phoneme_ipa(phoneme_t phoneme) {
    if (phoneme >= PHONEME_COUNT) {
        return "?";
    }
    return g_phoneme_ipa[phoneme];
}
