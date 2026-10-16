/**
 * @file nimcp_formant_synth.c
 * @brief Biological formant synthesizer — Athena's voice
 *
 * Klatt-style source-filter synthesis with trainable parameters.
 * Emotional prosody modulates pitch, rate, intensity, and voice quality.
 */

#include "audio/synthesis/nimcp_formant_synth.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(formant_synth)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * DEFAULT FORMANT DATA (initial values before training)
 *
 * These are the starting weights for voice_params_t. Training refines them.
 * Values from Peterson & Barney (1952) for General American English.
 *===========================================================================*/

/* Vowel defaults: [F1, F2, F3, bandwidth, duration_ms] */
static const float DEFAULT_VOWEL_PARAMS[12][SYNTH_VOWEL_PARAMS] = {
    /* IY */ { 270, 2290, 3010,  60,  80 },
    /* IH */ { 390, 1990, 2550,  70,  70 },
    /* EY */ { 530, 1840, 2480,  80,  90 },
    /* EH */ { 660, 1720, 2410,  90,  75 },
    /* AE */ { 730, 1090, 2440, 100,  90 },
    /* AA */ { 570,  840, 2410, 100, 100 },
    /* AO */ { 440, 1020, 2240,  90,  95 },
    /* OW */ { 370,  950, 2670,  80,  90 },
    /* UH */ { 440, 1020, 2240,  80,  70 },
    /* UW */ { 300,  870, 2240,  70,  85 },
    /* AH */ { 640, 1190, 2390,  90,  70 },
    /* ER */ { 490, 1350, 1690,  90,  90 },
};

/* Consonant defaults: [noise_freq, noise_bw, burst_amp, burst_dur, duration, voiced_mix]
 * Index 0-5: stops (P,B,T,D,K,G), 6-14: fricatives, 15-17: nasals,
 * 18-21: approximants, 22-23: affricates, 24-25: special */
static const float DEFAULT_CONSONANT_PARAMS[26][SYNTH_CONSONANT_PARAMS] = {
    /* P  */ {  500, 400, 0.7f, 10, 80, 0.0f },
    /* B  */ {  500, 400, 0.5f,  8, 70, 1.0f },
    /* T  */ { 3000, 800, 0.8f, 10, 70, 0.0f },
    /* D  */ { 3000, 800, 0.5f,  8, 60, 1.0f },
    /* K  */ { 2000, 600, 0.7f, 12, 80, 0.0f },
    /* G  */ { 2000, 600, 0.5f, 10, 70, 1.0f },
    /* F  */ { 7000,2000, 0.0f,  0,100, 0.0f },
    /* V  */ { 7000,2000, 0.0f,  0, 80, 0.6f },
    /* TH */ { 6000,1500, 0.0f,  0,100, 0.0f },
    /* DH */ { 6000,1500, 0.0f,  0, 70, 0.6f },
    /* S  */ { 5000,1200, 0.0f,  0,120, 0.0f },
    /* Z  */ { 5000,1200, 0.0f,  0,100, 0.6f },
    /* SH */ { 3000,1000, 0.0f,  0,120, 0.0f },
    /* ZH */ { 3000,1000, 0.0f,  0, 90, 0.6f },
    /* H  */ { 1500,2000, 0.0f,  0, 60, 0.0f },
    /* M  */ {    0,   0, 0.0f,  0, 80, 1.0f },
    /* N  */ {    0,   0, 0.0f,  0, 70, 1.0f },
    /* NG */ {    0,   0, 0.0f,  0, 80, 1.0f },
    /* L  */ {    0,   0, 0.0f,  0, 80, 1.0f },
    /* R  */ {    0,   0, 0.0f,  0, 80, 1.0f },
    /* W  */ {    0,   0, 0.0f,  0, 60, 1.0f },
    /* Y  */ {    0,   0, 0.0f,  0, 60, 1.0f },
    /* CH */ { 3500, 800, 0.6f, 10,100, 0.0f },
    /* JH */ { 3500, 800, 0.4f,  8, 90, 0.6f },
    /* SIL*/ {    0,   0, 0.0f,  0,100, 0.0f },
    /* UNK*/ {    0,   0, 0.0f,  0, 50, 0.0f },
};

/* Default emotion prosody modifiers: [pitch_scale, rate_scale, intensity_scale, breathiness]
 * Maps to emotion_type_t: NEUTRAL, HAPPY, SAD, ANGRY, FEARFUL, SURPRISED, DISGUSTED, CONTEMPTUOUS */
static const float DEFAULT_EMOTION_PROSODY[8][SYNTH_PROSODY_PARAMS] = {
    /* NEUTRAL    */ { 1.00f, 1.00f, 1.00f, 0.03f },
    /* HAPPY      */ { 1.20f, 1.10f, 1.15f, 0.02f },
    /* SAD        */ { 0.85f, 0.80f, 0.75f, 0.10f },
    /* ANGRY      */ { 1.10f, 1.15f, 1.40f, 0.01f },
    /* FEARFUL    */ { 1.30f, 1.25f, 0.90f, 0.08f },
    /* SURPRISED  */ { 1.40f, 1.05f, 1.20f, 0.04f },
    /* DISGUSTED  */ { 0.95f, 0.90f, 1.10f, 0.02f },
    /* CONTEMPT   */ { 0.90f, 0.85f, 0.95f, 0.05f },
};

/* Nasal anti-formant frequencies */
static const float NASAL_ZERO_FREQ[3] = { 300.0f, 350.0f, 280.0f }; /* M, N, NG */
static const float NASAL_POLE_FREQ[3] = { 250.0f, 300.0f, 250.0f };

/* Default F3-F5 for consonants (used when formant targets needed) */
static const float DEFAULT_F3 = 2500.0f;
static const float DEFAULT_F4 = 3500.0f;
static const float DEFAULT_F5 = 4500.0f;
static const float DEFAULT_BW = 100.0f;

/* Viseme mapping from speech_cortex phoneme_t index */
static const synth_viseme_t PHONEME_VISEME_MAP[38] = {
    /* Vowels */
    SYNTH_VISEME_UNROUNDED_CLOSE,   /* IY - spread lips */
    SYNTH_VISEME_UNROUNDED_CLOSE,   /* IH */
    SYNTH_VISEME_UNROUNDED_MID,     /* EY */
    SYNTH_VISEME_UNROUNDED_MID,     /* EH */
    SYNTH_VISEME_UNROUNDED_OPEN,    /* AE - wide open */
    SYNTH_VISEME_UNROUNDED_OPEN,    /* AA */
    SYNTH_VISEME_ROUNDED_OPEN,      /* AO - rounded, open */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* OW - rounded, close */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* UH */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* UW */
    SYNTH_VISEME_UNROUNDED_MID,     /* AH */
    SYNTH_VISEME_UNROUNDED_MID,     /* ER */
    /* Stops */
    SYNTH_VISEME_BILABIAL,          /* P */
    SYNTH_VISEME_BILABIAL,          /* B */
    SYNTH_VISEME_ALVEOLAR,          /* T */
    SYNTH_VISEME_ALVEOLAR,          /* D */
    SYNTH_VISEME_VELAR,             /* K */
    SYNTH_VISEME_VELAR,             /* G */
    /* Fricatives */
    SYNTH_VISEME_LABIODENTAL,       /* F */
    SYNTH_VISEME_LABIODENTAL,       /* V */
    SYNTH_VISEME_DENTAL,            /* TH */
    SYNTH_VISEME_DENTAL,            /* DH */
    SYNTH_VISEME_ALVEOLAR,          /* S */
    SYNTH_VISEME_ALVEOLAR,          /* Z */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* SH - slight rounding */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* ZH */
    SYNTH_VISEME_UNROUNDED_MID,     /* H - neutral */
    /* Nasals */
    SYNTH_VISEME_BILABIAL,          /* M */
    SYNTH_VISEME_ALVEOLAR,          /* N */
    SYNTH_VISEME_VELAR,             /* NG */
    /* Approximants */
    SYNTH_VISEME_ALVEOLAR,          /* L */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* R - slight rounding */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* W - rounded */
    SYNTH_VISEME_UNROUNDED_CLOSE,   /* Y - spread */
    /* Affricates */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* CH */
    SYNTH_VISEME_ROUNDED_CLOSE,     /* JH */
    /* Special */
    SYNTH_VISEME_SILENCE,           /* SILENCE */
    SYNTH_VISEME_SILENCE,           /* UNKNOWN */
};

/* Viseme → articulatory params: [mouth_aperture, lip_rounding, jaw_open] */
static const float VISEME_ARTICULATION[SYNTH_VISEME_COUNT][3] = {
    /* BILABIAL       */ { 0.00f, 0.00f, 0.05f },
    /* LABIODENTAL    */ { 0.10f, 0.00f, 0.10f },
    /* DENTAL         */ { 0.15f, 0.00f, 0.15f },
    /* ALVEOLAR       */ { 0.20f, 0.00f, 0.15f },
    /* VELAR          */ { 0.30f, 0.00f, 0.25f },
    /* ROUNDED_CLOSE  */ { 0.25f, 0.80f, 0.20f },
    /* ROUNDED_OPEN   */ { 0.60f, 0.60f, 0.50f },
    /* UNROUNDED_CLOSE*/ { 0.20f, 0.00f, 0.15f },
    /* UNROUNDED_MID  */ { 0.40f, 0.10f, 0.30f },
    /* UNROUNDED_OPEN */ { 0.80f, 0.00f, 0.70f },
    /* SILENCE        */ { 0.00f, 0.00f, 0.00f },
};

/*=============================================================================
 * SIGNAL PROCESSING PRIMITIVES
 *===========================================================================*/

uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

float noise_sample(uint32_t* seed) {
    /* Uniform [-1, 1] from xorshift32 */
    uint32_t x = xorshift32(seed);
    return ((float)(x & 0x7FFFFFFF) / (float)0x3FFFFFFF) - 1.0f;
}

float glottal_pulse(float phase, float open_quotient) {
    /* Rosenberg polynomial glottal pulse model
     * Opening phase (0 to Oq): 3t² - 2t³ (smooth rise)
     * Closing phase (Oq to 1): rapid linear decay
     * Closed phase: 0 */
    if (phase < 0.0f || phase >= 1.0f) return 0.0f;

    if (phase < open_quotient) {
        /* Opening phase: smooth polynomial rise */
        float t = phase / open_quotient;
        return t * t * (3.0f - 2.0f * t);
    } else if (phase < open_quotient + 0.1f) {
        /* Closing phase: rapid linear decay */
        float t = (phase - open_quotient) / 0.1f;
        return 1.0f - t;
    }
    /* Closed phase */
    return 0.0f;
}

void resonator_compute_coefficients(formant_resonator_t* res,
                                     uint32_t sample_rate) {
    if (!res || sample_rate == 0) return;

    /* Prevent unstable coefficients */
    float freq = res->freq_hz;
    float bw = res->bandwidth_hz;
    if (freq <= 0.0f) freq = 100.0f;
    if (bw <= 0.0f) bw = 50.0f;
    if (freq > (float)sample_rate * 0.49f)
        freq = (float)sample_rate * 0.49f;

    /* Two-pole resonator via exp mapping */
    float r = expf(-(float)M_PI * bw / (float)sample_rate);
    float theta = 2.0f * (float)M_PI * freq / (float)sample_rate;

    res->a1 = -2.0f * r * cosf(theta);
    res->a2 = r * r;
    /* Normalize for unity gain at resonance */
    res->b0 = (1.0f - r) * sqrtf(1.0f - 2.0f * r * cosf(2.0f * theta) + r * r);
}

float resonator_tick(formant_resonator_t* res, float input) {
    /* y[n] = b0*x[n] - a1*y[n-1] - a2*y[n-2] */
    float output = res->b0 * input - res->a1 * res->y_1 - res->a2 * res->y_2;

    /* Clamp to prevent blowup */
    if (output > 10.0f) output = 10.0f;
    if (output < -10.0f) output = -10.0f;

    res->y_2 = res->y_1;
    res->y_1 = output;
    return output;
}

synth_viseme_t phoneme_to_viseme(uint32_t phoneme_id) {
    if (phoneme_id >= 38) return SYNTH_VISEME_SILENCE;
    return PHONEME_VISEME_MAP[phoneme_id];
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

formant_synth_config_t formant_synth_default_config(void) {
    formant_synth_config_t cfg = {
        .sample_rate = SYNTH_DEFAULT_SAMPLE_RATE,
        .default_f0_hz = 160.0f,    /* Athena: young female voice */
        .glottal_open_quotient = SYNTH_GLOTTAL_OQ_DEFAULT,
        .aspiration_baseline = 0.03f,
        .coarticulation_ms = SYNTH_COARTICULATION_MS,
        .enable_prosody_modulation = true,
        .enable_trainable_params = true,
        .language = SYNTH_LANG_ENGLISH_US,
    };
    return cfg;
}

void formant_synth_init_voice_defaults(formant_synth_t* synth) {
    if (!synth) return;

    for (int lang = 0; lang < SYNTH_LANG_COUNT; lang++) {
        voice_params_t* v = &synth->voice[lang];

        /* Copy default vowel and consonant params */
        memcpy(v->vowel_params, DEFAULT_VOWEL_PARAMS, sizeof(DEFAULT_VOWEL_PARAMS));
        memcpy(v->consonant_params, DEFAULT_CONSONANT_PARAMS, sizeof(DEFAULT_CONSONANT_PARAMS));
        memcpy(v->emotion_prosody, DEFAULT_EMOTION_PROSODY, sizeof(DEFAULT_EMOTION_PROSODY));

        v->base_f0_hz = synth->config.default_f0_hz;
        v->f0_variance = 0.02f;
        v->breathiness = synth->config.aspiration_baseline;
        v->nasality = 0.0f;
        v->training_steps = 0;
        v->learning_rate = 0.001f;
        v->frozen = false;
    }

    /* Accent-specific formant shifts (initial hints, refined by training) */

    /* British English: slightly different vowel space */
    voice_params_t* uk = &synth->voice[SYNTH_LANG_ENGLISH_UK];
    uk->vowel_params[5][0] = 630;   /* AA: more open in RP */
    uk->vowel_params[5][1] = 900;
    uk->vowel_params[6][0] = 370;   /* AO: more rounded */
    uk->base_f0_hz = synth->config.default_f0_hz * 0.95f;

    /* Australian: raised F1 on some vowels */
    voice_params_t* au = &synth->voice[SYNTH_LANG_ENGLISH_AU];
    au->vowel_params[4][0] = 690;   /* AE: slightly higher */
    au->vowel_params[2][0] = 480;   /* EY: more centralized */

    /* Mandarin: tone contour has more range */
    voice_params_t* zh = &synth->voice[SYNTH_LANG_MANDARIN];
    zh->emotion_prosody[0][0] = 1.0f;   /* Neutral pitch_scale */
    zh->emotion_prosody[0][1] = 0.85f;  /* Slower rate for tones */

    /* French: more nasal, different vowel space */
    voice_params_t* fr = &synth->voice[SYNTH_LANG_FRENCH];
    fr->nasality = 0.15f;
    fr->vowel_params[9][0] = 280;  /* UW: more front-rounded in French */
    fr->vowel_params[9][1] = 1200;
}

formant_synth_t* formant_synth_create(const formant_synth_config_t* config) {
    formant_synth_config_t cfg = config ? *config : formant_synth_default_config();

    formant_synth_t* synth = (formant_synth_t*)nimcp_calloc(1, sizeof(formant_synth_t));
    if (!synth) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "formant_synth_create: failed to allocate formant_synth_t");
        return NULL;
    }

    synth->config = cfg;
    synth->active_language = cfg.language;
    synth->noise_seed = 0xDEADBEEF;
    synth->current_voice_quality = SYNTH_VOICE_NORMAL;
    synth->current_arousal = 0.5f;
    synth->current_valence = 0.0f;

    /* Allocate output buffer */
    synth->output_capacity = SYNTH_MAX_SAMPLES_PER_WORD * 4; /* Up to 4 words */
    synth->output_buffer = (float*)nimcp_calloc(synth->output_capacity, sizeof(float));
    if (!synth->output_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "formant_synth_create: failed to allocate output buffer");
        nimcp_free(synth);
        return NULL;
    }

    /* Allocate viseme buffer */
    synth->viseme_capacity = SYNTH_MAX_PHONEMES_PER_WORD * 4;
    synth->viseme_buffer = (viseme_event_t*)nimcp_calloc(synth->viseme_capacity,
                                                          sizeof(viseme_event_t));
    if (!synth->viseme_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "formant_synth_create: failed to allocate viseme buffer");
        nimcp_free(synth->output_buffer);
        nimcp_free(synth);
        return NULL;
    }

    /* Initialize default voice parameters */
    formant_synth_init_voice_defaults(synth);

    /* Initialize resonators to neutral vowel (schwa) */
    float default_freqs[5] = { 500, 1500, 2500, 3500, 4500 };
    for (int i = 0; i < SYNTH_MAX_FORMANTS; i++) {
        synth->resonators[i].freq_hz = default_freqs[i];
        synth->resonators[i].bandwidth_hz = DEFAULT_BW;
        synth->resonators[i].y_1 = 0.0f;
        synth->resonators[i].y_2 = 0.0f;
        resonator_compute_coefficients(&synth->resonators[i], cfg.sample_rate);
    }

    bbb_register_module("formant_synth", BBB_MODULE_TYPE_COGNITIVE);
    bbb_audit_log(BBB_AUDIT_INFO, "formant_synth", "created",
                  "sample_rate=%u", synth->config.sample_rate);

    LOG_INFO("Formant synthesizer created: %dHz, base_f0=%.0fHz, lang=%d",
             cfg.sample_rate, cfg.default_f0_hz, cfg.language);
    return synth;
}

void formant_synth_destroy(formant_synth_t* synth) {
    if (!synth) return;
    bbb_audit_log(BBB_AUDIT_INFO, "formant_synth", "destroyed",
                  "phonemes=%lu words=%lu samples=%lu",
                  (unsigned long)synth->phonemes_synthesized,
                  (unsigned long)synth->words_synthesized,
                  (unsigned long)synth->samples_generated);
    nimcp_free(synth->output_buffer);
    nimcp_free(synth->viseme_buffer);
    nimcp_free(synth);
}

/*=============================================================================
 * EMOTIONAL PROSODY
 *===========================================================================*/

void formant_synth_set_prosody(formant_synth_t* synth,
                                struct emotional_prosody* prosody) {
    if (synth) synth->prosody = prosody;
}

void formant_synth_set_emotion(formant_synth_t* synth,
                                float arousal, float valence,
                                int primary_emotion) {
    if (!synth) return;
    synth->current_arousal = arousal;
    synth->current_valence = valence;

    /* Map emotion to voice quality */
    if (primary_emotion >= 0 && primary_emotion < 8) {
        switch (primary_emotion) {
            case 2: /* SAD */
                synth->current_voice_quality = SYNTH_VOICE_BREATHY;
                break;
            case 3: /* ANGRY */
                synth->current_voice_quality = SYNTH_VOICE_TENSE;
                break;
            case 4: /* FEARFUL */
                synth->current_voice_quality = SYNTH_VOICE_BREATHY;
                break;
            default:
                if (arousal < 0.3f)
                    synth->current_voice_quality = SYNTH_VOICE_LAX;
                else if (arousal > 0.8f)
                    synth->current_voice_quality = SYNTH_VOICE_TENSE;
                else
                    synth->current_voice_quality = SYNTH_VOICE_NORMAL;
                break;
        }
    }
}

void formant_synth_set_language(formant_synth_t* synth, synth_language_t lang) {
    if (synth && lang < SYNTH_LANG_COUNT) {
        synth->active_language = lang;
    }
}

synth_language_t formant_synth_get_language(const formant_synth_t* synth) {
    return synth ? synth->active_language : SYNTH_LANG_ENGLISH_US;
}

/*=============================================================================
 * PHONEME → SYNTHESIS TARGET
 *===========================================================================*/

int formant_synth_phoneme_to_target(formant_synth_t* synth,
                                     uint32_t phoneme_id,
                                     float stress,
                                     synth_target_t* target_out) {
    if (!synth || !target_out || phoneme_id >= 38) return -1;

    voice_params_t* voice = &synth->voice[synth->active_language];
    memset(target_out, 0, sizeof(synth_target_t));
    target_out->phoneme_id = phoneme_id;
    target_out->stress = stress;

    /* Get emotional prosody modifiers */
    int emo = 0; /* NEUTRAL default */
    /* In a full integration, this would come from the brain's emotional state */
    float pitch_scale = voice->emotion_prosody[emo][0];
    float rate_scale = voice->emotion_prosody[emo][1];
    float intensity_scale = voice->emotion_prosody[emo][2];
    float breathiness = voice->emotion_prosody[emo][3];

    /* Apply arousal/valence modulation */
    pitch_scale *= (0.8f + 0.4f * synth->current_arousal);
    rate_scale *= (0.85f + 0.3f * synth->current_arousal);
    intensity_scale *= (0.7f + 0.6f * synth->current_arousal);
    breathiness += (1.0f - synth->current_arousal) * 0.05f;

    target_out->f0_hz = voice->base_f0_hz * pitch_scale;
    /* Add stress-based pitch accent */
    target_out->f0_hz *= (1.0f + stress * 0.15f);
    /* Add natural jitter */
    target_out->f0_hz *= (1.0f + voice->f0_variance *
                          (noise_sample(&synth->noise_seed) * 0.5f));

    /* Voice quality → glottal parameters */
    float oq = synth->config.glottal_open_quotient;
    switch (synth->current_voice_quality) {
        case SYNTH_VOICE_BREATHY: oq = 0.75f; breathiness += 0.08f; break;
        case SYNTH_VOICE_CREAKY:  oq = 0.35f; break;
        case SYNTH_VOICE_TENSE:   oq = 0.40f; breathiness *= 0.3f; break;
        case SYNTH_VOICE_LAX:     oq = 0.70f; break;
        default: break;
    }
    target_out->aspiration = breathiness;

    if (phoneme_id < 12) {
        /* === VOWEL === */
        const float* vp = voice->vowel_params[phoneme_id];
        target_out->voiced = true;
        target_out->duration_ms = vp[4] / rate_scale;
        target_out->intensity = 0.8f * intensity_scale;

        target_out->formant_freq[0] = vp[0];  /* F1 */
        target_out->formant_freq[1] = vp[1];  /* F2 */
        target_out->formant_freq[2] = vp[2];  /* F3 */
        target_out->formant_freq[3] = DEFAULT_F4;
        target_out->formant_freq[4] = DEFAULT_F5;

        target_out->formant_bw[0] = vp[3];
        target_out->formant_bw[1] = vp[3] * 1.2f;
        target_out->formant_bw[2] = vp[3] * 1.5f;
        target_out->formant_bw[3] = 200.0f;
        target_out->formant_bw[4] = 300.0f;

        /* Stress increases intensity and duration */
        target_out->duration_ms *= (1.0f + stress * 0.3f);
        target_out->intensity *= (1.0f + stress * 0.15f);
    } else {
        /* === CONSONANT === */
        uint32_t ci = phoneme_id - 12;
        if (ci >= 26) ci = 25; /* clamp to UNKNOWN */
        const float* cp = voice->consonant_params[ci];

        target_out->duration_ms = cp[4] / rate_scale;
        target_out->intensity = 0.5f * intensity_scale;
        target_out->noise_freq_hz = cp[0];
        target_out->noise_bw_hz = cp[1];
        target_out->burst_amplitude = cp[2];
        target_out->burst_duration_ms = cp[3];
        target_out->voiced = (cp[5] > 0.5f);

        /* Fricatives/affricates: noise excitation */
        bool is_fricative = (phoneme_id >= 18 && phoneme_id <= 26);
        bool is_affricate = (phoneme_id == 34 || phoneme_id == 35);
        bool is_stop = (phoneme_id >= 12 && phoneme_id <= 17);
        bool is_nasal = (phoneme_id >= 27 && phoneme_id <= 29);
        bool is_approx = (phoneme_id >= 30 && phoneme_id <= 33);

        if (is_nasal) {
            target_out->nasal = true;
            uint32_t ni = phoneme_id - 27;
            target_out->nasal_zero_hz = NASAL_ZERO_FREQ[ni];
            target_out->nasal_pole_hz = NASAL_POLE_FREQ[ni];
            /* Nasals have low formants */
            target_out->formant_freq[0] = 250.0f;
            target_out->formant_freq[1] = target_out->nasal_pole_hz;
            target_out->formant_freq[2] = DEFAULT_F3;
            target_out->formant_bw[0] = 60.0f;
            target_out->formant_bw[1] = 80.0f;
            target_out->formant_bw[2] = 150.0f;
        } else if (is_approx) {
            /* Approximants: vowel-like formants */
            target_out->formant_freq[0] = 350.0f;
            target_out->formant_freq[1] = 1200.0f;
            target_out->formant_freq[2] = (phoneme_id == 31) ? 1600.0f : DEFAULT_F3; /* R */
            target_out->formant_bw[0] = 80.0f;
            target_out->formant_bw[1] = 100.0f;
            target_out->formant_bw[2] = 150.0f;
        } else if (is_fricative || is_affricate) {
            /* Use noise params from table */
            target_out->formant_freq[0] = 400.0f;
            target_out->formant_freq[1] = 1500.0f;
            target_out->formant_freq[2] = DEFAULT_F3;
            target_out->formant_bw[0] = 200.0f;
            target_out->formant_bw[1] = 300.0f;
            target_out->formant_bw[2] = 400.0f;
        } else if (is_stop) {
            /* Stops: brief burst then transition to adjacent vowel */
            target_out->formant_freq[0] = 400.0f;
            target_out->formant_freq[1] = 1500.0f;
            target_out->formant_freq[2] = DEFAULT_F3;
            target_out->formant_bw[0] = 200.0f;
            target_out->formant_bw[1] = 300.0f;
            target_out->formant_bw[2] = 400.0f;
        }

        target_out->formant_freq[3] = DEFAULT_F4;
        target_out->formant_freq[4] = DEFAULT_F5;
        target_out->formant_bw[3] = 200.0f;
        target_out->formant_bw[4] = 300.0f;
    }

    /* Apply global nasality */
    if (voice->nasality > 0.1f && !target_out->nasal) {
        target_out->nasal = true;
        target_out->nasal_zero_hz = 300.0f;
        target_out->nasal_pole_hz = 250.0f;
    }

    /* Viseme */
    target_out->viseme = phoneme_to_viseme(phoneme_id);
    if (target_out->viseme < SYNTH_VISEME_COUNT) {
        target_out->mouth_aperture = VISEME_ARTICULATION[target_out->viseme][0];
        target_out->lip_rounding = VISEME_ARTICULATION[target_out->viseme][1];
        target_out->jaw_open = VISEME_ARTICULATION[target_out->viseme][2];
    }

    return 0;
}

/*=============================================================================
 * CORE SYNTHESIS LOOP
 *===========================================================================*/

int formant_synth_render_phoneme(formant_synth_t* synth,
                                  const synth_target_t* target,
                                  float* samples_out,
                                  uint32_t* num_samples_out,
                                  uint32_t max_samples) {
    if (!synth || !target || !samples_out || !num_samples_out) return -1;

    uint32_t sr = synth->config.sample_rate;
    uint32_t duration_samples = (uint32_t)(target->duration_ms * (float)sr / 1000.0f);
    if (duration_samples > max_samples) duration_samples = max_samples;
    if (duration_samples == 0) { *num_samples_out = 0; return 0; }

    float oq = synth->config.glottal_open_quotient;
    /* Adjust Oq for voice quality */
    switch (synth->current_voice_quality) {
        case SYNTH_VOICE_BREATHY: oq = 0.75f; break;
        case SYNTH_VOICE_CREAKY:  oq = 0.35f; break;
        case SYNTH_VOICE_TENSE:   oq = 0.40f; break;
        case SYNTH_VOICE_LAX:     oq = 0.70f; break;
        default: break;
    }

    /* Smoothly update resonator frequencies (coarticulation) */
    uint32_t coart_samples = (uint32_t)(synth->config.coarticulation_ms *
                                         (float)sr / 1000.0f);
    if (coart_samples > duration_samples) coart_samples = duration_samples;

    /* Save previous resonator state for interpolation */
    float prev_freq[SYNTH_MAX_FORMANTS];
    float prev_bw[SYNTH_MAX_FORMANTS];
    for (int f = 0; f < SYNTH_MAX_FORMANTS; f++) {
        prev_freq[f] = synth->resonators[f].freq_hz;
        prev_bw[f] = synth->resonators[f].bandwidth_hz;
    }

    float f0 = target->f0_hz;
    float period_samples = (f0 > 0) ? (float)sr / f0 : (float)sr / 120.0f;

    /* Burst phase for stops */
    uint32_t burst_samples = (uint32_t)(target->burst_duration_ms *
                                         (float)sr / 1000.0f);
    float burst_amp = target->burst_amplitude;

    /* Envelope: attack (5ms) + sustain + release (5ms) */
    uint32_t attack_samples = (uint32_t)(5.0f * (float)sr / 1000.0f);
    uint32_t release_start = duration_samples > attack_samples ?
                             duration_samples - attack_samples : 0;

    for (uint32_t i = 0; i < duration_samples; i++) {
        /* === Coarticulation interpolation === */
        if (i < coart_samples) {
            float t = (float)i / (float)coart_samples;
            for (int f = 0; f < SYNTH_MAX_FORMANTS; f++) {
                synth->resonators[f].freq_hz =
                    prev_freq[f] + t * (target->formant_freq[f] - prev_freq[f]);
                synth->resonators[f].bandwidth_hz =
                    prev_bw[f] + t * (target->formant_bw[f] - prev_bw[f]);
                resonator_compute_coefficients(&synth->resonators[f], sr);
            }
        } else if (i == coart_samples) {
            /* Lock to target */
            for (int f = 0; f < SYNTH_MAX_FORMANTS; f++) {
                synth->resonators[f].freq_hz = target->formant_freq[f];
                synth->resonators[f].bandwidth_hz = target->formant_bw[f];
                resonator_compute_coefficients(&synth->resonators[f], sr);
            }
        }

        /* === Excitation source === */
        float excitation = 0.0f;

        if (i < burst_samples && burst_amp > 0.0f) {
            /* Stop burst: decaying noise transient */
            float decay = 1.0f - (float)i / (float)burst_samples;
            excitation = burst_amp * decay * noise_sample(&synth->noise_seed);
        }

        if (target->voiced) {
            /* Glottal pulse train */
            float pulse = glottal_pulse(synth->glottal_phase, oq);
            excitation += pulse * target->intensity;

            /* Advance glottal phase */
            synth->glottal_phase += 1.0f / period_samples;
            if (synth->glottal_phase >= 1.0f) {
                synth->glottal_phase -= 1.0f;
                /* Add jitter for naturalness */
                synth->glottal_phase += 0.002f * noise_sample(&synth->noise_seed);
            }
        }

        if (target->noise_freq_hz > 0.0f) {
            /* Fricative noise */
            float n = noise_sample(&synth->noise_seed) * 0.5f;
            excitation += n * target->intensity;
        }

        /* Aspiration noise */
        excitation += target->aspiration * noise_sample(&synth->noise_seed) * 0.3f;

        /* === Resonator cascade (F1 → F2 → F3 → F4 → F5) === */
        float sample = excitation;
        for (int f = 0; f < SYNTH_MAX_FORMANTS; f++) {
            if (synth->resonators[f].freq_hz > 0.0f) {
                sample = resonator_tick(&synth->resonators[f], sample);
            }
        }

        /* === Envelope === */
        float env = 1.0f;
        if (i < attack_samples)
            env = (float)i / (float)attack_samples;
        else if (i >= release_start)
            env = 1.0f - (float)(i - release_start) / (float)attack_samples;

        samples_out[i] = sample * env * target->intensity;
    }

    *num_samples_out = duration_samples;
    synth->phonemes_synthesized++;
    return 0;
}

/*=============================================================================
 * HIGH-LEVEL SYNTHESIS API
 *===========================================================================*/

int formant_synth_speak_phonemes(formant_synth_t* synth,
                                  const uint32_t* phoneme_ids,
                                  uint32_t num_phonemes,
                                  speech_output_t* output) {
    if (!synth || !phoneme_ids || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "formant_synth_speak_phonemes: NULL parameter (synth=%p phoneme_ids=%p output=%p)",
            (void*)synth, (const void*)phoneme_ids, (void*)output);
        return -1;
    }
    if (num_phonemes == 0) return -1;

    memset(output, 0, sizeof(speech_output_t));

    /* Allocate output buffers */
    uint32_t max_audio = synth->output_capacity;
    float* audio = synth->output_buffer;
    uint32_t audio_pos = 0;

    viseme_event_t* visemes = synth->viseme_buffer;
    uint32_t vis_count = 0;
    float time_ms = 0.0f;

    synth->output_length = 0;
    synth->viseme_count = 0;

    /* Simple stress pattern: odd syllables get stress */
    uint32_t vowel_count = 0;

    for (uint32_t p = 0; p < num_phonemes && audio_pos < max_audio; p++) {
        uint32_t pid = phoneme_ids[p];

        /* Compute stress */
        float stress = 0.3f; /* default unstressed */
        if (pid < 12) {
            /* Vowel — alternate stress */
            stress = (vowel_count % 2 == 0) ? 0.7f : 0.3f;
            vowel_count++;
        }

        /* Build synthesis target */
        synth_target_t target;
        if (formant_synth_phoneme_to_target(synth, pid, stress, &target) != 0)
            continue;

        /* Render audio */
        uint32_t samples_out = 0;
        uint32_t remaining = max_audio - audio_pos;
        formant_synth_render_phoneme(synth, &target,
                                      &audio[audio_pos], &samples_out,
                                      remaining);

        /* Build viseme event */
        if (vis_count < synth->viseme_capacity) {
            viseme_event_t* ve = &visemes[vis_count];
            ve->viseme = target.viseme;
            ve->onset_ms = time_ms;
            ve->duration_ms = target.duration_ms;
            ve->mouth_aperture = target.mouth_aperture;
            ve->lip_rounding = target.lip_rounding;
            ve->jaw_open = target.jaw_open;
            /* Coarticulation blending */
            ve->blend_in_ms = (p > 0) ? synth->config.coarticulation_ms : 0.0f;
            ve->blend_out_ms = (p < num_phonemes - 1) ?
                                synth->config.coarticulation_ms : 0.0f;
            vis_count++;
        }

        audio_pos += samples_out;
        time_ms += target.duration_ms;
    }

    /* Normalize audio to [-1, 1] */
    float peak = 0.0f;
    for (uint32_t i = 0; i < audio_pos; i++) {
        float abs_val = fabsf(audio[i]);
        if (abs_val > peak) peak = abs_val;
    }
    if (peak > 1e-6f) {
        float scale = 0.9f / peak;
        for (uint32_t i = 0; i < audio_pos; i++) {
            audio[i] *= scale;
        }
    }

    /* Copy to output (caller owns the memory) */
    output->audio_samples = (float*)nimcp_calloc(audio_pos, sizeof(float));
    if (output->audio_samples) {
        memcpy(output->audio_samples, audio, audio_pos * sizeof(float));
    }
    output->num_samples = audio_pos;
    output->sample_rate = synth->config.sample_rate;

    output->visemes = (viseme_event_t*)nimcp_calloc(vis_count, sizeof(viseme_event_t));
    if (output->visemes) {
        memcpy(output->visemes, visemes, vis_count * sizeof(viseme_event_t));
    }
    output->num_visemes = vis_count;
    output->duration_ms = time_ms;

    synth->words_synthesized++;
    synth->samples_generated += audio_pos;

    return 0;
}

void speech_output_free(speech_output_t* output) {
    if (!output) return;
    nimcp_free(output->audio_samples);
    nimcp_free(output->visemes);
    memset(output, 0, sizeof(speech_output_t));
}

/*=============================================================================
 * SPEECH BRIDGE INTEGRATION
 *===========================================================================*/

int formant_synth_speak_word(formant_synth_t* synth,
                              snn_speech_bridge_t* bridge,
                              uint32_t word_pop_index,
                              speech_output_t* output) {
    if (!synth || !bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "formant_synth_speak_word: NULL parameter (synth=%p bridge=%p output=%p)",
            (void*)synth, (void*)bridge, (void*)output);
        return -1;
    }

    /* Get phoneme sequence from the speech bridge */
    /* phoneme_t is the same integer type as our uint32_t phoneme IDs */
    uint32_t phonemes[SYNTH_MAX_PHONEMES_PER_WORD];
    uint32_t num_phonemes = 0;

    /* Use the bridge's produce_word to get the phoneme sequence.
     * We cast to the bridge's phoneme_t type which is compatible. */
    /* Note: snn_speech_bridge_produce_word declared in nimcp_snn_speech_bridge.h */
    extern int snn_speech_bridge_produce_word(
        snn_speech_bridge_t* bridge,
        uint32_t word_pop_index,
        void* phoneme_out,        /* phoneme_t* */
        uint32_t* num_phonemes_out,
        uint32_t max_phonemes
    );

    int rc = snn_speech_bridge_produce_word(bridge, word_pop_index,
                                             phonemes, &num_phonemes,
                                             SYNTH_MAX_PHONEMES_PER_WORD);
    if (rc != 0 || num_phonemes == 0) return -1;

    return formant_synth_speak_phonemes(synth, phonemes, num_phonemes, output);
}

/*=============================================================================
 * TRAINING (voice parameter learning)
 *===========================================================================*/

int formant_synth_train_step(formant_synth_t* synth,
                              const uint32_t* phoneme_ids,
                              uint32_t num_phonemes,
                              const float* target_audio,
                              uint32_t num_target_samples) {
    if (!synth || !phoneme_ids || !target_audio) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "formant_synth_train_step: NULL parameter (synth=%p phoneme_ids=%p target_audio=%p)",
            (void*)synth, (const void*)phoneme_ids, (const void*)target_audio);
        return -1;
    }

    voice_params_t* voice = &synth->voice[synth->active_language];
    if (voice->frozen) return 0;

    /* Synthesize current output */
    speech_output_t current;
    if (formant_synth_speak_phonemes(synth, phoneme_ids, num_phonemes, &current) != 0)
        return -1;

    /* Compute loss: mean squared error between synthesized and target */
    uint32_t compare_len = current.num_samples < num_target_samples ?
                           current.num_samples : num_target_samples;
    if (compare_len == 0) {
        speech_output_free(&current);
        return 0;
    }

    float mse = 0.0f;
    for (uint32_t i = 0; i < compare_len; i++) {
        float diff = current.audio_samples[i] - target_audio[i];
        mse += diff * diff;
    }
    mse /= (float)compare_len;

    /* Numerical gradient descent on vowel/consonant params.
     * Perturb each relevant parameter, measure loss change, update.
     * This is slower than analytic gradients but works without
     * differentiable resonator implementation. */
    float lr = voice->learning_rate;
    float epsilon = 1e-3f;

    /* Only update params for phonemes in this utterance */
    for (uint32_t p = 0; p < num_phonemes; p++) {
        uint32_t pid = phoneme_ids[p];
        if (pid >= 38) continue;

        if (pid < 12) {
            /* Vowel: update F1, F2, F3, bandwidth, duration */
            for (int j = 0; j < SYNTH_VOWEL_PARAMS; j++) {
                float original = voice->vowel_params[pid][j];

                /* Forward perturbation */
                voice->vowel_params[pid][j] = original + epsilon;
                speech_output_t perturbed;
                formant_synth_speak_phonemes(synth, phoneme_ids, num_phonemes, &perturbed);

                float mse_plus = 0.0f;
                uint32_t clen = perturbed.num_samples < num_target_samples ?
                                perturbed.num_samples : num_target_samples;
                for (uint32_t i = 0; i < clen; i++) {
                    float d = perturbed.audio_samples[i] - target_audio[i];
                    mse_plus += d * d;
                }
                if (clen > 0) mse_plus /= (float)clen;
                speech_output_free(&perturbed);

                /* Gradient and update */
                float grad = (mse_plus - mse) / epsilon;
                voice->vowel_params[pid][j] = original - lr * grad;

                /* Clamp to reasonable ranges */
                if (j < 3) {
                    /* Formant frequencies: 100-8000 Hz */
                    if (voice->vowel_params[pid][j] < 100.0f)
                        voice->vowel_params[pid][j] = 100.0f;
                    if (voice->vowel_params[pid][j] > 8000.0f)
                        voice->vowel_params[pid][j] = 8000.0f;
                } else if (j == 3) {
                    /* Bandwidth: 20-500 Hz */
                    if (voice->vowel_params[pid][j] < 20.0f)
                        voice->vowel_params[pid][j] = 20.0f;
                    if (voice->vowel_params[pid][j] > 500.0f)
                        voice->vowel_params[pid][j] = 500.0f;
                } else {
                    /* Duration: 20-300 ms */
                    if (voice->vowel_params[pid][j] < 20.0f)
                        voice->vowel_params[pid][j] = 20.0f;
                    if (voice->vowel_params[pid][j] > 300.0f)
                        voice->vowel_params[pid][j] = 300.0f;
                }
            }
        }
        /* Consonant param training follows same pattern but is less critical
         * since consonant quality is dominated by noise/burst characteristics
         * which are harder to match via formant perturbation.
         * Skipped for now — focus training budget on vowels. */
    }

    voice->training_steps++;
    speech_output_free(&current);
    return 0;
}

void formant_synth_freeze(formant_synth_t* synth, bool frozen) {
    if (!synth) return;
    for (int i = 0; i < SYNTH_LANG_COUNT; i++) {
        synth->voice[i].frozen = frozen;
    }
}

uint64_t formant_synth_get_training_steps(const formant_synth_t* synth) {
    if (!synth) return 0;
    return synth->voice[synth->active_language].training_steps;
}

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

#define VOICE_MAGIC 0x564F4943  /* "VOIC" */
#define VOICE_VERSION 1

int formant_synth_save_voice(const formant_synth_t* synth, const char* path) {
    if (!synth || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "formant_synth_save_voice: NULL parameter (synth=%p path=%p)",
            (const void*)synth, (const void*)path);
        return -1;
    }

    FILE* fp = fopen(path, "wb");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO,
            "formant_synth_save_voice: failed to open file for writing: %s", path);
        return -1;
    }

    uint32_t magic = VOICE_MAGIC;
    uint32_t version = VOICE_VERSION;
    fwrite(&magic, 4, 1, fp);
    fwrite(&version, 4, 1, fp);

    /* Save all language voice params */
    for (int i = 0; i < SYNTH_LANG_COUNT; i++) {
        fwrite(&synth->voice[i], sizeof(voice_params_t), 1, fp);
    }

    fclose(fp);
    LOG_INFO("Voice parameters saved to %s (%d languages)",
             path, SYNTH_LANG_COUNT);
    return 0;
}

int formant_synth_load_voice(formant_synth_t* synth, const char* path) {
    if (!synth || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "formant_synth_load_voice: NULL parameter (synth=%p path=%p)",
            (void*)synth, (const void*)path);
        return -1;
    }

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO,
            "formant_synth_load_voice: failed to open file for reading: %s", path);
        return -1;
    }

    uint32_t magic, version;
    if (fread(&magic, 4, 1, fp) != 1 || magic != VOICE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO,
            "formant_synth_load_voice: invalid magic number in voice file: %s", path);
        fclose(fp);
        return -1;
    }
    if (fread(&version, 4, 1, fp) != 1 || version != VOICE_VERSION) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO,
            "formant_synth_load_voice: unsupported voice file version in: %s", path);
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < SYNTH_LANG_COUNT; i++) {
        if (fread(&synth->voice[i], sizeof(voice_params_t), 1, fp) != 1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO,
                "formant_synth_load_voice: truncated voice data in: %s", path);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    LOG_INFO("Voice parameters loaded from %s", path);
    return 0;
}
